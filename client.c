#include <stdio.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <math.h>
#include <time.h>
#include <string.h>
// #define TARGET_ADDR "172.17.0.2"
#define TARGET_ADDR "127.0.0.1"
#define PORT 8086
#define N_KEY 37 //total number of unique keys
#define a 1.345675 //parameter for zipf distribution to obtain a 90% populariy for 10% of the keys.
#define ratio 0.1 // get/put ratio
#define key_size 10
#define value_size 10
#define MAX_ENTRY_SIZE 11264
#define NUM_THREADS 25
#define NUM_OPS 10000//total number of operation for workload
double zeta = 0.0; //zeta fixed for zipf
char *keys[N_KEY] = { NULL };
// char * values[N_KEY] = {NULL};
double popularities[N_KEY];
double cdf[N_KEY];
int arr_rate = 100;
int arr_type = 1;
struct sockaddr_in *serv_addr;
pthread_mutex_t fp_mutex;
FILE *fp;

static char *rand_string(char *str, size_t size) {
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

  if (connect(sock, (struct sockaddr *) serv_addr, sizeof(struct sockaddr_in)) < 0) {
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

  if (connect(sock, (struct sockaddr *) serv_addr, sizeof(struct sockaddr_in)) < 0) {
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

int main(int argc, char const *argv[]) {
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

  serv_addr = (struct sockaddr_in *) malloc(sizeof(struct sockaddr_in));
  memset(serv_addr, '0', sizeof(serv_addr));

  serv_addr->sin_family = AF_INET;
  serv_addr->sin_port = htons(PORT);

  // Convert IPv4 and IPv6 addresses from text to binary form
  if (inet_pton(AF_INET, TARGET_ADDR, &serv_addr->sin_addr) <= 0) {
    printf("\nInvalid address/ Address not supported \n");
    return -1;
  }

  printf("Writing All the keys...\n");
  struct timeval wtvs, wtve, atvs, atve;
  gettimeofday(&wtvs, NULL);
  write_all_keys();
  gettimeofday(&wtve, NULL);

  printf("All keys are written.\n");
  printf("Starting running the workload...\n");

  pthread_t client_thread[NUM_THREADS];
  gettimeofday(&atvs, NULL);
  for (int i = 0; i < NUM_THREADS; i++)
    pthread_create(&client_thread[i], NULL, client_func, NULL);
  for (int i = 0; i < 10; i++)
    pthread_join(client_thread[i], NULL);
  gettimeofday(&atve, NULL);
  double a_time_taken =
    (double) ( atve.tv_usec - atvs.tv_usec ) / 1000000 + (double) ( atve.tv_sec - atvs.tv_sec );
  double w_time_taken =
    (double) ( wtve.tv_usec - wtvs.tv_usec ) / 1000000 + (double) ( wtve.tv_sec - wtvs.tv_sec );
  printf("Finished running the workload.\n");
  printf("Inserting all keys took: %f\n", w_time_taken);
  printf("Workload took %f\n", a_time_taken);
  fclose(fp);
  return 0;
}
