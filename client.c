#include <stdio.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include<semaphore.h>
#include <math.h>
#include <time.h>
#include <string.h>
#include "abd.h"
#include "common.h"
// #define TARGET_ADDR "172.17.0.2"
#define TARGET_ADDR "127.0.0.1"
// #define PORT 8086
#define N_KEY 37 //total number of unique keys
#define a 1.345675 //parameter for zipf distribution to obtain a 90% populariy for 10% of the keys.
#define ratio 0.1 // get/put ratio
#define key_size 10
#define value_size 10
//#define MAX_ENTRY_SIZE 11264
#define NUM_THREADS 25
#define NUM_OPS 10000//total number of operation for workload
double zeta = 0.0; //zeta fixed for zipf
#define NUM_NODES 1
char * NODES[] = {"127.0.0.1"};
char * keys[N_KEY] = {NULL};
// char * values[N_KEY] = {NULL};
double popularities[N_KEY];
double cdf[N_KEY];
int arr_rate = 100;
int arr_type = 1;
#define CLIENT_ID 10
pthread_mutex_t fp_mutex;
pthread_mutex_t lrq_m;//, lwq_m;
sem_t rq_sem, wq_sem;
FILE *fp;
char * nodes[] = {"127.0.0.1"};
struct sockaddr_in *serv_addrs[NUM_NODES];
static char *rand_string(char *str, size_t size)
{
  // int seed = time(NULL);
  // srand(seed);
  const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
  if (size) {
    //--size;
    for (size_t n = 0; n < size; n++) {
      int key = rand() % (int) ( sizeof charset - 1 );
      str[n] = charset[key];
    }
    str[size] = '\0';
  }
  return str;
}

char *rand_string_alloc(size_t size) {
  char *s = malloc(size + 1);
  if (s) {
    rand_string(s, size);
  }
  return s;
}

double calc_zeta() {
  double sum = 0.0;
  for (int i = 1; i < N_KEY + 1; i++)
    sum += (double) 1.0 / ( pow(i, a));

  return sum;
}

void generate_key_values() {
  int seed = time(NULL);
  srand(seed);
  for (int i = 0; i < N_KEY; i++) {
    char *_key = rand_string_alloc(key_size);
    keys[i] = _key;
  }
}

double zipf(int x) {
  return ( 1 / ( pow(x, a))) / zeta;
}

void generate_popularities() {
  for (int i = 0; i < N_KEY; i++) {
    popularities[i] = zipf(i + 1);
  }
}

void calc_cdf() {
  cdf[0] = popularities[0];
  for (int i = 1; i < N_KEY; i++) {
    cdf[i] = cdf[i - 1] + popularities[i];
  }
}

char *next_key() {
  // int seed = time(NULL);
  // srand(seed);
  double p = ( rand() / ( RAND_MAX + 1.0 )) * cdf[N_KEY - 1];
  //TODO: optimize it with binary search
  for (int i = 0; i < N_KEY; i++)
    if (p <= cdf[i]) {
      return keys[i];
    }
  return NULL;
}

double nextArrival() {
  // int seed = time(NULL);
  // srand(seed);
  if (arr_type == 1) {//unifrom
    return 1 / arr_rate;
  } else {//exponential
    return ( -1 * log(1 - ( rand() / ( RAND_MAX + 1.0 ))) / ( arr_rate ));
  }
  return 1 / arr_rate; //default: unifrom :D
}

int nextReqType() {
  // int seed = time(NULL);
  // srand(seed);
  double p = ( rand() / ( RAND_MAX + 1.0 ));
  //req types: 0 indicates get requst, 1 indicates put request
  if (p <= ratio) return 0;
  return 1;

}

void write_all_keys() {
  int seed = time(NULL);
  srand(seed);
  int sock = 0, valread;

  char buffer[MAX_ENTRY_SIZE] = { 0 };

  if (( sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    printf("\n Socket creation error \n");
    exit(0);
  }

  if (connect(sock, (struct sockaddr *)serv_addrs[0], sizeof(struct sockaddr_in)) < 0)
  {
      printf("\nConnection Failed \n");
      exit(0);
  }
  //To keep the system in steady state, we should have most popular keys in cache.
  //Therefor, we first write less popular keys and then popular keys to fill the cache
  printf("Start Writing...\n");
  for (int i = N_KEY - 1; i >= 0; i--) {
    char cmd[MAX_ENTRY_SIZE] = "";
    char *_cmd = "INSERT ";
    char *key = keys[i];
    char *s = " ";
    char *val = rand_string_alloc(value_size);
    strcat(cmd, _cmd);
    strcat(cmd, key);
    strcat(cmd, s);
    strcat(cmd, val);
    send(sock, cmd, strlen(cmd), 0);
    valread = read(sock, buffer, MAX_ENTRY_SIZE);
    if (valread > 0) {
      printf("Inserted %s, %d key(s).\n", key, N_KEY - i + 1);
    }
  }
  close(sock);
}

void *client_func() {
  int seed = time(NULL);
  srand(seed);
  int sock = 0, valread;
  char *gbg;

  char buffer[MAX_ENTRY_SIZE] = { 0 };

  if (( sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    printf("\n Socket creation error \n");
    exit(0);
  }

  if (connect(sock, (struct sockaddr *)serv_addrs[0], sizeof(struct sockaddr_in)) < 0)
  {
      printf("\nConnection Failed \n");
      exit(0);
  }
  int local_count = 0;
  while (local_count <= (int) ( NUM_OPS / NUM_THREADS )) {

    char cmd[MAX_ENTRY_SIZE] = "";
    char *_cmd = "PUT ";
    if (nextReqType() == 0) {
      _cmd = "GET ";

      char *key = next_key();
      strcat(cmd, _cmd);
      strcat(cmd, key);
    } else {

      char *key = next_key();
      char *s = " ";
      char *val = rand_string_alloc(value_size);

      strcat(cmd, _cmd);
      strcat(cmd, key);
      strcat(cmd, s);
      strcat(cmd, val);
    }

    struct timeval rtvs, rtve;
    gettimeofday(&rtvs, NULL);
    send(sock, cmd, strlen(cmd), 0);
    valread = read(sock, buffer, MAX_ENTRY_SIZE);
    if (valread == 0) {
      printf("Socket closed\n");
      close(sock);
      return (void *) gbg;
    }
    gettimeofday(&rtve, NULL);
    double time_taken =
      (double) ( rtve.tv_usec - rtvs.tv_usec ) / 1000000 + (double) ( rtve.tv_sec - rtvs.tv_sec );
    pthread_mutex_lock(&fp_mutex);
    fprintf(fp, "%f\n", time_taken);
    pthread_mutex_unlock(&fp_mutex);
    // printf("SENT %s || RESPONSE: buffer\n", cmd);
    printf("Sent %d requests so far\n", local_count + 1);
    usleep(nextArrival() * 1000000 * NUM_THREADS);
    local_count++;
  }
  close(sock);
  return (void *) gbg;
}

/**
 * Read Query to nodes for (t,v)
 * @param name The key to get
 */
void *send_abd_read_query(void * arg){
   abd_arg * _arg = (abd_arg *)(arg);
   int i = _arg->node_id;
   int sock = 0, valread;
   char *gbg;

   char buffer[MAX_ENTRY_SIZE] = {0};

   if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
   {
       printf("\n Socket creation error \n");
       exit(0);
   }

   if (connect(sock, (struct sockaddr *)serv_addrs[i], sizeof(struct sockaddr_in)) < 0)
   {
       printf("\nConnection Failed \n");
       exit(0);
   }
   char cmd[MAX_ENTRY_SIZE] = "";
   char * _cmd = "GET ";
   strcat(cmd, _cmd);
   strcat(cmd, _arg->key);
   char * s = " ";
   strcat(cmd, s);

   char *new_s = calloc(MAX_ENTRY_SIZE + MAX_TAG_SIZE + 3, sizeof(char));
   //write key , htag->value, htag->tag->tag, htag->tag->client-id
   snprintf(new_s, MAX_ENTRY_SIZE + MAX_TAG_SIZE + 3, "%s %d %d\n", "-", _arg->tag->timestamp, _arg->tag->client_id);

   strcat(cmd, new_s);   //read response

   printf("MATA0: %s\n",cmd );
   send(sock, cmd, strlen(cmd), 0);
   valread = read(sock, buffer, MAX_ENTRY_SIZE);
   if(valread == 0){
     printf("Socket closed\n");
     close(sock);
     return (void *) gbg;
   }
   pthread_mutex_lock(&lrq_m);
   char * save_ptr;
   char *  req_str = (char *) malloc(MAX_ENTRY_SIZE * sizeof(char));
   strcpy(req_str, buffer);

   //parse valread
   char * v = NULL;
   v = strtok_r(req_str, " ", &save_ptr);
   strcpy(_arg->value, v);
   abd_tag_t * tag = malloc(sizeof(*tag));
   tag->timestamp = (uint32_t) atoi(strtok_r(req_str, " ", &save_ptr));
   tag->client_id = (uint32_t) atoi(strtok_r(req_str, " ", &save_ptr));
   if(abd_tag_cmp(tag, _arg->tag)){
        _arg->tag = tag;
      }
   pthread_mutex_unlock(&lrq_m);
   free(req_str);
   sem_post(&rq_sem);
    close(sock);
 }
/**
 * Read Query to nodes for (t,v)
 * @param name The key to get
 */
abd_arg * abd_read_query(char *key){
  pthread_t query_thread[NUM_NODES];
  abd_arg * arg = malloc(sizeof(*arg));
  arg->key = key;
  arg->value = (char*)malloc(MAX_ENTRY_SIZE * sizeof(char));
  arg->tag = malloc(sizeof(abd_tag_t));
  arg->tag->client_id = CLIENT_ID;
  arg->tag->timestamp = 0;
  for (int i=0; i < NUM_NODES; i++){
      pthread_mutex_lock(&lrq_m);
      arg->node_id = i; //TODO: other way to pass node_id
      pthread_mutex_unlock(&lrq_m);
    pthread_create(&query_thread[i], NULL, send_abd_read_query, (void*)arg);
  }
  int sem_val;
  sem_getvalue(&rq_sem, &sem_val);
  while (sem_val < (int)NUM_NODES/2 + 1) {
    sem_getvalue(&rq_sem, &sem_val);
  }

  //TODO: force threads to Finished
  //TODO: reset the semaphore value
  return arg;
}

/**
 * Read Query to nodes for (t,v)
 * @param name The key to get
 */
void *send_abd_write_query(void * arg){
   abd_arg * _arg = (abd_arg *)(arg);
   int i = _arg->node_id;
   int sock = 0, valread;
   char *gbg;

   char buffer[MAX_ENTRY_SIZE] = {0};

   if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
   {
       printf("\n Socket creation error \n");
       exit(0);
   }

   if (connect(sock, (struct sockaddr *)serv_addrs[i], sizeof(struct sockaddr_in)) < 0)
   {
       printf("\nConnection Failed \n");
       exit(0);
   }
   char cmd[MAX_ENTRY_SIZE] = "";
   char * _cmd = "PUT ";
   strcat(cmd, _cmd);
   strcat(cmd, _arg->key);
   char * s = " ";
   strcat(cmd, s);   //read response
   strcat(cmd, _arg->value);   //read response
   send(sock, cmd, strlen(cmd), 0);
   valread = read(sock, buffer, MAX_ENTRY_SIZE);
   if(valread == 0){
     printf("Socket closed\n");
     close(sock);
     return (void *) gbg;
   }
   sem_post(&wq_sem);
    close(sock);
 }


abd_arg * abd_write_query(char *key, char * value){
  pthread_t query_thread[NUM_NODES];
  abd_arg * arg = malloc(sizeof(*arg));
  arg->key = key;
  arg->value = value;
  arg->tag = NULL;
  for (int i=0; i < NUM_NODES; i++){
      // pthread_mutex_lock(&lwq_m);
      arg->node_id = i;
      // pthread_mutex_unlock(&lwq_m);
    pthread_create(&query_thread[i], NULL, send_abd_write_query, (void*)arg);
  }
  int sem_val;
  sem_getvalue(&wq_sem, &sem_val);
  while (sem_val < (int)NUM_NODES/2 + 1) {
    sem_getvalue(&wq_sem, &sem_val);
  }

  //TODO: force threads to Finished
  //TODO: reset the semaphore value
  return arg;
}


/**
 * An implementation of ABD read protocol
 * @param name The key to get
 */
void abd_read(char *key){
  //do a read query to get the higherst tag,v
  abd_arg * htag = abd_read_query(key);
  char *new_value = calloc(MAX_ENTRY_SIZE + MAX_TAG_SIZE + 3, sizeof(char));
  //write key , htag->value, htag->tag->tag, htag->tag->client-id
  snprintf(new_value, MAX_ENTRY_SIZE + MAX_TAG_SIZE + 3, "%s %d %d\n", htag->value, htag->tag->timestamp, htag->tag->client_id);
  //now write "key value tag" to all servers!
  abd_write_query(key, new_value);


}

/**
 * An implementation of ABD write protocol
 * @param name The key to write
 * @param name The value for the given key
 * @param name The current timestamp for the client
 */
void abd_write(char *key, char * value, long timestamp){
//  abd_arg * htag = abd_read_query(key);
  char *new_value = calloc(MAX_ENTRY_SIZE + MAX_TAG_SIZE + 3, sizeof(char));
  //snprintf(new_value, MAX_ENTRY_SIZE + MAX_TAG_SIZE + 3, "%s %d %d\n", value, htag->tag->tag+1, htag->tag->client_id);
  //now write "key value tag" to all servers!

  snprintf(new_value, MAX_ENTRY_SIZE + MAX_TAG_SIZE + 3, "%s %d %d\n", value, 0, CLIENT_ID);

  abd_write_query(key, new_value);

}
void setup_server_addr(int i, char * target_addr){

  serv_addrs[i] = (struct sockaddr_in*) malloc (sizeof(struct sockaddr_in));
  memset(serv_addrs[i], '0', sizeof(serv_addrs[i]));

  serv_addrs[i]->sin_family = AF_INET;
  serv_addrs[i]->sin_port = htons(PORT);

  // Convert IPv4 and IPv6 addresses from text to binary form
  if(inet_pton(AF_INET, target_addr, &serv_addrs[i]->sin_addr)<=0)
  {
      printf("\nInvalid address/ Address not supported \n");
      return;
  }

}
int main(int argc, char const *argv[]){
  fp = fopen("response_time.log", "w");
  pthread_mutex_init(&fp_mutex, 0);
  pthread_mutex_init(&lrq_m, 0);
  // pthread_mutex_init(&lwq_m, 0);
    for (int i=0; i < NUM_NODES; i++){
    setup_server_addr(i, NODES[i]);
  }
  sem_init(&rq_sem, 0, 0);
  sem_init(&wq_sem, 0, 0);
  char * key = "Aman";
  char * value = "Jain";
  long timestamp = 10;
  printf("ATA00\n" );
  abd_write(key, value, timestamp);
  fclose(fp);
  return 0;
}
  
int old_main(int argc, char const *argv[])//wait! what? old main? :D
  // For the Glory of Old State
  // For her founders strong and great
  // For the future that we wait,
  // Raise the song, raise the song.
{
    fp = fopen("response_time.log", "w");
    pthread_mutex_init(&fp_mutex, 0);
    printf("Generating Keys...\n");
    generate_key_values();
    printf("Generated Keys.\n");
    zeta = calc_zeta();
    printf("Generating popularities...\n");
    generate_popularities();
    calc_cdf();
    printf("Generated popularities.\n");

    for (int i=0; i < NUM_NODES; i++){
      setup_server_addr(i, NODES[i]);
    }
    printf("Writing All the keys...\n");
    struct timeval  wtvs, wtve, atvs, atve;
    gettimeofday(&wtvs, NULL);
    write_all_keys();
    gettimeofday(&wtve, NULL);

    printf("All keys are written.\n");
    printf("Starting running the workload...\n" );
    sem_init(&rq_sem, 0, 0);
    sem_init(&wq_sem, 0, 0);
    pthread_t client_thread[NUM_THREADS];
    gettimeofday(&atvs, NULL);
    for(int i=0; i<NUM_THREADS; i++)
      pthread_create(&client_thread[i], NULL, client_func, NULL);
    for(int i=0; i<10; i++)
      pthread_join(client_thread[i], NULL);
    gettimeofday(&atve, NULL);
    double a_time_taken = (double) (atve.tv_usec - atvs.tv_usec) / 1000000 + (double) (atve.tv_sec - atvs.tv_sec);
    double w_time_taken = (double) (wtve.tv_usec - wtvs.tv_usec) / 1000000 + (double) (wtve.tv_sec - wtvs.tv_sec);
    printf("Finished running the workload.\n");
    printf("Inserting all keys took: %f\n", w_time_taken);
    printf("Workload took %f\n",a_time_taken);
    fclose(fp);
    return 0;
}
