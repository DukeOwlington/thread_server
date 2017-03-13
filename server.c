#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <pthread.h>

#define BUFLEN 512
#define PORT 8888
#define MAX_CLIENTS 10

pthread_mutex_t dozen_lock; /* lock thread, until dozen of clients there*/

/* initialize sockaddr_in struct */
struct sockaddr_in InitializeAddr(void) {
  struct sockaddr_in serv_addr;

  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(PORT);
  serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);

  return serv_addr;
}

int HandleTCPConnection(int usr_sock) {
  char buf[BUFLEN];

  fflush(stdout);
  memset(buf, 0, 512);
  /* receive a message from client
     * check if someone disconnected*/
  if (recv(usr_sock , buf , 512, 0) == 0) {
    close(usr_sock);
    return -1;
  }
  /* send the message back to client */
  write(usr_sock, buf , strlen(buf));
  printf("TCP message: %s\n", buf);
  return 0;
}

void *DozenClients(void *socket_desc) {
  int master_sock, efd, fd;
  int i, nfds;
  unsigned int addrlen;
  int conn_count;
  bool master_dead = false;
  struct epoll_event event, events[MAX_CLIENTS]; /* epoll events */
  struct sockaddr_in client;

  conn_count = 0;
  pthread_mutex_lock(&dozen_lock);
  master_sock = (*(int *)socket_desc);
  addrlen = sizeof(client);
  /* create epoll descriptor */
  efd = epoll_create(MAX_CLIENTS);
  /* add sockets to epoll monitor */
  event.data.fd = master_sock;
  event.events = EPOLLIN;

  if (epoll_ctl(efd, EPOLL_CTL_ADD, master_sock, &event) < 0) {
    perror("epoll_ctl");
    pthread_exit((void *)EXIT_FAILURE);
  }

  while (true) {
    if (conn_count == MAX_CLIENTS) {
      pthread_mutex_unlock(&dozen_lock);
      epoll_ctl(efd, EPOLL_CTL_DEL, master_sock, &event);
      master_dead = true;
    }

    nfds = epoll_wait(efd, events, MAX_CLIENTS, -1);

    for (i = 0; i < nfds; i++) {
      /* if new user wants to connect */
      if (events[i].data.fd == master_sock && (events[i].events & EPOLLIN)) {
        /* if 10 clients have taken the thread, then skip client adding */
        if ((fd = accept(master_sock, (struct sockaddr *)&client, (socklen_t*)&addrlen)) < 0) {
          perror("accept");
          pthread_exit((void *)EXIT_FAILURE);
        }
        event.data.fd = fd;
        if (epoll_ctl(efd, EPOLL_CTL_ADD, fd, &event) < 0) {
          perror("epoll_ctl");
          pthread_exit((void *)EXIT_FAILURE);
        }
        conn_count++;
      }
      /* else if user wants to leave a message */
      else if (events[i].events & EPOLLIN) {
        if (HandleTCPConnection(events[i].data.fd) < 0)
          epoll_ctl(efd, EPOLL_CTL_DEL, events[i].data.fd, &event);
          conn_count--;
      }
    }
    /* if thread was full but then no connections left */
    if (master_dead && conn_count == 0)
      break;
  }
  free(events);
  pthread_exit(NULL);
}

int main(void) {
  int master_sock;
  struct sockaddr_in server;

  /* create a stream socket */
  if ((master_sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
    perror("socket");
    return EXIT_FAILURE;
  }

  /* initialize addr struct */
  server = InitializeAddr();


  /* bind socket to port */
  if (bind(master_sock, (struct sockaddr*) &server, sizeof(server)) == -1) {
    perror("bind");
    return EXIT_FAILURE;
  }

  /* listen connections up to MAX_PENDING_CON in the queue */
  listen(master_sock , MAX_CLIENTS);

  if (pthread_mutex_init(&dozen_lock, NULL) < 0) {
    perror("mutex failed: ");
    return EXIT_FAILURE;
  }

  while (true) {
    pthread_t client_thread;
    pthread_mutex_lock(&dozen_lock);

    if(pthread_create(&client_thread, NULL,  DozenClients, (void *) &master_sock) < 0) {
      perror("could not create thread");
      return 1;
    }
    pthread_mutex_unlock(&dozen_lock);
    sleep(1);
  }
  pthread_mutex_destroy(&dozen_lock);
  close(master_sock);
  return 0;
}
