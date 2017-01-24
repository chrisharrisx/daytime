#include <arpa/inet.h>
#include <ctype.h>
#include <netdb.h>
#include <netinet/in.h>
#include <time.h>
#include <strings.h>
#include <stdio.h>
#include <stdlib.h>
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
 
int main(int argc, char **argv) {
  socklen_t addr_size = sizeof(struct sockaddr);
  char buff[MAXLINE];
  char *hostname = malloc(HOSTMAX);
  char *ip_address = malloc(ADDRESS_SIZE);
  int listenfd, connfd;
  char service_type[SERVICEMAX];
  struct sockaddr_in servaddr;
  struct sockaddr_in clientaddr;
  time_t ticks;

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
    connfd = accept(listenfd, (struct sockaddr *) &clientaddr, &addr_size);
    ticks = time(NULL);

    if (inet_ntop(AF_INET, &clientaddr.sin_addr, ip_address, ADDRESS_SIZE) <= 0) {
      printf("inet_pton error for %s\n", ip_address);
      exit(1);
    }

    getnameinfo((struct sockaddr *) &clientaddr, sizeof(clientaddr), hostname, HOSTMAX, service_type, SERVICEMAX, 0);
   
    snprintf(buff, sizeof(buff), "%.24s\r\n", ctime(&ticks));
    
    write(connfd, buff, strlen(buff));
    printf("Sending response to %s %s: %s", hostname, ip_address, buff);
    close(connfd);
  }
}
