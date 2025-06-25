#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include "main.h"

#define BUF_SIZE 4096

// ofc this would all be in a struct or something in a perfect OO world
int net_fd = -1;
static char buf[BUF_SIZE];
static int buf_ix = 0;
static int buf_len = 0;

char initSocket(const char *srvAddr, const char* port){
	struct addrinfo addrhints = {
		.ai_flags =
			// Use mapped addresses if the stars align
			AI_V4MAPPED |
			// Require numeric port numbers - don't accept service names
			AI_NUMERICSERV |
			// We plan to connect to the result - only return results for stuff that we can support (like no v6 addresses on v4-only hosts)
			AI_ADDRCONFIG,
		.ai_family = AF_UNSPEC,
		.ai_socktype = SOCK_STREAM,
		.ai_protocol = 0,
		// Beyond this point are unused
		.ai_addrlen = 0,
		.ai_addr = NULL,
		.ai_canonname = 0,
		.ai_next = NULL
	};
	struct addrinfo* addrinfo = NULL;
	int addrinfores = getaddrinfo(srvAddr, port, &addrhints, &addrinfo);
	if(addrinfores != 0 || addrinfo == NULL){
		const char* msg = "Unknown Error";
		switch(addrinfores){
			case EAI_AGAIN:
				msg = "Temporary name resolution failure";
				break;
			case EAI_FAIL:
				msg = "Permanent name resolution failure";
				break;
			case EAI_FAMILY:
				msg = "IPv4/6 not supported";
				break;
			case EAI_NODATA:
				msg = "Remote host has no addresses";
				break;
			case EAI_NONAME:
				msg = "Unknown remote host or service";
				break;
		}
		printf("`getaddrinfo` failed: %s (%d)\n", msg, addrinfores);
		printf("(for host \"%s\" + port \"%s\")\n", srvAddr, port);
		return 1;
	}
	net_fd = socket(addrinfo->ai_addr->sa_family, SOCK_STREAM, 0);
	if(net_fd < 0){
		printf("Failed to create socket: '%s'\n", strerror(errno));
		return 1;
	}
	char hostbuf[64] = "UNKNOWN HOST";
	char portbuf[32] = "UNKNOWN PORT";
	// Don't perform name resolution - just tell us what numeric address/port we're connecting to.
	if(0 != getnameinfo(addrinfo->ai_addr, addrinfo->ai_addrlen, hostbuf, 64, portbuf, 32, NI_NUMERICHOST|NI_NUMERICSERV)){
		puts("Failed to get address name");
	}
	if(addrinfo->ai_addr->sa_family == AF_INET){
		printf("Connecting to server at %s:%s\n", hostbuf, portbuf);
	}else{
		printf("Connecting to server at [%s]:%s\n", hostbuf, portbuf);
	}
	if(connect(net_fd, addrinfo->ai_addr, addrinfo->ai_addrlen)){
		printf("Failed to connect to server: '%s'\n", strerror(errno));
		return 1;
	}
	freeaddrinfo(addrinfo);
	// Set TCP_NODELAY for more real-time TCP, we're not terribly concerned with network congestion.
	int flag = 1;
	if(-1 == setsockopt(net_fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(int))){
		printf("Failed to set TCP_NODELAY: '%s'\n", strerror(errno));
	}
	return 0;
}

void closeSocket() {
	if (net_fd == -1) return;
	if (close(net_fd)) {
		printf("Error closing socket: %d\n", errno);
	}
	net_fd = -1;
}

char readData(void *dst_arg, int len) {
	char *dst = (char*)dst_arg;
	while (buf_ix + len > buf_len) {
		int available = buf_len - buf_ix;
		memcpy(dst, buf + buf_ix, available);
		len -= available;
		dst += available;
		int ret = recv(net_fd, buf, BUF_SIZE, 0);
		if (ret == 0) {
			if (globalRunning) puts("Remote host closed connection.");
			return 1;
		}
		if (ret < 0) {
			if (globalRunning) {
				printf(
					"Error encountered while reading from socket, errno is %d\n"
					"\t(hint: `errno` is a command that can help!)\n",
					errno
				);
			}
			return 1;
		}
		buf_len = ret;
		buf_ix = 0;
	}

	memcpy(dst, buf + buf_ix, len);
	buf_ix += len;
	return 0;
}

char sendData(char *src, int len) {
	while (len) {
		int ret = write(net_fd, src, len);
		if (ret < 0) {
			if (globalRunning) {
				printf("write() to socket failed, errno is %d\n", errno);
			}
			return 1;
		}
		src += ret;
		len -= ret;
	}
	return 0;
}
