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

#ifndef MULTI_LOOKUP_H
#define MULTI_LOOKUP_H

#include "util.h"
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define DEBUG 0
#define MINARGS 3
#define TIMER 1
#define USAGE "<inputFilePath> <outputFilePath>"
#define SBUFSIZE 1025
#define INPUTFS "%1024s"
#define QUEUE_SIZE 10
#define MAX_INPUT_FILES 10
#define NUM_RESOLVER_PROCS 10
#define MAX_NAME_LENGTH 1025
#define QUEUE_MAX_SIZE 10
#define QUEUE_FAILURE -1
#define QUEUE_SUCCESS 0

/* Data types */

typedef struct queue_node_s
{
  char payload[MAX_NAME_LENGTH];
  int has_value;
} queue_node;

typedef struct queue_s
{
  queue_node array[QUEUE_MAX_SIZE];
  int front;
  int rear;
  int maxSize;
} queue;

typedef struct
{
  queue requestq;
  int active_files;
  FILE* output_file;
  pthread_mutex_t queue_lock;
  pthread_mutex_t output_lock;
  pthread_cond_t queue_not_empty;
  pthread_cond_t queue_not_full;
} shared_t;

/* Functions */

/* Function to initialize a new queue
 * On success, returns queue size
 * On failure, returns QUEUE_FAILURE
 * Must be called before queue is used
 */
int
queue_init(queue* q, int size);

/* Function to test if queue is empty
 * Returns 1 if empty, 0 otherwise
 */
int
queue_is_empty(queue* q);

/* Function to test if queue is full
 * Returns 1 if full, 0 otherwise
 */
int
queue_is_full(queue* q);

/* Function add payload to end of FIFO queue
 * Returns QUEUE_SUCCESS if the push successeds.
 * Returns QUEUE_FAILURE if the push fails
 */
int
queue_push(queue* q, char* payload);

/* Function to return element from queue in FIFO order
 * Returns 0 on success and 1 if queue is empty
 */
int
queue_pop(queue* q, char* destination);

/* Function to initialize shared data */
int
init_shared(shared_t* shared, int num_files, FILE* output_file);

/* Producer function to place names from a file 
* onto the shared queue. Returns 0 on success.*/
int
request_proc(shared_t* shared, FILE* input_file);

/* Consumer function to lookup names from the 
* queue using dns, and write to an output file.
* Returns 0 on success. */
int
resolve_proc(shared_t* shared);

#endif