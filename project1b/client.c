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

#define INPUT_BUFFER_SIZE 1

struct termios oldattr, newattr;
pthread_mutex_t exit_mutex;

static int encrypt_flag;

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
  char buffer[INPUT_BUFFER_SIZE] = {};

  while (1) {
    int read_size = read(*sock_fd, buffer, INPUT_BUFFER_SIZE);
    if (read_size > 0) {
      write(1, buffer, read_size);
    } else {
      my_exit_call(1);
    }
  }

  return NULL;
}

int main(int argc, char **argv)
{
  int c;
  int log_fd = 1;
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

  char buffer[INPUT_BUFFER_SIZE] = {};
  while (1) {
    int read_size = read(0, buffer, INPUT_BUFFER_SIZE);
    write(sock_fd, buffer, read_size);
  }
  
  return 0;
}