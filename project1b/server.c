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

#define INPUT_BUFFER_SIZE 1
#define MAX_BACK_LOG      3

pthread_mutex_t exit_mutex;

static int encrypt_flag;

struct children_struct {
  int stdout_pipe[2];
  int stdin_pipe[2];
  int child_pid;
  int child_fd;
  pthread_t child_input_thread;
  pthread_t child_output_thread;
} children[MAX_BACK_LOG];

int my_exit_call(int status) {
  pthread_mutex_lock(&exit_mutex);
  exit(status);
  pthread_mutex_unlock(&exit_mutex);  
}

void exit_function ()
{
}

void *write_output_to_child(void *param)
{
  struct children_struct *child = (struct children_struct *)param;
  
  char buffer[INPUT_BUFFER_SIZE] = {};
  while (1) {
    int read_size = read(child->child_fd, buffer, INPUT_BUFFER_SIZE);
    if (read_size > 0) {
      write(child->stdout_pipe[1], buffer, read_size);
    } else {
      my_exit_call(1);
    }
  }

  return NULL;
}

void *read_input_from_child(void *param)
{
  struct children_struct *child = (struct children_struct *)param;
  
  char buffer[INPUT_BUFFER_SIZE] = {};
  while (1) {
    int read_size = read(child->stdin_pipe[0], buffer, INPUT_BUFFER_SIZE);
    if (read_size > 0) {
      write(child->child_fd, buffer, read_size);
    } else {
      my_exit_call(1);
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

  listen(sock_fd, MAX_BACK_LOG);
  socklen_t clilen = sizeof(cli_addr);
  int current_child_num = 0;
  
  while (1) {
    int client_fd = accept(sock_fd, (struct sockaddr *) &cli_addr, &clilen);
    children[current_child_num].child_fd = client_fd;

    if (client_fd < 0) {
       perror("ERROR on accept");
       exit(3);
    }

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

        close(1);
        dup(children[current_child_num].stdin_pipe[1]);
        close(2);
        dup(children[current_child_num].stdin_pipe[1]);
        close(children[current_child_num].stdin_pipe[1]);

        close(0);
        dup(children[current_child_num].stdout_pipe[0]);
        close(children[current_child_num].stdout_pipe[0]);

        int rc = execl("/bin/bash", "/bin/bash", NULL);

        if (rc < 0) {
          perror("Error calling execl.\n");
          exit(3);
        }
      } else {
        // Parent
        children[current_child_num].child_pid = child_pid;

        close(children[current_child_num].stdout_pipe[0]);
        close(children[current_child_num].stdin_pipe[1]);

        pthread_create(&children[current_child_num].child_input_thread, NULL, read_input_from_child, &children[current_child_num]);
        pthread_create(&children[current_child_num].child_output_thread, NULL, write_output_to_child, &children[current_child_num]);
      }
    } else {
      perror("Fork failed!");
      exit(3);
    }

    current_child_num ++;
  }
  return 0;
}