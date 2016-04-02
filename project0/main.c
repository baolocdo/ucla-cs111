#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>

#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>

#define MAX_SIZE 4096

static int segfault_flag = 0;

static void sigsegv_handler(int signo)
{
  fprintf(stderr, "Handler caught a SIGSEGV signal!\n");
  exit(3);
}

int main (int argc, char **argv)
{
  int c;
  // segfault_flag should be static, otherwise "initializer element is not a compile-time constant"
  static struct option long_options[] = {
    {"segfault", no_argument,       &segfault_flag, 's'},
    {"catch",    no_argument,       0,              'c'},
    {"input",    required_argument, 0,              'i'},
    {"output",   required_argument, 0,              'o'}
  };
  int ifd = 0;
  int ofd = 1;

  while (1) {
    int option_index = 0;

    c = getopt_long(argc, argv, "sci:o:", long_options, &option_index);
    if (c == -1)
      break;

    switch (c) {
      case 'c':
        signal(SIGSEGV, sigsegv_handler);
        break;

      case 'i':
        ifd = open(optarg, O_RDONLY);
        if (ifd >= 0) {
          close(0);
          dup(ifd);
          close(ifd);
        } else {
          fprintf(stderr, "Unable to open the specified input file: %s\n", optarg);
          perror("Unable to open the specified input file");
          exit(2);
        }

        break;

      case 'o':
        ofd = creat(optarg, 0666);
        if (ofd >= 0) {
          close(1);
          dup(ofd);
          close(ofd);
        } else {
          fprintf(stderr, "Unable to create the specified output file: %s\n", optarg);
          perror("Unable to create the specified output file");
          exit(2);
        }

        break;
    }
  }

  if (segfault_flag) {
    char *segfault_char = NULL;
    *segfault_char = 3;
  }
  
  int read_size = 0;
  char buf[MAX_SIZE] = {};
  
  // If --input and --output are specified, here stdin and stdout are already redirected
  // Otherwise we are reading from stdin and writing to stdout, and we can use Ctrl+D to simulate EOF, or Ctrl+C to exit
  while ((read_size = read(0, buf, MAX_SIZE)) > 0) {
    write(1, buf, read_size);
  }

  return 0;
}