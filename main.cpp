#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <allegro5/allegro.h>
#include <allegro5/allegro_opengl.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <unistd.h>
#include <regex.h>

#include "util.h"
#include "list.h"
#include "queue.h"
#include "graphics.h"
#include "font.h"
#include "ent.h"
#include "main.h"
#include "modules.h"
#include "net.h"
#include "handlerRegistrar.h"
#include "effects.h"
#include "map.h"
#include "serialize.h"
#include "box.h"
#include "colors.h"
#include "debugTree.h"

#include "entFuncs.h"
#include "entUpdaters.h"
#include "entGetters.h"

// Allegro's "custom events" are kinda a trashfire, but oh well.
// Eventually I should probably abandon Allegro entirely...
#define CUSTOM_EVT_TYPE 1025
static ALLEGRO_EVENT_SOURCE customSrc;
static ALLEGRO_EVENT customEvent;

#define numKeys 6

static char globalRunning = 1;

static list<player> players, phantomPlayers;
static int myPlayer;

static int32_t defaultColors[8] = {0xFFAA00, 0x00AA00, 0xFF0055, 0xFFFF00, 0xAA0000, 0x00FFAA, 0xFF5555, 0x55FF55};
static regex_t colorRegex;
static regmatch_t regexMatches[3];

int p1Codes[numKeys] = {ALLEGRO_KEY_A, ALLEGRO_KEY_D, ALLEGRO_KEY_W, ALLEGRO_KEY_S, ALLEGRO_KEY_SPACE, ALLEGRO_KEY_LSHIFT};
char p1Keys[numKeys];
static char mouseBtnDown = 0;
static char mouseSecondaryDown = 0;

static gamestate *rootState, *phantomState;

static ALLEGRO_DISPLAY *display;
static ALLEGRO_EVENT_QUEUE *evntQueue;

#define mouseSensitivity 0.0025

static double viewYaw = 0;
static double viewPitch = 0;
static int mouseX = 0;
static int mouseY = 0;
static char thirdPerson = 1;

//static ALLEGRO_VERTEX vertices[4];
//const int indices[6] = {0, 1, 2, 3, 2, 1};
//static int32_t cx = (displayWidth / 2) * PTS_PER_PX;
//static int32_t cy = (displayHeight / 2) * PTS_PER_PX;

// At the moment we use a byte to give the network message length, so this has to be fairly small
#define TEXT_BUF_LEN 200
static char inputTextBuffer[TEXT_BUF_LEN];
static char chatBuffer[TEXT_BUF_LEN];
static int bufferedTextLen = 0;
static int textInputMode = 0; // 0 - idle; 1 - typing; 2 - queued; 3 - queued + wants to type again

#define micro_hist_num 5
static int historical_phys_micros[micro_hist_num];
static int historical_draw_micros[micro_hist_num];
static int historical_flip_micros[micro_hist_num];
static int micro_hist_ix = micro_hist_num-1;
static struct timespec t1, t2, t3, t4;

#define frame_time_num 120
#define median_window 59
#define frame_nanos 33333333
pthread_mutex_t timingMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t timingCond = PTHREAD_COND_INITIALIZER;
static time_t startSec;
static long frameTimes[frame_time_num];
static int frameTimeIx;
static long medianTime;
static int serverLead = -1; // Starts at -1 since it hasn't provided our first frame yet (and we're mad about it)
static int latency;
#define FRAME_ID_MAX 128

#define BIN_CMD_LOAD 128
queue<list<char>> frameData;
queue<list<char>> outboundData;
list<char> syncData; // Temporary buffer for savegame data, for "/sync" command
char syncReady;
pthread_mutex_t outboundMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t outboundCond = PTHREAD_COND_INITIALIZER;

#define lock(mtx) if (int __ret = pthread_mutex_lock(&mtx)) printf("Mutex lock failed with code %d\n", __ret)
#define unlock(mtx) if (int __ret = pthread_mutex_unlock(&mtx)) printf("Mutex unlock failed with code %d\n", __ret)
#define wait(cond, mtx) if (int __ret = pthread_cond_wait(&cond, &mtx)) printf("Mutex cond wait failed with code %d\n", __ret)
#define signal(cond) if (int __ret = pthread_cond_signal(&cond)) printf("Mutex cond signal failed with code %d\n", __ret)

static void outerSetupFrame(list<player> *ps) {
	ent *p = (*ps)[myPlayer].entity;
	float up, forward;
	if (p && thirdPerson) {
		up = 32*PTS_PER_PX;
		forward = -64*PTS_PER_PX;
	} else {
		up = forward = 0;
	}
	setupFrame(-viewPitch, viewYaw, up, forward);
	if (p) {
		frameOffset[0] = -p->center[0];
		frameOffset[1] = -p->center[1];
		frameOffset[2] = -p->center[2];
	} else {
		frameOffset[0] = 0;
		frameOffset[1] = -15000;
		frameOffset[2] = 500*PTS_PER_PX;
	}
}

#define micros_per_frame (1000000 / FRAMERATE)

static float hudColor[3] = {0.0, 0.5, 0.5};
static float grnColor[3] = {0.0, 1.0, 0.0};
static float bluColor[3] = {0.0, 0.0, 1.0};
static float redColor[3] = {1.0, 0.0, 0.0};
static void drawHud(list<player> *ps) {
	setupText();
	drawHudText(chatBuffer, 1, 1, 1, hudColor);
	if (textInputMode) drawHudText(inputTextBuffer, 1, 3, 1, hudColor);

	float f1 = (double) historical_draw_micros[micro_hist_ix] / micros_per_frame;
	float f2 = (double) historical_flip_micros[micro_hist_ix] / micros_per_frame;
	float f3 = (double) historical_phys_micros[micro_hist_ix] / micros_per_frame;
	// Draw frame timing bars
	drawHudRect(0, 1 - 1.0/64, f1, 1.0/64, bluColor);
	drawHudRect(f1, 1 - 1.0/64, f2, 1.0/64, redColor);
	drawHudRect(f1+f2, 1 - 1.0/64, f3, 1.0/64, grnColor);

	// Draw ammo bars if applicable
	ent *p = (*ps)[myPlayer].entity;
	if (!p) return;
	int charge = p->state.sliders[3].v;
	float x = 0.5 - 3.0/128;
	while (charge >= 60) {
		drawHudRect(x, 0.5, 1.0/64, 1.0/64, hudColor);
		x += 1.0/64;
		charge -= 60;
	}
	drawHudRect(x, 0.5 + 1.0/256, (float)charge*(1.0/64/60), 1.0/128, hudColor);
}

static void resetPlayer(gamestate *gs, int i) {
	player *p = &(*gs->players)[i];
	p->reviveCounter = 0;
	p->entity = NULL;
	p->color = defaultColors[i % 8];
}

static void resetPlayers(gamestate *gs) {
	range(i, gs->players->num) resetPlayer(gs, i);
}

static void readFile(const char *name, list<char> *out) {
	int fd = open(name, O_RDONLY);
	if (fd == -1) {
		fprintf(stderr, "ERROR - Load failed - Couldn't read file '%s'\n", name);
		return;
	}
	int ret;
	do {
		out->setMaxUp(out->num + 1000);
		ret = read(fd, out->items + out->num, 1000);
		if (ret == -1) {
			fprintf(stderr, "Failed to read from file, errno is %d\n", errno);
			break;
		}
		out->num += ret;
	} while (ret);
	close(fd);
}

static void saveGame(const char *name) {
	list<char> data;
	data.init();
	serialize(rootState, &data);
	int fd = open(name, O_WRONLY | O_CREAT, 0664); // perms: rw-rw-r--
	if (fd == -1) {
		fprintf(stderr, "ERROR - Save failed - Couldn't write file '%s'\n", name);
		return;
	}
	int ret = write(fd, data.items, data.num);
	if (ret != data.num) {
		fprintf(stderr, "ERROR - Save failed - `write` returned %d when %d was expected\n", ret, data.num);
		// We could always retry a partial write but that's more effort that I'm not sure is necessary
		if (ret == -1) {
			fprintf(stderr, "errno is %d\n", errno);
		}
	}
	data.destroy();
	close(fd);
}

static void doInputs(ent *e, char *data) {
		/* Old joystick code
		ALLEGRO_JOYSTICK_STATE status;
		al_get_joystick_state(p->js, &status);
		if (status.button[0]) pushBtn1(p->entity);
		int i;
		for (i = 0; i < 2; i++) {
			dv[i] = axisMaxis * 1.25 * (status.stick[0].axis[i] - p->center[i]);
			if (dv[i] > axisMaxis) dv[i] = axisMaxis;
			else if (dv[i] < -axisMaxis) dv[i] = -axisMaxis;
		}
		*/
	if (data[0] & 1) pushBtn(e, 0);
	if (data[0] & 2) pushBtn(e, 1);
	if (data[0] & 4) pushBtn(e, 2);
	if (data[0] & 8) pushBtn(e, 3);
	if (data[1] || data[2]) {
		int axis[2] = {data[1], data[2]};
		pushAxis1(e, axis);
	}
	if (data[3] || data[4] || data[5]) {
		int look[3] = {data[3], data[4], data[5]};
		pushEyes(e, look);
	}
}

static void setupTyping() {
	inputTextBuffer[1] = '\0';
	inputTextBuffer[0] = '_';
	bufferedTextLen = 0;
}

static void sendControls(int frame) {
	// This is a cheap trick which I need to codify as a method.
	// It's a threadsafe way to get what the next `add` will be, assuming we're the only
	// thread that ever calls `add`.
	list<char> &out = outboundData.items[outboundData.end];

	out.setMaxUp(11); // 1 frame + 4 size + 6 input data
	out.num = 11;

	out[0] = (char) frame;
	// Size will go in 1-4, we populate it in a minute
	// Other buttons also go here, once they exist; bitfield
	out[5] = p1Keys[4] + 2*p1Keys[5] + 4*mouseBtnDown + 8*mouseSecondaryDown;

	int axis1 = p1Keys[1] - p1Keys[0];
	int axis2 = p1Keys[3] - p1Keys[2];
	double angle = viewYaw;
	double cosine = cos(angle);
	double sine = sin(angle);
	if (axis1 || axis2) {
		double r_x = cosine * axis1 - sine * axis2;
		double r_y = cosine * axis2 + sine * axis1;
		double mag_x = abs(r_x);
		double mag_y = abs(r_y);
		// Normally messy properties of floating point division
		// are mitigated by the fact that `axisMaxis` is a power of 2
		double divisor = (mag_x > mag_y ? mag_x : mag_y) / axisMaxis;
		out[6] = round(r_x / divisor);
		out[7] = round(r_y / divisor);
	} else {
		out[6] = out[7] = 0;
	}
	double pitchRadians = viewPitch;
	double pitchCos = cos(pitchRadians);
	cosine *= pitchCos;
	sine *= pitchCos;
	double pitchSine = sin(pitchRadians);
	double mag = abs(cosine);
	double mag2 = abs(sine);
	if (mag2 > mag) mag = mag2;
	mag2 = abs(pitchSine);
	if (mag2 > mag) mag = mag2;
	double divisor = mag / axisMaxis;
	out[8] = round(sine / divisor);
	out[9] = round(-cosine / divisor);
	out[10] = round(pitchSine / divisor);

	if (syncReady == 2) {
		out.add((char)BIN_CMD_LOAD);
		out.addAll(syncData);
	} else if (textInputMode >= 2) {
		if (!strncmp(inputTextBuffer, "/load", 5)) {
			const char *file = "savegame";
			if (bufferedTextLen > 6) file = inputTextBuffer + 6;
			out.add((char)BIN_CMD_LOAD);
			readFile(file, &out);
		} else {
			int start = out.num;
			out.num += bufferedTextLen;
			out.setMaxUp(out.num);
			memcpy(out.items + start, inputTextBuffer, bufferedTextLen);
		}
		textInputMode -= 2;
		if (textInputMode == 1) setupTyping();
	}
	*(int32_t*)(out.items + 1) = htonl(out.num - 5);
	sendData(out.items, out.num);

	lock(outboundMutex);
	if (syncReady == 2) syncReady = 0;
	outboundData.add();
	signal(outboundCond);
	unlock(outboundMutex);
	// Todo: Maybe `pop` doesn't make it immediately available for reclamation, so both ends have some wiggle room?
}

static void updateColor(player *p) {
	if (p->entity) {
		uStateSlider(&p->entity->state, 6, p->color);
	}
}

static void mkHeroes(gamestate *gs) {
	int numPlayers = gs->players->num;
	range(i, numPlayers) {
		player *p = &(*gs->players)[i];
		if (p->entity == NULL) {
			if (p->reviveCounter--) continue;
			p->entity = mkHero(gs, i, numPlayers);
			if (i == myPlayer) {
				viewPitch = M_PI_2;
				viewYaw = 0;
			}
			updateColor(p);
		}
	}
}

static void cleanupDeadHeroes(gamestate *gs) {
	int numPlayers = gs->players->num;
	range(i, numPlayers) {
		player *p = &(*gs->players)[i];
		if (p->entity && p->entity->dead) {
			p->entity = NULL;
			p->reviveCounter = FRAMERATE * 3;
			if (i == myPlayer) {
				viewPitch = M_PI_4 / 2;
				viewYaw = 0;
			}
		}
	}
}

char handleKey(int code, char pressed) {
	int i;
	for (i = numKeys-1; i >= 0; i--) {
		if (p1Codes[i] == code) {
			p1Keys[i] = pressed;
			return true;
		}
	}
	if (pressed && code == ALLEGRO_KEY_TAB) thirdPerson ^= 1;
	return false;
}

static void handleMouseMove(int dx, int dy) {
	viewYaw += dx * mouseSensitivity;
	if (viewYaw >= M_PI) viewYaw -= 2*M_PI;
	else if (viewYaw < -M_PI) viewYaw += 2*M_PI;
	viewPitch += dy * mouseSensitivity;
	if (viewPitch > M_PI_2) viewPitch = M_PI_2;
	else if (viewPitch < -M_PI_2) viewPitch = -M_PI_2;
}

static void updateMedianTime() {
	int ixStart = (frameTimeIx - serverLead - median_window + frame_time_num) % frame_time_num;
	int ixEnd = serverLead <= 0 ? frameTimeIx : (ixStart + median_window) % frame_time_num;
	int lt = 0;
	int le = 0;
	long stepDown = 0;
	long stepUp = INT64_MAX;
	for (int ix = ixStart; ix != ixEnd; ix = (ix+1) % frame_time_num) {
		long t = frameTimes[ix];
		// TODO Should we pull `medianTime` ahead of time??
		//      (What does this mean? Was I thinking about threading? Surely it doesn't matter, right?)
		if (t < medianTime) {
			lt++;
			le++;
			if (t > stepDown) stepDown = t;
		} else if (t > medianTime) {
			if (t < stepUp) stepUp = t;
		} else {
			le++;
		}
	}
	long old = medianTime;
	if (le <= median_window/2) {
		medianTime = stepUp;
	} else if (lt > median_window/2) {
		medianTime = stepDown;
	} else {
		return;
	}
	long delt = medianTime - old;
	if (delt > 3000000 || delt < -3000000) { // Around 10% of frame time, definitely notable
		printf("Delay jumped by %ld ns\n", delt);
	}
}

static void processCmd(gamestate *gs, player *p, char *data, int chars, char isMe, char isReal) {
	if (chars && *(unsigned char*)data == BIN_CMD_LOAD) {
		if (!isReal) return;
		doCleanup(rootState);
		// Can't make a new gamestate here (as we might be tempted to),
		// since stuff further up the call stack (like `doUpdate`) has a reference
		// to the existing `gamestate` pointer
		resetGamestate(rootState);
		resetPlayers(rootState);

		list<char> fakeList;
		fakeList.items = data+1;
		fakeList.num = fakeList.max = chars - 1;

		deserialize(rootState, &fakeList);
		return;
	}
	// Default case, just display it. Message could get lost here if multiple people send on the same frame,
	// but it will at least be consistent? Maybe fixing this is a TODO
	if (chars < TEXT_BUF_LEN) {
		memcpy(chatBuffer, data, chars);
		chatBuffer[chars] = '\0';
		char wasCommand = 1;
		if (!strncmp(chatBuffer, "/save", 5)) {
			if (isMe && isReal) {
				const char *name = "savegame";
				if (chars > 6) name = chatBuffer + 6;
				printf("Saving game to %s\n", name);
				saveGame(name);
			}
		} else if (!strncmp(chatBuffer, "/sync", 5)) {
			if (isMe && isReal && !syncReady) {
				syncData.num = 0;
				serialize(rootState, &syncData);
				syncReady = 1;
			}
		} else if (!strncmp(chatBuffer, "/tree", 5)) {
			if (isMe && isReal) {
				printTree(gs);
			}
		} else if (chars >= 6 && !strncmp(chatBuffer, "/c ", 3)) {
			int32_t color;
			// First, check for 6-digit hex color representation
			if (!regexec(&colorRegex, chatBuffer+3, 3, regexMatches, 0)) {
				color = strtol(chatBuffer + 3 + regexMatches[2].rm_so, NULL, 16);
			} else {
				// Next, check named CSS colors, as an easter egg for Paul
				color = findColor(chatBuffer + 3);
				if (color == -1) {
					// Finally, fall back to the old, weird, 6-bit color representation
					color = (chatBuffer[3] - '0') + 4 * (chatBuffer[4] - '0') + 16 * (chatBuffer[5] - '0');
					// Convert from weird 6-bit repr (with all its quirks) to regular 32-bit color
					color = (color&3)*255/3*0x10000 + (color&12)/4*255/3*0x100 + (color&48)/16*255/3;
				}
			}
			// Timberwolf is a cursed color, you can only leave with a _silver_ bullet
			if (p->color != 0xd9d6cf || color == 0xc0c0c0) {
				p->color = color;
				updateColor(p);
			}
		} else {
			wasCommand = 0;
		}
		if (wasCommand) chatBuffer[0] = '\0';
	} else {
		fputs("Incoming \"chat\" buffer was too long, ignoring\n", stderr);
	}
}

static void doWholeStep(gamestate *state, char *inputData, char *data2) {
	// TODO: If view direction ever matters, that should carry over rather than have a constant default
	// (buttons, move_x, move_y, look_x, look_y, look_z)
	static char defaultData[6] = {0, 0, 0, 0, 0, 0};

	unsigned char numPlayers = *inputData++;
	list<player> &players = *state->players;
	players.setMaxUp(numPlayers);
	while (players.num < numPlayers) {
		resetPlayer(state, players.num++);
	}

	doCrushtainer(state);
	createDebris(state);
	mkHeroes(state);

	range(i, numPlayers) {
		char isMe = i == myPlayer;
		char *toProcess;
		if (data2 && isMe) {
			toProcess = data2 + 1;
		} else {
			toProcess = inputData;
		}
		int32_t size = ntohl(*(int32_t*)inputData);
		inputData += 4 + size;

		size = ntohl(*(int32_t*)toProcess);
		char *data;
		if (size == 0) {
			data = defaultData;
		} else {
			data = toProcess + 4;
			if (size > 6 && (isMe || !data2)) {
				processCmd(state, &players[i], toProcess + 10, size - 6, isMe, !data2);
			}
		}
		if (players[i].entity != NULL) {
			doInputs(players[i].entity, data);
		}
	}

	doUpdates(state);

	// Gravity should be applied right before physics;
	// `doPhysics` has a flush at the beginning, so we're sure it hits every single entity (nothing new is created that we miss), 
	// and also things that are "at rest" on a surface will see a "stationary" vertical velocity (except during collisions)
	doGravity(state);

	doPhysics(state); 

	// This happens just before finishing the step so we can be 100% sure whether the player's entity is dead or not
	cleanupDeadHeroes(state);
	finishStep(state);
}

static void cloneToPhantom() {
	doCleanup(phantomState);
	free(phantomState);
	phantomState = dup(rootState, &phantomPlayers);
}

static void* pacedThreadFunc(void *_arg) {
	int reqdOutboundSize = latency;
	long destNanos;
	timespec t;
	
	list<char> latestFrameData;
	{
		// Initialize it with valid (but "empty") data
		int numPlayers = rootState->players->num;
		latestFrameData.init(1 + numPlayers*4);
		latestFrameData.add(numPlayers);
		range(i, numPlayers*4) latestFrameData.add(0);
	}

	lock(outboundMutex);
	range(i, latency) {
		// Populate the initial `latency` frames with empty data,
		// since we didn't have the chance to send anything there.
		list<char> &tmp = outboundData.add();
		tmp.num = 0;
		range(j, 5) tmp.add(0); // 1 byte frame number (doesn't matter), 4 bytes size (all zero)
	}
	unlock(outboundMutex);

	while (1) {
		lock(timingMutex);
		while (medianTime == INT64_MAX && globalRunning) {
			puts("Server isn't responding, pausing pacing thread...");
			wait(timingCond, timingMutex);
		}
		destNanos = medianTime;
		unlock(timingMutex);
		if (!globalRunning) break;

		clock_gettime(CLOCK_MONOTONIC_RAW, &t);
		destNanos = destNanos - t.tv_nsec - 1000000000 * (t.tv_sec - startSec);
		if (destNanos > 999999999) {
			puts("WARN - Tried to wait for more than a second???");
			// No idea why this would ever come up, but also it runs afoul of the spec to
			// request an entire second or more in the tv_nsec field.
			destNanos = 999999999;
		}
		if (destNanos > 0) {
			t.tv_sec = 0;
			t.tv_nsec = destNanos;
			if (nanosleep(&t, NULL)) continue; // Something happened, we don't really care what, do it over again
		}
		al_emit_user_event(&customSrc, &customEvent, NULL);
		reqdOutboundSize++;

		// Begin setting up the tick, including some hard-coded things like gravity / lava
		clock_gettime(CLOCK_MONOTONIC_RAW, &t1);
		outerSetupFrame(&phantomPlayers);
		doDrawing(phantomState);
		drawHud(&phantomPlayers);

		clock_gettime(CLOCK_MONOTONIC_RAW, &t2);
		al_flip_display();

		clock_gettime(CLOCK_MONOTONIC_RAW, &t3);
		int framesProcessed = 0;
		while(1) {
			lock(timingMutex); // Should we hang onto this lock for longer??
			list<char> *prevFrame;
			if (framesProcessed) prevFrame = &frameData.pop();
			// TODO Should we manually de-allocate very large frames so they don't hang around holding memory?
			int size = frameData.size();
			if (size == 0 || size <= serverLead) {
				if (framesProcessed && prevFrame->num < 100) {
					// We want to exclude large frames from being counted as the "latest",
					// since we probably don't want to run them multiple times in the case of a delay.
					// The way we check right now means it could actually exlude some normal frames too
					// if they were delivered close to (and processed with) the large frame,
					// but that's not really a concern.
					latestFrameData.num = 0;
					latestFrameData.addAll(*prevFrame);
				}
				unlock(timingMutex);
				break;
			}
			char *data = frameData.items[frameData.start].items;
			unlock(timingMutex);
			framesProcessed++;
			doWholeStep(rootState, data, NULL);
		}

		lock(outboundMutex);
		while (outboundData.size() < reqdOutboundSize) {
			if (!globalRunning) {
				unlock(outboundMutex);
				goto paced_exit;
			}
			wait(outboundCond, outboundMutex);
		}
		outboundData.multipop(framesProcessed);
		reqdOutboundSize -= framesProcessed;
		if (syncReady == 1) syncReady = 2;
		unlock(outboundMutex);

		int outboundStart;
		if (framesProcessed) {
			cloneToPhantom();
			outboundStart = 0;
		} else {
			outboundStart = reqdOutboundSize - 1;
		}
		while (outboundStart < reqdOutboundSize) {
			// TODO Is all this locking and re-locking even necessary?
			// I don't think it's even going to try to re-lock until we tell it to send again,
			// so we're just wasting effort. Maybe should expand this to be one big
			// continuation of the previous lock.
			lock(outboundMutex);
			char *myData = outboundData.peek(outboundStart++).items;
			unlock(outboundMutex);
			doWholeStep(phantomState, latestFrameData.items, myData);
		}

		clock_gettime(CLOCK_MONOTONIC_RAW, &t4);
		{
			int drawMicros = 1000000 * (t2.tv_sec - t1.tv_sec) + (t2.tv_nsec - t1.tv_nsec) / 1000;
			int flipMicros = 1000000 * (t3.tv_sec - t2.tv_sec) + (t3.tv_nsec - t2.tv_nsec) / 1000;
			int physMicros = 1000000 * (t4.tv_sec - t3.tv_sec) + (t4.tv_nsec - t3.tv_nsec) / 1000;
			micro_hist_ix = (micro_hist_ix+1)%micro_hist_num;
			historical_draw_micros[micro_hist_ix] = drawMicros;
			historical_flip_micros[micro_hist_ix] = flipMicros;
			historical_phys_micros[micro_hist_ix] = physMicros;
		}

		// if `serverLead == 0` here, I *think* that means the server has sent us the packet for this frame, but nothing extra yet
		lock(timingMutex);
		serverLead--;
		range(i, frame_time_num) frameTimes[i] += frame_nanos;
		medianTime += frame_nanos;
		updateMedianTime();
		unlock(timingMutex);
	}

	paced_exit:;
	latestFrameData.destroy();
	return NULL;
}

static void* inputThreadFunc(void *arg) {
	char mouse_grabbed = 0;
	char running = 1;
	int frame = *(char*)arg;
	// Could use an Allegro timer here, but we expect the delay to be updated moment-to-moment so this is easier
	//ALLEGRO_TIMEOUT timeout;
	ALLEGRO_EVENT evnt;
	evnt.type = 0;
	while (running) {
		/*
		if (evnt.type != CUSTOM_EVT_TYPE) al_init_timeout(&timeout, 1); // 1.0 seconds from now
		bool hasEvent = al_wait_for_event_until(evntQueue, &evnt, &timeout);
		if (!hasEvent) {
			evnt.type = 0;
			puts("Boo");
			continue;
		}
		*/
		al_wait_for_event(evntQueue, &evnt);
		switch(evnt.type) {
			case ALLEGRO_EVENT_DISPLAY_CLOSE:
				// Make sure the main thread knows to stop, it will handle the others (including this thread)
				globalRunning = 0;
				// It could potentially be waiting on this condition
				lock(timingMutex);
				signal(timingCond);
				unlock(timingMutex);
				// Could also be waiting here, we just want it to wake up so it knows it's time to leave
				lock(outboundMutex);
				signal(outboundCond);
				unlock(outboundMutex);
				break;
			case ALLEGRO_EVENT_KEY_UP:
				handleKey(evnt.keyboard.keycode, 0);
				break;
			case ALLEGRO_EVENT_KEY_DOWN:
				if (textInputMode == 1) break;
				if (evnt.keyboard.keycode == ALLEGRO_KEY_ESCAPE) {
					al_ungrab_mouse();
					al_show_mouse_cursor(display);
					mouse_grabbed = 0;
				}
				handleKey(evnt.keyboard.keycode, 1);
				break;
			case ALLEGRO_EVENT_KEY_CHAR:
				if (textInputMode == 1) {
					int c = evnt.keyboard.unichar;
					if (c >= 0x20 && c <= 0xFE && bufferedTextLen+2 < TEXT_BUF_LEN) {
						inputTextBuffer[bufferedTextLen+2] = '\0';
						inputTextBuffer[bufferedTextLen+1] = '_';
						inputTextBuffer[bufferedTextLen] = c;
						bufferedTextLen++;
					} else if (c == '\r') {
						inputTextBuffer[bufferedTextLen] = '\0';
						textInputMode = 2;
					} else if (evnt.keyboard.keycode == ALLEGRO_KEY_ESCAPE) {
						textInputMode = 0;
					} else if (c == '\b' && bufferedTextLen) {
						inputTextBuffer[bufferedTextLen] = '\0';
						inputTextBuffer[bufferedTextLen-1] = '_';
						bufferedTextLen--;
					}
				} else {
					if (evnt.keyboard.keycode == ALLEGRO_KEY_T) {
						if (textInputMode == 0) setupTyping();
						textInputMode |= 1;
					} else if (evnt.keyboard.keycode == ALLEGRO_KEY_SLASH && !textInputMode) {
						inputTextBuffer[0] = '/';
						inputTextBuffer[1] = '_';
						inputTextBuffer[2] = '\0';
						bufferedTextLen = 1;
						textInputMode = 1;
					}
				}
				break;
			case ALLEGRO_EVENT_MOUSE_BUTTON_DOWN:
				if (!mouse_grabbed) {
					if ( (mouse_grabbed = al_grab_mouse(display)) ) {
						al_hide_mouse_cursor(display);
					}
					break;
				}
				{
					int btn = evnt.mouse.button;
					if (btn == 1) mouseBtnDown = 1;
					else if (btn == 2) mouseSecondaryDown = 1;
				}
				break;
			case ALLEGRO_EVENT_MOUSE_LEAVE_DISPLAY:
				mouse_grabbed = 0;
				mouseBtnDown = 0;
				mouseSecondaryDown = 0;
				al_show_mouse_cursor(display);
				break;
			case ALLEGRO_EVENT_MOUSE_BUTTON_UP:
				{
					int btn = evnt.mouse.button;
					if (btn == 1) mouseBtnDown = 0;
					else if (btn == 2) mouseSecondaryDown = 0;
				}
				break;
			case ALLEGRO_EVENT_MOUSE_AXES:
				{
					// Allegro promises to track dx and dy,
					// but sadly those numbers are just plain wrong
					// if you add in requests to warp the mouse
					// (yes, even accounting for the separate warp event)
					int x = evnt.mouse.x;
					int y = evnt.mouse.y;
					if (mouse_grabbed) {
						handleMouseMove(x - mouseX, y - mouseY);
						// No idea if these are sensible criteria for warping the mouse,
						// but it makes sense to meeeee
						if (
							abs(x - displayWidth/2) > displayWidth * 0.2
							|| abs(y - displayHeight/2) > displayHeight * 0.2
						) {
							al_set_mouse_xy(display, displayWidth/2, displayHeight/2);
						}
					}
					mouseX = x;
					mouseY = y;
				}
				break;
			case ALLEGRO_EVENT_MOUSE_WARPED:
				mouseX = evnt.mouse.x;
				mouseY = evnt.mouse.y;
				break;
			case CUSTOM_EVT_TYPE:
				int x = (int) evnt.user.data1;
				if (x == -1) running = 0;
				else sendControls(frame);
				frame = (frame + 1) % FRAME_ID_MAX;
				break;
		}
	}
	return NULL;
}

static void setupPlayers(gamestate *gs, int numPlayers) {
	memset(p1Keys, 0, sizeof(p1Keys));
	gs->players->setMaxUp(numPlayers);
	gs->players->num = numPlayers;
	resetPlayers(gs);
}

static void* netThreadFunc(void *arg) {
	// The 0.3 here is pretty arbitrary;
	// lower values (> 0) are better at handling long network latency,
	// while higher values (< 1) are better at handling inconsistencies in the latency (specifically from the server to us)
	long serverDelay = frame_nanos * latency * 0.4;

	char expectedFrame = *(char*)arg;
	while (1) {
		char frame;
		if (readData(&frame, 1)) break;
		if (frame != expectedFrame) {
			printf("Didn't get right frame value, expected %d but got %d\n", expectedFrame, frame);
			break;
		}
		expectedFrame = (expectedFrame + 1) % FRAME_ID_MAX;

		timespec now;
		clock_gettime(CLOCK_MONOTONIC_RAW, &now);
		long nanos = 1000000000 * (now.tv_sec - startSec) + now.tv_nsec;
		nanos = nanos - frame_nanos * serverLead + serverDelay;

		// This is a cheap trick which I need to codify as a method.
		// It's a threadsafe way to get what the next `add` will be, assuming we're the only
		// thread that ever calls `add`.
		list<char> &thisFrameData = frameData.items[frameData.end];
		thisFrameData.num = 0;
		unsigned char numPlayers;
		readData(&numPlayers, 1);
		thisFrameData.add(numPlayers);
		for (int i = 0; i < numPlayers; i++) {
			int32_t netSize;
			if (readData(&netSize, 4)) goto done;
			int32_t size = ntohl(netSize);
			if (size && size < 6) {
				fprintf(stderr, "Fatal error, can't handle player net input of size %hhd\n", size);
				goto done;
			}
			thisFrameData.setMaxUp(thisFrameData.num + size + 4);
			*(int32_t*)(thisFrameData.items + thisFrameData.num) = netSize;
			thisFrameData.num += 4;
			if (readData(thisFrameData.items + thisFrameData.num, size)) goto done;
			thisFrameData.num += size;
		}

		lock(timingMutex);
		frameData.add();
		serverLead++;
		frameTimes[frameTimeIx] = nanos;
		frameTimeIx = (frameTimeIx + 1) % frame_time_num;
		if (serverLead <= 0) {
			char asleep = (medianTime == INT64_MAX);
			updateMedianTime();
			if (asleep && medianTime != INT64_MAX) {
				signal(timingCond);
			}
		}
		unlock(timingMutex);
	}
	done:;
	return NULL;
}

const char* usage = "Arguments: server_addr [port]\n\tport default is 15000";

int main(int argc, char **argv) {
	// Parse args
	if (argc < 2 || argc > 3) {
		puts(usage);
		return 1;
	}
	char *srvAddr = argv[1];
	int port;
	if (argc > 2) {
		port = atoi(argv[2]);
		printf("Using specified port of %d\n", port);
	} else {
		port = 15000;
	}

	regcomp(&colorRegex, "^ *(#|0x)?([0-9a-f]{6}) *$", REG_EXTENDED|REG_ICASE);
	players.init();
	phantomPlayers.init();

	rootState = mkGamestate(&players);
	phantomState = mkGamestate(&phantomPlayers);

	init_registrar();
	init_entFuncs();

	// Set up allegro stuff
	if (!al_install_system(ALLEGRO_VERSION_INT, NULL)) {
		fputs("Couldn't init Allegro\n", stderr);
		return 1;
	}
	/* Joystick code is still laying around places, but it's all out of date,
	   and I had some comment about how they "don't account for yaw when sending inputs".
	   TODO maybe?
	if (al_install_joystick()) {
		numPlayers = al_get_num_joysticks() + 1;
	} else {
		numPlayers = 1;
		fputs("Couldn't install joystick driver\n", stderr);
	}
	 */
	al_set_new_display_flags(ALLEGRO_OPENGL);
	al_set_new_display_option(ALLEGRO_DEPTH_SIZE, 16, ALLEGRO_SUGGEST);
	display = al_create_display(displayWidth, displayHeight);
	if (!display) {
		fputs("Couldn't create our display\n", stderr);
		return 1;
	}
	if (!al_install_keyboard()) {
		fputs("No keyboard support, which would make the game unplayable\n", stderr);
		return 1;
	}
	if (!al_install_mouse()) {
		fputs("No mouse support.\n", stderr);
	}
	initGraphics(); // OpenGL Setup (calls initFont())
	colors_init();
	velbox_init();
	ent_init();
	initMods(); //Set up modules
	frameData.init();
	outboundData.init();
	syncData.init();

	// Other general game setup, including networking
	puts("Connecting to host...");
	if (initSocket(srvAddr, port)) return 1;
	puts("Done.");
	puts("Awaiting setup info...");
	char clientCounts[4];
	if (readData(clientCounts, 4)) {
		puts("Error, aborting!");
		return 1;
	}
	if (clientCounts[0] != (char)0x80) {
		printf("Bad initial byte %hhd, aborting!\n", clientCounts[0]);
		return 1;
	}
	myPlayer = clientCounts[1];
	latency = clientCounts[2];
	char startFrame = clientCounts[3];
	printf("Done, I am client #%d\n", myPlayer);

	// `myPlayer + 1` is the minimum number where our index makes sense,
	// try to avoid things catching on fire
	setupPlayers(rootState, myPlayer + 1);
	// Map loading
	mkMap(rootState);
	// Loading from file is one thing, but programatically defined maps may need a `flush` so things aren't weird the first time around.
	// `flush` isn't exposed since this is the one time we need it, and it's not like it really matters,
	// so we just do the second half of a normal step to get the same effect.
	doPhysics(rootState);
	finishStep(rootState);
	// Not 100% sure this part is necessary, but it's simpler if we know the phantom state is also in a valid state.
	cloneToPhantom();

	// Allegro Events
	//ALLEGRO_TIMER *timer = al_create_timer(ALLEGRO_BPS_TO_SECS(FRAMERATE));
	al_init_user_event_source(&customSrc);
	evntQueue = al_create_event_queue();
	al_register_event_source(evntQueue, &customSrc);
	al_register_event_source(evntQueue, al_get_display_event_source(display));
	//al_register_event_source(queue, al_get_timer_event_source(timer));
	al_register_event_source(evntQueue, al_get_keyboard_event_source());
	if (al_is_mouse_installed()) {
		al_register_event_source(evntQueue, al_get_mouse_event_source());
		// For now, don't capture mouse on startup. Might prevent mouse warpiness in first frame, idk, feel free to mess around
	}

	inputTextBuffer[0] = inputTextBuffer[TEXT_BUF_LEN-1] = '\0';
	chatBuffer[0] = chatBuffer[TEXT_BUF_LEN-1] = '\0';

	// Pre-populate timing buffer, set `startSec`
	{
		timespec now;
		clock_gettime(CLOCK_MONOTONIC_RAW, &now);
		startSec = now.tv_sec;
		long firstFrame = now.tv_nsec + frame_nanos;
		range(i, frame_time_num) {
			frameTimes[i] = firstFrame;
		}
		updateMedianTime();
	}
	customEvent.user.type = CUSTOM_EVT_TYPE;
	customEvent.user.data1 = 0;


	//Main loop
	pthread_t inputThread;
	pthread_t netThread;
	{
		int ret = pthread_create(&inputThread, NULL, &inputThreadFunc, &startFrame);
		if (ret) {
			printf("pthread_create returned %d for inputThread\n", ret);
			return 1;
		}
		ret = pthread_create(&netThread, NULL, netThreadFunc, &startFrame);
		if (ret) {
			printf("pthread_create returned %d for netThread\n", ret);
			pthread_cancel(inputThread); // No idea if this works, this is a failure case anyway
			return 1;
		}
	}
	// Main thread lives in here until the program exits
	pacedThreadFunc(NULL);

	puts("Beginning cleanup.");
	puts("Cancelling network thread...");
	{
		closeSocket();
		int ret = pthread_join(netThread, NULL);
		if (ret) printf("Error while joining pacing thread: %d\n", ret);
	}
	puts("Done.");
	puts("Cancelling input thread...");
	{
		customEvent.user.data1 = -1;
		al_emit_user_event(&customSrc, &customEvent, NULL);
		//int ret = pthread_cancel(inputThread);
		//if (ret) printf("Error cancelling input thread: %d\n", ret);
		int ret = pthread_join(inputThread, NULL);
		if (ret) printf("Error while joining input thread: %d\n", ret);
	}
	puts("Done.");
	puts("Cleaning up game objects...");
	doCleanup(rootState);
	free(rootState);
	doCleanup(phantomState);
	free(phantomState);
	puts("Done.");
	puts("Cleaning up simple interal components...");
	syncData.destroy();
	frameData.destroy();
	outboundData.destroy();
	destroyFont();
	destroy_registrar();
	ent_destroy();
	velbox_destroy();
	colors_destroy();
	puts("Done.");
	puts("Cleaning up Allegro...");
	al_unregister_event_source(evntQueue, &customSrc);
	al_destroy_user_event_source(&customSrc);
	al_destroy_event_queue(evntQueue);
	al_destroy_display(display);
	al_uninstall_system();
	puts("Done.");
	puts("Final misc cleanup...");
	players.destroy();
	phantomPlayers.destroy();
	regfree(&colorRegex);
	puts("Done.");
	puts("Cleanup complete, goodbye!");
	return 0;
}
