#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <winsock.h>

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h> 

#include "types.h"

extern struct sockaddr_in	servaddr;


int socket_bind(int &sockfd, char *address, int port);
int socket_send(int sockfd, char *buffer, int size);
int socket_recv(int sockfd, char *buff, int size);
int socket_recv(int sockfd, char *buff, int size, int delay);
int socket_recvfrom(int sockfd, char *buff, int size, char *from, int port, int length);
int socket_sendto(int sockfd, char *buff, int size, const char *to, int port);
int socket_strtoaddr(const char *str, int port, sockaddr_in &addr);
int socket_connect(int &sockfd, const char *server, int port);
void socket_closesock(int sockfd);

char *inet_ntop4(const u_char *src, char *dst, socklen_t size);
char *inet_ntop(int af, const void *src, char *dst, socklen_t size);
int inet_pton(int af, const char *server, void *vaddr);