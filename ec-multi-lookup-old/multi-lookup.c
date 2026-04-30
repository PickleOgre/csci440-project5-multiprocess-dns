/*
 * File: multi-lookup.c
 * Author: Josiah Lawrence
 * Project: CSCI 3753 Programming Assignment 2
 * Create Date: 2026/04/15
 * Description:
 * 	This file contains the threaded
 *      solution to this assignment.
 *  Extra credit implemented:
 *    - Match # processes to cores
 *
 */

#include "multi-lookup.h"

void*
request_thr(void* arg)
{
  RequesterArgs* args = (RequesterArgs*)arg;
  char hostname[MAX_NAME_LENGTH];
  srand(time(NULL));

  /* Loop through file and push names to queue */
  while (fscanf(args->input_file, INPUTFS, hostname) != EOF) {
    while (1) {
      pthread_mutex_lock(args->queue_lock);
      if (queue_is_full(args->requestq)) {
        pthread_mutex_unlock(args->queue_lock);

        int microseconds = rand() % 101;
        struct timespec ts = { .tv_sec = 0, .tv_nsec = microseconds * 1000 };
        nanosleep(&ts, NULL);

        continue;
      } else {
        char* hostname_heap = malloc(MAX_NAME_LENGTH);
        strncpy(hostname_heap, hostname, MAX_NAME_LENGTH);
        queue_push(args->requestq, hostname_heap);
        pthread_mutex_unlock(args->queue_lock);
        break;
      }
    }
  }

  /* All lines read -> close file */
  fclose(args->input_file);

  /* Decrement the active files counter */
  pthread_mutex_lock(args->counter_lock);
  (*args->active_files)--;
  pthread_mutex_unlock(args->counter_lock);

  return NULL;
}

void*
resolve_thr(void* arg)
{
  ResolverArgs* args = (ResolverArgs*)arg;
  srand(time(NULL));
  char* hostname;
  char firstipstr[INET6_ADDRSTRLEN];

  while (1) {
    pthread_mutex_lock(args->queue_lock);
    if (queue_is_empty(args->requestq)) {
      pthread_mutex_lock(args->counter_lock);
      if (*args->active_files == 0) {
        // No files active -> done.
        pthread_mutex_unlock(args->queue_lock);
        pthread_mutex_unlock(args->counter_lock);
        break;
      } else {
        // Queue empty but requesters running:
        // unlock and sleep.
        pthread_mutex_unlock(args->queue_lock);
        pthread_mutex_unlock(args->counter_lock);

        int microseconds = rand() % 101;
        struct timespec ts = { .tv_sec = 0, .tv_nsec = microseconds * 1000 };
        nanosleep(&ts, NULL);

        continue;
      }
    } else {
      // Pop a name from the queue
      hostname = (char*)queue_pop(args->requestq);
      pthread_mutex_unlock(args->queue_lock);
    }

    /* Lookup hostname and get IP string */
    if (dnslookup(hostname, firstipstr, sizeof(firstipstr)) == UTIL_FAILURE) {
      fprintf(stderr, "dnslookup error: %s\n", hostname);
      strncpy(firstipstr, "", sizeof(firstipstr));
    }

    /* Write to Output File */
    pthread_mutex_lock(args->output_lock);
    fprintf(args->output_file, "%s,%s\n", hostname, firstipstr);
    pthread_mutex_unlock(args->output_lock);

    // free the hostname buffer
    free(hostname);
  }
  return NULL;
}

int
main(int argc, char* argv[])
{
  FILE* input_files[MAX_INPUT_FILES];
  FILE* output_file = NULL;
  int num_files;    // The number of name files
  int active_files; // The number of uncompleted name files
  char errorstr[SBUFSIZE];
  pthread_t requester_threads[MAX_INPUT_FILES];
  pthread_t resolver_threads[MAX_RESOLVER_THREADS];
  queue requestq;
  pthread_mutex_t queue_lock;
  pthread_mutex_t output_lock;
  pthread_mutex_t counter_lock;
  RequesterArgs requester_args[MAX_INPUT_FILES];
  int i;
  long nprocs = sysconf(_SC_NPROCESSORS_ONLN);
  int num_resolvers;

  /* Check Arguments */
  if (argc < MINARGS) {
    fprintf(stderr, "Not enough arguments: %d\n", (argc - 1));
    fprintf(stderr, "Usage:\n %s %s\n", argv[0], USAGE);
    return EXIT_FAILURE;
  };

  /* Open the output file */
  output_file = fopen(argv[(argc - 1)], "w");
  if (!output_file) {
    perror("Error Opening Output File");
    return EXIT_FAILURE;
  }

  /* Open the input files and */
  /* create array of file pointers */
  num_files = 0;
  for (i = 0; i < argc - 2; i++) {
    input_files[num_files] = fopen(argv[i + 1], "r");
    if (!input_files[num_files]) {
      sprintf(errorstr, "Error Opening Input File: %s", argv[i + 1]);
      perror(errorstr);
      continue;
    }
    num_files++;
  }

  /* Initialize shared state */
  queue_init(&requestq, QUEUE_SIZE);
  pthread_mutex_init(&queue_lock, NULL);
  pthread_mutex_init(&output_lock, NULL);
  pthread_mutex_init(&counter_lock, NULL);
  active_files = num_files;

  /* Spawn resolver thread pool */
  num_resolvers = (int)nprocs;
  if (num_resolvers < MIN_RESOLVER_THREADS) num_resolvers = MIN_RESOLVER_THREADS;
  if (num_resolvers > MAX_RESOLVER_THREADS) num_resolvers = MAX_RESOLVER_THREADS;
  ResolverArgs resolver_arg = { .requestq = &requestq,
                                .queue_lock = &queue_lock,
                                .output_file = output_file,
                                .output_lock = &output_lock,
                                .active_files = &active_files,
                                .counter_lock = &counter_lock };
  for (i = 0; i < num_resolvers; i++) {
    pthread_create(&resolver_threads[i], NULL, resolve_thr, &resolver_arg);
  }

  /* Spawn requester thread pool */
  for (i = 0; i < num_files; i++) {
    requester_args[i] = (RequesterArgs){ .queue_lock = &queue_lock,
                                         .requestq = &requestq,
                                         .active_files = &active_files,
                                         .input_file = input_files[i],
                                         .counter_lock = &counter_lock };
    pthread_create(&requester_threads[i], NULL, request_thr, &requester_args[i]);
  }

  /* Join all threads */
  for (i = 0; i < num_files; i++) {
    pthread_join(requester_threads[i], NULL);
  }
  for (i = 0; i < num_resolvers; i++) {
    pthread_join(resolver_threads[i], NULL);
  }

  /* Close Output File */
  fclose(output_file);

  /* Free queue memory */
  queue_cleanup(&requestq);

  return EXIT_SUCCESS;
}
