#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include <string.h>

#define _GNU_SOURCE
#include <pthread.h>

int num_threads;
int num_iterations;
char opt_sync;
int opt_yield;

long long counter;
pthread_mutex_t add_mutex;

void add(long long *pointer, long long value) {
  long long sum = *pointer + value;
  if (opt_yield)
    // remember to change back to pthread_yield on Linux
    pthread_yield_np();
  *pointer = sum;
}

void* thread_func() {
  int i = 0;
  for (i = 0; i < num_iterations; i++) {
    switch (opt_sync) {
      case 's':
        add(&counter, 1);
        break;
      case 'm':
        pthread_mutex_lock(&add_mutex);
        add(&counter, 1);
        pthread_mutex_unlock(&add_mutex);
        break;
      case 'c':
        break;
      default:
        break;  
    }
  }
  return NULL;
}

int main(int argc, char **argv)
{
  int c;

  static struct option long_options[] = {
    {"yield",        no_argument,       &opt_yield,  'y'},
    {"threads",      required_argument, 0,           't'},
    {"iterations",   required_argument, 0,           'i'},
    {"sync",         required_argument, 0,           's'},
  };

  while (1) {
    int option_index = 0;

    c = getopt_long(argc, argv, "yt:i:s:", long_options, &option_index);
    if (c == -1)
      break;
    
    switch (c) {
      case 't':
        num_threads = atoi(optarg);
        break;
      case 'i':
        num_iterations = atoi(optarg);
        break;
      case 's':
        opt_sync = optarg[0];
    }
  }

  printf("sync method: %c\n", opt_sync);

  int i = 0;
  for (i = 0; i < num_threads; i++) {

  }

  if (opt_sync == 'm') {
    pthread_mutex_init(&add_mutex, NULL);
  }

  for (i = 0; i < num_threads; i++) {

  }


  return 0;
}