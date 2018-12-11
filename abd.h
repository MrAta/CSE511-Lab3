//
// Created by sps5394 on 12/1/18.
//

#ifndef P3_CSRF_ABD_H
#define P3_CSRF_ABD_H

#include <stdint.h>

typedef struct {
  uint32_t timestamp;
  uint32_t client_id;
} abd_tag_t;

int abd_tag_cmp(abd_tag_t *one, abd_tag_t *two);
typedef struct {
  char * value;
  char * key;
  abd_tag_t *tag;
} abd_message;

typedef struct{
  int node_id;
  char * key;
  char * value;
  abd_tag_t *tag;
} abd_arg;

#endif //P3_CSRF_ABD_H
