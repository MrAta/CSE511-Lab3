//
// Created by sps5394 on 12/2/18.
//

#include "blocking_node.h"
apr_queue_t *channel;
apr_pool_t *allocator;

int initialize_blocking_node() {
  apr_initialize();
  apr_pool_create(&allocator, NULL);
  apr_queue_create(&channel, 10, allocator);
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
  pthread_create(&listener_thread, NULL, client_message_listen, (void *) attribute);
  connected_socks[connected_clients++] = sock;
  return 0;
}

int listen_client_connections(int port) {
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
    if (pthread_create(handler_thread, NULL, client_message_listen, (void *) attribute) != 0) {
      perror("Could not start handler");
      continue;
    }
    connected_socks[connected_clients++] = newsockfd;
  }
}


