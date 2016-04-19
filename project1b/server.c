#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>

#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <string.h>

#include <sys/types.h>
#include <sys/wait.h>

#include <netinet/in.h>
#include <pthread.h>

#include <mcrypt.h>

#define INPUT_BUFFER_SIZE 512
#define MAX_BACK_LOG      3

pthread_mutex_t exit_mutex;

static int encrypt_flag;
MCRYPT td;

struct children_struct {
  int stdout_pipe[2];
  int stdin_pipe[2];
  int child_pid;
  int child_fd;
  pthread_t child_input_thread;
  pthread_t child_output_thread;
} children[MAX_BACK_LOG];

int current_child_num = 0;

// The exit call is wrapped in critical section, as it may be called from multiple threads;
// We don't want to have the potential of calling two exits at the same time, which will mess up with the atexit's registered function.
// This is inherited from lab1a code
int my_exit_call(int status) {
  pthread_mutex_lock(&exit_mutex);
  int i = 0;
  for (i = 0; i < current_child_num; i++) {
    kill(children[i].child_pid, SIGINT);
  }
  exit(status);
  pthread_mutex_unlock(&exit_mutex);  
}

void *write_output_to_child(void *param)
{
  struct children_struct *child = (struct children_struct *)param;
  
  char buffer[INPUT_BUFFER_SIZE] = {};
  while (1) {
    // redirected server's stdin 
    int read_size = read(0, buffer, INPUT_BUFFER_SIZE);

    if (encrypt_flag) {
      mdecrypt_generic(td, buffer, read_size);
    }

    if (read_size > 0) {
      int write_size = write(child->stdout_pipe[1], buffer, read_size);
      if (write_size <= 0) {
        // Shell write error, return 2
        my_exit_call(2);
      }
    } else {
      // network read error or EOF, return 1
      my_exit_call(1);
    }
  }

  return NULL;
}

static void sigpipe_handler(int signo)
{
  // receive sigpipe (from shell), exit with status 2
  my_exit_call(2);
}

void *read_input_from_child(void *param)
{
  struct children_struct *child = (struct children_struct *)param; 
  
  char buffer[INPUT_BUFFER_SIZE] = {};
  while (1) {
    int read_size = read(child->stdin_pipe[0], buffer, INPUT_BUFFER_SIZE);
    
    if (read_size > 0) {
      if (encrypt_flag) {
        mcrypt_generic(td, buffer, read_size);
      }
      // redirected server's stdout
      int write_size = write(1, buffer, read_size);
      if (write_size <= 0) {
        // network write error, return 2
        my_exit_call(1);
      }
    } else {
      // shell read error or EOF, return 1
      my_exit_call(2);
    }
  }

  return NULL;
}

int main(int argc, char **argv)
{
  int c;
  int port_num = 40438;
  int sock_fd = -1;
  struct sockaddr_in serv_addr, cli_addr;
  
  static struct option long_options[] = {
    {"encrypt", no_argument,       &encrypt_flag, 'e'},
    {"port",    required_argument, 0,             'p'}
  };

  while (1) {
    int option_index = 0;

    c = getopt_long(argc, argv, "ep:", long_options, &option_index);
    if (c == -1)
      break;
    switch (c) {
      case 'p':
        port_num = atoi(optarg);
        break;
    }
  }

  sock_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (sock_fd < 0) {
    perror("Error opening socket");
    exit(3);
  }

  memset((char *) &serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = INADDR_ANY;
  serv_addr.sin_port = htons(port_num);
  
  if (bind(sock_fd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
    perror("ERROR on binding");
    exit(3);
  }
  
  pthread_mutex_init(&exit_mutex, NULL);

  listen(sock_fd, MAX_BACK_LOG);
  socklen_t clilen = sizeof(cli_addr);

  signal(SIGPIPE, sigpipe_handler);
  
  // mcrypt setup
  int i;
  char *key;
  char password[20];
  char *IV;
  int keysize = 16; /* 128 bits */

  if (encrypt_flag) {
    key = calloc(1, keysize);
    int key_file_fd = open("my.key", O_RDONLY);
    if (key_file_fd < 0) {
      perror("Key file read error.\n");
    }
    int read_size = read(key_file_fd, password, 20);
    password[read_size] = 0;
    memmove(key, password, strlen(password));

    td = mcrypt_module_open("twofish", NULL, "cfb", NULL);
    if (td == MCRYPT_FAILED) {
      perror("Mcrypt failed.\n");
      exit(3);
    }
    IV = malloc(mcrypt_enc_get_iv_size(td));

    // we make sure to use the same pseudo-random number for IV on both the client and the server.
    //srand(time(0));
    for (i = 0; i < mcrypt_enc_get_iv_size(td); i++) {
      IV[i]=rand();
    }
    i = mcrypt_generic_init(td, key, keysize, IV);
    if (i < 0) {
      mcrypt_perror(i);
      exit(3);
    }
  }

  while (1) {
    int client_fd = accept(sock_fd, (struct sockaddr *) &cli_addr, &clilen);
    if (client_fd < 0) {
       perror("ERROR on accept");
       exit(1);
    }

    children[current_child_num].child_fd = client_fd;

    if (pipe(children[current_child_num].stdin_pipe) == -1) {
      perror("Cannot instantiate stdin_pipe!");
      exit(3);
    }

    if (pipe(children[current_child_num].stdout_pipe) == -1) {
      perror("Cannot instantiate stdout_pipe!");
      exit(3);
    }

    pid_t child_pid;
    child_pid = fork();

    if (child_pid >= 0) {
      if(child_pid == 0) {
        // Child
        close(children[current_child_num].stdout_pipe[1]);
        close(children[current_child_num].stdin_pipe[0]);

        dup2(children[current_child_num].stdin_pipe[1], 1);
        dup2(children[current_child_num].stdin_pipe[1], 2);
        close(children[current_child_num].stdin_pipe[1]);

        dup2(children[current_child_num].stdout_pipe[0], 0);
        close(children[current_child_num].stdout_pipe[0]);

        int rc = execl("/bin/bash", "/bin/bash", NULL);

        if (rc < 0) {
          perror("Error calling execl.\n");
          exit(3);
        }
      } else {
        // Parent
        if (current_child_num > 0) {
          // Too many children! we are not supposed to support this given that server's stdio are mapped
          continue;
        }
        children[current_child_num].child_pid = child_pid;

        close(children[current_child_num].stdout_pipe[0]);
        close(children[current_child_num].stdin_pipe[1]);

        dup2(children[current_child_num].child_fd, 1);
        dup2(children[current_child_num].child_fd, 2);
        dup2(children[current_child_num].child_fd, 0);

        pthread_create(&children[current_child_num].child_input_thread, NULL, read_input_from_child, &children[current_child_num]);
        pthread_create(&children[current_child_num].child_output_thread, NULL, write_output_to_child, &children[current_child_num]);

        // we are done dup'ing the fd, and can close it now
        close(children[current_child_num].child_fd);
      }
    } else {
      perror("Fork failed!");
      exit(3);
    }

    current_child_num ++;
  }
  return 0;
}