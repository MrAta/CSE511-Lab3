//
// Created by sps5394 on 12/2/18.
//

#include "pqueue.h"

pthread_mutex_t *m_mutex;

void initialize(pqueue *p) {
  reset(p);
  mutex = malloc(sizeof(pthread_mutex_t));
  if (pthread_mutex_init(mutex, NULL)) {
    perror("Could not initialize mutex\n");
    exit(1);
  }
}

void reset(pqueue *p) {
  p->rear = -1;
  p->front = -1;
}

int empty(pqueue *p) {
  if (p->rear == -1)
    return ( 1 );

  return ( 0 );
}

int full(pqueue *p) {
  // LOCK REASON: read and front are accessed separately.
  // May be a location of data races
  pthread_mutex_lock(mutex);
  if (( p->rear + 1 ) % MAX == p->front) {
    pthread_mutex_unlock(mutex);
    return ( 1 );
  }
  pthread_mutex_unlock(mutex);
  return ( 0 );
}

int enqueue(pqueue *p, pqueue_data_t x) {
  int i;
  if (full(p)) {
    printf("\nOverflow\n");
    return -1;
  }
  if (empty(p)) {
    pthread_mutex_lock(mutex);
    p->rear = p->front = 0;
    p->data[0] = x;
    pthread_mutex_unlock(mutex);
  } else {
    pthread_mutex_lock(mutex);
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
    pthread_mutex_unlock(mutex);
  }
  return 0;
}

pqueue_data_t dequeue(pqueue *p) {
  pqueue_data_t x;
  if (empty(p)) {
    printf("\nUnderflow..\n");
    return NULL;
  } else {
    pthread_mutex_lock(mutex);
    x = p->data[p->front];
    if (p->rear == p->front)   //delete the last element
      reset(p);
    else
      p->front = ( p->front + 1 ) % MAX;
    pthread_mutex_unlock(mutex);
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

pqueue_data_t peek(pqueue *p) {
  pqueue_data_t x;
  if (empty(p)) {
    printf("\nUnderflow\n");
    return NULL;
  }
  pthread_mutex_lock(mutex);
  x = p->data[p->front];
  pthread_mutex_unlock(mutex);
  return ( x );
}