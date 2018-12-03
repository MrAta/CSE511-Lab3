//
// Created by sps5394 on 12/2/18.
//

#ifndef P3_CSRF_DICT_H
#define P3_CSRF_DICT_H
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "pqueue.h"
/**
 * Code courtesy of Vijay Mathew (User, StackOverflow).
 * Link: https://stackoverflow.com/questions/4384359/quick-way-to-implement-dictionary-in-c
 */

typedef char * hash_key_t;
typedef pqueue * hash_value_t;

struct nlist { /* table entry: */
  struct nlist *next; /* next entry in chain */
  hash_key_t name; /* defined name */
  hash_value_t defn; /* replacement text */
};

#define HASHSIZE 101
static struct nlist *hashtab[HASHSIZE]; /* pointer table */

/* Duplicate a given string */
char * strdup(const char *s);

/* lookup: look for s in hashtab */
struct nlist *lookup(hash_key_t s);

/* install: put (name, defn) in hashtab */
struct nlist *install(hash_key_t name, hash_value_t defn);
#endif //P3_CSRF_DICT_H
