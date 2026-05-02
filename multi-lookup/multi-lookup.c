/*
 * File: multi-lookup.c
 * Author: Josiah Lawrence
 * Project: CSCI 440 Final
 * Create Date: 2026/04/15
 * Modify Date: 2026/04/26
 * Description:
 *  This is a multiprocess implementation of a
 *  bulk DNS lookup program.
 *
 */

#include "multi-lookup.h"

/* Queue Functions */

int
queue_init(queue* q, int size)
{
  int i;

  /* user specified size or default */
  if (size > 0 && size < QUEUE_MAX_SIZE) {
    q->maxSize = size;
  } else {
    q->maxSize = QUEUE_MAX_SIZE;
  }

  /* Set array to NULL */
  for (i = 0; i < q->maxSize; ++i) {
    q->array[i].has_value = 0;
  }

  /* setup circular buffer values */
  q->front = 0;
  q->rear = 0;

  return q->maxSize;
}

int
queue_is_empty(queue* q)
{
  if ((q->front == q->rear) && (q->array[q->front].has_value == 0)) {
    return 1;
  } else {
    return 0;
  }
}

int
queue_is_full(queue* q)
{
  if ((q->front == q->rear) && (q->array[q->front].has_value != 0)) {
    return 1;
  } else {
    return 0;
  }
}

int
queue_pop(queue* q, char* destination)
{
  if (queue_is_empty(q)) {
    return 1;
  }

  strncpy(destination, q->array[q->front].payload, MAX_NAME_LENGTH);
  q->array[q->front].has_value = 0;
  q->front = ((q->front + 1) % q->maxSize);

  return 0;
}

int
queue_push(queue* q, char* new_payload)
{

  if (queue_is_full(q)) {
    return QUEUE_FAILURE;
  }

  strncpy(q->array[q->rear].payload, new_payload, MAX_NAME_LENGTH);
  q->array[q->rear].has_value = 1;
  q->rear = ((q->rear + 1) % q->maxSize);

  return QUEUE_SUCCESS;
}

/* Multiproc Functions */

int
init_shared(shared_t* shared, int num_files, FILE* output_file)
{
  queue_init(&shared->requestq, QUEUE_SIZE);
  shared->active_files = num_files;
  shared->output_file = output_file;

  pthread_mutexattr_t m_attr;
  pthread_mutexattr_init(&m_attr);
  pthread_mutexattr_setpshared(&m_attr, PTHREAD_PROCESS_SHARED);
  pthread_mutex_init(&shared->queue_lock, &m_attr);
  pthread_mutex_init(&shared->output_lock, &m_attr);
  pthread_mutexattr_destroy(&m_attr);

  pthread_condattr_t c_attr;
  pthread_condattr_init(&c_attr);
  pthread_condattr_setpshared(&c_attr, PTHREAD_PROCESS_SHARED);
  pthread_cond_init(&shared->queue_not_full, &c_attr);
  pthread_cond_init(&shared->queue_not_empty, &c_attr);
  pthread_condattr_destroy(&c_attr);

  return 0;
}

int
request_proc(shared_t* shared, FILE* input_file)
{
  char hostname[MAX_NAME_LENGTH];

  /* Loop through file and push names to queue */
  while (fscanf(input_file, INPUTFS, hostname) != EOF) {
    pthread_mutex_lock(&shared->queue_lock);
    while (queue_is_full(&shared->requestq)) {
      pthread_cond_wait(&shared->queue_not_full, &shared->queue_lock);
    }

    queue_push(&shared->requestq, hostname);

    pthread_cond_signal(&shared->queue_not_empty);
    pthread_mutex_unlock(&shared->queue_lock);
  }

  /* All lines read -> close file */
  fclose(input_file);

  /* Decrement the active files counter */
  pthread_mutex_lock(&shared->queue_lock);
  (shared->active_files)--;
  pthread_cond_broadcast(&shared->queue_not_empty);
  pthread_mutex_unlock(&shared->queue_lock);

  return 0;
}

int
resolve_proc(shared_t* shared)
{
  char hostname[MAX_NAME_LENGTH];
  char firstipstr[INET6_ADDRSTRLEN];

  pthread_mutex_lock(&shared->queue_lock);
  while (1) {
    while (queue_is_empty(&shared->requestq)) {
      // Check if we're done before sleeping
      if (shared->active_files == 0) {
        pthread_mutex_unlock(&shared->queue_lock);
        return 0;
      }
      pthread_cond_wait(&shared->queue_not_empty, &shared->queue_lock);
    }
    if (queue_pop(&shared->requestq, hostname)) fprintf(stderr, "queue pop error: %s\n", hostname);
    pthread_cond_signal(&shared->queue_not_full); // wake a waiting requester
    pthread_mutex_unlock(&shared->queue_lock);
    /* Lookup hostname and get IP string */
    if (dnslookup(hostname, firstipstr, sizeof(firstipstr)) == UTIL_FAILURE) {
      fprintf(stderr, "dnslookup error: %s\n", hostname);
      strncpy(firstipstr, "", sizeof(firstipstr));
    }

    /* Write to Output File */
    pthread_mutex_lock(&shared->output_lock);
    fprintf(shared->output_file, "%s, %s\n", hostname, firstipstr);
    fflush(shared->output_file);
    if (DEBUG) fprintf(stderr, "%s, %s\n", hostname, firstipstr);
    pthread_mutex_unlock(&shared->output_lock);

    pthread_mutex_lock(&shared->queue_lock); // re-acquire for next iteration
  }
  return 1;
}

/* Main Function */

int
main(int argc, char* argv[])
{
  FILE* input_files[MAX_INPUT_FILES];
  FILE* output_file = NULL;
  int num_files; // The number of name files
  char errorstr[SBUFSIZE];
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
  shared_t* shared = mmap(NULL,
                          sizeof(shared_t),
                          PROT_READ | PROT_WRITE,
                          MAP_SHARED | MAP_ANONYMOUS,
                          -1,
                          0);

  init_shared(shared, num_files, output_file);

  /* Spawn resolver process pool */
  for (i = 0; i < NUM_RESOLVER_PROCS; i++) {
    pid_t pid = fork();
    if (pid == 0) {
      if (resolve_proc(shared)) fprintf(stderr, "resolver error: %d", i);
      exit(0);
    } else if (pid < 0) {
      perror("Error while forking resolver");
      exit(1);
    }
  }

  /* Spawn requester process pool */
  for (i = 0; i < num_files; i++) {
    pid_t pid = fork();
    if (pid == 0) {
      request_proc(shared, input_files[i]);
      exit(0);
    } else if (pid < 0) {
      perror("Error while forking requester");
      exit(1);
    }
  }

  /* Wait for all processes to finish */
  pid_t wpid;
  while ((wpid = wait(NULL)) > 0);

  pthread_mutex_destroy(&shared->output_lock);
  pthread_mutex_destroy(&shared->queue_lock);
  pthread_cond_destroy(&shared->queue_not_empty);
  pthread_cond_destroy(&shared->queue_not_full);

  if (TIMER) {
    /* Check time and print elapsed */
    gettimeofday(&end, 0);
    long seconds = end.tv_sec - begin.tv_sec;
    long microseconds = end.tv_usec - begin.tv_usec;
    printf("%ld\n", seconds * 1000000 + microseconds);
  }

  /* Close Output File */
  fclose(output_file);

  /* Free shared memory */
  munmap(shared, sizeof(shared_t));

  return EXIT_SUCCESS;
}
