#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

#include <pthread.h>

#define DEBUG_MSG_BUFFER_SIZE 512

int num_threads = 1;
int num_iterations = 1;
char opt_sync;
int opt_yield;

long long counter = 0;
pthread_mutex_t add_mutex;
int add_spin = 0;
pthread_t *threads;

void add(long long *pointer, long long value) {
  long long sum = *pointer + value;
  if (opt_yield)
    pthread_yield();
  *pointer = sum;
}

void* thread_func() {
  int i = 0;
  long long prev = 0;
  long long sum = 0;

  for (i = 0; i < num_iterations; i++) {
    switch (opt_sync) {
      case 'c':
        do {
          prev = counter;
          sum = prev + 1;
          if (opt_yield) {
            pthread_yield();
          }
        } while (__sync_val_compare_and_swap(&counter, prev, sum) != prev);
        break;
      case 'm':
        pthread_mutex_lock(&add_mutex);
        add(&counter, 1);
        pthread_mutex_unlock(&add_mutex);
        break;
      case 's':
        while (__sync_lock_test_and_set(&add_spin, 1));
        add(&counter, 1);
        __sync_lock_release(&add_spin);
        break;
      default:
        add(&counter, 1);
        break;
    }
  }

  for (i = 0; i < num_iterations; i++) {
    switch (opt_sync) {
      case 'c':
        do {
          prev = counter;
          sum = prev - 1;
          if (opt_yield) {
            pthread_yield();
          }
        } while (__sync_val_compare_and_swap(&counter, prev, sum) != prev);
        break;
      case 'm':
        pthread_mutex_lock(&add_mutex);
        add(&counter, -1);
        pthread_mutex_unlock(&add_mutex);
        break;
      case 's':
        while (__sync_lock_test_and_set(&add_spin, 1));
        add(&counter, -1);
        __sync_lock_release(&add_spin);
        break;
      default:
        add(&counter, -1);
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

  int i = 0;
  int ret = 0;
  threads = (pthread_t *)malloc(sizeof(pthread_t) * num_threads);

  struct timespec start_time;
  clock_gettime(CLOCK_MONOTONIC, &start_time);

  if (opt_sync == 'm') {
    pthread_mutex_init(&add_mutex, NULL);
  }

  for (i = 0; i < num_threads; i++) {
    ret = pthread_create(&(threads[i]), NULL, thread_func, NULL);
    if (ret < 0) {
      exit(2);
    }
  }

  for (i = 0; i < num_threads; i++) {
    ret = pthread_join(threads[i], NULL);
    if (ret < 0) {
      exit(2);
    }
  }

  struct timespec end_time;
  clock_gettime(CLOCK_MONOTONIC, &end_time);
  
  long long elasped_time_ns = (end_time.tv_sec - start_time.tv_sec) * 1000000000;
  elasped_time_ns += end_time.tv_nsec;
  elasped_time_ns -= start_time.tv_nsec;
  
  char debug_msg[DEBUG_MSG_BUFFER_SIZE] = {};
  long long num_operations = num_threads * num_iterations * 2;
  int size = sprintf(debug_msg, "%d threads x %d iterations x (add + subtract) = %lld operations\n", num_threads, num_iterations, num_operations);
  write(1, debug_msg, size);
  
  if (counter != 0) {
    size = sprintf(debug_msg, "ERROR: final count = %lld\n", counter);
    write(2, debug_msg, size);
    ret = 1;
  } else {
    ret = 0;
  }

  size = sprintf(debug_msg, "elasped time: %lldns\n", elasped_time_ns);
  write(1, debug_msg, size);

  size = sprintf(debug_msg, "per operation: %lldns\n", elasped_time_ns / num_operations);
  write(1, debug_msg, size);

  free(threads);
  return ret;
}