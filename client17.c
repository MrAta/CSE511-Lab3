#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#define PORT 8086
// #define SERV_IP "127.0.0.1"
#define SERV_IP "130.203.16.49"

char *reqs[10] = {
    "INSERT Aman Jain",
    "GET Aman",
    "PUT Aman Jain",
    "PUT Aman Jain2",
    "DELETE Aman",
    "INSERT Fay Tay",
    "GET Fay",
    "Put Fay Tay",
    "Put Fay Tay2",
    "DELETE Fay"
};
// char *mello[] = {"DELETE Fay"};
// char *cello[] = {"GET Fay"};
// char *dello[] = {"INSERT Fay Tay"};
// char *fello[] = {"INSERT Aman Jain"};
// char *kello[] = {"DELETE Aman"};
struct sockaddr_in *serv_addr;

void *client_func() {
    int sock = 0, valread;

    char buffer[1024] = {0};

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        printf("\n Socket creation error \n");
        exit(0);
    }

    if (connect(sock, (struct sockaddr *)serv_addr, sizeof(struct sockaddr_in)) < 0)
    {
        printf("\nConnection Failed \n");
        exit(0);
    }

    char *b = reqs[rand()%10];
    send(sock, b, strlen(b), 0);
    printf("REQUEST SENT: %s\n", b);
    valread = read( sock , buffer, 1024);
    printf("RESPONSE: %s\n",buffer );
    close(sock);
}

int main(int argc, char const *argv[])
{
    pthread_t client_thread[10];

    serv_addr = (struct sockaddr_in*) malloc (sizeof(struct sockaddr_in));

    memset(serv_addr, '0', sizeof(serv_addr));

    serv_addr->sin_family = AF_INET;
    serv_addr->sin_port = htons(PORT);

    // Convert IPv4 and IPv6 addresses from text to binary form
    if(inet_pton(AF_INET, SERV_IP, &serv_addr->sin_addr)<=0)
    {
        printf("\nInvalid address/ Address not supported \n");
        return -1;
    }

    for (int i=0; i< 10; i++)
    pthread_create(&client_thread[i], NULL, client_func, NULL);
for (int i=0; i< 10; i++)
    pthread_join(client_thread[i], NULL);

    return 0;
}
