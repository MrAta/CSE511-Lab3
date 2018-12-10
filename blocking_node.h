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
#include <fcntl.h>
// #include "apr-2/include/apr_queue.h"
#include "journal.h"
#include "pqueue.h"
#include "abd.h"
#include "data_types.h"
#include "server-part1.h"

#define QUEUED_CONNECTIONS 5
#define MAX_PEERS 10

/**
 * NOTE: In order to link against the apr library perform the following steps:
 * - Build the library as required by your system
 * - Get the built lib folder and ensure it exists in the current directory
 *      as libapr
 * - Set the environment variable LD_LIBRARY_PATH to the libapr/lib path
 */
// extern apr_queue_t *channel;
// extern apr_pool_t *allocator;
extern pqueue *lock_queue;
extern uint32_t timestamp;
// TODO: @Quinn Add this to the command line args
extern uint32_t node_id;

extern int connected_socks[MAX_PEERS];
extern int connected_clients;

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
 * Listen for any incoming client (server peers?) connections.
 * This also creates the necessary channel and starts listening
 * for any messages from this client.
 * NOTE: the connect_client and listen_client_connections
 * functions should be mutually exclusively called as it does
 * not distinguish between new connections and connections that
 * have already been established
 * @param port Port to listen on
 * @return 0 if successful, 1 if failure
 */
void *listen_peer_connections(void *port);

// TODO: @Quinn Implement
/**
 * Client listener thread function
 * This function listens for any messages from a peer node
 * and adds that message to the processing queue
 */
void *peer_message_listen(void *); // TODO: Define functionality

// TODO: @Quinn Implement
/**
 * Sends a particular message to all connected nodes in the "swarm"
 * @return
 */
int broadcast_write(char *key, char *value, int write_type); // TODO: Define properly


/************** Distributed computing functions ***************/
/**
 * Sends a locking request to the rest of the nodes in the swarm
 * Once it locks, it can enter a distributed critical section.
 * This function will make many blocking calls.
 * @return 0 if successfully locked, 1 if failed.
 */
int distributed_lock();

// TODO: @Quinn Implement
/**
 * This function sends a lock request (peer message) to the server node connected
 * at sock
 * @param timestamp struct containing the current server timestamp and node id
 * @param sock
 * @return
 */
int send_peer_message(peer_message_t *message, int sock);

//TODO: @Quinn: call this when a lock request is received
/**
 * This function will handle an incoming lock request and respond as needed
 * @param message a parsed message struct containing data from a node
 * @return 0 if success; 1 if failure
 */
// int handle_peer_message(peer_message_t *message, listener_attr_t *attribute);
int handle_peer_message(peer_message_t *message, peer_t *peer);

/**
 * Takes the needed action upon receiving a REQUEST message type.
 */
// void *handle_request_lock(void *arg);
// int handle_request_lock(int nid);
// int handle_request_lock(peer_message_t *message, peer_t *peer);
int handle_request_lock(peer_t *peer);

/**
 * Sends an unlocking request to the rest of the nodes in the swarm
 * Once it unlocks, it must lock again to enter a crit section.
 * This function will make many blocking calls.
 * @return 0 if successfully unlocked, 1 if failed.
 */
int distributed_unlock();

// TODO: @Quinn Implement
/**
 * This function writes the key and value to its own persistent store
 * and broadcasts it out to all other nodes
 * @param key Key to write for
 * @param value The value associated with key
 * @return 0 if successful; 1 if failed.
 */
// int server_write_request(char *key, char *value); // ********
int server_write_request(peer_message_t *message); // ********

// // TODO: Quinn Implement
// /**
//  * Handle an incoming write request from a server that holds the lock
//  * @param key Key to write value for
//  * @param value Value associated with the key
//  * @return 0 if successful; 1 if failed.
//  */
// int handle_server_write_request(char *key, char *value); // ********

/**
 * Runs any initializing needed for a type II node, and begins listening for peer connections.
 * @return 0 if successful; 1 if failed.
 */
int initialize_blocking_node();

void update_timestamp(uint32_t new_timestamp);

/************** DATA STRUCTURE FUNCTIONS **************/
// TODO: @Quinn implement
/**
 * Packs the data from message into the buffer
 * @param buffer a pointer to a buffer of data. This function will allocate
 * the buffer as needed
 * @param message pointer to struct to marshall from
 * @return number of bytes written, -1 if failure
 */
int marshall_pm(char **buffer, peer_message_t *message);

// TODO: @Quinn implement
/**
 * Unmarshalls a buffer of data into the message type
 * @param buffer buffer containing data for the message
 * @param message message struct to write data to
 * @ 0 on success, -1 if failure
 */
int unmarshall_pm(char *buffer, peer_message_t *message);

int next_peer_index(); // return index of next 'available' peer node in our array; return -1 on fail/not accepting anymore peer connections; maybe add timeout field to peers and check timeout values to evict old ones
int init_peer_array(); // initializes all the fields of all the peer nodes of our array
int reset_peer(peer_t *peer); // reset fields of peer node; occurs if we disconnect from a peer for whatever reason

#endif //P3_CSRF_CLIENT_BLOCKING_H
