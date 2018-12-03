//
// Created by sps5394 on 12/2/18.
//

#ifndef P3_CSRF_CLIENT_COMMON_H
#define P3_CSRF_CLIENT_COMMON_H

#include <stdlib.h>
#include <math.h>
#include <sys/time.h>

#define N_KEY 37 //total number of unique keys
#define a 1.345675 //parameter for zipf distribution to obtain a 90% populariy for 10% of the keys.
#define ratio 0.1 // get/put ratio
#define key_size 10
#define value_size 10
#define MAX_ENTRY_SIZE 11264
#define NUM_THREADS 25

#define NUM_OPS 10000//total number of operation for workload

int arr_rate = 100;
int arr_type = 1;
double zeta = 0.0; //zeta fixed for zipf
char * keys[N_KEY] = {NULL};
// char * values[N_KEY] = {NULL};
double popularities[N_KEY];
double cdf[N_KEY];

static char *rand_string(char *str, size_t size);

char* rand_string_alloc(size_t size);

double calc_zeta();

void generate_key_values();

double zipf(int x);

void generate_popularities();

void calc_cdf();

char * next_key();

double nextArrival();

int nextReqType();
#endif //P3_CSRF_CLIENT_COMMON_H
