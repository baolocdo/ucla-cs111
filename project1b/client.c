#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include <string.h>

#include <termios.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/wait.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#include <mcrypt.h>

#define INPUT_BUFFER_SIZE 1
#define RECEIVE_BUFFER_SIZE 512
#define LOG_MSG_BUFFER_SIZE 1024

struct termios oldattr, newattr;
pthread_mutex_t exit_mutex;
int log_fd = -1;

static int encrypt_flag;
MCRYPT td;

// The exit call is wrapped in critical section, as it may be called from multiple threads;
// We don't want to have the potential of calling two exits at the same time, which will mess up with the atexit's registered function.
// This is inherited from lab1a code
int my_exit_call(int status) {
  pthread_mutex_lock(&exit_mutex);
  exit(status);
  pthread_mutex_unlock(&exit_mutex);  
}

void exit_function ()
{
  // Restore the shell status
  tcsetattr(STDIN_FILENO, TCSANOW, &oldattr);
}

void *read_input_from_server(void *param)
{
  int *sock_fd = (int *)param;
  char buffer[RECEIVE_BUFFER_SIZE + 1] = {};
  char log_msg[LOG_MSG_BUFFER_SIZE] = {};

  while (1) {
    int read_size = read(*sock_fd, buffer, RECEIVE_BUFFER_SIZE);
    if (read_size > 0) {
      buffer[read_size] = 0;
      if (log_fd > 0) {
        int log_msg_size = sprintf(log_msg, "RECEIVED %d BYTES: %s\n", read_size, buffer);
        write(log_fd, log_msg, log_msg_size);
      }
      
      if (encrypt_flag) {
        mdecrypt_generic(td, buffer, read_size);
      }

      int write_size = write(1, buffer, read_size);
      if (write_size <= 0) {
        // Socket write error
        my_exit_call(1);
      }
    } else {
      // Socket read error or EOF
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
  struct sockaddr_in serv_addr;

  static struct option long_options[] = {
    {"encrypt", no_argument,       &encrypt_flag, 'e'},
    {"port",    required_argument, 0,             'p'},
    {"log",     required_argument, 0,             'l'}
  };

  while (1) {
    int option_index = 0;

    c = getopt_long(argc, argv, "ep:l:", long_options, &option_index);
    if (c == -1)
      break;
    switch (c) {
      case 'p':
        port_num = atoi(optarg);
        break;
      case 'l':
        log_fd = creat(optarg, 0666);
        break;
    }
  }

  if (tcgetattr(STDIN_FILENO, &oldattr) == -1) {
    perror("Cannot get attr of terminal!");
    // a custom exit status for all other unexpected errors
    exit(3);
  }

  newattr = oldattr;
  atexit(exit_function);
  newattr.c_lflag &= ~(ICANON);

  if (tcsetattr(STDIN_FILENO, TCSANOW, &newattr) == -1) {
    perror("Cannot set attr of terminal!");
    exit(3);
  }

  sock_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (sock_fd < 0) {
    perror("Error opening socket");
    exit(3);
  }

  memset((char *) &serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(port_num);
  serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

  if (connect(sock_fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
    perror("Error connecting");
    exit(3);
  }

  pthread_t server_input_thread;
  pthread_mutex_init(&exit_mutex, NULL);
  pthread_create(&server_input_thread, NULL, read_input_from_server, &sock_fd);
  
  // mcrypt setup
  int i;
  char *key;
  char password[20] = {};
  char *IV;
  int keysize = 16; /* 128 bits */

  if (encrypt_flag) {
    key = calloc(1, keysize);
    int key_file_fd = open("my.key", O_RDONLY);
    if (key_file_fd < 0) {
      perror("Key file read error.\n");
      exit(3);
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

  char buffer[INPUT_BUFFER_SIZE + 1] = {};
  char log_msg[LOG_MSG_BUFFER_SIZE] = {};

  while (1) {
    // tested with single-size buffer
    int read_size = read(0, buffer, INPUT_BUFFER_SIZE);
    // so that buffer terminates correctly when doing string ops such as sprintf %s
    buffer[read_size] = 0;

    // the Ctrl+D character; this is fine as we've a single-size buffer
    if (buffer[0] == 4) {
      my_exit_call(0);
    }

    if (encrypt_flag) {
      mcrypt_generic(td, buffer, 1);
    }
    
    int sent_size = write(sock_fd, buffer, read_size);
    if (sent_size > 0) {
      if (log_fd > 0) {
        int log_msg_size = sprintf(log_msg, "SENT %d BYTES: %s\n", sent_size, buffer);
        write(log_fd, log_msg, log_msg_size);
      }
    }
  }

  return 0;
}