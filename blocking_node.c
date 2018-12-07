//
// Created by sps5394 on 12/2/18.
//

#include <assert.h>
#include "blocking_node.h"

// GLOBAL VARIABLES
apr_queue_t *channel;
apr_pool_t *allocator;
int connected_clients;
int connected_socks[MAX_CLIENTS];
int waiting_index[MAX_CLIENTS];
pqueue *lock_queue;
uint32_t timestamp;
int locked;
sem_t *sem[MAX_CLIENTS];
pthread_mutex_t *mutex;

// TODO: FIGURE OUT HOW TIMESTAMPS ARE UPDATED

int initialize_blocking_node() {
  // Configure Apache Portable Runtime Library
  apr_initialize();
  apr_pool_create(&allocator, NULL);
  // Create Channel for inter-thread communication
  apr_queue_create(&channel, 10, allocator);
  // Create and initialize lock priority queue
  lock_queue = malloc(sizeof(pqueue));
  initialize(lock_queue);

  // INITIALIZE GLOBAL VARIABLES
  timestamp = 0;
  connected_clients = 0;

  pthread_mutex_init(mutex, NULL);
  return 0;
}

int connect_server(char *ip, int port) {
  //
  //
  // LOCAL VARIABLES
  //
  //
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
  return sock;
}

int connect_peer(char *ip, int port) {
  //
  //
  // LOCAL VARIABLES
  //
  //
  int sock;
  pthread_t listener_thread;
  listener_attr_t *attribute;

  sock = connect_server(ip, port);
  if (sock == -1) {
    printf("Client connect failed\n");
    return 1;
  }
  attribute = malloc(sizeof(listener_attr_t));
  attribute->socket = sock;
  attribute->channel = channel;
  pthread_create(&listener_thread, NULL, peer_message_listen, (void *) attribute);
  connected_socks[connected_clients] = sock;
  // INITIALIZE GLOBAL SEMAPHORES
  if (( sem[connected_clients] = sem_open("bl_wait_sem", O_CREAT | O_EXCL, 0, 0)) ==
      SEM_FAILED) {
    perror("Could not open semaphore\n");
    return -1;
  }
  connected_clients++;
  return 0;
}

int listen_peer_connections(int port) {
  //
  //
  // LOCAL VARIABLES
  //
  //
  int sockfd, opt;
  struct sockaddr_in address;
  listener_attr_t *attribute;

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
    return EXIT_FAILURE;
  }
  while (1) {
    socklen_t cli_addr_size = sizeof(address);
    int newsockfd = accept(sockfd, (struct sockaddr *) &address, &cli_addr_size);
    printf("Got new connection\n");
    if (newsockfd < 0) {
      perror("Could not accept connection");
      continue;
    }
    attribute = malloc(sizeof(attribute));
    attribute->socket = newsockfd;
    attribute->channel = channel;
    pthread_t *handler_thread = (pthread_t *) malloc(sizeof(pthread_t));
    if (pthread_create(handler_thread, NULL, peer_message_listen, (void *) attribute) != 0) {
      perror("Could not start handler");
      continue;
    }
    connected_socks[connected_clients] = newsockfd;
    // INITIALIZE GLOBAL SEMAPHORES
    if (
      ( sem[connected_clients] = sem_open("bl_wait_sem", O_CREAT | O_EXCL, 0, 0)) ==
      SEM_FAILED) {
      perror("Could not open semaphore\n");
      return -1;
    }
    connected_clients++;
  }
}

void *peer_message_listen(void *param) {
  return NULL;
}

int distributed_lock() {
  //
  //
  // LOCAL VARIABLES
  //
  //
  lock_message_t *msg;
//  char *msg_buf;
//  int size;

  pthread_mutex_lock(mutex);
  if (locked) return -1;

  // CREATE NEW REQUEST OBJECT
  msg = malloc(sizeof(lock_message_t));
  msg->timestamp = timestamp;
  msg->client_id = node_id;
  msg->request_type = REQUEST;

  // ADD REQUEST TO Q_{node_id}
  if (enqueue(lock_queue, msg)) {
    printf("Enqueue to PQ failed\n");
    pthread_mutex_unlock(mutex);
    return 1;
  }

  // BROADCAST REQUEST TO ALL PROCESSES
//  if (( size = marshall(&msg_buf, msg)) == 0) {
//    printf("Could not marshall message\n");
//    return 1;
//  }
  for (int i = 0; i < connected_clients; i++) {
    if (send_lock_request(msg, connected_socks[i])) {
      printf("Could not send lock request to node: %d\n", i);
      pthread_mutex_unlock(mutex);
      return -1;
    }
    waiting_index[i] = 1;
  }
//  if (broadcast_message(msg_buf, size)) {
//    printf("Could not broadcast message\n");
//    return 1;
//  }
  for (int i = 0; i < connected_clients; i++) {
    if (sem_wait(sem[i])) {
      perror("Could not block on semaphore\n");
      pthread_mutex_unlock(mutex);
      return 1;
    }
  }
  while (peek(lock_queue) == msg);
  // NOW IN CRITICAL SECTION
  locked = 1;
  pthread_mutex_unlock(mutex);
  return 0;
}

int distributed_unlock() {
  //
  //
  // LOCAL VARIABLES
  //
  //
  lock_message_t *msg;
//  char *msg_buf;
//  int size;

  // POP HEAD OF Q_{node_i}
  dequeue(lock_queue);

  // CREATE NEW REQUEST OBJECT
  msg = malloc(sizeof(lock_message_t));
  msg->timestamp = timestamp;
  msg->client_id = node_id;
  msg->request_type = RELEASE;

  // BROADCAST REQUEST TO ALL PROCESSES
//  if (( size = marshall(&msg_buf, msg)) == 0) {
//    printf("Could not marshall message\n");
//    return 1;
//  }
  for (int i = 0; i < connected_clients; i++) {
    if (send_lock_request(msg, connected_socks[i])) {
      printf("Could not send lock request to node: %d\n", i);
      return -1;
    }
  }
//  if (broadcast_message(msg_buf, size)) {
//    printf("Could not broadcast message\n");
//    return 1;
//  }
  pthread_mutex_lock(mutex);
  locked = 0;
  pthread_mutex_unlock(mutex);
  return 0;
}

int handle_lock_request(lock_message_t *message) {
  //
  //
  // LOCAL VARIABLES
  //
  //


  switch (message->request_type) {
    case REQUEST:
      return perform_dist_lock(message);

    case RELEASE:
      dequeue(lock_queue);
      return 0;

    case REPLY:
      sem_post(sem[message->client_id]);
      waiting_index[message->client_id] = 0;
      return 0;

    case SERVER_WRITE:
    default:
      printf("You've  made a grave mistake. I cannot handle this\n");
      assert(0);
      return 1;
  }
}

int perform_dist_lock(lock_message_t *incoming_message) {
  //
  //
  // LOCAL VARIABLES
  //
  //
  lock_message_t *outgoing_message;
  // ADD MESSAGE TO PRIORITY QUEUE
  assert(incoming_message->request_type == REQUEST);
  enqueue(lock_queue, incoming_message);

  // IF WAITING FOR NODE TO REPLY CURRENTLY, BLOCK UNTIL REPLY
  if (waiting_index[incoming_message->client_id]) {
    sem_wait(sem[incoming_message->client_id]);
    sem_post(sem[incoming_message->client_id]);
  }
  // REPLY
  outgoing_message = malloc(sizeof(lock_message_t));
  outgoing_message->client_id = node_id;
  outgoing_message->request_type = REPLY;
  outgoing_message->timestamp = timestamp;

  send_lock_request(incoming_message, connected_socks[incoming_message->client_id]);

  return 0;
}

int main() {
}
