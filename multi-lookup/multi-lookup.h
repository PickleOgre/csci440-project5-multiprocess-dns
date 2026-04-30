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

#include "queue.h"
#include "util.h"
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#define MINARGS 3
#define USAGE "<inputFilePath> <outputFilePath>"
#define SBUFSIZE 1025
#define INPUTFS "%1024s"
#define QUEUE_SIZE 10
#define MAX_INPUT_FILES 10
#define NUM_RESOLVER_THREADS 10
#define MAX_NAME_LENGTH 1025

/* Data types */
typedef struct
{
  FILE* input_file;
  queue* requestq;
  pthread_mutex_t* queue_lock;
  int* active_files;
  pthread_mutex_t* counter_lock;
  pthread_cond_t* queue_not_full;
  pthread_cond_t* queue_not_empty;
} RequesterArgs;

typedef struct
{
  queue* requestq;
  pthread_mutex_t* queue_lock;
  FILE* output_file;
  pthread_mutex_t* output_lock;
  int* active_files;
  pthread_mutex_t* counter_lock;
  pthread_cond_t* queue_not_empty;
  pthread_cond_t* queue_not_full;
} ResolverArgs;

/* Functions */
void*
request_thr(void* arg);

void*
resolve_thr(void* arg);

#endif