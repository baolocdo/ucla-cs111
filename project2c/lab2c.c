#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

#include <pthread.h>

#include "SortedList.h"

#define DEBUG_MSG_BUFFER_SIZE 512
#define KEY_STR_LENGTH 3

int num_threads = 1;
int num_iterations = 1;
int num_lists = 1;

char opt_sync;
int opt_yield = 0;

pthread_mutex_t *list_mutex;
int *list_spin;

pthread_t *threads;

SortedListElement_t *list_head;
SortedListElement_t *list_elements;

int hash_key(const char *key, int len) {
  int i = 0;
  int product = 0;
  for (i = 0; i < len; i++) {
    product += key[i];
  }
  return product % num_lists;
}

void* thread_func(void *param) {
  int *offset = (int *)param;
  int i = 0;
  int bucket = 0;

  // insert
  for (i = 0; i < num_iterations; i++) {
    bucket = hash_key(list_elements[*offset + i].key, KEY_STR_LENGTH);
    switch (opt_sync) {
      case 'm':
        pthread_mutex_lock(&list_mutex[bucket]);
        SortedList_insert(&list_head[bucket], &list_elements[*offset + i]);
        pthread_mutex_unlock(&list_mutex[bucket]);
        break;
      case 's':
        while (__sync_lock_test_and_set(&list_spin[bucket], 1));
        SortedList_insert(&list_head[bucket], &list_elements[*offset + i]);
        __sync_lock_release(&list_spin[bucket]);
        break;
      default:
        SortedList_insert(&list_head[bucket], &list_elements[*offset + i]);
        break;
    }
  }
  
  // get length: we are not using the length for anything here
  int length = 0;
  switch (opt_sync) {
    case 'm':
      for (i = 0; i < num_lists; i++) {
        pthread_mutex_lock(&list_mutex[i]);
        length += SortedList_length(&list_head[i]);
        pthread_mutex_unlock(&list_mutex[i]);
      }
      break;
    case 's':
      for (i = 0; i < num_lists; i++) {
        while (__sync_lock_test_and_set(&list_spin[i], 1));
        length += SortedList_length(&list_head[i]);
        __sync_lock_release(&list_spin[i]);
      }
      break;
    default:
      for (i = 0; i < num_lists; i++) {
        length += SortedList_length(&list_head[i]);
      }
      break;
  }

  // look up and delete
  for (i = 0; i < num_iterations; i++) {
    SortedListElement_t *element;
    bucket = hash_key(list_elements[*offset + i].key, KEY_STR_LENGTH);
    switch (opt_sync) {
      case 'm':
        pthread_mutex_lock(&list_mutex[bucket]);
        element = SortedList_lookup(&list_head[bucket], list_elements[*offset + i].key);
        SortedList_delete(element);
        pthread_mutex_unlock(&list_mutex[bucket]);
        break;
      case 's':
        while (__sync_lock_test_and_set(&list_spin[bucket], 1));
        element = SortedList_lookup(&list_head[bucket], list_elements[*offset + i].key);
        SortedList_delete(element);
        __sync_lock_release(&list_spin[bucket]);
        break;
      default:
        element = SortedList_lookup(&list_head[bucket], list_elements[*offset + i].key);
        // by removing the condition, we may "force" a segfault here if the lookup results in NULL
        //if (element != NULL) {
        SortedList_delete(element);
        //}
        break;
    }
  }

  return NULL;
}

int main(int argc, char **argv)
{
  int c;
  int i = 0;
  int j = 0;

  static struct option long_options[] = {
    {"yield",        required_argument, 0,           'y'},
    {"threads",      required_argument, 0,           't'},
    {"iterations",   required_argument, 0,           'i'},
    {"sync",         required_argument, 0,           's'},
    {"lists",        required_argument, 0,           'l'}
  };

  while (1) {
    int option_index = 0;

    c = getopt_long(argc, argv, "y:t:i:s:l:", long_options, &option_index);
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
        break;
      case 'l':
        num_lists = atoi(optarg);
        break;
      case 'y':
        for (i = 0; i < strlen(optarg); i++) {
          if (optarg[i] == 'i')
            opt_yield |= INSERT_YIELD;
          if (optarg[i] == 'd')
            opt_yield |= DELETE_YIELD;
          if (optarg[i] == 's')
            opt_yield |= SEARCH_YIELD;
        }
        break;
    }
  }

  int ret = 0;
  threads = (pthread_t *)malloc(sizeof(pthread_t) * num_threads);

  // initialize empty list
  list_head = (SortedListElement_t *)malloc(sizeof(SortedListElement_t) * num_lists);
  for (i = 0; i < num_lists; i++) {
    list_head[i].next = &list_head[i];
    list_head[i].key = NULL;
    list_head[i].prev = &list_head[i];
  }

  // initialize list elements
  int num_elements = num_threads * num_iterations;
  list_elements = (SortedListElement_t *)malloc(sizeof(SortedListElement_t) * num_elements);
  
  for (i = 0; i < num_elements; i++) {
    char *key_str = (char *)malloc(sizeof(char) * (KEY_STR_LENGTH + 1));
    for (j = 0; j < KEY_STR_LENGTH; j++) {
      // key ranges from a...z
      key_str[j] = rand() % 26 + 65;
    }
    list_elements[i].key = key_str;
  }
  
  // assign each thread a certain point in the element array to work with
  int *num_elements_offsets = (int *)malloc(sizeof(int) * num_threads);
  for (i = 0; i < num_threads; i++) {
    num_elements_offsets[i] = i * num_iterations;
  }
  
  // Start timer; we include lock initialization time as well
  struct timespec start_time;
  clock_gettime(CLOCK_MONOTONIC, &start_time);

  if (opt_sync == 'm') {
    list_mutex = (pthread_mutex_t *)malloc(num_lists * sizeof(pthread_mutex_t));
    for (i = 0; i < num_lists; i++) {
      pthread_mutex_init(&list_mutex[i], NULL);
    }
  }

  if (opt_sync == 's') {
    list_spin = (int *)malloc(num_lists * sizeof(int));
    for (i = 0; i < num_lists; i++) {
      list_spin[i] = 0;
    }
  }

  for (i = 0; i < num_threads; i++) {
    ret = pthread_create(&(threads[i]), NULL, thread_func, &num_elements_offsets[i]);
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
  
  char debug_msg[DEBUG_MSG_BUFFER_SIZE] = {};
  int size = 0;
  
  long long elasped_time_ns = (end_time.tv_sec - start_time.tv_sec) * 1000000000;
  elasped_time_ns += end_time.tv_nsec;
  elasped_time_ns -= start_time.tv_nsec;
  
  long long num_operations = num_elements * 2;
  size = sprintf(debug_msg, "%d threads x %d iterations x (insert + lookup/delete) = %lld operations\n", num_threads, num_iterations, num_operations);
  write(1, debug_msg, size);

  size = sprintf(debug_msg, "elasped time: %lldns\n", elasped_time_ns);
  write(1, debug_msg, size);

  // Here we apply the "correction"
  size = sprintf(debug_msg, "per operation: %fns\n", (float)(elasped_time_ns / num_operations / ((float)num_elements / num_lists)));
  write(1, debug_msg, size);
  
  // All sub lists are expected to be empty after we are done
  int list_length = 0;
  for (i = 0; i < num_lists; i++) {
    list_length = SortedList_length(&list_head[i]);
    if (list_length != 0) {
      size = sprintf(debug_msg, "ERROR: final count = %d\n", list_length);
      write(2, debug_msg, size);
      ret = 1;
      break;
    }
  }

  // free what we allocated on heap: these are not needed as we are exiting the program anyway;
  free(threads);
  free(num_elements_offsets);
  free(list_head);
  if (opt_sync == 'm') {
    free(list_mutex);
  } else if (opt_sync == 's') {
    free(list_spin);
  }
  return ret;
}