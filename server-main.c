//
// Created by sps5394 on 10/18/18.
//

#include <stdio.h>
#include <stdlib.h>
#include "server-part1.h"
#include "c0.h"

int main(int argc, char *argv[]) {
  int rem_args = 0;
  if (argc < 2) {
    printf("Usage:\n(1) abd\n(2) blocking\n(optional) specify r to enter recovery mode\n");
    return 1;
  }

  if (argv[1] && ( strncmp(argv[1], "1", 1) == 0 )) {
    blocking = 0;
    if (argv[2] && ( strncmp(argv[2], "r", 1) == 0 )) {
      printf("Recovering from crash...\n");
      _T = NULL;
      if (recover() != 0) {
        printf("Error during recovery.\n");
        // continue or stop?
      } else {
        printf("Recovery complete!\n");
      }
    } else {
      _T = NULL; // dont reinitialize to null inside run_server_1 cause then all of our nodes from recovery are leaked
    }
    printf("Beginning [abd] server...\n");
  } else if (argv[1] && ( strncmp(argv[1], "2", 1) == 0 )) {
    blocking = 1;
    if (argv[2]) {
      node_id = atoi(argv[2]);
      printf("node_id: %d\n", node_id);
    } else {
      printf("No node_id specified for blocking server.\n");
      return 1;
    }
    if (argv[3] && ( strncmp(argv[3], "r", 1) == 0 )) {
      rem_args = argc - 4;
      printf("Recovering from crash...\n");
      _T = NULL;
      if (recover() != 0) {
        printf("Error during recovery.\n");
        // continue or stop?
      } else {
        printf("Recovery complete!\n");
      }
    } else {
      rem_args = argc - 3;
      _T = NULL; // dont reinitialize to null inside run_server_1 cause then all of our nodes from recovery are leaked
    }
    printf("Beginning [blocking] server...\n");
  } else {
    printf("Invalid arguments.\n");
  }

  printf("Waiting for new connections...\n");
  return run_server_1(rem_args, argv[3] ? &argv[3] : &argv[2]);
}
