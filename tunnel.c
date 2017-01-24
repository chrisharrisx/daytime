#include <arpa/inet.h>
#include <ctype.h>
#include <netdb.h>
#include <netinet/in.h>
#include <time.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define ADDRESS_SIZE 128
#define HOSTMAX 255
#define IPMAX 128
#define LISTENQ 1024
#define MAXLINE 4096
#define SERVICEMAX 32

void send_request_and_respond(char *, int, int, char *, char *);
void get_ip_from_hostname(char *dest, char **ip_address);
 
int main(int argc, char **argv) {
  socklen_t addr_size = sizeof(struct sockaddr);
  char recvline[MAXLINE + 1];
  char *hostname = (char *) malloc(HOSTMAX);
  char *ip_address = (char *) malloc(ADDRESS_SIZE);
  int listenfd, connfd;
  char service_type[SERVICEMAX];
  struct sockaddr_in servaddr;
  struct sockaddr_in clientaddr;

  if (argc < 2) {
    printf("Usage: %s <port number>\n", argv[0]);
    exit(1);
  }

  for (int i = 0; argv[1][i] != '\0'; i++) {
    if (!isdigit(argv[1][i])) {
      puts("Port number must be an integer");
    }
  }
  int port_num = atoi(argv[1]);

  if (port_num < 1024) {
    puts("Port number must be greater than 1024");
    exit(1);
  }

  listenfd = socket(AF_INET, SOCK_STREAM, 0);
  
  addr_size = ADDRESS_SIZE; 
  bzero(ip_address, ADDRESS_SIZE);
  bzero(&clientaddr, sizeof(struct sockaddr_in));
  bzero(&servaddr, sizeof(struct sockaddr_in));
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
  servaddr.sin_port = htons(port_num); /* daytime server */
  
  bind(listenfd, (struct sockaddr *) &servaddr, sizeof(servaddr));
  listen(listenfd, LISTENQ);
 
  for ( ; ; ) {
    // Listen for connections from client
    connfd = accept(listenfd, (struct sockaddr *) &clientaddr, &addr_size);
    char fwd_address[HOSTMAX];
    int fwd_port = -1;

    if (inet_ntop(AF_INET, &clientaddr.sin_addr, ip_address, ADDRESS_SIZE) <= 0) {
      printf("inet_pton error for %s\n", ip_address);
      exit(1);
    }
    getnameinfo((struct sockaddr *) &clientaddr, sizeof(clientaddr), hostname, HOSTMAX, service_type, SERVICEMAX, 0);

    // Client is now connected, wait to receive forwarding address
    int n;

    while ((n = read(connfd, recvline, MAXLINE)) > 0) {
      recvline[n] = 0; /* null terminate */
      
      if (sscanf(recvline, "%s %d", fwd_address, &fwd_port) < 2) {
        puts("Read error");
        exit(1);
      }

      printf("Received request from %s %s\n", hostname, ip_address);
      printf("Forwarding address: %s\nForwarding port: %d\n", fwd_address, fwd_port);
    }

    if (n < 0) {
      printf("read error\n");
      exit(1);
    }

    // Forwarding address has been received.
    // Contact server, wait for reply, then send response to client
    send_request_and_respond(fwd_address, fwd_port, connfd, hostname, ip_address);
  }
}

void send_request_and_respond(char *fwd_address, int fwd_port, int connfd, char *hostname, char *ip_address) {
  bool fwd_hostname_set = false;
  char *fwd_hostname = (char *) malloc(HOSTMAX);
  char *fwd_ip_address = (char *) malloc(IPMAX);
  char buff[MAXLINE];
  char response[MAXLINE];
  int fwd_sockfd;
  struct sockaddr_in fwd_servaddr;

  // set hostname and ip address for destination
  if (!isdigit(fwd_address[0])) { 
    fwd_hostname = fwd_address;
    fwd_hostname_set = true;

    // get ip address from hostname
    get_ip_from_hostname(fwd_address, &fwd_ip_address);
  }
  else {  // user specified an ip address and port
    fwd_ip_address = fwd_address;
  }

  // Create socket
  if ((fwd_sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    printf("socket error\n");
    exit(1);
  }

  bzero(&fwd_servaddr, sizeof(struct sockaddr_in));
  fwd_servaddr.sin_family = AF_INET;
  fwd_servaddr.sin_port = htons(fwd_port); /* daytime server */

  if (inet_pton(AF_INET, fwd_ip_address, &fwd_servaddr.sin_addr) <= 0) {
    printf("inet_pton error for %s\n", fwd_address);
    exit(1);
  }

  // Connect to socket
  if (connect(fwd_sockfd, (struct sockaddr *) &fwd_servaddr, sizeof(fwd_servaddr)) < 0) {
    printf("connect error\n");
    exit(1);
  }

  // If IP address specified as destination, lookup hostname
  if (!fwd_hostname_set) {
    char service_type[SERVICEMAX];
    getnameinfo((struct sockaddr *) &fwd_servaddr, sizeof(fwd_servaddr), fwd_hostname, HOSTMAX, service_type, SERVICEMAX, 0);
  }

  // Get response from server
  int n;

  while ((n = read(fwd_sockfd, response, MAXLINE)) > 0) {
    response[n] = 0; /* null terminate */
  }

  if (n < 0) {
    printf("read error\n");
    exit(1);
  }

  close(fwd_sockfd);

  // Send response to client
  int return_value;
  snprintf(buff, sizeof(buff), "%.26s\r\n", response);
  if ((return_value = write(connfd, buff, strlen(buff))) == -1) {
    perror("write() error");
  }
  else {
    printf("Sending response to %s %s: %s", hostname, ip_address, buff);
  }
  
  close(connfd);
}

void get_ip_from_hostname(char *dest, char **ip_address) {
  struct addrinfo info, *infoptr;
  memset(&info, 0, sizeof(info));

  int error = getaddrinfo(dest, NULL, &info, &infoptr);
  if (error) {
    fprintf(stderr, "getaddrinfo() error: %s\n", gai_strerror(error));
    exit(1);
  }

  struct addrinfo *ptr;
  char service[SERVICEMAX];

  for (ptr = infoptr; ptr != NULL; ptr = ptr->ai_next) {
    getnameinfo(ptr->ai_addr, ptr->ai_addrlen, *ip_address, IPMAX, service, SERVICEMAX, NI_NUMERICHOST);
  }

  freeaddrinfo(infoptr);
}
