#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <pthread.h>

#define BUFLEN 512
#define PORT 8888

void *HandleClientMsg(void *sock_desc) {
    /* get the socket descriptor */
    char buf[BUFLEN];
    int sock = *(int*)socket_desc;
    int read_size;

    /* receive a message from client */
    while( (read_size = recv(sock , client_message , 2000 , 0)) > 0 ) {
        /* send the message back to client */
        write(sock , client_message , strlen(client_message));
    }

    free(socket_desc);
    return 0;
}

int main(void) {
  struct sockaddr_in server;
  struct sockaddr_in sender;
  int *new_sock;
  int sock, cl_sock;
  int recv_len;
  unsigned int slen;

  slen = sizeof(sender);

  /* create a stream socket */
  if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
    perror("socket");
    return EXIT_FAILURE;
  }

  /* zero out the structure */
  memset((char *) &server, 0, sizeof(server));

  server.sin_family = AF_INET;
  server.sin_port = htons(PORT);
  server.sin_addr.s_addr = htonl(INADDR_ANY);

  /* bind socket to port */
  if (bind(sock, (struct sockaddr*) &server, sizeof(server)) == -1) {
    perror("bind");
    return EXIT_FAILURE;
  }

  /* listen connections up to 3 in the queue */
  listen(sock , 3);

  /* keep listening for data */
  while ((cl_sock = accept(sock, (struct sockaddr *)&sender, (socklen_t*)&slen))) {
    pthread_t client_thread;
    new_sock = malloc(1);
    *new_sock = client_sock;

    if(pthread_create(&client_thread, NULL,  HandleClientMsg, (void*) new_sock) < 0) {
      perror("could not create thread");
      return 1;
    }
  }

  if (cl_sock < 0) {
    perror("accept failed");
    return 1;
  }
  close(sock);
  return 0;
}
