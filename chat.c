/*
 *  Here is the starting point for your netster part.1 definitions. Add the 
 *  appropriate comment header as defined in the code formatting guidelines
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <pthread.h>

#define BACKLOG 5 // connections in the queue
#define DEFAULT_MSG_SIZE 256 // default message size
#define THREAD_COUNT 2

int tcp_server(char*, long);
int udp_server(char*, long);

int tcp_client(char*, long);
int udp_client(char*, long);

// these functions run inside a thread
void* tcp_chat(void*);
//int udp_chat(void*)

/* Add function definitions */
void chat_server(char* iface, long port, int use_udp) {
  if (use_udp)
    udp_server(iface, port);
  else
    tcp_server(iface, port);
    //printf("%s\n", iface);
}

int tcp_server(char* iface, long port) {
	// create a tcp socket file descriptor with ipv4 address
	// domain -- AF_INET is for ipv4 address
	// type -- SOCK_STREAM is for tcp connection
	// protocol is zero
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

  int connections = 0;

  for (;;) {
    // create threads here
    pthread_t t[THREAD_COUNT];
    
    // infinite loop for incoming connections
    for (int t_index = 0; t_index < THREAD_COUNT; t_index++) {
      // accept the client connections that are in the queue
      struct sockaddr_in client_addr;
      // as accept takes the socklen_t*
      socklen_t client_addr_size = sizeof(client_addr);

      // new socket file descriptor for the incoming connection
      int new_sockfd = accept(sockfd, (struct sockaddr*) &client_addr, &client_addr_size);
      connections++;
      if (new_sockfd < 0) {
        printf("error while receiving client connection!\n");
        continue;
      }
      
      // print client ip
      char client_ip[INET_ADDRSTRLEN];
      inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
      printf("connection %i from ('%s', %ld)\n", connections, client_ip, port);

      // create thread here
      int socks[2] = {new_sockfd, sockfd};
      pthread_create(t + t_index, NULL, tcp_chat, socks);
    }

    // for ever loop 
    for (;;) {
      // a global variable
      // detach and create new thread
    }

    for (int i = 0; i < THREAD_COUNT; i++) {
        pthread_join(t[i], NULL);
        printf("Thread %d complete!\n", i);
    }
  }

  return 0; 
}

void* tcp_chat(void *arg) {

  int new_sockfd = *(int*)arg;
  int sockfd = *(((int *)arg) + 1);

  // chat loop
  for (;;) {
    // communicate with the client here
    // message from client
    char message[DEFAULT_MSG_SIZE];
    memset(message, '\0', DEFAULT_MSG_SIZE);

    int received = recv(new_sockfd, message, DEFAULT_MSG_SIZE, 0);
    if (received < 0) {
      printf("Error while receiving message from the client!\n");
      return NULL;
    }

    if (strncmp(message, "hello\n", 6) == 0) {
      char* response = "world\n";
      int sent = send(new_sockfd, response, strlen(response), 0);
      if (sent < 0)
        printf("Error while sending message to the client!\n");
      continue;
    }
    else if (strncmp(message, "goodbye\n", 8) == 0) {
      // respond with farewell message
      char* response = "farewell\n";
      int sent = send(new_sockfd, response, strlen(response), 0);
      if (sent < 0)
        printf("Error while sending message to the client!\n");
      // close the connection here
      close(new_sockfd);
      break;
    }
    else if (strncmp(message, "exit\n", 5) == 0) {
      char* response = "ok\n";
      int sent = send(new_sockfd, response, strlen(response), 0);
      if (sent < 0)
        printf("Error while sending message to the client!\n");
      // close all the connections and the server here
      close(new_sockfd);
      close(sockfd);
      exit(0);
    }
    else {
      // echo the message back
      int sent = send(new_sockfd, message, strlen(message), 0);
      if (sent < 0)
        printf("Error while sending message to the client!\n");
      continue;
    }
  }

  return NULL;
}

int udp_server(char* iface, long port) {
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

  // pthread_t t[THREAD_COUNT];

  // // create two threads for accepting
  // for (int t_index = 0; t_index < THREAD_COUNT; t_index++) {
  //   pthread_create(t + t_index, tcp_chat, socks);
  // }

  // infinite loop for incoming connections
  for (;;) {
    char message[DEFAULT_MSG_SIZE];
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    memset(&client_addr, 0, sizeof(client_addr));
    memset(message, '\0', sizeof(message));
    
    // read the first message
    int received = recvfrom(sockfd, message, DEFAULT_MSG_SIZE, 0, (struct sockaddr *) &client_addr, &client_addr_len);
    if (received < 0) {
      printf("Error while receiving message from the client!\n");
      continue;
    }

    if (strncmp(message, "hello\n", 6) == 0) {
      char* response = "world\n";
      int sent = sendto(sockfd, response, strlen(response), 0, (struct sockaddr *) &client_addr, client_addr_len);
      if (sent < 0)
        printf("Error while sending message to the client!\n");
      continue;
    }
    else if (strncmp(message, "goodbye\n", 8) == 0) {
      // respond with farewell message
      char* response = "farewell\n";
      int sent = sendto(sockfd, response, strlen(response), 0, (struct sockaddr *) &client_addr, client_addr_len);
      if (sent < 0)
        printf("Error while sending message to the client!\n");
      break;
    }
    else if (strncmp(message, "exit\n", 5) == 0) {
      char* response = "ok\n";
      int sent = sendto(sockfd, response, strlen(response), 0, (struct sockaddr *) &client_addr, client_addr_len);
      if (sent < 0)
        printf("Error while sending message to the client!\n");
      break;
    }
    else {
      // echo the message back
      int sent = sendto(sockfd, message, strlen(message), 0, (struct sockaddr *) &client_addr, client_addr_len);
      if (sent < 0)
        printf("Error while sending message to the client!\n");
      continue;
    }
  }

  return 0; 
}

int udp_chat(int sockfd) {
  return 0;
}


void chat_client(char* host, long port, int use_udp) { 
  
  if (use_udp)
    udp_client(host, port);
  else
    tcp_client(host, port);

}


int tcp_client(char* host, long port) {
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

  // chat with server here
  for (;;) {
    char message[DEFAULT_MSG_SIZE];
    memset(message, '\0', DEFAULT_MSG_SIZE);

    int index = 0;
    while ((message[index++] = getchar()) != '\n');
    // send message to the server
    // use 'strlen' instead of 'sizeof' as message can be of any length
    int sent = send(sockfd, message, strlen(message), 0);
    if (sent < 0) {
      printf("Error while sending message to the server!\n");
      return -1;
    }

    // get message from the server
    char server_message[DEFAULT_MSG_SIZE];
    memset(server_message, '\0', DEFAULT_MSG_SIZE);

    int reveived = recv(sockfd, server_message, DEFAULT_MSG_SIZE, 0);
    if (reveived < 0) {
      printf("Error while receiving message to the server!\n");
      // close and exit
      close(sockfd);
      return -1;
    }

    printf("%s", server_message);

    if ((strcmp(message, "ok\n") != 0 && strcmp(message, "farewell\n") != 0) && 
        (strcmp(server_message, "ok\n") == 0 || strcmp(server_message, "farewell\n") == 0)) {
      close(sockfd);
      // success
      return 0;
    }
  }

  return 0;
}


int udp_client(char* host, long port) {
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

  // chat with server here
  for (;;) {
    char message[DEFAULT_MSG_SIZE];
    memset(message, '\0', DEFAULT_MSG_SIZE);

    int index = 0;
    while ((message[index++] = getchar()) != '\n');
    // send message to the server
    // use 'strlen' instead of 'sizeof' as message can be of any length
    int sent = sendto(sockfd, message, strlen(message), 0, res->ai_addr, res->ai_addrlen);
    if (sent < 0) {
      printf("Error while sending message to the server!\n");
      return -1;
    }

    // get message from the server
    char server_message[DEFAULT_MSG_SIZE];
    memset(server_message, '\0', DEFAULT_MSG_SIZE);

    int reveived = recvfrom(sockfd, server_message, DEFAULT_MSG_SIZE, 0, res->ai_addr, &res->ai_addrlen);
    if (reveived < 0) {
      printf("Error while receiving message to the server!\n");
      // close and exit
      close(sockfd);
      return -1;
    }

    printf("%s", server_message);

    if ((strcmp(message, "ok\n") != 0 && strcmp(message, "farewell\n") != 0) && 
        (strcmp(server_message, "ok\n") == 0 || strcmp(server_message, "farewell\n") == 0)) {
      close(sockfd);
      // success
      return 0;
    }
  }

  return 0;
}
