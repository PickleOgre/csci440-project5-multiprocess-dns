/*
 * File: multi-lookup.c
 * Author: Josiah Lawrence
 * Project: CSCI 440 Final
 * Create Date: 2026/04/15
 * Modify Date: 2026/04/26
 * Description:
 *  This is a threaded implementation of a
 *  bulk DNS lookup program.
 *
 */

#include "multi-lookup.h"

void*
request_thr(void* arg)
{
  RequesterArgs* args = (RequesterArgs*)arg;
  char hostname[MAX_NAME_LENGTH];

  /* Loop through file and push names to queue */
  while (fscanf(args->input_file, INPUTFS, hostname) != EOF) {
    pthread_mutex_lock(args->queue_lock);
    while (queue_is_full(args->requestq)) {
      pthread_cond_wait(args->queue_not_full, args->queue_lock);
    }
    char* hostname_heap = malloc(MAX_NAME_LENGTH);
    strncpy(hostname_heap, hostname, MAX_NAME_LENGTH);
    queue_push(args->requestq, hostname_heap);
    pthread_cond_signal(args->queue_not_empty);
    pthread_mutex_unlock(args->queue_lock);
  }

  /* All lines read -> close file */
  fclose(args->input_file);

  /* Decrement the active files counter */
  pthread_mutex_lock(args->counter_lock);
  (*args->active_files)--;
  pthread_mutex_unlock(args->counter_lock);
  pthread_cond_broadcast(args->queue_not_empty);


  return NULL;
}

void*
resolve_thr(void* arg)
{
  ResolverArgs* args = (ResolverArgs*)arg;
  char* hostname;
  char firstipstr[INET6_ADDRSTRLEN];

  pthread_mutex_lock(args->queue_lock);
  while (1) {
    while (queue_is_empty(args->requestq)) {
      // Check if we're done before sleeping
      pthread_mutex_lock(args->counter_lock);
      int done = (*args->active_files == 0);
      pthread_mutex_unlock(args->counter_lock);
      if (done) {
        pthread_mutex_unlock(args->queue_lock);
        return NULL;
      }
      pthread_cond_wait(args->queue_not_empty, args->queue_lock);
    }
    hostname = (char*)queue_pop(args->requestq);
    pthread_cond_signal(args->queue_not_full); // wake a waiting requester
    pthread_mutex_unlock(args->queue_lock);

    /* Lookup hostname and get IP string */
    if (dnslookup(hostname, firstipstr, sizeof(firstipstr)) == UTIL_FAILURE) {
      fprintf(stderr, "dnslookup error: %s\n", hostname);
      strncpy(firstipstr, "", sizeof(firstipstr));
    }

    /* Write to Output File */
    pthread_mutex_lock(args->output_lock);
    fprintf(args->output_file, "%s,%s\n", hostname, firstipstr);
    pthread_mutex_unlock(args->output_lock);

    /* free the hostname buffer */
    free(hostname);

    pthread_mutex_lock(args->queue_lock); // re-acquire for next iteration
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
  pthread_t resolver_threads[NUM_RESOLVER_THREADS];
  queue requestq;
  pthread_mutex_t queue_lock;
  pthread_mutex_t output_lock;
  pthread_mutex_t counter_lock;
  pthread_cond_t queue_not_full;
  pthread_cond_t queue_not_empty;
  RequesterArgs requester_args[MAX_INPUT_FILES];
  int i;

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

  /* Start timer */
  struct timeval begin, end;
  gettimeofday(&begin, 0);


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
  pthread_cond_init(&queue_not_full, NULL);
  pthread_cond_init(&queue_not_empty, NULL);
  active_files = num_files;

  /* Spawn resolver thread pool */
  ResolverArgs resolver_arg = {
    .requestq = &requestq,
    .queue_lock = &queue_lock,
    .output_file = output_file,
    .output_lock = &output_lock,
    .active_files = &active_files,
    .counter_lock = &counter_lock,
    .queue_not_empty = &queue_not_empty,
    .queue_not_full = &queue_not_full,
  };
  for (i = 0; i < NUM_RESOLVER_THREADS; i++) {
    pthread_create(&resolver_threads[i], NULL, resolve_thr, &resolver_arg);
  }

  /* Spawn requester thread pool */
  for (i = 0; i < num_files; i++) {
    requester_args[i] = (RequesterArgs){
      .queue_lock = &queue_lock,
      .requestq = &requestq,
      .active_files = &active_files,
      .input_file = input_files[i],
      .counter_lock = &counter_lock,
      .queue_not_full = &queue_not_full,
      .queue_not_empty = &queue_not_empty,
    };

    pthread_create(
      &requester_threads[i], NULL, request_thr, &requester_args[i]);
  }

  /* Join all threads */
  for (i = 0; i < num_files; i++) {
    pthread_join(requester_threads[i], NULL);
  }
  for (i = 0; i < NUM_RESOLVER_THREADS; i++) {
    pthread_join(resolver_threads[i], NULL);
  }

  
  pthread_cond_destroy(&queue_not_empty);
  pthread_cond_destroy(&queue_not_full);
  
  /* Stop timer and print elapsed time */
  gettimeofday(&end, 0);
  long seconds = end.tv_sec - begin.tv_sec;
  long microseconds = end.tv_usec - begin.tv_usec;
  printf("%ld\n", seconds * 1000000 + microseconds);
  /* Close Output File */
  fclose(output_file);

  /* Free queue memory */
  queue_cleanup(&requestq);

  return EXIT_SUCCESS;
}
