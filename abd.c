//
// Created by sps5394 on 12/1/18.
//

#include "abd.h"

int abd_tag_cmp(abd_tag_t *one, abd_tag_t *two) {
  if (one->tag > two->tag) {
    return 1;
  }
  if (one->tag == two->tag && one->client_id > two->client_id) {
    return 1;
  }
  return 0;
}