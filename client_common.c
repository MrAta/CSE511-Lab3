//
// Created by sps5394 on 12/2/18.
//

#include "client_common.h"
int nextReqType() {
  double p = (rand() / (RAND_MAX+1.0));
  //req types: 0 indicates get request, 1 indicates put request
  if(p <= ratio) return 0;
  return 1;

}

double nextArrival() {
  if (arr_type == 1){//uniform
    return 1/arr_rate;
  }
  else{//exponential
    return (-1 * log(1 - (rand()/(RAND_MAX+1.0)))/(arr_rate));
  }
  return 1/arr_rate; //default: uniform
}

char *next_key() {
  double p = (rand() / (RAND_MAX+1.0))*cdf[N_KEY - 1];
  //TODO: optimize it with binary search
  for(int i=0; i< N_KEY; i++)
    if(p <= cdf[i]){
      return keys[i];
    }
  return NULL;
}

void calc_cdf() {
  cdf[0] = popularities[0];
  for(int i=1; i < N_KEY; i++){
    cdf[i] = cdf[i-1] + popularities[i];
  }
}

void generate_popularities() {
  for(int i=0; i < N_KEY; i++){
    popularities[i] = zipf(i+1);
  }
}

double zipf(int x) {
  return (1/(pow(x,a)))/zeta;
}

void generate_key_values() {
  int seed = time(NULL);
  srand(seed);
  for(int i=0; i< N_KEY; i++){
    char * _key = rand_string_alloc(key_size);
    keys[i] = _key;
  }
}

double calc_zeta() {
  double sum = 0.0;
  for(int i=1; i < N_KEY+1; i++)
    sum += (double)1.0/(pow(i,a));

  return sum;
}

char *rand_string_alloc(size_t size) {
  char *s = malloc(size + 1);
  if (s) {
    rand_string(s, size);
  }
  return s;
}

char *rand_string(char *str, size_t size) {
  const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
  if (size) {
    for (size_t n = 0; n < size; n++) {
      int key = rand() % (int) (sizeof charset - 1);
      str[n] = charset[key];
    }
    str[size] = '\0';
  }
  return str;
}