//
// Created by sps5394 on 12/2/18.
//

#include <assert.h>
#include "blocking_node.h"
#include "journal.h"

// GLOBAL VARIABLES
int peer_index;
peer_t peers[MAX_PEERS];
pqueue *lock_queue;
uint32_t timestamp;
pthread_mutex_t *m_mutex;
pthread_mutex_t ts_mutex;
pthread_mutex_t lp_mutex;
uint32_t node_id = 0;
int replies_received = 0;
int REPLIES_QUORUM = 2;
int requested_lock = 0;

int initialize_blocking_node() {
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
  if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &(int) { 1 }, sizeof(int)) < 0)
    perror("setsockopt(SO_REUSEADDR) failed");
  if (connect(sock, (struct sockaddr *) serv_addr, sizeof(struct sockaddr_in)) < 0) {
    printf("\nConnection Failed \n");
    return -1;
  }
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
  peers[peer_index].peer_request_lock_pending = 0;
  peers[peer_index].sem = (sem_t *) calloc(sizeof(sem_t), sizeof(char));
  if (sem_init(peers[peer_index].sem, 0, 1)) {
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
  int port = *(int *) p;
  char *peer_id = (char *) calloc(4, sizeof(char));

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

    socklen_t cli_addr_size = sizeof(address);
    int newsockfd = accept(sockfd, (struct sockaddr *) &address, &cli_addr_size);
    printf("Got new peer connection\n");

    peer_index = next_peer_index();
    if (peer_index == -1) {
      printf("Cannot connect to anymore peers. Disconnect from 1 or more to connect to another.");
      close(newsockfd);
      continue;
    }
    if (newsockfd < 0) {
      perror("Could not accept connection");
      continue;
    }
    if (read(newsockfd, peer_id, 4) != 4) {
      perror("Could not connect to peer: ");
      if (peer_id) { free(peer_id); }
      return -1;
    };
    // send the peer our node_id; client should send first, so this server reads first
    write(newsockfd, (void *) &node_id, 4);
    peers[peer_index].valid = 1;
    peers[peer_index].peer_node_id = *(int *) peer_id;
    peers[peer_index].sock = newsockfd;
    peers[peer_index].peer_request_lock_pending = 0;
    peers[peer_index].sem = (sem_t *) calloc(sizeof(sem_t), sizeof(char));
    if (sem_init(peers[peer_index].sem, 0, 1)) {
      perror("Could not init semaphore\n");
      if (peer_id) { free(peer_id); }
      return -1;
    }
    pthread_t *listener_thread = (pthread_t *) malloc(sizeof(pthread_t));
    pthread_create(listener_thread, NULL, peer_message_listen, (void *) &peers[peer_index]);
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

  peer_t *peer = (peer_t *) arg;
  char *peer_msg_buf = (char *) calloc(MAX_MESSAGE_SIZE, sizeof(char));
  peer_message_t *peer_msg = (peer_message_t *) calloc(sizeof(peer_message_t), sizeof(char));

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
      continue;
    }

    if (handle_peer_message(peer_msg, peer) != 0) {
      perror("Error during handle_peer_message:");
      free(peer_msg_buf);
      free(peer_msg);
      peer_msg_buf = NULL;
      peer_msg_buf = (char *) calloc(MAX_MESSAGE_SIZE, sizeof(char *));
      peer_msg = NULL;
      peer_msg = (peer_message_t *) calloc(sizeof(peer_message_t), sizeof(char *));
      continue;
    }

    free(peer_msg_buf);
    peer_msg_buf = NULL;
    peer_msg_buf = (char *) calloc(MAX_MESSAGE_SIZE, sizeof(char *));
    peer_msg = NULL;
    peer_msg = (peer_message_t *) calloc(sizeof(peer_message_t), sizeof(char *));
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
  free(peer_msg_buf);
  free(peer_msg);
  return NULL;
}

int distributed_lock() {
  // CREATE NEW REQUEST OBJECT
  update_timestamp(timestamp + 1);
  peer_message_t *msg = malloc(sizeof(peer_message_t));
  msg->timestamp = timestamp;
  msg->node_id = node_id;
  msg->message_type = REQUEST_LOCK;
  msg->key = (char *) calloc(2, sizeof(char));
  msg->value = (char *) calloc(2, sizeof(char));
  strncpy(msg->key, "1", 2);
  strncpy(msg->value, "1", 2);
  msg->write_type = 0;

  // ADD REQUEST TO Q_{node_id}
  if (enqueue(lock_queue, msg)) {
    printf("Enqueue to PQ failed\n");
    return -1;
  }
  printf("Current lock queue:");
  print(lock_queue);
  pthread_mutex_lock(m_mutex);
  requested_lock = 1;
  for (int i = 0; i < MAX_PEERS; i++) {
    if (!peers[i].valid) {
      continue;
    }
    if (send_peer_message(msg, peers[i].sock) != 0) {
      printf("Could not send REQUEST_LOCK to node: %d\n", peers[i].peer_node_id);
      pthread_mutex_unlock(m_mutex);
      return -1;
    }
    printf("Sent REQUEST_LOCK to peer: %d\n", peers[i].peer_node_id);
  }

  while (1) {
    pthread_mutex_lock(&lp_mutex);
    if (replies_received >= REPLIES_QUORUM) { // must receive replies from all peers
      replies_received = 0; // reset for next thread
      requested_lock = 0; // reset for next thread; protected by m_mutex
      pthread_mutex_unlock(&lp_mutex);
      break;
    }
    pthread_mutex_unlock(&lp_mutex);
  }
  pthread_mutex_unlock(
    m_mutex); // only one thread can send REQUEST_LOCKs and wait for all responses

  while (1) {
    // old --- now multiple threads can spin on queue until they get the lock;
    // only 1 thread can spin if not using locks in this loop;
    // which is fine because we only have 1 waiting_index and sem array,
    // otherwise we'd need one per c-s helper thread; once the waiting_index/sem's are good,
    // another helper thread
    if (peek(lock_queue) == msg) {
      break; /* old --> keep mutex too */
    }
  }

  // NOW IN CRITICAL SECTION //
  return 0;
}

int distributed_unlock() {
  //
  //
  // LOCAL VARIABLES
  //
  //

  // POP HEAD OF Q_{node_i}
  peer_message_t *msg = dequeue(lock_queue);
  free(msg);
  printf("Current lock queue:");
  print(lock_queue);
  // CREATE NEW REQUEST OBJECT
  update_timestamp(timestamp + 1);
  msg = malloc(sizeof(peer_message_t));
  msg->timestamp = timestamp;
  msg->node_id = node_id;
  msg->message_type = RELEASE_LOCK;
  msg->key = (char *) calloc(2, sizeof(char));
  msg->value = (char *) calloc(2, sizeof(char));
  strncpy(msg->key, "1", 2);
  strncpy(msg->value, "1", 2);
  msg->write_type = 0;

  // BROADCAST REQUEST TO ALL PROCESSES
  for (int i = 0; i < MAX_PEERS; i++) {
    if (!peers[i].valid) {
      continue;
    }
    if (send_peer_message(msg, peers[i].sock) != 0) {
      // might be issue here if node_id doesnt match the peer_index counter value
      printf("Could not send RELEASE_LOCK to node: %d\n", i);
      return -1; // spin until we are able to send all RELEASE_LOCKs
    }
  }
  free(msg);
  return 0;
}

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
      printf("Received REQUEST_LOCK from peer: %d\n", peer->peer_node_id);
      enqueue(lock_queue, message);
      printf("Current lock queue:");
      print(lock_queue);
      pthread_mutex_lock(&lp_mutex);
      if (requested_lock) { // block any request_lock until we receive all reply_lock
        printf("Peer %d requested lock, but still waiting on response\n", message->node_id);
        printf("errno: %s\n", strerror(errno));
        peer->peer_request_lock_pending = 1;
      } else { // we did not request a lock
        handle_request_lock(peer);
        printf("Sent REPLY_LOCK to peer: %d\n", peer->peer_node_id);
      }
      pthread_mutex_unlock(&lp_mutex);
      printf("Current lock queue:");
      print(lock_queue);
      return 0;

    case RELEASE_LOCK:
      printf("Received RELEASE_LOCK from peer: %d\n", peer->peer_node_id);
      peer_message_t *msg = dequeue(lock_queue);
      free(msg);
      printf("Current lock queue:");
      print(lock_queue);
      return 0;

    case REPLY_LOCK:
      printf("Received REPLY_LOCK from peer: %d\n", peer->peer_node_id);
      pthread_mutex_lock(&lp_mutex);
      if (peer->peer_request_lock_pending) {
        handle_request_lock(peer);
        peer->peer_request_lock_pending = 0;
        printf("Sent REPLY_LOCK to peer: %d\n", peer->peer_node_id);
      }
      replies_received++;
      pthread_mutex_unlock(&lp_mutex);
      printf("Current lock queue:");
      print(lock_queue);
      return 0;

    case SERVER_WRITE:
      printf("Received SERVER_WRITE from peer: %d\n", peer->peer_node_id);
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
      } else {
        printf("Got a write req from someone who doesn't have the lock\n");
      }
      printf("Current lock queue:");
      print(lock_queue);
      return 0;
    default:
      printf("You've  made a grave mistake. I cannot handle this\n");
      return 0;
  }
}

int handle_request_lock(peer_t *peer) {
  peer_message_t *outgoing_message;
  update_timestamp(timestamp + 1);
  // REPLY
  outgoing_message = malloc(sizeof(peer_message_t));
  outgoing_message->node_id = node_id;
  outgoing_message->message_type = REPLY_LOCK;
  outgoing_message->timestamp = timestamp;
  outgoing_message->key = (char *) calloc(2, sizeof(char));
  outgoing_message->value = (char *) calloc(2, sizeof(char));
  strncpy(outgoing_message->key, "1", 2);
  strncpy(outgoing_message->value, "1", 2);
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

int marshall_pm(char *buffer, peer_message_t *message) {
  int counter = 0;
  memcpy(buffer + counter, &( message->message_type ), sizeof(int));
  counter += sizeof(int);
  memcpy(buffer + counter, &( message->timestamp ), sizeof(int));
  counter += sizeof(int);
  memcpy(buffer + counter, &( message->node_id ), sizeof(int));
  counter += sizeof(int);
  memcpy(buffer + counter, &( message->write_type ), sizeof(int));
  counter += sizeof(int);
  memcpy(buffer + counter, message->key, strlen(message->key));
  counter += strlen(message->key);
  memcpy(buffer + counter, " ", 1); // keep space so we can tokenize (k,v) instead of having length field
  counter += 1;
  memcpy(buffer + counter, message->value, strlen(message->value));
  counter += strlen(message->value);
  memcpy(buffer + counter, "\0", 1);
  counter += 1;
  return counter;
}

int unmarshall_pm(char *peer_msg_buf, peer_message_t *peer_msg) {
  char *save_ptr = NULL;
  int counter = 0;
  memcpy(&( peer_msg->message_type ), peer_msg_buf + counter, sizeof(int));
  counter += sizeof(int);
  memcpy(&( peer_msg->timestamp ), peer_msg_buf + counter, sizeof(int));
  counter += sizeof(int);
  memcpy(&( peer_msg->node_id ), peer_msg_buf + counter, sizeof(int));
  counter += sizeof(int);
  memcpy(&( peer_msg->write_type ), peer_msg_buf + counter, sizeof(int));
  counter += sizeof(int);

  char *k = strtok_r(peer_msg_buf + counter, " ", &save_ptr);
  if (k != NULL) {
    peer_msg->key = (char *) calloc(strlen(k), sizeof(char));
    memcpy(peer_msg->key, k, strlen(k));
  }
  char *v = strtok_r(NULL, " ", &save_ptr); // should break on  \0
  if (v != NULL) {
    peer_msg->value = (char *) calloc(strlen(v), sizeof(char));
    memcpy(peer_msg->value, v, strlen(v));
  }
  return 0;
}

int send_peer_message(peer_message_t *message, int sock) {
  char *buf = (char *) calloc(MAX_MESSAGE_SIZE, sizeof(char));
  marshall_pm(buf, message);
  if (write(sock, buf, MAX_MESSAGE_SIZE) != MAX_MESSAGE_SIZE) {
    printf("error during send_peer_message\n");
  }
  free(buf);
  return 0;
}

int broadcast_write(char *key, char *value, int write_type) {
  update_timestamp(timestamp + 1);
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
    peers[i].peer_request_lock_pending = 0;
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
  peer->peer_request_lock_pending = 0;
  if (peer->sem) { free(peer->sem); }
  peer->sem = NULL;

  return 0;
}
