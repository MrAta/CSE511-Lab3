//
// Created by sps5394 on 12/7/18.
//

#ifndef P3_CSRF_DATA_TYPES_H
#define P3_CSRF_DATA_TYPES_H

#include "apr/include/apr_queue.h"

typedef struct {
  int socket;
  apr_queue_t *channel;
} listener_attr_t;

typedef enum {
  REQUEST, // Requests a lock
  RELEASE, // Releases a lock
  REPLY, // Reply to a lock request
  SERVER_WRITE // Server write request. This message type should have no
               // locking effect at all. This request type should only be
               // sent when a lock is held.
} lock_request_t;

typedef struct {
  uint32_t timestamp;
  uint32_t client_id;
  lock_request_t request_type;
} lock_message_t;

typedef lock_message_t * pqueue_data_t;

#endif //P3_CSRF_DATA_TYPES_H
