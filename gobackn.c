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
#include <errno.h>
#include <time.h>
#include <fcntl.h>

/*
 *  Here is the starting point for your netster part.4 definitions. Add the 
 *  appropriate comment header as defined in the code formatting guidelines
 */

/* Add function definitions */
#define DEFAULT_MSG_SIZE 255 // default message size
#define FAILED -1 // failure
#define TIMEOUT 0.0006
#define N 128 // packet window length

struct packet {
  unsigned int header;
  char payload[DEFAULT_MSG_SIZE];
  unsigned int size;
};

struct Node {
  struct packet* pkt;
  struct Node* next;
};

void gbn_server(char* iface, long port, FILE* fp) {
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

  int nxt_pkt_num = 0;
  struct packet snd_pkt;
  struct packet rcv_pkt;

  memset(&rcv_pkt, '\0', sizeof(rcv_pkt));

  for (;;) {
    recvfrom(sockfd, &rcv_pkt, sizeof(struct packet), 0, (struct sockaddr *) &client_addr, &client_addr_len);

    if (nxt_pkt_num == rcv_pkt.header || rcv_pkt.size == -1) {
      memset(&snd_pkt, '\0', sizeof(snd_pkt));
      snd_pkt.header = rcv_pkt.header;
      snd_pkt.size = rcv_pkt.size;
      sendto(sockfd, &snd_pkt, sizeof(struct packet), 0, (struct sockaddr *) &client_addr, client_addr_len);

      // close connection
      if (rcv_pkt.size == -1) break;

      fwrite(rcv_pkt.payload, 1, rcv_pkt.size, fp);
      nxt_pkt_num = rcv_pkt.header + 1;
      memset(&rcv_pkt, '\0', sizeof(rcv_pkt));
    }
  }
  
  close(sockfd);
  return;
}

void gbn_client(char* host, long port, FILE* fp) {
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
  // this is just to make the call non blocking
  // we use an explicit timer to resend the packets in the actual algorithm
  // struct timeval timeout = {
  //   .tv_sec = 0,
  //   .tv_usec = 10
  // };
  // int setsock = setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*) &timeout, sizeof(timeout));
  fcntl(sockfd, F_SETFL, O_NONBLOCK); 
  // if (setsock < 0) {
  //   printf("Error setting timeout!\n");
  //   return;
  // }

  // base is the last unacknowledged packet number
  unsigned int base = 0;
  unsigned int next_seq_num = 0;

  // maintain a linked list for the buffer
  // we discard the head after a successful ack
  struct Node* head = NULL;
  struct Node* prev_node = NULL;

  clock_t end_t;
  clock_t start_t = clock();
  // clock_t debug_start_t = start_t;
  size_t read_size = 1;
  unsigned int n = N;
  char missed_ack = 0;

  for (;;) {
    if (next_seq_num < base + n && read_size > 0) {
      struct packet* snd_pkt;
      snd_pkt = (struct packet*) malloc(sizeof(struct packet));

      // create a new node
      struct Node* node;
      node = (struct Node*) malloc(sizeof(struct Node));
      node->next = NULL;
      node->pkt = snd_pkt;

      if (prev_node == NULL)
        prev_node = node;
      else {
        prev_node->next = node;
        prev_node = node;
      }

      if (head == NULL)
        head = node;

      if (base == next_seq_num) start_t = clock(); // start the timer

      snd_pkt->header = next_seq_num;
      read_size = fread(snd_pkt->payload, 1, DEFAULT_MSG_SIZE, fp);
      // printf("Read size %li\n", read_size);
      snd_pkt->size = read_size;
      sendto(sockfd, snd_pkt, sizeof(struct packet), 0, next_res->ai_addr, next_res->ai_addrlen);
      next_seq_num++;
      // printf("Next Seq No: %i | Base: %i | n: %i\n", next_seq_num, base, n);

    }
    struct packet* rcv_pkt;
    rcv_pkt = (struct packet*) malloc(sizeof(struct packet));
    int rcv = recvfrom(sockfd, rcv_pkt, sizeof(struct packet), 0, next_res->ai_addr, &(next_res->ai_addrlen));

    if (rcv != FAILED) {
      // received the correct header
      if (rcv_pkt->header == base) {
        // printf("Base %i pkt received\n", base);
        struct Node* temp = head;
        head = head->next;
        free(temp->pkt);
        free(temp);
        if (++base == next_seq_num && read_size == 0)
          break;
        // n++;
        continue;
      }
      // some other packet is received!
      missed_ack += 1;
    }

    // check the timer as recveing failed
    end_t = clock();
    clock_t duration = (end_t - start_t);
    double tot_t = (double) duration / CLOCKS_PER_SEC;
    // printf("Total Time: %f\n", tot_t);

    // resend the packets
    if (tot_t >= TIMEOUT || missed_ack == 3) {
      missed_ack = 0;
      start_t = clock();

      // n /= 2;
      // if (n < 4) n = 4;
      
      struct Node* temp = head;
      while (temp != NULL) {
        // printf("Resending Packet: %i\n", temp->pkt->header);
        sendto(sockfd, temp->pkt, sizeof(struct packet), 0, next_res->ai_addr, next_res->ai_addrlen);
        temp = temp->next;
      }
    }
  }

  // send the closing packet
  start_t = clock();
  for (;;) {
    struct packet* snd_pkt;
    struct packet* rcv_pkt;
    rcv_pkt = (struct packet*) malloc(sizeof(struct packet));
    snd_pkt = (struct packet*) malloc(sizeof(struct packet));
    snd_pkt->size = -1;
    sendto(sockfd, snd_pkt, sizeof(struct packet), 0, next_res->ai_addr, next_res->ai_addrlen);
    unsigned int rcv = recvfrom(sockfd, rcv_pkt, sizeof(struct packet), 0, next_res->ai_addr, &(next_res->ai_addrlen));

    if (rcv == FAILED || rcv_pkt->size != -1) {
      end_t = clock();
      clock_t duration = end_t - start_t;
      double tot_t = (double) duration / CLOCKS_PER_SEC;
      if (tot_t < 0.0025) continue;
    }
    break;
  }

  // debug code
  // clock_t debug_end_t = clock();
  // rewind(fp);
  // fseek(fp, 0L, SEEK_END);
  // double sz = (double) ftell(fp) / 1024;
  // printf("Debug start: %f\n", (double) debug_start_t / CLOCKS_PER_SEC);
  // printf("Debug end: %f\n", (double) debug_end_t / CLOCKS_PER_SEC);
  // clock_t d = debug_end_t - debug_start_t;
  // double tot_t = (double) d / CLOCKS_PER_SEC;
  // printf("Time taken: %f\n", tot_t);
  // printf("File Size: %f\n", sz);
  // printf("Speed (Kbps): %f\n", sz/tot_t);

  close(sockfd);

  return;
}