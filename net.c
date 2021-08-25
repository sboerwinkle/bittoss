#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>

#define BUF_SIZE 4096

// ofc this would all be in a struct or something in a perfect OO world
static int sockfd = -1;
static char buf[BUF_SIZE];
static int buf_ix = 0;
static int buf_len = 0;

char initSocket(char *srvAddr, int port) {
	// This function lifted from Martin Boerwinkle's Ring Game
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if(sockfd < 0){
		puts("Failed to create socket");
		return 1;
	}

	struct sockaddr_in server;
       	server.sin_family = AF_INET;
	server.sin_addr.s_addr = inet_addr(srvAddr);
	server.sin_port=htons(port);

	printf("Connecting to server at %s\n", srvAddr);
	if(connect(sockfd, (struct sockaddr*)&server, sizeof(server))){
		puts("Failed to connect to server");
		return 1;
	}
	return 0;
}

void closeSocket() {
	if (sockfd == -1) return;
	if (close(sockfd)) {
		printf("Error closing socket: %d\n", errno);
	}
	sockfd = -1;
}

static void cp(char* dst, char* src, int len) {
	// Going to assume looping is faster than a syscall for the amounts of data we're dealing with here
	while (len--) *(dst++) = *(src++);
}

char readData(char *dst, int len) {
	while (buf_ix + len > buf_len) {
		int available = buf_len - buf_ix;
		cp(dst, buf + buf_ix, available);
		len -= available;
		dst += available;
		int ret = recv(sockfd, buf, BUF_SIZE, 0);
		if (ret == 0) {
			puts("Remote host closed connection.");
			return 1;
		}
		if (ret < 0) {
			printf("Error encountered while reading from socket, errno is %d\n\t(hint: `errno` is a command that can help!)\n", errno);
			return 1;
		}
		buf_len = ret;
		buf_ix = 0;
	}

	cp(dst, buf + buf_ix, len);
	buf_ix += len;
	return 0;
}

char sendData(char *src, int len) {
	while (len) {
		int ret = write(sockfd, src, len);
		if (ret < 0) {
			printf("Error encountered while writing to socket: %d\n", ret);
			return 1;
		}
		src += ret;
		len -= ret;
	}
	return 0;
}
