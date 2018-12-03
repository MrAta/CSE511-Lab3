//
// Created by sps5394 on 12/2/18.
//

#ifndef P3_CSRF_CLIENT_BLOCKING_H
#define P3_CSRF_CLIENT_BLOCKING_H

#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <math.h>
#include "apr/include/apr_queue.h"
#include "client_common.h"
#define PORT 8086
#define MAX_CLIENTS 10
#define QUEUED_CONNECTIONS 5
pthread_mutex_t fp_mutex;

extern apr_queue_t *channel;
extern apr_pool_t *allocator;

typedef struct {
  int socket;
  apr_queue_t *channel;
} listener_attr_t;

int connected_socks[MAX_CLIENTS];
int connected_clients;

int initialize_blocking_node();

/**
 * Creates a TCP connection to a server as specified
 * @param ip The ip address of the server
 * @param port The port of the server to connect to
 * @return socket fd if successful, -1 if failure
 */
int connect_server(char *ip, int port);

/**
 * Connect to a client node in the distributed network.
 * Also starts listening for any incoming messages from this client.
 * @param ip IP address of the client to connect to
 * @param port Port of the client to connect to
 * @return 0 if successful, -1 if failure
 */
int connect_peer(char *ip, int port);

/**
 * Listen for any incoming client connections.
 * This also creates the necessary channel and starts listening
 * for any messages from this client.
 * NOTE: the connect_client and listen_client_connections
 * functions should be mutually exclusively called as it does
 * not distinguish between new connections and connections that
 * have already been established
 * @param port Port to listen on
 * @return 0 if successful, 1 if failure
 */
int listen_client_connections(int port);

/**
 * Client listener thread function
 */
void *client_message_listen(void *);

int broadcast_message(); // TODO: Define properly



#endif //P3_CSRF_CLIENT_BLOCKING_H
