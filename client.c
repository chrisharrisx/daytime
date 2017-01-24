#include <arpa/inet.h>
#include <ctype.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdbool.h>
#include <stdlib.h>
#include <strings.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define MAXLINE 4096
#define HOSTMAX 255
#define IPMAX 128
#define SERVICEMAX 32
#define DESTINATION 1
#define PORT 2
#define SEC_DESTINATION 3
#define SEC_PORT 4
#define LISTENQ 1024

void parse_args(int, char **, int, int, int *, char **, char **, bool *);
bool validate_portnumber(char **, int);
void get_ip_from_hostname(char *, char *);
void read_response(char *, char *, int, int, char [], bool, char *, char *);

int main(int argc, char **argv) {
	bool hostname_set = false;
	bool sec_hostname_set = false;
	char buff[MAXLINE];
	char recvline[MAXLINE];
	char *hostname = (char *) malloc(HOSTMAX);
	char *sec_hostname = (char *) malloc(HOSTMAX);
	char *ip_address = (char *) malloc(IPMAX);
	char *sec_ip_address = (char *) malloc(IPMAX);
	int sockfd;
	int port_num = -1;
	int sec_port_num = -1;
	struct sockaddr_in servaddr;

	if (argc != 3 && argc != 5) {
		printf("usage: client [<tunnel name/ip> <tunnel port>] <server name/ip> <server port>\n");
		exit(1);
	}
	parse_args(argc, argv, DESTINATION, PORT, &port_num, &ip_address, &hostname, &hostname_set);

	// Create socket
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		printf("socket error\n");
		exit(1);
	}

	bzero(&servaddr, sizeof(struct sockaddr_in));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(port_num); /* daytime server */

	// Copy IP address to servaddr struct
	if (inet_pton(AF_INET, ip_address, &servaddr.sin_addr) <= 0) {
		printf("inet_pton error for %s\n", argv[DESTINATION]);
		exit(1);
	}

	// Connect to socket
	if (connect(sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0) {
		printf("connect error\n");
		exit(1);
	}

	// If IP address specified as destination, lookup hostname
	if (!hostname_set) {
		char service_type[SERVICEMAX];
		getnameinfo((struct sockaddr *) &servaddr, sizeof(servaddr), hostname, HOSTMAX, service_type, SERVICEMAX, 0);
	}

	if (argc == 3) { // Connect directly to server and wait for reply
		read_response(hostname, ip_address, sockfd, port_num, recvline, false, NULL, NULL);
	}
	else { // Connect to tunnel, send server address and port, then wait for reply
		parse_args(argc, argv, SEC_DESTINATION, SEC_PORT, &sec_port_num, &sec_ip_address, &sec_hostname, &sec_hostname_set);

		if (!sec_hostname_set) {
			char service_type[SERVICEMAX];
			getnameinfo((struct sockaddr *) &servaddr, sizeof(servaddr), sec_hostname, HOSTMAX, service_type, SERVICEMAX, 0);
		}

		snprintf(buff, sizeof(buff), "%s %s\r\n", argv[SEC_DESTINATION], argv[SEC_PORT]);
		write(sockfd, buff, strlen(buff));

		shutdown(sockfd, SHUT_WR);

		read_response(hostname, ip_address, sockfd, port_num, recvline, true, sec_hostname, sec_ip_address);
	}

	exit(0);
}

void parse_args(int argc, char **argv, int destination, int destination_port, 
	int *port_num, char **ip_address, char **hostname, bool *hostname_set) {
	
	if (validate_portnumber(argv, destination_port)) {
		*port_num = atoi(argv[destination_port]);
	}
	else {
		exit(1);
	}

	// set hostname and ip address for destination
	if (!isdigit(argv[destination][0])) { 
		*hostname = argv[destination];
		*hostname_set = true;

		// get ip address from hostname
		get_ip_from_hostname(argv[destination], *ip_address);
	}
	else {	// user specified an ip address and port
		*ip_address = argv[destination];
	}
}

bool validate_portnumber(char **argv, int port_type) {
	for (int i = 0; argv[port_type][i] != '\0'; i++) {
    if (!isdigit(argv[port_type][i])) {
      puts("Port number must be an integer");
      return false;
    }
  }
  int port = atoi(argv[port_type]);

  if (port < 1024) {
    puts("Port number must be greater than 1024");
    return false;
  }

  return true;
}

void get_ip_from_hostname(char *dest, char *ip_address) {
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
		getnameinfo(ptr->ai_addr, ptr->ai_addrlen, ip_address, IPMAX, service, SERVICEMAX, NI_NUMERICHOST);
	}

	freeaddrinfo(infoptr);
}

void read_response(char *hostname, char *ip_address, int sockfd, int port_num, char recvline[], bool isTunnel,
	char *sec_host, char *sec_ip) {
	int n;

	while ((n = read(sockfd, recvline, MAXLINE)) > 0) {
		recvline[n] = 0; /* null terminate */
	}

	if (n < 0) {
		printf("read error\n");
		exit(1);
	}

	if (isTunnel) {
		printf("Server Name: %s\n", sec_host);
		printf("IP Address: %s\n", sec_ip);
		printf("Time: %s", recvline);

		printf("Via Tunnel: %s\n", hostname);
		printf("IP Address: %s\n", ip_address);
		printf("Port Number: %d\n", port_num);
	}
	else {
		printf("Server Name: %s\n", hostname);
		printf("IP Address: %s\n", ip_address);
		printf("Time: %s", recvline);
	}
}