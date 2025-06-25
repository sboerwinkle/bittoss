// This is "net2", which is an abstraction over "net".
// I tried to think of a more descriptive name, but at the end of the day it's still networking stuff.

#include <pthread.h>
#include <arpa/inet.h>

#include "list.h"
#include "queue.h"
#include "main.h"
#include "net.h"

#include "net2.h"

pthread_mutex_t netMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t netCond = PTHREAD_COND_INITIALIZER;

// This is the stuff we're going to mutex lock
// ===========================================

// For each upcoming frame, for each player, some data
//            queue <             list <     list<char>>>
queue<list<list<char>>> frameData;
int finalizedFrames;
char asleep = 0;

// ============= End mutex lock ==============

static int numPlayers, maxPlayers;
static int32_t expectedFrame;
static list<list<char>> availBuffers;
static const list<char> dummyBuffer = {.items = NULL, .num = 0, .max = 1};

struct message {
	int player;
	int frameOffset; // We do modulus math when writing this, so reading it is simpler later
	list<char> data;
};

static list<message> pendingMessages;

// A "normal" message should be under 50 bytes; at time of writing, it's about 16 for frame data, plus any command / chat.
#define MSG_SIZE_GUESS 50

static void reclaimBuffer(list<char> *buf) {
	// A "big" message, like a level load, is in the 10K range.
	// Todo: This is a `realloc`, which we could try to move outside of the mutex'd region
	//       if we really wanted. (`reclaimBuffer` is mostly called from the mutex'd region
	//       currently.)
	if (buf->max > 1'000) buf->setMax(MSG_SIZE_GUESS);
	buf->num = 0;
	availBuffers.add(*buf);
}

static void supplyBuffer(list<char> *buf) {
	if (availBuffers.num) {
		availBuffers.num--;
		*buf = availBuffers[availBuffers.num];
	} else {
		buf->init(MSG_SIZE_GUESS);
	}
}


/*
TODO:
Need to refactor this to be called once per "data to read" event.
Then, add `mypoll.cpp`, which handles dispatching to net2.cpp and watch.cpp.
(We're okay with it blocking if all the data isn't here yet, but maybe mention that)
We'll need to move some state that's outside the loop to be file-level state.
We'll also want timout and periodic `globalRunning` check in `mypoll.cpp`, since
we have a chance to do that properly now.

Finally, we probably want that same structure in retoss, as putting all the fd-reading stuff
on one thread feels nicer than spinning up a thread per fd.
(edit: it's not as clear-cut as that, but unless I'm doing a bunch of cacheable work (and maybe mess with CPU pinning),
 it seems like the overhead from threads and maybe caching the same region (?) makes polling worth it,
 even before I consider that it's easier to stop all fd-related operations at the end of the program this way.)
*/

char net2_read() {
	int32_t frame;
	if (readData(&frame, 4)) return 1;
	frame = ntohl(frame);
	if (frame != expectedFrame) {
		printf("Didn't get right frame value, expected %d but got %d\n", expectedFrame, frame);
		return 1;
	}
	expectedFrame = (expectedFrame + 1) % FRAME_ID_MAX;

	int mostAhead = 0;

	{
		unsigned char tmp;
		readData(&tmp, 1);
		numPlayers = tmp;
	}
	if (numPlayers != maxPlayers) {
		if (numPlayers > maxPlayers) {
			maxPlayers = numPlayers;
		} else {
			printf("Number of players dropped (%d -> %d). This is unexpected and could cause bad memory reads, so we're aborting now.\n", maxPlayers, numPlayers);
			// Would be tricky to make happen, but e.g. if we're looking at a buffer group for the first time this
			// frame and initialize it to a lower number of players, the game thread (some frames behind)
			// might still be reading it based on a higher number of players.
			return 1;
		}
	}
	range(i, numPlayers) {
		// Server will send -1 here if it doesn't have a connected client at all for this player index.
		// For the moment we don't differentiate that case from a silent client (0 messages).
		char numMessages;
		if (readData(&numMessages, 1)) return 1;
		range(j, numMessages) {

			byte size;
			if (readData(&size, 1)) return 1;
			// We are expecting exactly 6 bytes here
			if (size != 6) {
				fprintf(stderr, "Fatal error, player net input should declare size 6, not %d\n", size);
				return 1;
			}

			int32_t msgFrame;
			if (readData(&msgFrame, 4)) return 1;
			msgFrame = ntohl(msgFrame);

			// Todo: Server has the ability to rewrite absolute frames into frame offsets on its end,
			//       might actually be more natural that way.
			int32_t frameOffset = (msgFrame - frame + FRAME_ID_MAX) % FRAME_ID_MAX;
#ifdef DEBUG
			if (frameOffset < 0 || frameOffset > MAX_AHEAD) {
				printf("Invalid frame offset %d (%d - %d)\n", frameOffset, msgFrame, frame);
			}
#endif
			if (frameOffset > mostAhead) mostAhead = frameOffset;

			message *m = &pendingMessages.add();
			supplyBuffer(&m->data);
			list<char> &data = m->data;
			m->player = i;
			m->frameOffset = frameOffset;

			data.setMaxUp(size);
			if (readData(data.items, size)) return 1;
			data.num = size;

			// Now read in any commands
			if (readData(&size, 1)) return 1;
			data.add(size);
			while (size--) {
				int32_t netCmdLen;
				if (readData(&netCmdLen, 4)) return 1;
				int32_t cmdLen = ntohl(netCmdLen);
				int end = data.num + cmdLen + 4;
				data.setMaxUp(end);
				*(int32_t*)(data.items + data.num) = netCmdLen;
				if (readData(data.items + data.num + 4, cmdLen)) return 1;
				data.num = end;
			}
		}
	}

	// Now putting all that info into mutex'd vars for the game thread to make use of.
	lock(netMutex);

	int size = frameData.size();
	int reqdSize = finalizedFrames + 1 + mostAhead;
	// Make sure we have enough buffer groups for everything we need to write down.
	if (reqdSize > size) {
		frameData.setSize(reqdSize);
		// If we put more buffer groups on the queue,
		// reclaim any buffers that were in them from last time.
		// (The queue is circular and recycles entries)
		for (int i = size; i < reqdSize; i++) {
			list<list<char>> &players = frameData.peek(i);
			range(j, players.num) {
				if (players[j].items) {
					reclaimBuffer(&players[j]);
					players[j] = dummyBuffer;
				}
			}
		}
		size = reqdSize;
	}
	// Make sure each buffer group from here on has enough buffers,
	// partly because we're about to write to them,
	// but also partly because the game thread doesn't like worrying
	// about whether they exist or not when reading them.
	for (int i = finalizedFrames; i < size; i++) {
		list<list<char>> &players = frameData.peek(i);
		if (players.num < numPlayers) {
			players.setMaxUp(numPlayers);
			for (int j = players.num; j < numPlayers; j++) {
				players[j] = dummyBuffer;
			}
			players.num = numPlayers;
		}
	}

	// Now that we've guaranteed enough space along both dimensions, put our stuff in.
	range(i, pendingMessages.num) {
		message &m = pendingMessages[i];
		list<char> *dest = &frameData.peek(finalizedFrames + m.frameOffset)[m.player];

		// At present, anything in `frameData` when we unlock the mutex may be read and held onto by
		// the game thread, until the game thread removes it from the queue. This means we can't
		// overwrite / reclaim any of these buffers (excepting the dummy buffer, which is never invalid).
		//
		// We also can't base an overwrite decision on any kind of information from the game thread,
		// since that could be ahead/behind on other clients and we don't want desync.
		if (!dest->items) {
			*dest = m.data;
		} else {
			fputs("Not an issue, but this case shouldn't happen with the new server rules?\n", stderr);
			reclaimBuffer(&m.data);
		}
	}
	pendingMessages.num = 0;

	finalizedFrames++;

	if (asleep) {
		signal(netCond);
	}
	unlock(netMutex);

	return 0;
}

void net2_init(int _numPlayers, int _frame) {
	maxPlayers = numPlayers = _numPlayers;
	expectedFrame = _frame;
	// Don't need mutex lock here since this is before multithreading happens
	frameData.init();
	list<list<char>> &starterFrame = frameData.add();
	range(i, numPlayers) starterFrame.add(dummyBuffer);
	finalizedFrames = 1;

	availBuffers.init();
	pendingMessages.init();
}

void net2_destroy() {
	range(i, pendingMessages.num) {
		reclaimBuffer(&pendingMessages[i].data);
	}
	pendingMessages.destroy();

	range(i, availBuffers.num) {
		availBuffers[i].destroy();
	}
	availBuffers.destroy();

	range(i, frameData.max) {
		list<list<char>> &players = frameData.items[i];
		range(j, players.num) {
			if (players[j].items) players[j].destroy();
		}
		// players.destroy(); // Handled by `frameData.destroy()`, since the queue was responsible for initing them
	}
	frameData.destroy();
}
