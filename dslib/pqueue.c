//
// Created by sps5394 on 12/2/18.
//

#include "pqueue.h"

void initialize(pqueue *p) {
  p->rear = -1;
  p->front = -1;
}

int empty(pqueue *p) {
  if (p->rear == -1)
    return ( 1 );

  return ( 0 );
}

int full(pqueue *p) {
  if (( p->rear + 1 ) % MAX == p->front)
    return ( 1 );

  return ( 0 );
}

int enqueue(pqueue *p, pqueue_data_t x) {
  int i;
  if (full(p)) {
    printf("\nOverflow\n");
    return -1;
  }
  if (empty(p)) {
    p->rear = p->front = 0;
    p->data[0] = x;
  } else {
    i = p->rear;
    while (pqueue_data_cmp(x, p->data[i]) == 1) {
      p->data[( i + 1 ) % MAX] = p->data[i];
      i = ( i - 1 + MAX ) % MAX; //anticlockwise movement inside the queue
      if (( i + 1 ) % MAX == p->front)
        break;
    }

    //insert x
    i = ( i + 1 ) % MAX;
    p->data[i] = x;

    //re-adjust rear
    p->rear = ( p->rear + 1 ) % MAX;
  }
  return 0;
}

pqueue_data_t dequeue(pqueue *p) {
  pqueue_data_t x;
  if (empty(p)) {
    printf("\nUnderflow..\n");
    return NULL;
  } else {
    x = p->data[p->front];
    if (p->rear == p->front)   //delete the last element
      initialize(p);
    else
      p->front = ( p->front + 1 ) % MAX;
  }
  return ( x );
}

void print(pqueue *p) {
  int i;
  pqueue_data_t x;

  if (empty(p)) {
    printf("\nQueue is empty..\n");
  } else {
    i = p->front;
    while (i != p->rear) {
      x = p->data[i];
      printf("\n%d", x);
      i = ( i + 1 ) % MAX;
    }
    //prints the last element
    x = p->data[i];
    printf("\n%d", x);
  }
}