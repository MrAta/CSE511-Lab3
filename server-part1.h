//
// Created by sps5394 on 10/18/18.
//

#ifndef P1_CSRF_SERVER_PART1_H
#define P1_CSRF_SERVER_PART1_H

#include <stdio.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <pthread.h>
#include "cache.h"
#include "journal.h"
#include <signal.h>
#include <time.h>
#include <errno.h>
#include "c1.h"
#include "abd.h"
#include "blocking_node.h"

#define PORT 8086
#define PEER_PORT 8087
#define QUEUED_CONNECTIONS 5

#define CACHE_SIZE 101

extern c0_node * _T;
extern struct sockaddr_in address;

int create_server_1();

static void my_timer_handler(int sig, siginfo_t *si, void *uc);

/**
 * The loop for the server. Continuously loops
 * and listens on the server socket
 * @return 0 if success 1 if failure
 */
int loop_and_listen_1();

/**
 * Main program for the server
 * @param blocking 1 if run the servers with ABD protocol
 * @return 0 if success 1 if failure
 */
int run_server_1(int make_blocking);

/**
 * Setup signal mask for helper threads (blocking handler) then call server helper
 * @param arg 
 * @return void * 
 */
void *setup_sigs_and_exec_handler(void *arg);

/**
 * Thread handler for the server upon accepting a new connection
 */
void server_handler(void *arg);

/**
 * Thread handler for the PART 2 BLOCKING server upon accepting a new connection
 */
void server_handler_blocking(void *arg);

/**
 * Handles the PUT request on the server
 * @param key Key as parsed from the request
 * @param value Value to put as part of the key
 * @param ret_buffer char **, allocates and returns if any data is to be returned
 * @param ret_size int *, the size of the buffer returned, 0 otherwise
 * @return 0 is success 1 if failure
 */
int server_1_put_request(char *key, char *value, char **ret_buffer, int *ret_size);

/**
 * Handles the GET request on the server
 * @param key Key as parsed from the request
 * @param ret_buffer char **, allocates and returns the data read from the DB
 * @param ret_size int *, the size of the read data
 * @return 0 if success 1 if failure
 */
int server_1_get_request(char *key, char **ret_buffer, int *ret_size);

/**
 * Handles the DELETE request on the server
 * @param key Key as parsed from the request
 * @param ret_buffer char **, allocates and returns the data read from the DB
 * @param ret_size int *, the size of the read data
 * @return 0 if success 1 if failure
 */
int server_1_delete_request(char *key, char **ret_buffer, int *ret_size);

/**
 * Handles the INSERT request on the server
 * @param key Key as parsed from the request
 * @param value Value to put as part of the key
 * @param ret_buffer char **, allocates and returns if any data is to be returned
 * @param ret_size int *, the size of the buffer returned, 0 otherwise
 * @return 0 is success 1 if failure
 */
int server_1_insert_request(char *key, char *value, char **ret_buffer, int *ret_size);

/*********************** ABD Algorithm *******************/
/**
 *
 * @param key
 * @param value
 * @param tag
 * @param response
 * @param response_size
 */
int server_1_put_request_abd(char *key, char *value, abd_tag_t *tag, char **response,
                             int *response_size);

/**
 *
 * @param key
 * @param value
 * @param tag
 * @param response
 * @param response_size
 */
int server_1_insert_request_abd(char *key, char *value, abd_tag_t *tag, char **response,
                                int *response_size);

/**
 *
 * @param key
 * @param tag
 * @param response
 * @param response_size
 */
int server_1_delete_request_abd(char *key, abd_tag_t *tag, char **response, int *response_size);

#endif //P1_CSRF_SERVER_PART1_H
