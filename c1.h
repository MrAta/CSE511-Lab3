//
// Created by MrAta on 11/13/18.
//
#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif
#ifndef P2_CSRF_C1_H
#define P2_CSRF_C1_H
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include "common.h"


#define DB_DIR "./.db"
#define SSTABLE "sstable"
#define MAX_C0_SIZE 100
#define INDEX_SIZE 62
extern char *charset;

extern int file_counter;

typedef struct c0_node {
  char *key;
  char *value;
  struct c0_node *left, *right;
  int ht;
  int flag; //0:valid, 1: invlaid
} c0_node;

typedef struct {
  int counter_value;
  long ssindex[INDEX_SIZE];
} c1_metadata;

extern int file_counter;
extern c1_metadata *metadata;

void c1_init();

int c1_batch_insert(c0_node *nodes[], int size);

char *c1_get(char *key);

/************** PRIVATE FUNCTIONS **************/
int char_to_idx(char c);

char *c1_search(FILE* file, char *key, int startline, int endline);

int merge(FILE *file1, FILE *file2);

int dump_metadata(c1_metadata *md);

int load_metadata(c1_metadata **md);

int marshall_metadata(c1_metadata *md, char **buffer);

int unmarshall_metadata(c1_metadata *md, char *buffer);

int update_sstable(FILE *dfile, c1_metadata *md);


// Code from: https://stackoverflow.com/questions/22697407/reading-text-file-into-char-array

#endif
