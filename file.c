#include "file.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>

#define BACKLOG 5 // connections in the queue
#define DEFAULT_MSG_SIZE 256 // default message size

/*
 *  Here is the starting point for your netster part.2 definitions. Add the 
 *  appropriate comment header as defined in the code formatting guidelines
 */

/* Add function definitions */
int tcp_file_server(char* iface, long port, FILE* fp);
int udp_file_server(char* iface, long port, FILE* fp);

int tcp_file_client(char* host, long port, FILE* fp);
int udp_file_client(char* host, long port, FILE* fp);

void file_server(char* iface, long port, int use_udp, FILE* fp) {
  if (use_udp)
    udp_file_server(iface, port, fp);
  else
    tcp_file_server(iface, port, fp);
}

int tcp_file_server(char* iface, long port, FILE* fp) {
  int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	// socket creation failed
	if (sockfd < 0) {
		printf("Error while creating a socket!\n");
		return -1;
	}
	
	struct sockaddr_in server_addr = {
		.sin_family = AF_INET,
    .sin_port = htons(port),
		.sin_addr.s_addr = inet_addr(iface)
  };

  // bind the socket to a port
  // socaddr_in -- ipv4 is cast to sockaddr
  int binding = bind(sockfd, (struct sockaddr*) &server_addr, sizeof(server_addr)); 
  if (binding < 0) {
    printf("Error while binding the port!\n");
    return -1;
  }

  // listen for connections
  int listening = listen(sockfd, BACKLOG);
  if (listening < 0) {
    printf("error while listening!\n");
    return -1;
  }

  struct sockaddr_in client_addr;
  // as accept takes the socklen_t*
  socklen_t client_addr_size = sizeof(client_addr);

  // new socket file descriptor for the incoming connection
  int new_sockfd = accept(sockfd, (struct sockaddr*) &client_addr, &client_addr_size);
  if (new_sockfd < 0) {
    printf("error while receiving client connection!\n");
  }

  char buffer[DEFAULT_MSG_SIZE];
  memset(buffer, '\0', sizeof(buffer));

  for (;;) {
    int received = recv(new_sockfd, buffer, sizeof(buffer), 0);
    if (received <= 0)
      break;
    fwrite(buffer, sizeof(char), strlen(buffer), fp);
    memset(buffer, '\0', sizeof(buffer));
  }
  
  close(new_sockfd);
  close(sockfd);

  return 0;
}

int udp_file_server(char* iface, long port, FILE* fp) {
  int sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	// socket creation failed
	if (sockfd < 0) {
		printf("Error while creating a socket!\n");
		return -1;
	}
	
	struct sockaddr_in server_addr = {
		.sin_family = AF_INET,
    .sin_port = htons(port),
		.sin_addr.s_addr = inet_addr(iface)
  };

  // bind the socket to a port
  // socaddr_in -- ipv4 is cast to sockaddr
  int binding = bind(sockfd, (struct sockaddr*) &server_addr, sizeof(server_addr)); 
  if (binding < 0) {
    printf("Error while binding the port!\n");
    return -1;
  }

  char buffer[DEFAULT_MSG_SIZE];
  struct sockaddr_in client_addr;
  socklen_t client_addr_len = sizeof(client_addr);

  memset(&client_addr, 0, sizeof(client_addr));
  memset(buffer, '\0', sizeof(buffer));

  for (;;) {
    int received = recvfrom(sockfd, buffer, DEFAULT_MSG_SIZE, 0, (struct sockaddr *) &client_addr, &client_addr_len);
    if (received <= 0)
      break;
    fwrite(buffer, sizeof(char), strlen(buffer), fp);
    memset(buffer, '\0', sizeof(buffer));
  }
  
  close(sockfd);
  return 0;
}

void file_client(char* host, long port, int use_udp, FILE* fp) {
  if (use_udp)
    udp_file_client(host, port, fp);
  else
    tcp_file_client(host, port, fp);
}

int tcp_file_client(char* host, long port, FILE* fp) {
  int sockfd;
  struct addrinfo* res;

  // using spic c99's designated initializers
  // avoiding memsets
  struct addrinfo hints = {
    .ai_family = AF_INET,
    .ai_socktype = SOCK_STREAM,
    .ai_flags = AI_PASSIVE
  };

  // convert port number to string
  char str_port[8];
  sprintf(str_port, "%ld", port);

  // getaddrinfo to get linked list of results 
  getaddrinfo(host, str_port, &hints, &res);

  // create a socket file descriptor for the connection
  sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);

  if (sockfd < 0) {
    printf("Error while creating client socket!\n");
    return -1;
  }

  // connect to the server -- here they are on the same host
  // connect takes addr of server which is in res->ai_addr
  // we can use res->ai_addrlen helper for size
  int connection = connect(sockfd, res->ai_addr, res->ai_addrlen);
  if (connection < 0) {
    printf("Error while connecting to the host!\n");
    return -1;
  }

  char buffer[DEFAULT_MSG_SIZE];
  memset(buffer, '\0', sizeof(buffer));

  while (fgets(buffer, DEFAULT_MSG_SIZE, fp) != NULL) {
    int sent = send(sockfd, buffer, sizeof(buffer), 0);
    if (sent < 0) {
      printf("Error sending file!\n");
      return -1;
    }
    memset(buffer, '\0', sizeof(buffer));
  }

  send(sockfd, '\0', 0, 0);
  close(sockfd);

  return 0;
}

int udp_file_client(char* host, long port, FILE* fp) {
  int sockfd;
  struct addrinfo* res;

  // using spic c99's designated initializers
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
  getaddrinfo(host, str_port, &hints, &res);

  // create a socket file descriptor for the connection
  sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
  if (sockfd < 0) {
    printf("Error while creating client socket!\n");
    return -1;
  }

  char buffer[DEFAULT_MSG_SIZE];
  memset(buffer, '\0', DEFAULT_MSG_SIZE);

  while (fgets(buffer, DEFAULT_MSG_SIZE, fp) != NULL) {
    int sent = sendto(sockfd, buffer, sizeof(buffer), 0, res->ai_addr, res->ai_addrlen);
    if (sent < 0) {
      printf("Error sending file!\n");
      return -1;
    }
    memset(buffer, '\0', sizeof(buffer));
  }

  sendto(sockfd, '\0', 0, 0, res->ai_addr, res->ai_addrlen);

  return 0;
}
