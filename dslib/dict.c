//
// Created by sps5394 on 12/2/18.
//

#include "dict.h"

/* hash: form hash value for string s */
unsigned hash(char *s);

unsigned hash(char *s) {
  unsigned hashval;
  for (hashval = 0; *s != '\0'; s++)
    hashval = *s + 31 * hashval;
  return hashval % HASHSIZE;
}

struct nlist *install(hash_key_t name, hash_value_t defn) {
  struct nlist *np;
  unsigned hashval;
  if (( np = lookup(name)) == NULL) { /* not found */
    np = (struct nlist *) malloc(sizeof(*np));
    if (np == NULL || ( np->name = strdup(name)) == NULL)
      return NULL;
    hashval = hash(name);
    np->next = hashtab[hashval];
    hashtab[hashval] = np;
  } else {
    free((void *) np->defn); /*free previous defn */
  }
  np->defn = defn;
  return np;
}

struct nlist *lookup(hash_key_t s) {
  struct nlist *np;
  for (np = hashtab[hash(s)]; np != NULL; np = np->next)
    if (strcmp(s, np->name) == 0)
      return np; /* found */
  return NULL; /* not found */
}

char *strdup(const char *s) {
  char *p;
  p = (char *) malloc(strlen(s) + 1); /* +1 for ’\0’ */
  if (p != NULL)
    strcpy(p, s);
  return p;
}