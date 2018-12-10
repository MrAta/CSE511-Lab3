//
// Created by sps5394 on 12/2/18.
//

#include <assert.h>
#include "blocking_node.h"
#include "journal.h"

// GLOBAL VARIABLES
// apr_queue_t *channel;
// apr_pool_t *allocator;
int peer_index;
// int connected_socks[MAX_PEERS];
peer_t peers[MAX_PEERS];
// int waiting_index[MAX_PEERS];
pqueue *lock_queue;
uint32_t timestamp;
// int locked;
// sem_t *sem[MAX_PEERS];
pthread_mutex_t *m_mutex;
pthread_mutex_t ts_mutex;
pthread_mutex_t lp_mutex;
pthread_mutex_t pq_mutex;
// int rq_locks_pending[MAX_PEERS] = { 0 };
uint32_t node_id = 0;


// TODO: FIGURE OUT HOW TIMESTAMPS ARE UPDATED


int initialize_blocking_node() {
  // Configure Apache Portable Runtime Library
  // apr_initialize();
  // apr_pool_create(&allocator, NULL);
  // Create Channel for inter-thread communication
  // apr_queue_create(&channel, 50, allocator);
  // Create and initialize lock priority queue
  lock_queue = malloc(sizeof(pqueue));
  initialize(lock_queue);

  // INITIALIZE GLOBAL VARIABLES
  timestamp = 0;
  peer_index = 0;

  if (init_peer_array() != 0) {
    perror("Error initializing peer array: ");
    return -1;
  }
  m_mutex = malloc(sizeof(pthread_mutex_t));

  pthread_mutex_init(m_mutex, NULL);

  return 0;
}

int connect_server(char *ip, int port) {
  struct sockaddr_in *serv_addr;
  int sock;

  serv_addr = (struct sockaddr_in *) calloc(sizeof(struct sockaddr_in), sizeof(char));

  serv_addr->sin_family = AF_INET;
  serv_addr->sin_port = htons(port);

  // Convert IPv4 and IPv6 addresses from text to binary form
  if (inet_pton(AF_INET, ip, &serv_addr->sin_addr) <= 0) {
    printf("\nInvalid address/ Address not supported \n");
    return -1;
  }
  if (( sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    printf("\n Socket creation error \n");
    return -1;
  }

  if (connect(sock, (struct sockaddr *) serv_addr, sizeof(struct sockaddr_in)) < 0) {
    printf("\nConnection Failed \n");
    return -1;
  }
  if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &(int) { 1 }, sizeof(int)) < 0)
    perror("setsockopt(SO_REUSEADDR) failed");
  return sock;
}

int connect_peer(char *ip, int port) {
  // int idx = next_peer_index();
  peer_index = next_peer_index();
  // if (idx == -1) {
  if (peer_index == -1) {
    printf("Cannot connect to anymore peers. Disconnect from 1 or more to connect to another.\n");
    return -1;
  }
  // peer_index = idx;

  int sock;
  // pthread_t listener_thread;
  // listener_attr_t *attribute;

  sock = connect_server(ip, port);
  if (sock == -1) {
    printf("Client (peer) connect failed\n");
    return -1;
  }
  // attribute = malloc(sizeof(listener_attr_t));
  // attribute->socket = sock;
  // attribute->channel = channel;
  write(sock, (void *) &node_id, 4); // send the peer our node_id
  char *peer_id = (char *) calloc(4, sizeof(char));
  if (read(sock, peer_id, 4) != 4) {
    printf("Could not connect to peer [%s]\n", ip);
    return -1;
  };
  peers[peer_index].valid = 1;
  // peers[peer_index].peer_node_id = atoi(peer_id);
  peers[peer_index].peer_node_id = *(int *) peer_id;
  peers[peer_index].sock = sock;
  peers[peer_index].request_lock_pending = 0;
  peers[peer_index].sem = (sem_t *) calloc(sizeof(sem_t), sizeof(char));
  if (sem_init(peers[peer_index].sem, 0, 0)) {
    perror("Could not init semaphore\n");
    return -1;
  }
  pthread_t *listener_thread = (pthread_t *) malloc(sizeof(pthread_t));
  // pthread_create(listener_thread, NULL, peer_message_listen, (void *) attribute);
  pthread_create(listener_thread, NULL, peer_message_listen,
                 (void *) &peers[peer_index]); // send address of this peers array element to the listener thread
  // connected_socks[peer_index] = sock;
  // INITIALIZE GLOBAL SEMAPHORES
//  if (( sem[peer_index] = sem_open("bl_wait_sem", O_CREAT | O_EXCL, 0, 0)) ==
//      SEM_FAILED) {
//    perror("Could not open semaphore\n");
//    return -1;
//  }
  // sem[peer_index] = malloc(sizeof(sem_t));
  // if (sem_init(sem[peer_index], 0, 0)) {
  //   perror("Could not open semaphore\n");
  //   return -1;
  // }
  // peer_index++; // dont need this with next_peer_index()
  printf("Successfully connected to peer: [%d][%s:%d]\n", *(int *) peer_id, ip, port);
  if (peer_id) { free(peer_id); }
  return 0;
}

void *listen_peer_connections(void *p) {
  //
  //
  // LOCAL VARIABLES
  //
  //
  int sockfd, opt;
  struct sockaddr_in address;
  // listener_attr_t *attribute;
  int port = *(int *) p;
  char *peer_id = (char *) calloc(4, sizeof(char));
  // int port = (int) p;

  // Creating socket file descriptor
  if (( sockfd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
    perror("socket failed");
    exit(EXIT_FAILURE);
  }

  // Forcefully attaching socket to the port 8080
  if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT,
                 &opt, sizeof(opt))) {
    perror("setsockopt");
    exit(EXIT_FAILURE);
  }
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(port);

  // Forcefully attaching socket to the port 8080
  if (bind(sockfd, (struct sockaddr *) &address,
           sizeof(address)) < 0) {
    perror("bind failed");
    exit(EXIT_FAILURE);
  }
  if (listen(sockfd, QUEUED_CONNECTIONS) != 0) {
    perror("listen failed");
    if (peer_id) { free(peer_id); }
    return EXIT_FAILURE;
  }
  while (1) { // listening for new peers

    // if (peer_index > MAX_PEERS) { continue; } // TODO: what else here?

    socklen_t cli_addr_size = sizeof(address);
    int newsockfd = accept(sockfd, (struct sockaddr *) &address, &cli_addr_size);
    printf("Got new peer connection\n");

    // int idx = next_peer_index();
    peer_index = next_peer_index();
    // if (idx == -1) {
    if (peer_index == -1) {
      printf("Cannot connect to anymore peers. Disconnect from 1 or more to connect to another.");
      close(newsockfd);
      continue;
      // return -1; // dont return on the listener
    }
    // peer_index = idx;

    if (newsockfd < 0) {
      perror("Could not accept connection");
      continue;
    }
    // attribute = malloc(sizeof(attribute));
    // attribute->socket = newsockfd;
    // attribute->channel = channel;
    // peer_id = (char *)calloc(4, sizeof(char));
    if (read(newsockfd, peer_id, 4) != 4) {
      perror("Could not connect to peer: ");
      if (peer_id) { free(peer_id); }
      return -1;
    };
    write(newsockfd, (void *) &node_id,
          4); // send the peer our node_id; client should send first, so this server reads first
    peers[peer_index].valid = 1;
    // peers[peer_index].peer_node_id = atoi(peer_id);
    peers[peer_index].peer_node_id = *(int *) peer_id;
    peers[peer_index].sock = newsockfd;
    peers[peer_index].request_lock_pending = 0;
    peers[peer_index].sem = (sem_t *) calloc(sizeof(sem_t), sizeof(char));
    if (sem_init(peers[peer_index].sem, 0, 0)) {
      perror("Could not init semaphore\n");
      if (peer_id) { free(peer_id); }
      return -1;
    }
    pthread_t *listener_thread = (pthread_t *) malloc(sizeof(pthread_t));
    pthread_create(listener_thread, NULL, peer_message_listen, (void *) &peers[peer_index]);
    // if (pthread_create(listener_thread, NULL, peer_message_listen, (void *) attribute) != 0) { // hand off to another thread to monitor this socket
    //   perror("Could not start handler");
    //   continue;
    // }
    // connected_socks[peer_index] = newsockfd;
    // INITIALIZE GLOBAL SEMAPHORES
    // sem[peer_index] = malloc(sizeof(sem_t));
    // if (sem_init(sem[peer_index], 0, 0)) {
    //   perror("Could not open semaphore\n");
    //   return -1;
    // }
    // peer_index++; // dont need this with next_peer_index()
    printf("Accepted connection from peer: [%d]\n", *(int *) peer_id);
    free(peer_id);
    peer_id = (char *) calloc(4, sizeof(char));
  }

  if (peer_id) { free(peer_id); }
  return 0;
}

void *peer_message_listen(void *arg) {
  // TODO: what to do for logging for peer messages? different log file?
  // *** expecting messages of the form: message_type, timestamp, node_id, key, [value]
  // loop while still receiving peer messages (until peer socket closes)

  // listener_attr_t *attribute = (listener_attr_t *) arg;
  peer_t *peer = (peer_t *) arg;
  char *peer_msg_buf = (char *) calloc(MAX_MESSAGE_SIZE, sizeof(char));
  peer_message_t *peer_msg = (peer_message_t *) calloc(sizeof(peer_message_t), sizeof(char));
  char *response = NULL;
  int response_size;
  transaction txn;

  // while (read(attribute->socket, peer_msg_buf, MAX_MESSAGE_SIZE) > 0) {
  while (read(peer->sock, peer_msg_buf, MAX_MESSAGE_SIZE) > 0) {
    if (unmarshall_pm(peer_msg_buf, peer_msg) != 0) {
      perror("Error during unmarshall_pm:");
      free(peer_msg_buf);
      free(peer_msg);
      peer_msg_buf = NULL;
      peer_msg_buf = (char *) calloc(MAX_MESSAGE_SIZE, sizeof(char *));
      peer_msg = NULL;
      peer_msg = (peer_message_t *) calloc(sizeof(peer_message_t), sizeof(char *));
      response = NULL;
      continue;
    }

    // if (handle_peer_message(peer_msg, attribute) != 0) {
    if (handle_peer_message(peer_msg, peer) != 0) {
      perror("Error during handle_peer_message:");
      free(peer_msg_buf);
      free(peer_msg);
      peer_msg_buf = NULL;
      peer_msg_buf = (char *) calloc(MAX_MESSAGE_SIZE, sizeof(char *));
      peer_msg = NULL;
      peer_msg = (peer_message_t *) calloc(sizeof(peer_message_t), sizeof(char *));
      response = NULL;
      continue;
    }

    if (response != NULL) { free(response); }
    free(peer_msg_buf);
    free(peer_msg);
    peer_msg_buf = NULL;
    peer_msg_buf = (char *) calloc(MAX_MESSAGE_SIZE, sizeof(char *));
    peer_msg = NULL;
    peer_msg = (char *) calloc(sizeof(peer_message_t), sizeof(char *));
    response = NULL;
  }

  // TODO: if connection closed by peer...
  // reset waiting_index for this peer
  // reset connected_socks for this peer
  // reset sem for this peer
  // decrement connected_peer
  // note: need better mechanism for identifying this peer in each array (could just search thru connected_socks for index to all)
  printf("Disconnected from peer [%d]\nResetting peer node...\n", peer->peer_node_id);
  if (reset_peer(peer) != 0) {
    perror("Error resetting peer after disconnection: ");
  }
  printf("Node clean.\n");

  // free(arg); // malloc'd in listen_peer_connections;---- no dont free this, its on stack
  // attribute = NULL;
  free(peer_msg_buf);
  free(peer_msg);
  peer_msg_buf = NULL;
  peer_msg = NULL;
  response = NULL;
  // close(attribute->socket);
  return NULL;
}

int distributed_lock() { // used by client-server handler thread right (?)
  //
  //
  // LOCAL VARIABLES
  //
  //
  // peer_message_t *msg;
//  char *msg_buf;
//  int size;

  // pthread_mutex_lock(mutex);

  // if (locked) {
  //   pthread_mutex_unlock(mutex);
  //   return -1;
  // }

  // CREATE NEW REQUEST OBJECT
  peer_message_t *msg = malloc(sizeof(peer_message_t));
  msg->timestamp = timestamp;
  msg->node_id = node_id;
  msg->message_type = REQUEST_LOCK;
  msg->key = (char *) calloc(2, sizeof(char));
  msg->value = (char *) calloc(2, sizeof(char));
  memcpy(msg->key, "1", 1);
  memcpy(msg->value, "1", 1);
  // msg->write_type = -1;
  msg->write_type = 0;

  // ADD REQUEST TO Q_{node_id}
  pthread_mutex_lock(&pq_mutex);
  if (enqueue(lock_queue,
              msg)) { // TODO: this is thread safe queue right? -- allow threads to enqueue their messages so they can be served the lock in correct order
    printf("Enqueue to PQ failed\n");
    // pthread_mutex_unlock(mutex);
    return -1;
  }
  pthread_mutex_unlock(&pq_mutex);

  // BROADCAST REQUEST TO ALL PROCESSES
//  if (( size = marshall(&msg_buf, msg)) == 0) {
//    printf("Could not marshall message\n");
//    return 1;
//  }
  pthread_mutex_lock(m_mutex);
  update_timestamp(timestamp + 1); // TODO: do we need lock around updates to timestamp?
  pthread_mutex_unlock(m_mutex);
//  int num_expected = 0;
  for (int i = 0; i < MAX_PEERS; i++) {
    if (!peers[i].valid) {
      continue;
    }
//    num_expected++;
    if (send_peer_message(msg, peers[i].sock) != 0) {
      // might be issue here if node_id doesnt match the peer_index counter value
      printf("Could not send REQUEST_LOCK to node: %d\n", peers[i].peer_node_id);
      pthread_mutex_unlock(m_mutex);
      return -1;
    }
    peers[i].request_lock_pending = 1;
    sem_wait(peers[i].sem); // set lock for all peers (effectively the same as waiting_index)
    if (peers[i].request_lock_pending == 0) {
      // Someone else already got the response to this and I am the last thread.
      sem_post(peers[i].sem); // TODO: Do this conditionally so that the future call does not fail
    }
//    sem_init(peers[i].sem, 0, 0);
    peers[i].request_lock_pending = 0;
    // waiting_index[i] = 1;
  }
//  while(num_responses < num_expected);
//  if (broadcast_message(msg_buf, size)) {
//    printf("Could not broadcast message\n");
//    return 1;
//  }
//  for (int i = 0; i < MAX_PEERS; i++) {
//    // might be issue here if node_id doesnt match the peer_index counter value
//    if (!peers[i].valid) {
//      continue;
//    }
//    // instead of waiting for all REPLY_LOCKs, should we get rid of this and just have a loop spin on a global counter for # of REPLYs gotten?
//    // then after that loop breaks, we spin on the queue until we are allowed to have lock?
//    if (sem_wait(peers[i].sem) != 0) {
//      // wait until we are able to acquire lock for all peers (i.e. we have
//      // received REPLY_LOCK from all of them and sem_post'ed them);
//      // WILL get stuck/block forever if peer doesnt respond; how can set timeout?
//      perror("Could not block on semaphore\n");
//      pthread_mutex_unlock(mutex);
//      return -1;
//    }
//    sem_post(peers[i].sem); // unlock; heard from this peer and next thread can send to this peer
//
//    // if we are waiting on a REPLY_LOCK and a peer helper thread receives a REQUEST_LOCK it will set the locks pending flag so that once the peer helper thread receives a REPLY_LOCK, posts sem, then this sem_wait passes above -> we will send the peer the REPLY_LOCK they are waiting for
//
//    // check if we have a REQUEST_LOCK to handle for this peer
//    pthread_mutex_lock(
//      &lp_mutex); // lp_mutex is for synch b/w helper peer threads and client-server helper threads on this flag array
//    if (peers[i].request_lock_pending) {
//      handle_request_lock(&peers[i]);
//      peers[i].request_lock_pending = 0;
//    }
//    pthread_mutex_unlock(&lp_mutex);
//  }
//  pthread_mutex_unlock(mutex);

  // while (peek(lock_queue) == msg); // old -- *** wait until we acquire lock ***; TODO: should this be != ?
  while (1) { // old --- now multiple threads can spin on queue until they get the lock; only 1 thread can spin if not using locks in this loop; which is fine because we only have 1 waiting_index and sem array, otherwise we'd need one per c-s helper thread; once the waiting_index/sem's are good, another helper thread 
    // pthread_mutex_lock(mutex); // do we even really need to lock/unlock in this loop?
    pthread_mutex_lock(&pq_mutex);
    if (peek(lock_queue) == msg) {
      pthread_mutex_unlock(&pq_mutex);
      // pthread_mutex_unlock(mutex);
      break; /* old --> keep mutex too */
    }
    pthread_mutex_unlock(&pq_mutex);
    // pthread_mutex_unlock(mutex);
  }
  // while (peek(lock_queue) == msg); // only one thread has mutex so only one thread can spin here, the rest will wait at mutex_lock above, should be fine except other threads wont be able to get their REQUEST_LOCK messages out while we're spinning

  // NOW IN CRITICAL SECTION // ************

  // TODO: dont need anymore? being at the head of pqueue means you have lock
  // locked = 1;
  // pthread_mutex_unlock(&pq_mutex);
  // pthread_mutex_unlock(mutex);
  return 0;
}

int distributed_unlock() {
  //
  //
  // LOCAL VARIABLES
  //
  //
  // peer_message_t *msg;
//  char *msg_buf;
//  int size;

  // POP HEAD OF Q_{node_i}
  // TODO: should we do this here or after we send RELEASE_LOCK??? prob here
  pthread_mutex_lock(&pq_mutex);
  dequeue(lock_queue);
  pthread_mutex_unlock(&pq_mutex);

  // CREATE NEW REQUEST OBJECT
  // msg = malloc(sizeof(peer_message_t));
  peer_message_t *msg = malloc(sizeof(peer_message_t));
  msg->timestamp = timestamp;
  msg->node_id = node_id;
  msg->message_type = RELEASE_LOCK;
  msg->key = (char *) calloc(2, sizeof(char));
  msg->value = (char *) calloc(2, sizeof(char));
  memcpy(msg->key, "1", 1);
  memcpy(msg->value, "1", 1);
  // msg->write_type = -1;
  msg->write_type = 0;

  // BROADCAST REQUEST TO ALL PROCESSES
//  if (( size = marshall(&msg_buf, msg)) == 0) {
//    printf("Could not marshall message\n");
//    return 1;
//  }
  update_timestamp(timestamp + 1);
  for (int i = 0; i < MAX_PEERS; i++) {
    if (!peers[i].valid) {
      continue;
    }
    if (send_peer_message(msg, peers[i].sock) !=
        0) { // might be issue here if node_id doesnt match the peer_index counter value
      printf("Could not send RELEASE_LOCK to node: %d\n", i);
      return -1; // spin until we are able to send all RELEASE_LOCKs
    }
  }
//  if (broadcast_message(msg_buf, size)) {
//    printf("Could not broadcast message\n");
//    return 1;
//  }

  // TODO: dont need anymore?
  // pthread_mutex_lock(mutex);
  // locked = 0;
  // pthread_mutex_unlock(mutex);

  return 0;
}

// int handle_peer_message(peer_message_t *message, listener_attr_t *attribute) {
int handle_peer_message(peer_message_t *message, peer_t *peer) {
  //
  //
  // LOCAL VARIABLES
  //
  //
  char *buf;
  int size;
  update_timestamp(message->timestamp);
  switch (message->message_type) {
    case REQUEST_LOCK:
      // return handle_request_lock(message);
      pthread_mutex_lock(&pq_mutex);
      enqueue(lock_queue, message);
      pthread_mutex_unlock(&pq_mutex);
      pthread_t *t = malloc(sizeof(pthread_t));
      pthread_create(t, NULL, request_lock_handler, peer);
//      request_lock_handler(peer);

      // if (sem_trywait(sem[message->node_id]) != 0) { // lock is available if we are not waiting on a REPLY_LOCK; otherwise we are, so set flag to be handled up top
//      if (sem_trywait(peer->sem) != 0) {
//        printf("Peer requested lock, but still waiting on response\n");
//        printf("errno: %s\n", strerror(errno));
//        pthread_mutex_lock(&lp_mutex);
//        // enqueue(lock_queue, message);
//        // rq_locks_pending[message->node_id] = 1;
//        peer->request_lock_pending = 1;
//        pthread_mutex_unlock(&lp_mutex);
//      } else {
      // not waiting on REPLY_LOCK from them, go ahead and send REPLY_LOCK to peer
      // handle_request_lock(message->node_id);
      // handle_request_lock(message, peer);
//        handle_request_lock(peer);
//      }
      // peer_message_t *copy = (peer_message_t *) calloc(sizeof(peer_message_t), sizeof(char));
      // memcpy(copy, message, sizeof(peer_message_t));
      // pthread_t *handler_thread = (pthread_t *) malloc(sizeof(pthread_t));
      // if (pthread_create(handler_thread, NULL, handle_request_lock, (void *) copy) != 0) {
      //   perror("Could not REQUEST_LOCK handler:");
      // }
      return 0;

    case RELEASE_LOCK:
      pthread_mutex_lock(&pq_mutex);
      dequeue(lock_queue);
      pthread_mutex_unlock(&pq_mutex);
      return 0;

    case REPLY_LOCK:
      // sem_post(sem[message->node_id]); // done waiting for reply from message->node_id (some server node)
      sem_post(peer->sem);
      // waiting_index[message->node_id] = 0;
      return 0;

    case SERVER_WRITE:
      // return 0; // TODO?
      // check that incoming_message->node_id == peek(lock_queue)->node_id
      if (message->node_id == peek(
        lock_queue)->node_id) { // only act if the SERVER_WRITE is coming from the server that holds the lock; otherwise ignore
        switch (message->write_type) {
          case INSERT:
            return server_1_insert_request(message->key, message->value, &buf, &size);
          case PUT:
            return server_1_put_request(message->key, message->value, &buf, &size);
          case DELETE:
            return server_1_delete_request(message->key, &buf, &size);
          default:
            printf("Bad request for server write\n");
            return -1;
        }
//        return server_write_request(message);
      } else {
        printf("Got a write req from someone who doesn't have the lock\n");
      }
      return 0;
    default:
      printf("You've  made a grave mistake. I cannot handle this\n");
      // assert(0); return -1; // abort or just ignore the message?
      return 0;
  }
}

void *request_lock_handler(void *p) {
  peer_t *peer = (peer_t *) p;
  if (peer->request_lock_pending) {
    sem_wait(peer->sem);
    peer->request_lock_pending = 0;
    sem_post(peer->sem);
//    sem_init(peer->sem, 0, 0);
  } else {
    handle_request_lock(peer);
  }
  return NULL;
}

int server_write_request(peer_message_t *message) {
  char *response = NULL;
  int response_size = 0;
  int rc = server_1_put_request(message->key, message->value, &response, &response_size);
  if (response != NULL) { free(response); } // dont need to send anything back, just free this
  return rc;
}

// void *handle_request_lock(void *arg) {
// int handle_request_lock(int nid) {
int handle_request_lock(peer_t *peer) {
  // peer_message_t *incoming_message = (peer_message_t *) arg;
  peer_message_t *outgoing_message;

  // ADD MESSAGE TO PRIORITY QUEUE
  // assert(incoming_message->message_type == REQUEST_LOCK);
  // enqueue(lock_queue, incoming_message);

  // IF WAITING FOR NODE TO REPLY CURRENTLY, BLOCK UNTIL REPLY
  // if (waiting_index[incoming_message->node_id]) {
  //   // apr_queue_push(channel, (void *)outgoing_message); // push message to queue to handle once we get a REPLY_LOCK
  //   sem_wait(sem[incoming_message->node_id]);
  //   sem_post(sem[incoming_message->node_id]);
  // }
  update_timestamp(timestamp + 1);
  // REPLY
  outgoing_message = malloc(sizeof(peer_message_t));
  outgoing_message->node_id = node_id;
  outgoing_message->message_type = REPLY_LOCK;
  outgoing_message->timestamp = timestamp;
  outgoing_message->key = (char *) calloc(2, sizeof(char));
  outgoing_message->value = (char *) calloc(2, sizeof(char));
  memcpy(outgoing_message->key, "1", 1);
  memcpy(outgoing_message->value, "1", 1);
  outgoing_message->write_type = 0;

  send_peer_message(outgoing_message, peer->sock);

  // free(incoming_message);
  free(outgoing_message);

  return 0;
}

void update_timestamp(uint32_t new_timestamp) {
  pthread_mutex_lock(&ts_mutex);
  timestamp = timestamp > new_timestamp ? timestamp : new_timestamp;
  pthread_mutex_unlock(&ts_mutex);
}

int marshall_pm(char **buffer, peer_message_t *message) {
  memcpy(( *buffer ), &( message->message_type ), sizeof(int));
  // memcpy(( *buffer ) + sizeof(int), " ", 1);
  memcpy(( *buffer ) + sizeof(int), &( message->timestamp ), sizeof(int));
  // memcpy(( *buffer ) + sizeof(int) + 1, &(message->timestamp), sizeof(int));
  // memcpy(( *buffer ) + sizeof(int) + 1 + sizeof(int), " ", 1);
  memcpy(( *buffer ) + sizeof(int) + sizeof(int), &( message->node_id ), sizeof(int));
  // memcpy(( *buffer ) + sizeof(int) + 1 + sizeof(int) + 1, &(message->node_id), sizeof(int));
  // memcpy(( *buffer ) + sizeof(int) + 1 + sizeof(int) + 1 + sizeof(int), " ", 1);
  memcpy(( *buffer ) + sizeof(int) + sizeof(int) + sizeof(int), &( message->write_type ),
         sizeof(int));
  // memcpy(( *buffer ) + sizeof(int) + 1 + sizeof(int) + 1 + sizeof(int) + 1, &(message->write_type), sizeof(int));
  // memcpy(( *buffer ) + sizeof(int) + 1 + sizeof(int) + 1 + sizeof(int) + 1 + sizeof(int), " ", 1);
  memcpy(( *buffer ) + sizeof(int) + sizeof(int) + sizeof(int) + sizeof(int), message->key,
         strlen(message->key));
  // memcpy(( *buffer ) + sizeof(int) + 1 + sizeof(int) + 1 + sizeof(int) + 1 + sizeof(int) + 1, message->key, strlen(message->key));
  // memcpy(( *buffer ) + sizeof(int) + 1 + sizeof(int) + 1 + sizeof(int) + 1 + sizeof(int) + 1 + strlen(message->key), " ", 1);
  memcpy(( *buffer ) + sizeof(int) + sizeof(int) + sizeof(int) + sizeof(int) + strlen(message->key),
         " ", 1); // keep space so we can tokenize (k,v) instead of having length field
  memcpy(
    ( *buffer ) + sizeof(int) + sizeof(int) + sizeof(int) + sizeof(int) + strlen(message->key) + 1,
    message->value, strlen(message->value));
  // memcpy(( *buffer ) + sizeof(int) + 1 + sizeof(int) + 1 + sizeof(int) + 1 + sizeof(int) + 1 + strlen(message->key) + 1, message->value, strlen(message->value));
  // memcpy(( *buffer ) + sizeof(int) + 1 + sizeof(int) + 1 + sizeof(int) + 1 + strlen(message->key) + 1 + strlen(message->value) + 1, &(message->write_type), sizeof(int));
  // memcpy(( *buffer ) + sizeof(int) + 1 + sizeof(int) + 1 + sizeof(int) + 1 + sizeof(int) + 1 + strlen(message->key) + 1 + strlen(message->value), "\0", 1);
  memcpy(
    ( *buffer ) + sizeof(int) + sizeof(int) + sizeof(int) + sizeof(int) + strlen(message->key) + 1 +
    strlen(message->value), "\0", 1);
  // assert(size <= MAX_MESSAGE_SIZE); // ??
  // return ( sizeof(int) + 1 + sizeof(int) + 1 + sizeof(int) + 1 + strlen(message->key) + 1 + strlen(message->value) + 1 + sizeof(int) + 1 );
  return ( sizeof(int) + sizeof(int) + sizeof(int) + sizeof(int) + strlen(message->key) + 1 +
           strlen(message->value) + 1 );
}

int unmarshall_pm(char *peer_msg_buf, peer_message_t *peer_msg) {
  char *save_ptr = NULL;

  memcpy(&( peer_msg->message_type ), peer_msg_buf, sizeof(int));
  memcpy(&( peer_msg->timestamp ), peer_msg_buf + sizeof(int), sizeof(int));
  memcpy(&( peer_msg->node_id ), peer_msg_buf + 2 * sizeof(int), sizeof(int));
  memcpy(&( peer_msg->write_type ), peer_msg_buf + 3 * sizeof(int), sizeof(int));

  char *k = strtok_r(peer_msg_buf + 4 * sizeof(int), " ", &save_ptr);
  if (k != NULL) {
    peer_msg->key = (char *) calloc(strlen(k), sizeof(char));
    memcpy(peer_msg->key, k, strlen(k));
  }

  char *v = strtok_r(NULL, " ", &save_ptr); // should break on  \0
  if (v != NULL) {
    peer_msg->value = (char *) calloc(strlen(v), sizeof(char));
    memcpy(peer_msg->value, v, strlen(v));
  }

  // // peer_msg->message_type = atoi(strtok_r(peer_msg_buf, " ", &save_ptr));
  // peer_msg->timestamp = atoi(strtok_r(NULL, " ", &save_ptr)); // TODO: proper checking on these ints since timestamp can be 0 but atoi returns 0 on error
  // peer_msg->node_id = atoi(strtok_r(NULL, " ", &save_ptr));

  // char *k = strtok_r(NULL, " ", &save_ptr);
  // peer_msg->key = (char *) calloc(strlen(k), sizeof(char));
  // memcpy(peer_msg->key, k, strlen(k));

  // char *v = strtok_r(NULL, " ", &save_ptr);
  // peer_msg->value = (char *) calloc(strlen(v), sizeof(char));
  // memcpy(peer_msg->value, v, strlen(v));

  // peer_msg->write_type = atoi(strtok_r(NULL, " ", &save_ptr)); // will break on \0 term, no space at end

  return 0;
}

int send_peer_message(peer_message_t *message, int sock) {
  char *buf = (char *) calloc(MAX_MESSAGE_SIZE, sizeof(char));
  int numbytes = marshall_pm(&buf, message);
  write(sock, buf, numbytes);
  free(buf);
  return 0;
}

int broadcast_write(char *key, char *value, int write_type) {
  peer_message_t *msg = malloc(sizeof(peer_message_t));
  msg->node_id = node_id;
  msg->message_type = SERVER_WRITE;
  msg->timestamp = timestamp;

  if (key) {
    msg->key = (char *) calloc(strlen(key), sizeof(char));
    strcpy(msg->key, key);
  } else {
    msg->key = (char *) calloc(2, sizeof(char));
    strncpy(msg->key, "1", 2);
  }

  if (value) {
    msg->value = (char *) calloc(strlen(value), sizeof(char));
    strcpy(msg->value, value);
  } else {
    msg->value = (char *) calloc(2, sizeof(char));
    strncpy(msg->value, "1", 2);
  }

  msg->write_type = write_type;
  update_timestamp(timestamp + 1);

  for (int i = 0; i < MAX_PEERS; i++) {
    if (!peers[i].valid) {
      continue;
    }
    if (send_peer_message(msg, peers[i].sock) != 0) {
      // might be issue here if node_id doesnt match the peer_index counter value
      printf("Could not send SERVER_WRITE to node: %d\n", i);
      return -1; // ??
      // continue; // continue or stop? prob should keep spinning on this until we are able to send SERVER_WRITE to all servers
    }
  }

  // TODO: free key/value anywhere allocated
  free(msg->key);
  free(msg->value);
  free(msg);

  return 0;
}

// int main() {
//   return 0;
// }

int next_peer_index() {
  int idx = -1;
  for (int i = 0; i < MAX_PEERS; i++) {
    if (peers[i].valid == 0) {
      idx = i;
      break;
    }
  }

  return idx;
}

int init_peer_array() {
  for (int i = 0; i < MAX_PEERS; i++) {
    peers[i].valid = 0;
    peers[i].peer_node_id = -1;
    peers[i].sock = -1;
    peers[i].request_lock_pending = 0;
    peers[i].sem = NULL;
  }

  return 0;
}

int reset_peer(peer_t *peer) {
  if (peer == NULL) {
    printf("Peer is null.\n");
    return -1;
  }
  peer->valid = 0;
  peer->peer_node_id = -1;
  close(peer->sock);
  peer->sock = -1;
  peer->request_lock_pending = 0;
  if (peer->sem) { free(peer->sem); }
  peer->sem = NULL;

  return 0;
}
