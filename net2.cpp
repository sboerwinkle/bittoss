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

static int numPlayers;
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
	//       if we really wanted. (`reclaimBuffer` is called exclusively from the mutex'd
	//       region, since we're reclaiming from `frameData`.)
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

void* netThreadFunc(void *startFrame) {
	int32_t expectedFrame = *(int32_t*)startFrame;
	availBuffers.init();
	pendingMessages.init();
	int maxPlayers = numPlayers;

	while (1) {
		int32_t frame;
		if (readData(&frame, 4)) break;
		frame = ntohl(frame);
		if (frame != expectedFrame) {
			printf("Didn't get right frame value, expected %d but got %d\n", expectedFrame, frame);
			break;
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
				goto done;
			}
		}
		for (int i = 0; i < numPlayers; i++) {
			// Server will send -1 here if it doesn't have a connected client at all for this player index.
			// For the moment we don't differentiate that case from a silent client (0 messages).
			char numMessages;
			if (readData(&numMessages, 1)) goto done;
			range(j, numMessages) {

				int32_t netSize;
				if (readData(&netSize, 4)) goto done;
				int32_t size = ntohl(netSize);
				// Minimum valid size is 4 bytes frame id + 6 bytes data. Maybe make this a #define somewhere?
				if (size && size < 10) {
					fprintf(stderr, "Fatal error, can't handle player net input of size %hhd\n", size);
					goto done;
				}

				int32_t msgFrame;
				if (readData(&msgFrame, 4)) goto done;
				size -= 4;
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
				m->player = i;
				m->frameOffset = frameOffset;

				m->data.setMaxUp(size);
				if (readData(m->data.items, size)) goto done;
				m->data.num = size;
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
			// Writing the same player+frame twice is rare, but may happen if the server has to overwrite
			// a client-provided frame id to keep it inside the guaranteed bounds [0, MAX_AHEAD].
			// We also can't base an overwrite decision on any kind of information from the game thread,
			// since that could be ahead/behind on other clients and we don't want desync.
			if (!dest->items) {
				*dest = m.data;
			}
		}
		pendingMessages.num = 0;

		finalizedFrames++;

		if (asleep) {
			signal(netCond);
		}
		unlock(netMutex);
	}
	done:;

	range(i, pendingMessages.num) {
		reclaimBuffer(&pendingMessages[i].data);
	}
	pendingMessages.destroy();

	range(i, availBuffers.num) {
		availBuffers[i].destroy();
	}
	availBuffers.destroy();

	return NULL;
}



void net2_init(int _numPlayers) {
	numPlayers = _numPlayers;
	// Don't need mutex lock here since this is before multithreading happens
	frameData.init();
	list<list<char>> starterFrame = frameData.add();
	range(i, numPlayers) starterFrame.add(dummyBuffer);
	finalizedFrames = 1;
}

void net2_destroy() {
	range(i, frameData.max) {
		list<list<char>> &players = frameData.items[i];
		range(j, players.num) {
			if (players[j].items) players[j].destroy();
		}
		// players.destroy(); // Handled by `frameData.destroy()`, since the queue was responsible for initing them
	}
	frameData.destroy();
}
