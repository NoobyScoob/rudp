#include "file.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netdb.h>
#include <unistd.h>

/*
 *  Here is the starting point for your netster part.3 definitions. Add the 
 *  appropriate comment header as defined in the code formatting guidelines
 */

/* Add function definitions */

#define DEFAULT_MSG_SIZE 256 // default message size
#define DEFAULT_TIMER 50000

struct payload {
  int seq_no;
  char data[DEFAULT_MSG_SIZE];
  int data_size;
};

void stopandwait_server(char* iface, long port, FILE* fp) {
  int sockfd;
  struct addrinfo *res, *next_res;
  struct addrinfo hints = {
    .ai_family = AF_INET,
    .ai_socktype = SOCK_DGRAM,
    .ai_flags = AI_PASSIVE
  };

  char str_port[8];
  sprintf(str_port, "%ld", port);
  if (getaddrinfo(iface, str_port, &hints, &res) != 0) {
    printf("Error in getaddrinfo!\n");
    return;
  }

  // hop the dns for the socket connection
  for (next_res = res; ;next_res = next_res->ai_next) {
    sockfd = socket(next_res->ai_family, next_res->ai_socktype, next_res->ai_protocol);
    if (sockfd < 0) close(sockfd);
    else break;
  }
  // free up the initial res pointer
  freeaddrinfo(res);

  // bind the socket to a port
  // socaddr_in -- ipv4 is cast to sockaddr
  int binding = bind(sockfd, next_res->ai_addr, next_res->ai_addrlen); 
  if (binding < 0) {
    printf("Error while binding the port!\n");
    return;
  }

  struct sockaddr_in client_addr;
  socklen_t client_addr_len = sizeof(client_addr);

  memset(&client_addr, 0, sizeof(client_addr));

  // last packet we got
  int packet_no = -1000;
  struct payload msg_send;
  struct payload msg_recv;
  memset(&msg_recv, '\0', sizeof(msg_recv));

  for (;;) {
    recvfrom(sockfd, &msg_recv, sizeof(struct payload), 0, (struct sockaddr *) &client_addr, &client_addr_len);
    // send acknowledgement
    memset(&msg_send, '\0', sizeof(msg_send));
    msg_send.seq_no = msg_recv.seq_no;
    sendto(sockfd, &msg_send, sizeof(struct payload), 0, (struct sockaddr *) &client_addr, client_addr_len);
    // if its an empty packet close the server
    if (msg_recv.data_size == 0) break;

    if (msg_recv.seq_no != packet_no) {
      // write the respective packet
      fwrite(msg_recv.data, 1, msg_recv.data_size, fp);
      packet_no = msg_recv.seq_no;
      memset(&msg_recv, '\0', sizeof(msg_recv));
    }
  }
  
  close(sockfd);
  return;
}

void stopandwait_client(char* host, long port, FILE* fp) {
  int sockfd;
  struct addrinfo *res, *next_res;

  // using spec c99's designated initializers
  // avoiding memsets
  struct addrinfo hints = {
    .ai_family = AF_INET,
    .ai_socktype = SOCK_DGRAM,
    .ai_flags = AI_PASSIVE
  };

  // convert port number to string
  char str_port[8];
  sprintf(str_port, "%ld", port);

  // getaddrinfo to get linked list of results 
  if (getaddrinfo(host, str_port, &hints, &res) != 0) {
    printf("Error in getaddrinfo!\n");
    return;
  }

  // hop the dns for the socket connection
  for (next_res = res; ;next_res = next_res->ai_next) {
    sockfd = socket(next_res->ai_family, next_res->ai_socktype, next_res->ai_protocol);
    if (sockfd < 0) close(sockfd);
    else break;
  }
  // free up the initial res pointer
  freeaddrinfo(res);

  // set socket timer 
  struct timeval timeout = {
    .tv_sec = 0,
    .tv_usec = DEFAULT_TIMER
  };
  int setsock = setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
  if (setsock < 0) {
    printf("Error setting timeout!\n");
    return;
  }

  unsigned int packet_no = 0;
  struct payload msg_send;
  struct payload msg_recv;

  memset(msg_send.data, '\0', DEFAULT_MSG_SIZE);
  memset(msg_recv.data, '\0', DEFAULT_MSG_SIZE);

  // client loop
  for (;;) {
    size_t read_size = fread(msg_send.data, 1, DEFAULT_MSG_SIZE, fp);
    msg_send.seq_no = packet_no;
    msg_send.data_size = read_size;

    for (;;) {
      sendto(sockfd, &msg_send, sizeof(struct payload), 0, next_res->ai_addr, next_res->ai_addrlen);
      recvfrom(sockfd, &msg_recv, sizeof(struct payload), 0, next_res->ai_addr, &(next_res->ai_addrlen));
      // check the sequence no of the acknowledgement
      if (msg_recv.seq_no == packet_no) {
        if (read_size == 0) {
          close(sockfd);
          return;
        }
        packet_no++;
        break;
      }
    }
  }

  close(sockfd);
  return;
}