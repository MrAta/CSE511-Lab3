//
// Created by sps5394 on 10/18/18.
//

#include <stdio.h>
#include <stdlib.h>
#include "server-part1.h"
#include "c0.h"

int main(int argc, char *argv[]) {
  if (argc < 2) {
    printf("Must provide value for what sort of server to run\n");
    return 1;
  }
  if (argv[2] && ( strncmp(argv[2], "r", 1) == 0 )) {
    printf("Recovering from crash...\n");
    _T = NULL;
    if (recover() != 0) {
      printf("Error during recovery.\n");
    } else {
      printf("Recovery complete!\n");
    }
  } else {
    _T = NULL; // dont reinitialize to null inside run_server_1 cause then all of our nodes from recovery are leaked
  }
  printf("\nWaiting for new connections...\n");
  return run_server_1(atoi(argv[1]));
}
