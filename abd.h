//
// Created by sps5394 on 12/1/18.
//

#ifndef P3_CSRF_ABD_H
#define P3_CSRF_ABD_H

#include <stdint.h>

typedef struct {
  uint32_t tag;
  uint32_t client_id;
} abd_tag_t;

int abd_tag_cmp(abd_tag_t *one, abd_tag_t *two);

#endif //P3_CSRF_ABD_H
