#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>

#include <termios.h>

#define INPUT_BUFFER_SIZE 2

static int shell_flag = 0;
char line_feed[2] = {0x0D, 0x0A};

int map_and_write_buffer(char * buffer, int current_buffer_len)
{
  int res = 0;
  int temp = 0;
  for (int i = 0; i < current_buffer_len; i++) {
    if (*(buffer + i) == 0x0D || *(buffer + i) == 0x0A) {
      //printf("Map happened when printing!\n");
      temp = write(1, line_feed, 2);
    } else {
      //printf("%d\n", *(buffer + i));
      temp = write(1, buffer + i, 1);
    }
    
    if (temp > 0) {
      res += temp;
    } else {
      return temp;
    }
  }
  return res;
}

int main (int argc, char **argv)
{
  int c;
  static struct option long_options[] = {
    {"shell",    no_argument,       &shell_flag,              's'}
  };

  while (1) {
    int option_index = 0;

    c = getopt_long(argc, argv, "s", long_options, &option_index);
    if (c == -1)
      break;
  }

  if (!shell_flag) {
    printf("part one!\n");
    struct termios oldattr, newattr;
    char buffer[INPUT_BUFFER_SIZE] = {};

    if (tcgetattr(STDIN_FILENO, &oldattr) == -1) {

    }
    newattr = oldattr;
    newattr.c_lflag &= ~(ICANON | ECHO);

    if (tcsetattr(STDIN_FILENO, TCSANOW, &newattr) == -1) {

    }
    
    int current_buffer_len = 0;
    while (1) {
      if (current_buffer_len == INPUT_BUFFER_SIZE) {
        // TODO: 
        // After reading the Q&A on Piazza, I still don't get "when to output the characters", assuming it to be "when the buffer's filled up",
        // which gives us the problem of "output when control sequence is received"
        map_and_write_buffer(buffer, current_buffer_len);

        // Do something "reasonable" when we reach the size of the buffer
        current_buffer_len = 0; 
      }

      int read_size = read(0, buffer + current_buffer_len, 1);
      if (read_size > 0) {
        if (buffer[current_buffer_len] == 4) {
          // Predefined control sequence Ctrl+D, EOF; we print what's in the buffer, then reset terminal and exit
          map_and_write_buffer(buffer, current_buffer_len);
          break;
        }
        current_buffer_len += read_size;
        // debugging what's in the buffer after each read
        /*
        printf("State of the buffer:\n");
        for (int i = 0; i < current_buffer_len; i++) {
          printf("%d\n", buffer[i]);
        }
        */
      } else {

      }
    }
    
    tcsetattr( STDIN_FILENO, TCSANOW, &oldattr );
  } else {
    
  }

  return 0;
}