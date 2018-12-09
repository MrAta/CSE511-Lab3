//
// Created by sps5394 on 12/7/18.
//

#ifndef P3_CSRF_DATA_TYPES_H
#define P3_CSRF_DATA_TYPES_H

// #include "apr/include/apr_queue.h"
#include "common.h"

#define MAX_MESSAGE_SIZE (1 + 4 + 4 + MAX_ENTRY_SIZE) // message_type, timestamp, node_id, key, [value] (value needed in this case of SERVER_WRITE)

typedef struct {
  int socket;
  // apr_queue_t *channel;
} listener_attr_t;

typedef enum {
  REQUEST_LOCK, // Requests a lock
  RELEASE_LOCK, // Releases a lock
  REPLY_LOCK, // Reply to a lock request
  SERVER_WRITE, // Server write request. This message type should have no
                // locking effect at all. This request type should only be
                // sent when a lock is held.
  // CONNECT_PEER // connect to peer
} peer_message_type_t;

typedef struct {
  peer_message_type_t message_type;
  uint32_t timestamp;
  uint32_t node_id;
  char *key;
  char *value;
  int write_type;
} peer_message_t;

// typedef peer_message_t * pqueue_data_t;

#endif //P3_CSRF_DATA_TYPES_H
