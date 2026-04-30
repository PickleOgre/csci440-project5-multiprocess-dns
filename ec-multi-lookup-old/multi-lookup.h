/*
 * File: multi-lookup.c
 * Author: Josiah Lawrence
 * Project: CSCI 3753 Programming Assignment 2
 * Create Date: 2026/04/15
 * Description:
 * 	This is the header file for the threaded
 *      solution to this assignment.
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
#include <time.h>
#include <unistd.h>

#define MINARGS 3
#define USAGE "<inputFilePath> <outputFilePath>"
#define SBUFSIZE 1025
#define INPUTFS "%1024s"
#define QUEUE_SIZE 10

/* Limits */
#define MAX_INPUT_FILES 10
#define MAX_RESOLVER_THREADS 10
#define MIN_RESOLVER_THREADS 2
#define MAX_NAME_LENGTH 1025
#define MAX_IP_LENGTH INET6_ADDRSTRLEN

/* Data types */
typedef struct
{
  FILE* input_file;
  queue* requestq;
  pthread_mutex_t* queue_lock;
  int* active_files;
  pthread_mutex_t* counter_lock;
} RequesterArgs;

typedef struct
{
  queue* requestq;
  pthread_mutex_t* queue_lock;
  FILE* output_file;
  pthread_mutex_t* output_lock;
  int* active_files;
  pthread_mutex_t* counter_lock;
} ResolverArgs;

/* Functions */
void*
request_thr(void* arg);

void*
resolve_thr(void* arg);

#endif