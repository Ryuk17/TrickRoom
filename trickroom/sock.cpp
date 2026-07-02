#include "sock.h"

struct sockaddr_in	servaddr;

int socket_bind(int &sockfd, char *address, int port)
{
	int sndbuf;
	int rcvbuf;
	socklen_t arglen = sizeof(int);

	if (sockfd != -1)
		return 0;

	sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sockfd == -1)
	{
		printf("socket error");
		return -1;
	}

	getsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, (char *)&sndbuf, &arglen);
	printf("SO_SNDBUF = %d\n", sndbuf);

	getsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, (char *)&rcvbuf, &arglen);
	printf("SO_RCVBUF = %d\n", rcvbuf);

	//	if (sndbuf < 8192)
	{
		sndbuf = 65507; //default 8192
		setsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, (char *)&sndbuf, sizeof(sndbuf));
		printf("Setting SO_SNDBUF to %d\n", sndbuf);
		getsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, (char *)&sndbuf, &arglen);
		printf("SO_SNDBUF = %d\n", sndbuf);
	}

	//	if (rcvbuf < 8192)
	{
		rcvbuf = 65507; //default 8192
		setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, (char *)&rcvbuf, sizeof(rcvbuf));
		printf("Setting SO_RCVBUF to %d\n", rcvbuf);
		getsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, (char *)&rcvbuf, &arglen);
		printf("SO_RCVBUF = %d\n", rcvbuf);
	}


#ifdef _WINDOWS_
	unsigned long nonblock = 1;
	ioctlsocket(sockfd, FIONBIO, &nonblock);
#else
	fcntl(sockfd, F_SETFL, O_NONBLOCK);
#endif

	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(port);
	if (address != NULL)
		servaddr.sin_addr.s_addr = inet_addr(address);
	else
		servaddr.sin_addr.s_addr = htonl(INADDR_ANY);

	if ((::bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr))) == -1)
	{
		printf("bind error: port in use\r\n");
		return -1;
	}

	printf("bound on: %s:%d\n", inet_ntoa(servaddr.sin_addr), htons(servaddr.sin_port));
	return 0;
}

int socket_send(int sockfd, char *buffer, int size)
{
	return ::send(sockfd, buffer, size, 0);
}

int socket_recv(int sockfd, char *buff, int size)
{
	return ::recv(sockfd, buff, size, 0);
}

int socket_recv(int sockfd, char *buff, int size, int delay)
{
	int ret;
	struct timeval	timeout = { 0, 0 };
	fd_set		readfds;

	timeout.tv_sec = delay;

	FD_ZERO(&readfds);
	FD_SET(sockfd, &readfds);
	ret = select(sockfd + 1, &readfds, NULL, NULL, &timeout);

	if (ret == -1 || ret == 0)
		return 0;

	if (FD_ISSET(sockfd, &readfds))
		return ::recv(sockfd, buff, size, 0);
	else
		return 0;
}

int socket_recvfrom(int sockfd, char *buff, int size, char *from, int port, int length)
{
	sockaddr_in		addr;
	socklen_t		sock_length = sizeof(sockaddr_in);
	int ret;
	int n = 0;
	struct timeval	timeout = { 0, 0 };
	fd_set		readfds;

	FD_ZERO(&readfds);
	FD_SET(sockfd, &readfds);
	ret = select(sockfd + 1, &readfds, NULL, NULL, &timeout);

	if (ret == -1 || ret == 0)
	{
		return 0;
	}


	if (FD_ISSET(sockfd, &readfds))
	{
		n = recvfrom(sockfd, buff, size, 0, (sockaddr *)&addr, &sock_length);

		if (n != -1)
		{
			snprintf(from, length, "%s:%d", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
			return n;
		}
		else if (n == -1)
		{
			int err = WSAGetLastError();

			if (err != WSAEWOULDBLOCK)
			{
				printf("recv returned -1 error %d\r\n", err);
				return -1;
			}
		}

	}
	return 0;
}

int socket_sendto(int sockfd, char *buff, int size, const char *to, int port)
{
	struct sockaddr_in	addr;
	socklen_t		sock_length = sizeof(sockaddr_in);

	socket_strtoaddr(to, port, addr);
	return sendto(sockfd, buff, size, 0, (sockaddr *)&addr, sock_length);
}


int socket_strtoaddr(const char *str, int port, sockaddr_in &addr)
{
	char ip[32] = { 0 };
	int a, b, c, d;

	memset(&addr, 0, sizeof(sockaddr_in));
	addr.sin_family = AF_INET;
	if (sscanf(str, "%d.%d.%d.%d:%d", &a, &b, &c, &d, &port) == 5)
	{
		sprintf(ip, "%d.%d.%d.%d", a, b, c, d);
		addr.sin_addr.s_addr = inet_addr(ip);
	}
	else if (sscanf(str, "%31s:%d", ip, &port) == 2)
	{
		addr.sin_addr.s_addr = inet_addr(ip);
	}
	else if (sscanf(str, "%d.%d.%d.%d", &a, &b, &c, &d) == 4)
	{
		sprintf(ip, "%d.%d.%d.%d", a, b, c, d);
		addr.sin_addr.s_addr = inet_addr(ip);
	}
	else
	{
		printf("strtoaddr() invalid address %s", str);
		return -1;
	}
	addr.sin_port = htons(port);
	return 0;
}

char *inet_ntop4(const u_char *src, char *dst, socklen_t size)
{
	static const char fmt[] = "%u.%u.%u.%u";
	char tmp[sizeof "255.255.255.255"];
	int l;

	l = snprintf(tmp, sizeof(tmp), fmt, src[0], src[1], src[2], src[3]);
	if (l <= 0 || (socklen_t)l >= size) {
		return (NULL);
	}
	strncpy(dst, tmp, size);
	return (dst);
}

char *inet_ntop(int af, const void *src, char *dst, socklen_t size)
{
	switch (af) {
	case AF_INET:
		return (inet_ntop4((u_char *)src, (char *)dst, size));
	default:
		return (NULL);
	}
	/* NOTREACHED */
}

int socket_connect(int &sockfd, const char *server, int port)
{
	int sndbuf;
	int rcvbuf;
	int ret = 0;
	socklen_t arglen = sizeof(int);

	if ((sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
	{
		printf("socket error");
		return -1;
	}

	getsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, (char *)&sndbuf, &arglen);
	printf("SO_SNDBUF = %d\n", sndbuf);

	getsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, (char *)&rcvbuf, &arglen);
	printf("SO_RCVBUF = %d\n", rcvbuf);

	if (sndbuf < 65507)
	{
		sndbuf = 65507;  //default 8192
		setsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, (char *)&sndbuf, sizeof(sndbuf));
		printf("Setting SO_SNDBUF to %d\n", sndbuf);
		getsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, (char *)&sndbuf, &arglen);
		printf("SO_SNDBUF = %d\n", sndbuf);
	}

	if (rcvbuf < 65507)
	{
		rcvbuf = 65507; //default 8192
		setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, (char *)&rcvbuf, sizeof(rcvbuf));
		printf("Setting SO_RCVBUF to %d\n", rcvbuf);
		getsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, (char *)&rcvbuf, &arglen);
		printf("SO_RCVBUF = %d\n", rcvbuf);
	}

#ifdef _WINDOWS_
	unsigned long nonblock = 1;
	ioctlsocket(sockfd, FIONBIO, &nonblock);
#else
	fcntl(sockfd, F_SETFL, O_NONBLOCK);
#endif

	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(port);
	servaddr.sin_addr.s_addr = inet_addr(server);
	ret = inet_pton(AF_INET, server, (in_addr *)&servaddr.sin_addr);

	if (ret == 0)
	{
		printf("inet_pton invalid server");
		return -1;
	}
	else if (ret == -1)
	{
		printf("inet_pton error");
		return -1;
	}

	if (::connect(sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr)) == -1)
	{
		printf("connect error");
		return -1;
	}
	return 0;
}

void socket_closesock(int sockfd)
{
	closesocket(sockfd);
}


int inet_pton(int af, const char *server, void *vaddr)
{
	in_addr *addr = (in_addr *)vaddr;
	struct hostent *host = gethostbyname(server);
	if (host)
		*addr = *((struct in_addr *)*host->h_addr_list);
	return 1;
}
