#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>

#include <termios.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/wait.h>

#include <pthread.h>

#define INPUT_BUFFER_SIZE 1

static int shell_flag = 0;
char line_feed[2] = {0x0D, 0x0A};

struct termios oldattr, newattr;
int shell_pid;
// Shell running is not necessary given that we expect a SIGPIPE after sending a SIGINT to terminal (, which gets forwarded to shell)
// We kept it here only for the purpose of not sending SIGINT or SIGUP repeatly to shell after one SIGINT's already sent.
int shell_running = 0;

pthread_mutex_t exit_mutex;

// stdout pipe from the child process's perspective 
int stdout_pipe[2];
int stdin_pipe[2];

int my_exit_call(int status) {
  pthread_mutex_lock(&exit_mutex);
  exit(status);
  pthread_mutex_unlock(&exit_mutex);  
}

int map_and_write_buffer(int fd, char * buffer, int current_buffer_len)
{
  int res = 0;
  int temp = 0;
  int i = 0;
  for (i = 0; i < current_buffer_len; i++) {
    if (*(buffer + i) == 0x0D || *(buffer + i) == 0x0A) {
      // This is for mapping the <cr> and <lf> characters
      temp = write(fd, line_feed, 2);
    } else if (*(buffer + i) == 4) {
      // Receiving EOF from the terminal
      if (shell_flag && shell_running) {
        close(stdout_pipe[1]);
        close(stdin_pipe[0]);
        kill(shell_pid, SIGHUP);
        my_exit_call(0);
      } else {
        my_exit_call(0);
      }
    } else if (*(buffer + i) == 3) {
      // Receiving Ctrl + C from terminal, this can only happen when ISIG is on
      // This code is duplicated in the SIGINT handler, in case that ISIG is off
      // Such duplication may or may not be desired: we may want ONLY Ctrl + C to trigger SIGINT on shell, and SIGINT on terminal to behave as it should behave
      // Or we may want BOTH Ctrl + C and SIGINT to trigger SIGINT on shell...I see this as not clearly defined by the spec...
      if (shell_flag && shell_running) {
        kill(shell_pid, SIGINT);
        shell_running = 0;
      } else {
        temp = write(fd, buffer + i, 1);
      }
    }else {
      temp = write(fd, buffer + i, 1);
    }
    
    // This returns the number of characters written or -1 for error, though this return value is not used by the code
    if (temp > 0) {
      res += temp;
    } else {
      return temp;
    }
  }
  return res;
}

static void sigint_handler(int signo)
{
  if (shell_running) {
    kill(shell_pid, SIGINT);
    shell_running = 0;
  }
}

static void sigpipe_handler(int signo)
{
  my_exit_call(1);
}

void exit_function ()
{
  if (shell_flag) {
    int shell_status;
    waitpid(shell_pid, &shell_status, 0);

    if (WIFEXITED(shell_status)) {
      printf("Shell exits with status: %d\n", WEXITSTATUS(shell_status));
    } else if (WIFSIGNALED(shell_status)) {
      printf("Shell was signalled to exit! Signo: %d\n", WTERMSIG(shell_status));
    }
  }
  
  // Restore the shell status
  tcsetattr(STDIN_FILENO, TCSANOW, &oldattr);
}

void *read_input_from_child(void *param)
{
  int *pipe = (int *)param;
  char buffer[INPUT_BUFFER_SIZE] = {};

  while (1) {
    int read_size = read(*pipe, buffer, INPUT_BUFFER_SIZE);
    if (read_size > 0) {
      int i = 0;
      for (i = 0; i < read_size; i++) {
        // Receiving EOF "character" from shell, essentially doing the same thing the same as the sigpipe handler
        if (*(buffer + i) == 4) {
          my_exit_call(0);
        } else {
          write(1, buffer + i, 1);
        }
      }
    } else {
      my_exit_call(1);
      // We don't want to call "exit" in the pipe-read thread as well as the main thread, so main thread handles the exit if EOF is received
      //exit(1);
    }
  }

  return NULL;
}

int main(int argc, char **argv)
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

  if (tcgetattr(STDIN_FILENO, &oldattr) == -1) {
    perror("Cannot get attr of terminal!");
    // a custom exit status for all other unexpected errors
    exit(3);
  }
  newattr = oldattr;

  // we call atexit after the oldattr's set
  atexit(exit_function);

  // This is the expected flags
  // The ISIG bit defines if the terminal sends SIGINT or not when Ctrl+C, or EOF or not when Ctrl+D
  newattr.c_lflag &= ~(ICANON | ECHO);

  if (tcsetattr(STDIN_FILENO, TCSANOW, &newattr) == -1) {
    perror("Cannot set attr of terminal!");
    exit(3);
  }

  char buffer[INPUT_BUFFER_SIZE] = {};

  if (!shell_flag) {
    // current_buffer_len is basically for show, as explained in the "while"
    int current_buffer_len = 0;
    while (1) {
      int read_size = read(0, buffer, INPUT_BUFFER_SIZE);
      if (read_size > 0) {
        current_buffer_len += read_size;
        map_and_write_buffer(1, buffer, current_buffer_len);
        current_buffer_len = 0;
      }
       
      // For the requirement of "do something 'reasonable' when we reach the size of the buffer"
      // With the code as-is, the buffer's not supposed to overflow: if input's larger than buffer size, while-read will be executed again; this path's just for show.
      // I find the "do something reasonable when buffer overflows" to be too vague
      if (current_buffer_len == INPUT_BUFFER_SIZE) {
        map_and_write_buffer(1, buffer, current_buffer_len);
        current_buffer_len = 0; 
      }
    }
  } else {
    if (pipe(stdin_pipe) == -1) {
      perror("Cannot instantiate stdin_pipe!");
      exit(3);
    }

    if (pipe(stdout_pipe) == -1) {
      perror("Cannot instantiate stdout_pipe!");
      exit(3);
    }

    pid_t child_pid;
    child_pid = fork();

    if (child_pid >= 0) {
      if(child_pid == 0) {
        // Child
        close(stdout_pipe[1]);
        close(stdin_pipe[0]);

        close(1);
        dup(stdin_pipe[1]);
        close(2);
        dup(stdin_pipe[1]);
        close(stdin_pipe[1]);

        close(0);
        dup(stdout_pipe[0]);
        close(stdout_pipe[0]);

        int rc = execl("/bin/bash", "/bin/bash", NULL);
      } else {
        // Parent
        shell_pid = child_pid;
        signal(SIGINT, sigint_handler);
        signal(SIGPIPE, sigpipe_handler);

        close(stdout_pipe[0]);
        close(stdin_pipe[1]);

        // We create a thread to handle the input pipe from child process
        pthread_t child_input_thread;
        pthread_mutex_init(&exit_mutex, NULL);
        pthread_create(&child_input_thread, NULL, read_input_from_child, &stdin_pipe[0]);
        shell_running = 1;

        while (1) {
          int read_size = read(0, buffer, INPUT_BUFFER_SIZE);
          if (read_size > 0) {
            // If shell is not running (killed/exits), we would receive a SIGPIPE
            write(stdout_pipe[1], buffer, read_size);
            // We write the received character to terminal's stdout second, so in the case of SIGPIPE, the character triggering 
            // SIGPIPE (any character you enter after the shell's killed), will not be printed
            map_and_write_buffer(1, buffer, read_size);
          }
        }
      }
    } else {
      perror("Fork failed!");
      exit(3);
    }
  }

  return 0;
}