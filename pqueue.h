//
// Created by sps5394 on 12/2/18.
//

#ifndef P3_CSRF_PQUEUE_H
#define P3_CSRF_PQUEUE_H

#include <stdio.h>
#include <stdlib.h>
/**
 * Code courtesy of The Crazy Programmer
 * Link: https://www.thecrazyprogrammer.com/2017/06/priority-queue-c-c.html
 */
#define MAX 30
typedef int pqueue_data_t;

typedef struct pqueue {
  pqueue_data_t data[MAX];
  int rear, front;
} pqueue;

/**
 * Instantiates a priority queue in the provided parameter
 * @param p Allocated reference to a pqueue
 */
void initialize(pqueue *p);

/**
 * Returns 1 if queue is empty, else returns 0
 */
int empty(pqueue *p);

/**
 * Returns 1 if queue is full, else returns 0
 */
int full(pqueue *p);

/**
 * Add a data element to the priority queue
 * Returns 0 if successful, 1 if failure
 */
int enqueue(pqueue *p, pqueue_data_t x);

/**
 * Removes and returns the next element from the pqueue
 */
pqueue_data_t dequeue(pqueue *p);

/**
 * Debugging function to print out all elements in the queue
 */
void print(pqueue *p);

/******* DATA TYPE FUNCTIONS *******/

/**
 * Returns 0 if first == second
 * Returns 1 if first > second
 * Returns -1 if first < second
 */
int pqueue_data_cmp(pqueue_data_t first, pqueue_data_t second);

#endif //P3_CSRF_PQUEUE_H
