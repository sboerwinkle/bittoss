// This is pretty simple, but I got the skeleton of it from ChatGPT
#include <poll.h>
#include <stdio.h>
#include <unistd.h>

#include "list.h"
#include "net.h"
#include "net2.h"
#include "watch.h"
#include "main.h"

#define NUMFDS 2
static struct pollfd fds[NUMFDS];

void* mypoll_threadFunc(void *arg) {
	while (1) {
		// 200ms timeout.
		// I could probably also set an "ignore" signal handler
		// for SIGUSR and use it to wake up sleeping calls when
		// it's time to quit, but frames are faster than 200ms
		// anyway so it probably doesn't matter.
		int ret = poll(fds, NUMFDS, 200); // 200ms timeout
		if (!globalRunning) return NULL;

		if (ret == -1) {
			perror("mypoll");
			return NULL;
		}

		if (fds[0].revents & POLLIN) {
			if (net2_read()) {
				closeSocket();
				// This makes `poll` ignore this entry going forwards
				fds[0].fd = -1;
			}
		}

		if (fds[1].revents & POLLIN) {
			watch_read();
		}
	}
}

void mypoll_init() {
	fds[0].fd = net_fd;
	fds[0].events = POLLIN;

	fds[1].fd = watch_fd;
	fds[1].events = POLLIN;
}

void mypoll_destroy() {
	// no-op
}
