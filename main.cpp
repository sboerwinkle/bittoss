#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <GL/gl.h>
#include <GLFW/glfw3.h>
#include <arpa/inet.h>
#include <pthread.h>

#include "util.h"
#include "list.h"
#include "queue.h"
#include "bloc.h"

#include "config.h"
#include "ent.h"
#include "font.h"
#include "gamestring.h"
#include "graphics.h"
#include "modules.h"
#include "net.h"
#include "net2.h"
#include "handlerRegistrar.h"
#include "effects.h"
#include "map.h"
#include "serialize.h"
#include "box.h"
#include "colors.h"
#include "debugTree.h"
#include "edit.h"
#include "file.h"
#include "hud.h"
#include "raycast.h"
#include "controlBuffer.h"

#include "entFuncs.h"
#include "entUpdaters.h"
#include "entGetters.h"

#include "modules/player.h"

#include "main.h"

#define numKeys 6
#define TEXT_BUF_LEN 200

typedef bloc<char, TEXT_BUF_LEN> strbuf;

char globalRunning = 1;

static int myPlayer;

static char const * defaultColors[6] = {
	"2.1", "1.1",
	"2.2", "1.2",
	"2.3", "1.3"
};

int p1Codes[numKeys] = {GLFW_KEY_A, GLFW_KEY_D, GLFW_KEY_W, GLFW_KEY_S, GLFW_KEY_SPACE, GLFW_KEY_LEFT_SHIFT};

struct inputs {
	struct {
		char p1Keys[numKeys];
		double viewYaw = 0;
		double viewPitch = 0;
	} basic;
	// Don't really need to bother with capturing fractional-frame events for things like
	// the movement keys or shift.
	controlBuffer spaceBuf;
	controlBuffer lmbBuf;
	controlBuffer rmbBuf;

	// If this struct gets much bigger,
	// may want to consider a `queue` of message objects or something...
};
inputs activeInputs = {0}, sharedInputs = {0};

char textBuffer[TEXT_BUF_LEN];
char textSendInd;
queue<strbuf> outboundTextQueue;

struct {
	int width, height;
	char changed;
} framebuffer;

static struct {
	gamestate *pickup, *dropoff;
	long nanos;
} renderData = {0};
pthread_mutex_t renderMutex = PTHREAD_MUTEX_INITIALIZER;
gamestate *renderedState;
long renderStartNanos = 0;
char manualGlFinish = 1;
char showFps = 0;

static int typingLen = -1;

static char chatBuffer[TEXT_BUF_LEN];
static char loopbackCommandBuffer[TEXT_BUF_LEN];
static char loadedFilename[TEXT_BUF_LEN];

// Mostly, nobody outside this file should use `rootState`,
// but it's helpful to have for debug prints somtimes.
gamestate *rootState;
static gamestate *phantomState;

static GLFWwindow *display;

#define mouseSensitivity 0.0025

static double mouseX = 0;
static double mouseY = 0;
static char thirdPerson = 1;
static char ctrlPressed = 0, altPressed = 0;
static int wheelIncr = 200;
static char mouseGrabbed = 0;
static int mouseDragMode = 0, mouseDragInput = 0, mouseDragOutput = 0, mouseDragFlip = 0;
static int mouseDragSize = 30;

static int32_t ghostCenter[3] = {0, -15000, 16000};

static tick_t seat_tick, seat_tick_old;


static long inputs_nanos = 0;
static long update_nanos = 0;
static long follow_nanos = 0;
static struct timespec t1, t2, t3, t4;
static int performanceFrames = 0, performanceIters = 0;
#define PERF_FRAMES_MAX 120

// 1 sec = 1 billion nanos
#define BILLION  1000000000
// This should be 666.. repeating,
// but we want to step a little slower.
// The interplay of these constants affects
// how patient and aggressive we are when
// it comes to watching for and responding to
// lag spikes. I'm not sure about these values,
// but hopefully they're about right?
#define STEP_NANOS   67666666
#define FASTER_NANOS 66000000
#define PENALTY_FRAMES 90

static int fasterFrames = 0;
static time_t startSec;

#define BIN_CMD_SYNC 128
#define BIN_CMD_IMPORT 129
#define BIN_CMD_LOAD 130
#define BIN_CMD_ADD 131
queue<list<char>> outboundData;
list<char> syncData; // Temporary buffer for savegame data, for "/sync" command
static int syncNeeded = 0;
pthread_mutex_t sharedInputsMutex = PTHREAD_MUTEX_INITIALIZER;
static char isLoader = 0;
static char doReload = 0;

// Simple utility to get "time" in just a regular (long) number.
// `startSec` exists so we're more confident we don't overflow.
static long nowNanos() {
	timespec now;
	clock_gettime(CLOCK_MONOTONIC_RAW, &now);
	return BILLION * (now.tv_sec - startSec) + now.tv_nsec;
}

static void outerSetupFrame(list<player> *ps, gamestate *gs, int32_t *oldPos, int32_t *newPos, float interpRatio) {
	// It's a different thread that writes the pitch/yaw values, and I just realized access
	// has been unsynchronized for a while. It's possible that `double` writes are effectively
	// atomic on modern processors, but whatever the case:
	// A) Nobody's complained about weirdness here
	// B) If there was weirdness it would just manifest as weird visual frames, not a crash or desync
	double pitchRadians = activeInputs.basic.viewPitch;
	double yawRadians = activeInputs.basic.viewYaw;
	double cosine = cos(yawRadians);
	double sine = sin(yawRadians);

	ent *e = (*ps)[myPlayer].entity;
	if (e) {
		memcpy(ghostCenter, e->center, sizeof(ghostCenter));
		memcpy(oldPos, e->old, sizeof(e->old));
	} else {
		// This was pretty much copied from serializeControls,
		// but there's just enough different that factoring it
		// out as common code would make things ugly.
		const char *const k = sharedInputs.basic.p1Keys;
		int axis1 = k[1] - k[0];
		int axis2 = k[3] - k[2];
		double r_x = cosine * axis1 - sine * axis2;
		double r_y = cosine * axis2 + sine * axis1;

		// As a point of trivia, movement as a ghost is circular,
		// whereas movement as a player is square (diagonals are faster).
		// TODO: Move speed when dead is currently dependent on framerate, which is goofy leftover behavior
		ghostCenter[0] += r_x * 500;
		ghostCenter[1] += r_y * 500;
		ghostCenter[2] += 500 * (k[5] - sharedInputs.spaceBuf.latest);
		// So long as we're calling this method every frame (not every update),
		// there's no need to have `oldPos` different from `newPos` in the case where
		// we're dead, because we update ghostCenter continuously anyway.
		memcpy(oldPos, ghostCenter, sizeof(ghostCenter));
	}
	memcpy(newPos, ghostCenter, sizeof(ghostCenter));

	float up = 0, forward = 0;
	if (thirdPerson) {
		// Cast a ray to figure out where to put the camera...
		double ray[3];
		double pitchCos = cos(pitchRadians);
		ray[0] = -sine * pitchCos;
		ray[1] = cosine * pitchCos;
		ray[2] = -sin(pitchRadians);
		forward = -cameraCast(gs, ray, e, oldPos, newPos, interpRatio);
		if (forward < -80*PTS_PER_PX) forward = -80*PTS_PER_PX;
	}
	if (e) {
		ent *h = e->holder;
		// Checking the values of handlers to determine type is sort of frowned upon for
		// interactions between ents, but I think it's more fine for UI stuff
		char piloting = (h && (h->tick == seat_tick || h->tick == seat_tick_old));
		if (piloting) e = h; // This centers the camera on the seat
		if (thirdPerson) {
			// For now we don't raycast this case, since it would be at a funny angle.
			// If pilotable things become more common and this becomes a problem,
			// we can revisit how to think about this.
			if (piloting && h->numSliders >= 2) {
				up = getSlider(h, h->numSliders - 2);
				forward = -getSlider(h, h->numSliders - 1);
			}
		}
	}
	setupFrame(-pitchRadians, yawRadians, up, forward);
}

static float overlayColor[3] = {0.0, 0.5, 0.5};
static float grnColor[3] = {0.0, 1.0, 0.0};
static float bluColor[3] = {0.0, 0.0, 1.0};
static float redColor[3] = {1.0, 0.0, 0.0};
static float whiteColor[3] = {1.0, 1.0, 1.0};

static void drawTags(list<player> *ps, int32_t const *const oldPos, int32_t const *const newPos, float const ratio) {
	setupTags();
	range(i, ps->num) {
		if (i == myPlayer) continue;
		player &p = (*ps)[i];
		ent *e = p.entity;
		if (!e) continue;

		int32_t pos[3];
		range(dim, 3) {
			int32_t a = e->old[dim] - oldPos[dim];
			int32_t b = e->center[dim] - newPos[dim];
			pos[dim] = a + (b-a)*ratio;
		}
		pos[2] += -(e->radius[2] + 50); // 50 is arbitrary here

		drawTag(p.name, pos, 6000, whiteColor);
	}
}

static void drawOverlay(list<player> *ps) {
	setupText();

	const char* drawMe = chatBuffer;
	if (syncNeeded > MAX_AHEAD*2) drawMe = "CTRL+R TO SYNC";
	drawHudText(drawMe, 0, 0, 0.5, 0.5, 1, overlayColor);
	// The very last char of this buffer is always and forever '\0',
	// so while unsynchronized reads to data owned by another thread is bad this is probably actually okay
	if (typingLen >= 0) drawHudText(textBuffer, 0, 0, 0.5, 1.6, 1, overlayColor);
	// Todo: Draw boxes for pending messages in queue?

	float f1 = (double) inputs_nanos / STEP_NANOS;
	float f2 = (double) update_nanos / STEP_NANOS;
	float f3 = (double) follow_nanos / STEP_NANOS;
	// Draw frame timing bars
	drawHudRect(0, 1 - 1.0/64, f1, 1.0/64, bluColor);
	drawHudRect(f1, 1 - 1.0/64, f2, 1.0/64, redColor);
	drawHudRect(f1+f2, 1 - 1.0/64, f3, 1.0/64, grnColor);

	// Draw actual hud elements
	drawHud((*ps)[myPlayer].entity);
}

static void drawFps(long drawingNanos, long totalNanos) {
	char fps[7];
	snprintf(fps, 7, "%c%5.1lf", manualGlFinish ? '!' : ' ', (double) BILLION / totalNanos);
	drawHudText(fps, 1, 0, -6.5, 0.5, 1, overlayColor);
	// + 1 as a cheap way to avoid NAN
	float drawingRatio = (float)drawingNanos/(totalNanos+1);
	drawHudRect(0, 1 - 2.0/64, drawingRatio, 1.0/64, bluColor);
}

static void resetPlayer(gamestate *gs, int i) {
	player *p = &gs->players[i];
	// A really big number; new players should be considered as "dead a long time" so they spawn immediately
	p->reviveCounter = 100000;
	p->data = 0;
	p->entity = NULL;
	p->color = findColor(defaultColors[i % 6]);
	snprintf(p->name, NAME_BUF_LEN, "p%d", i);
	p->name[NAME_BUF_LEN-1] = '\0';
}

static void resetPlayers(gamestate *gs) {
	range(i, gs->players.num) resetPlayer(gs, i);
}

static void setupPlayers(gamestate *gs, int numPlayers) {
	gs->players.setMaxUp(numPlayers);
	gs->players.num = numPlayers;
	resetPlayers(gs);
}

static void saveGame(const char *name) {
	list<char> data;
	data.init();
	serialize(rootState, &data);
	writeFile(name, &data);
	data.destroy();
}

static void doInputs(ent *e, char const *data) {
	int32_t otherButtons = 0;
	if (data[0] &  1) pushBtn(e, 0);
	if (data[0] &  2) pushBtn(e, 1);
	if (data[0] &  4) pushBtn(e, 2); // LMB
	if (data[0] &  8) otherButtons |= PLAYER_BTN_ALTFIRE1; // Shift+LMB
	if (data[0] & 16) pushBtn(e, 3); // RMB
	if (data[0] & 32) otherButtons |= PLAYER_BTN_ALTFIRE2; // Shift+RMB
	int32_t axis[2] = {data[1], data[2]};
	player_setAxis(e, axis);
	int32_t look[3] = {data[3], data[4], data[5]};
	player_setLook(e, look);
	player_setButtons(e, otherButtons);
}

static void doDefaultInputs(ent *e) {
	range(i, 4) {
		if (getButton(e, i)) pushBtn(e, i);
	}
}

static char parseAddCmd(list<char> *out, const char *c) {
	out->add((char)BIN_CMD_ADD);
	range(i, 6) {
		int x;
		if (!getNum(&c, &x)) {
			printf("/add got %d args but needs, like, 7\n", i);
			return 1;
		}
		write32(out, x);
	}
	if (*c != ' ') {
		puts("/add needs a filename after all those numbers");
		return 1;
	}
	return readFile(c+1, out);
}

static char isCmd(const char* input, const char *cmd) {
	int l = strlen(cmd);
	return !strncmp(input, cmd, l) && (input[l] == ' ' || input[l] == '\0');
}

static void serializeControls(int32_t frame, list<char> *_out) {
	list<char> &out = *_out;

	out.setMaxUp(14); // 4 size + 4 frame + 6 input data
	out.num = 14;

	const char *const k = sharedInputs.basic.p1Keys;

	// Size will go in 0-3, we populate it in a minute
	*(int32_t*)(out.items + 4) = htonl(frame);
	// Other buttons also go here, once they exist; bitfield
	out[8] = sharedInputs.spaceBuf.pop() + 2*k[5] + 4*sharedInputs.lmbBuf.pop() + 16*sharedInputs.rmbBuf.pop();

	int axis1 = k[1] - k[0];
	int axis2 = k[3] - k[2];
	double angle = sharedInputs.basic.viewYaw;
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
		out[9] = round(r_x / divisor);
		out[10] = round(r_y / divisor);
	} else {
		out[9] = out[10] = 0;
	}
	double pitchRadians = sharedInputs.basic.viewPitch;
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
	out[11] = round(sine / divisor);
	out[12] = round(-cosine / divisor);
	out[13] = round(pitchSine / divisor);

	if (syncData.num) {
		out.add((char)BIN_CMD_SYNC);
		out.addAll(syncData);
		syncData.num = 0;
	} else if (doReload) {
		doReload = 0;
		out.add((char)BIN_CMD_ADD);
		// 6 numbers * 4 bytes
		range(i, 24) out.add(0);
		if (readFile(loadedFilename, &out)) {
			fputs("ERROR: Couldn't read file for reload\n", stderr);
		}
	} else if (outboundTextQueue.size()) {
		// This didn't use to be a queue. Now that it is, we could update this section
		// to process multiple items in the event that the first `n` are actually
		// locally-processed commands.
		char *const text = outboundTextQueue.pop().items;
		if (isCmd(text, "/help")) {
			/* TODO:
			This needs /data, /c,
			probably /save, /load, and keybinds,
			maybe some basic gameplay information,
			and info on the docs
			*/
			puts(
				"Complete documentation can be found in docs/edit_reference.html.\n"
				"If you're not editing, you probably only need:\n"
				"`/data` (to set your team),\n"
				"`/c` (to set your color),\n"
				"`/name` (to set your name),\n"
				"and `/save` and `/load`.\n"
				"Controls are WASD, Shift, Space, Tab, and the mouse.\n"
			);
		} else if (isCmd(text, "/incr")) {
			int32_t x;
			const char *c = text + 5;
			if (getNum(&c, &x)) wheelIncr = x;
			else printf("incr: %d\n", wheelIncr);
		} else if (isCmd(text, "/perf")) {
			performanceFrames = PERF_FRAMES_MAX;
			performanceIters = 10;
		} else if (isCmd(text, "/load")) {
			const char *file = "savegame";
			if (text[5]) file = text + 6;
			strcpy(loadedFilename, file);
			out.add((char)BIN_CMD_LOAD);
			readFile(file, &out);
		} else if (isCmd(text, "/loader")) {
			int32_t x;
			const char *c = text + 7;
			if (getNum(&c, &x)) isLoader = !!x;
			printf("loader: %s\n", isLoader ? "Y" : "N");
		} else if (isCmd(text, "/file")) {
			if (text[5]) strcpy(loadedFilename, text + 6);
			printf("Filename: %s\n", loadedFilename);
		} else if (!strncmp(text, "/import ", 8)) {
			out.add((char)BIN_CMD_IMPORT);
			readFile(text + 8, &out);
		} else if (isCmd(text, "/add")) {
			int initial = out.num;
			if (parseAddCmd(&out, text+4)) {
				out.num = initial;
			}
		} else if (isCmd(text, "/save") || isCmd(text, "/export") || isCmd(text, "/sync")) {
			// Commands that don't get roundtripped to the server.
			// Some - like /save and /export - we partly do this because they write to the filesystem
			// and we don't want to listen to the server about what we should write and where
			// (even if it's restricted to the `data/` directory and should be safe).
			// Also, other clients don't care if we issue these commands, so no reason to broadcast them.
			strcpy(loopbackCommandBuffer, text);
			// We could probably process them right here,
			// but it's less to worry about if the serialization out happens at the same
			// point in the tick cycle as the deserialization in.
		} else {
			// Else just put it in the outbound buffer.
			// /reseed is a special case and gets rewritten, however
			if (isCmd(text, "/reseed")) {
				timespec now;
				clock_gettime(CLOCK_MONOTONIC_RAW, &now);
				int sec_truncate = now.tv_sec;
				snprintf(text, TEXT_BUF_LEN, "/seed %d", sec_truncate);
			}
			int start = out.num;
			int len = strlen(text);
			out.num += len;
			out.setMaxUp(out.num);
			memcpy(out.items + start, text, len);
		}
	}

	*(int32_t*)out.items = htonl(out.num - 4);
}

static void updateResolution() {
	if (framebuffer.changed) {
		framebuffer.changed = 0;
		setDisplaySize(framebuffer.width, framebuffer.height);
	}
}

static void handleSharedInputs(int outboundFrame) {
	list<char> *out = &outboundData.add();
	if (out->max > 1000) out->setMax(100); // Todo maybe bring this in line with the limits in net2.cpp, just for consistency

	lock(sharedInputsMutex);

	serializeControls(outboundFrame, out);

	unlock(sharedInputsMutex);

	sendData(out->items, out->num);
}

static void mkHeroes(gamestate *gs) {
	int numPlayers = gs->players.num;
	range(i, numPlayers) {
		player *p = &gs->players[i];
		if (p->entity == NULL) {
			if (p->reviveCounter < 90) continue;
			p->entity = mkHero(gs, i, numPlayers);
			p->entity->color = p->color;
			p->reviveCounter = 0;
		}
	}
}

static void cleanupDeadHeroes(gamestate *gs) {
	int numPlayers = gs->players.num;
	range(i, numPlayers) {
		player *p = &gs->players[i];
		if (p->entity && p->entity->dead) {
			p->entity = NULL;
			if (i == myPlayer) thirdPerson = 1;
		}
	}
}

static int yawToAxis(int offset) {
	double viewYaw = activeInputs.basic.viewYaw;
	static int const directions[4] = {-2, 0, 1, -1};
	int directionIx = (viewYaw + 5*M_PI_4) / M_PI_2;
	return directions[(directionIx+offset) & 3];
}

void shareInputs() {
	lock(sharedInputsMutex);
	sharedInputs.basic = activeInputs.basic;
	sharedInputs.spaceBuf.consume(&activeInputs.spaceBuf);
	sharedInputs.lmbBuf.consume(&activeInputs.lmbBuf);
	sharedInputs.rmbBuf.consume(&activeInputs.rmbBuf);
	if (textSendInd) {
		textSendInd = 0;
		strcpy(outboundTextQueue.add().items, textBuffer);
	}
	unlock(sharedInputsMutex);
}

char handleKey(int code, char pressed) {
	int i;
	for (i = numKeys-1; i >= 0; i--) {
		if (p1Codes[i] == code) {
			if (i == 4) {
				activeInputs.spaceBuf.push(pressed);
			} else {
				activeInputs.basic.p1Keys[i] = pressed;
			}
			return true;
		}
	}
	if (pressed && code == GLFW_KEY_TAB) {
		if (ctrlPressed) {
			if (!textSendInd) {
				strcpy(textBuffer, "/edit");
				textSendInd = 1;
				typingLen = -1;
			}
		} else {
			thirdPerson ^= 1;
		}
	} else if (code == GLFW_KEY_LEFT_CONTROL || code == GLFW_KEY_RIGHT_CONTROL) {
		ctrlPressed = pressed;
		if (mouseDragMode) mouseDragMode = -1;
	} else if (code == GLFW_KEY_LEFT_ALT || code == GLFW_KEY_RIGHT_ALT) {
		altPressed = pressed;
		if (mouseDragMode) mouseDragMode = -1;
	} else if (code == GLFW_KEY_F3 && pressed) {
		if (ctrlPressed) manualGlFinish ^= 1;
		else showFps ^= 1;
	}
	return false;
}

static void handleMouseMove(double dx, double dy) {
	double viewYaw = activeInputs.basic.viewYaw;
	double viewPitch = activeInputs.basic.viewPitch;

	viewYaw += dx * mouseSensitivity;
	if (viewYaw >= M_PI) viewYaw -= 2*M_PI;
	else if (viewYaw < -M_PI) viewYaw += 2*M_PI;
	viewPitch += dy * mouseSensitivity;
	if (viewPitch > M_PI_2) viewPitch = M_PI_2;
	else if (viewPitch < -M_PI_2) viewPitch = -M_PI_2;

	activeInputs.basic.viewYaw = viewYaw;
	activeInputs.basic.viewPitch = viewPitch;
}

static void handleMouseDrag(double xpos, double ypos) {
	if (mouseDragMode == 1) {
		char negative;
		// Once they move far enough, we choose a direction,
		// and proceed to drag mode 2.
		if (fabs(xpos-mouseX) >= mouseDragSize) {
			mouseDragMode = 2;
			mouseDragInput = 0; // 0 -> X
			negative = xpos < mouseX;
		} else if (fabs(ypos-mouseY) >= mouseDragSize) {
			mouseDragMode = 2;
			mouseDragInput = 1; // 1 -> Y
			negative = ypos < mouseY;
		} else {
			// Still waiting in drag mode 1, nothing further to do.
			return;
		}

		// Ugly stuff for determining mouseDragOutput
		if (mouseDragInput) {
			if (activeInputs.basic.viewPitch > M_PI_4) {
				mouseDragOutput = yawToAxis(0);
			} else if (activeInputs.basic.viewPitch < -M_PI_4) {
				mouseDragOutput = yawToAxis(2);
			} else {
				mouseDragOutput = 2;
			}
		} else {
			mouseDragOutput = yawToAxis(-1);
		}
		// TODO Really this should be `shiftPressed`, since keys[5] is only *left* shift
		mouseDragFlip = negative ^ activeInputs.basic.p1Keys[5];
		if (mouseDragFlip) mouseDragOutput = ~mouseDragOutput;
	}
	// From here, we assume we're in drag mode 2.

	// While dragging the mouse it's possible to enqueue these messages
	// very very quickly. They are sent to the server at 15Hz, however,
	// so can wait and combine these messages into a larger drag amount
	// if we have pending messages. (We check two places because of how
	// the handoff between the game and input threads is orchestrated.)
	if (textSendInd || outboundTextQueue.size()) return;

	double o, x;
	if (mouseDragInput) o = mouseY, x = ypos;
	else o = mouseX, x = xpos;

	int steps = (x-o)/mouseDragSize;
	if (!steps) return;

	if (mouseDragInput) mouseY += steps * mouseDragSize;
	else mouseX += steps * mouseDragSize;

	if (mouseDragFlip) steps *= -1;
	snprintf(textBuffer, TEXT_BUF_LEN, "%s %d %d", ctrlPressed ? "/p" : "/s", mouseDragOutput, steps*wheelIncr);
	textSendInd = 1;
	typingLen = -1;
}

static void processLoopbackCommand(gamestate *gs) {
	const char* const c = loopbackCommandBuffer;
	int chars = strlen(c);
	if (isCmd(c, "/save")) {
		const char *name = "savegame";
		if (chars > 6) name = c + 6;
		printf("Saving game to %s\n", name);
		saveGame(name);
	} else if (isCmd(c, "/export")) {
		if (chars <= 8) {
			puts("/export requries a filename");
			return;
		}
		ent *me = gs->players[myPlayer].entity;
		edit_export(gs, me, c + 8);
	} else if (isCmd(c, "/sync")) {
		syncData.num = 0;
		serialize(rootState, &syncData);
	} else {
		printf("Unknown loopback command: %s\n", c);
	}
}

static char editCmds(gamestate *gs, ent *me, char verbose) {
#define cmd(s, x) if (isCmd(chatBuffer, s)) do {x; return 1;} while(0)
	cmd("/inside", edit_selectInside(gs, me, chatBuffer + 7));
	cmd("/held", edit_selectHeld(gs, me));
	cmd("/tree", edit_selectHeldRecursive(gs, me));
	cmd("/wires", edit_selectWires(gs, me));

	cmd("/weight", edit_m_weight(gs, me, chatBuffer + 7, verbose));
	cmd("/drawself", edit_m_drawself(gs, me, chatBuffer + 9, verbose));
	cmd("/friction", edit_m_friction(gs, me, chatBuffer + 9, verbose));
	cmd("/decor", edit_m_decor(gs, me));
	cmd("/paper", edit_m_paper(gs, me));
	cmd("/wood", edit_m_wood(gs, me));
	cmd("/stone", edit_m_stone(gs, me));
	cmd("/metal", edit_m_metal(gs, me));
	cmd("/wall", edit_m_wall(gs, me));
	cmd("/ghost", edit_m_ghost(gs, me));

	cmd("/dumb", edit_t_dumb(gs, me));
	cmd("/cursed", edit_t_cursed(gs, me));
	cmd("/fragile", edit_t_fragile(gs, me));
	if (tryOtherCommand(gs, me, chatBuffer+1)) return 1;
	cmd("/scoreboard", edit_m_t_scoreboard(gs, me));
	cmd("/veheye", edit_m_t_veh_eye(gs, me)); // TODO remove?

	cmd("/copy", edit_copy(gs, me));
	cmd("/flip", edit_flip(gs, me));
	cmd("/turn", edit_rotate(gs, me, verbose));
	cmd("/scale", edit_scale(gs, me, chatBuffer + 6, verbose));
	cmd("/scale!", edit_scale_force(gs, me, chatBuffer + 7, verbose));

	cmd("/pickup", edit_pickup(gs, me, chatBuffer + 7));
	cmd("/drop", edit_drop(gs, me));
	cmd("/wire", edit_wire(gs, me));
	cmd("/unwire", edit_unwire(gs, me));

	cmd("/factory", edit_factory(gs, me));

	cmd("/str", edit_string_get(chatBuffer+4, verbose));
	cmd("/setstr", edit_string_set(gs, chatBuffer+7, verbose));

	cmd("/b", edit_create(gs, me, chatBuffer + 2, verbose));
	cmd("/p", edit_push(gs, me, chatBuffer + 2));
	cmd("/center", edit_center(gs, me));
	cmd("/s", edit_stretch(gs, me, chatBuffer + 2, verbose));
	cmd("/d", edit_rm(gs, me));
	cmd("/slider", edit_slider(gs, me, chatBuffer + 7, verbose));
	cmd("/sl", edit_slider(gs, me, chatBuffer + 3, verbose));
	cmd("/hl", edit_highlight(gs, me));
	cmd("/m", if (verbose) edit_measure(gs, me));
	cmd("/i", if (verbose) edit_info(me));
#undef cmd
	return 0;
}

static void processCmd(gamestate *gs, player *p, char const *data, int chars, char isMe, char isReal) {
	if (chars && (*(unsigned char*)data == BIN_CMD_LOAD || *(unsigned char*)data == BIN_CMD_SYNC)) {
		if (!isReal) return;
		char isSync = *(unsigned char*)data == BIN_CMD_SYNC;

		if (isSync) {
			if (syncNeeded > MAX_AHEAD) syncNeeded = 0;
		} else {
			isLoader = isMe;
		}

		list<player> oldPlayers;
		oldPlayers.init(rootState->players);
		int numPlayers = rootState->players.num;
		int32_t seed = rootState->rand;
		doCleanup(rootState);
		// Can't make a new gamestate here (as we might be tempted to),
		// since stuff further up the call stack (like `doUpdate`) has a reference
		// to the existing `gamestate` pointer
		resetGamestate(rootState);
		setupPlayers(rootState, numPlayers);
		range(i, numPlayers) {
			player &newPlayer = rootState->players[i];
			strcpy(newPlayer.name, oldPlayers[i].name);
			newPlayer.color = oldPlayers[i].color;
			newPlayer.data = oldPlayers[i].data;
		}
		oldPlayers.destroy();
		rootState->rand = seed;

		list<const char> const fakeList = {.items=data+1, .num = chars-1, .max = chars-1};

		deserialize(rootState, &fakeList, isSync);
		return;
	}
	if (chars && *(unsigned char*)data == BIN_CMD_IMPORT) {
		if (!(gs->gamerules & RULE_EDIT)) {
			if (isReal && isMe) puts("/import only works with /rule 10 enabled");
			return;
		}

		list<const char> const fakeList = {.items=data+1, .num = chars-1, .max = chars-1};
		char addToBuffer = p->entity && getSlider(p->entity, PLAYER_EDIT_SLIDER);
		edit_import(gs, p->entity, &fakeList, addToBuffer);
		return;
	}
	if (chars >= 25 && *(unsigned char*)data == BIN_CMD_ADD) {
		list<const char> const fakeList = {.items=data+25, .num = chars-25, .max = chars-25};
		int32_t nums[6];
		range(i, 6) nums[i] = ntohl(*(int32_t*)(data+1+4*i));
		deserializeSelected(gs, &fakeList, nums, nums+3);
		return;
	}
	if (chars < TEXT_BUF_LEN) {
		memcpy(chatBuffer, data, chars);
		chatBuffer[chars] = '\0';
		char wasCommand = 1;
		if (isCmd(chatBuffer, "/data")) {
			const char* c = chatBuffer + 5;
			int32_t data;
			if (getNum(&c, &data)) {
				int32_t tmp;
				while (getNum(&c, &tmp)) {
					data = data * 0x100 + tmp;
				}
				p->data = data;
			} else {
				if (isMe && isReal) {
					printf("data: 0x%X\n", p->data);
				}
			}
		} else if (isCmd(chatBuffer, "/syncme")) {
			if (isReal && !isMe) syncNeeded = 1;
		} else if (isCmd(chatBuffer, "/_tree")) {
			if (isMe && isReal) {
				printTree(gs);
			}
		} else if (isCmd(chatBuffer, "/seed")) {
			int32_t input;
			const char *c = chatBuffer + 5;
			if (getNum(&c, &input)) {
				gs->rand = input;
			} else if (isMe && isReal) {
				printf("Seed is %d\n", gs->rand);
			}
		} else if (isCmd(chatBuffer, "/edit")) {
			ent *e = p->entity;
			if (e) {
				int edit = !getSlider(e, PLAYER_EDIT_SLIDER);
				// Reach in and tweak internal state to toggle edit mode
				if (!edit || (gs->gamerules & RULE_EDIT)) {
					uSlider(e, PLAYER_EDIT_SLIDER, edit);
					if (isReal && isMe) {
						printf("Your edit toolset is %s\n", edit ? "ON" : "OFF");
					}
				}
			}
		} else if (isCmd(chatBuffer, "/rule")) {
			if (chars >= 7) {
				int rule = atoi(chatBuffer+6);
				int32_t mask = 1 << rule;
				gs->gamerules ^= mask;
				if (isReal) {
					printf("Game rule %d is now %s\n", rule, (gs->gamerules & mask) ? "ON" : "OFF");
				}
			} else if (isMe && isReal) {
				char const * const * line = rulesHelp;
				puts(*line++);
				int32_t test = 1;
				while (*line && test > 0) {
					putchar((gs->gamerules & test) ? 'X' : '.');
					putchar(' ');
					puts(*line++);
					test *= 2;
				}
			}
		} else if (isCmd(chatBuffer, "/name")) {
			if (chars >= 6) {
				snprintf(p->name, NAME_BUF_LEN, "%s", chatBuffer + 6);
			} else if (isMe && isReal) {
				printf("Name is %s\n", p->name);
			}
		} else if (chars >= 6 && !strncmp(chatBuffer, "/c ", 3)) {
			int32_t color = edit_color(p->entity, chatBuffer + 3, !!(gs->gamerules & RULE_EDIT));
			if (color != -2) p->color = color;
		} else if ((gs->gamerules & RULE_EDIT) && chatBuffer[0] == '/') {
			wasCommand = editCmds(gs, p->entity, isMe && isReal);
		} else {
			wasCommand = 0;
		}
		if (wasCommand) chatBuffer[0] = '\0';
	} else {
		fputs("Incoming \"chat\" buffer was too long, ignoring\n", stderr);
	}
}

static void doWholeStep(gamestate *state, list<list<char>> const *_inputData, char isReal) {
	list<list<char>> const &inputData = *_inputData;
	int numPlayers = inputData.num;
	list<player> &players = state->players;
	players.setMaxUp(numPlayers);
	while (players.num < numPlayers) {
		resetPlayer(state, players.num++);
	}

	// As much as I'd like rules and such to reside in ents,
	// there'll probably always be a need for some global gamerules
	if (state->gamerules & EFFECT_CRUSH) doCrushtainer(state);
	if (state->gamerules & EFFECT_CRUSH_BIG) doBigCrushtainer(state);
	if (state->gamerules & EFFECT_LAVA) doLava(state);
	if (state->gamerules & EFFECT_BLOCKS) createDebris(state);
	if (state->gamerules & EFFECT_BOUNCE) doBouncetainer(state);

	range(i, numPlayers) {
		char isMe = i == myPlayer;
		list<char> const &data = inputData[i];

		ent *e = players[i].entity;
		if (e != NULL) {
			if (data.num >= 6) { // Really we should only need a non-zero amount of data, net2.cpp also checks that the size isn't too small
				doInputs(e, data.items);
			} else {
				doDefaultInputs(e);
			}
		} else {
			players[i].reviveCounter++;
		}
		// Some edit commands depend on the player's view direction,
		// so especially if they missed last frame we really want to
		// process the command *after* we process their inputs.
		if (data.num > 6 && (isMe || isReal)) {
			processCmd(state, &players[i], data.items + 6, data.num - 6, isMe, isReal);
		}
	}
	if (isReal && *loopbackCommandBuffer) {
		processLoopbackCommand(state);
		*loopbackCommandBuffer = '\0';
	}

	doUpdates(state);

	// Hero creation happens here, right after all the ticks.
	// This means all the creation stuff will be flushed out before
	// the new ents process any ticks themselves, and they'll see
	// correctly initialized state.
	if (state->gamerules & EFFECT_SPAWN) mkHeroes(state);

	// Flushes stuff like deaths and pickups, but not velocity (yet).
	prepPhysics(state);

	// Gravity is applied at a very specific time in the cycle.
	// We want ents that are resting on a surface to see the same velocity as the surface,
	//   so it has to be between tick handlers and velocity flush / collisions.
	// More specifically, flushing pickups / drops changes which ents are rootEnts (and are eligible for gravity),
	//   so it has to be between pickup flush and velocity flush.
	// Things also get weird if changes to weightlessness take effect on a different frame than changes in collisions,
	//   so it has to be after flushing typeMask / collideMask.
	// This doesn't leave a lot of space for where gravity has to go.
	if (state->gamerules & EFFECT_GRAV) doGravity(state);

	doPhysics(state); 

	// This happens just before finishing the step so we can be 100% sure whether the player's entity is dead or not
	cleanupDeadHeroes(state);
	finishStep(state);
}

void showMessage(gamestate const * const gs, char const * const msg) {
	// Anybody can request a message, and we don't hold it against them.
	// However, the message that is drawn to the screen isn't tied to a gamestate -
	// which means that if some gamestate gets rolled back (as happens very often),
	// any message it requested is still going to be visible even if that doesn't make sense.
	// As a result, we don't show messages until they happen on the "real" timeline.
	if (gs != rootState) return;
	int len = strnlen(msg, TEXT_BUF_LEN-1);
	// We're weird about this partly because of overflows,
	// and partly so if the draw thread catches us at a bad time
	// it writes a minimum of nonsense to the screen
	// (since the new terminator is put in first).
	chatBuffer[len] = '\0';
	memcpy(chatBuffer, msg, len);
}

void requestReload(gamestate const * const gs) {
	if (gs != rootState) return;
	if (isLoader) doReload = 1;
}

static void newPhantom(gamestate *gs) {
	phantomState = dup(gs);
}

static void insertOutbound(list<char> *dest, list<char> *src) {
	// The trouble is that the data we sent out includes some header info,
	// like the frame and size. By the time we have our nice list of
	// who's doing what this frame, those two fields have been removed.
	// We have to do the same here. Fortunately, it's assumed these
	// lists are stricly read-only, so it's okay to make a "fake" list here!
	dest->num = src->num - 8;
	dest->max = src->max - 8; // Does this even matter if RO?
	dest->items = src->items + 8;
}

static void* gameThreadFunc(void *startFramePtr) {
	long performanceTotal = 0;
	int32_t outboundFrame = *(int32_t*)startFramePtr;

	list<list<char>> playerDatas;
	lock(netMutex);
	// `playerDatas` holds our current guesses for each player's inputs.
	// It is a list of "weak references" to lists, just in that it isn't
	// reponsible for freeing any of them. We do have to be mindful that
	// they don't go out of scope, however. As a result, there is always
	// at least one finalized frame. The first finalized frame is set up
	// with some dummy values for just this purpose, during net2_init().
	playerDatas.init(frameData.peek(0));
	unlock(netMutex);

	long destNanos = nowNanos() - STEP_NANOS*3; // Start out a bit behind

	while (globalRunning) {
		long sleepNanos = destNanos - nowNanos();
		if (sleepNanos > 999999999) {
			puts("WARN - Tried to wait for more than a second???");
			// No idea why this would ever come up, but also it runs afoul of the spec to
			// request an entire second or more in the tv_nsec field.
			sleepNanos = 999999999;
		}
		char behindClock = 0;
		if (sleepNanos > 0) {
			timespec t;
			t.tv_sec = 0;
			t.tv_nsec = sleepNanos;
			if (nanosleep(&t, NULL)) continue; // Something happened, we don't really care what, do it over again
		} else {
			behindClock = 1;
		}

		clock_gettime(CLOCK_MONOTONIC_RAW, &t1);

		// Wake up and send player inputs
		handleSharedInputs(outboundFrame);
		outboundFrame = (outboundFrame + 1) % FRAME_ID_MAX;

		clock_gettime(CLOCK_MONOTONIC_RAW, &t2);

		lock(netMutex);
		// Do one more step of simulation on the phantom state and send to render thread.
		// We won't have to re-simulate every frame, so if we waited until after incorporating
		// the latest data we'd have extra variability in when frames get to the render thread.
		// Also, this keeps the latency between the keyboard and the screen slightly lower.
		{
			int outboundSize = outboundData.size();
			insertOutbound(&playerDatas[myPlayer], &outboundData.peek(outboundSize - 1));
			// There's always one leftover finalized frame, so we adjust by one here (or rather, don't adjust by one when normally we should, for index vs size)
			if (outboundSize < frameData.size()) {
				// This scenario is a little unlikely, as it means we've received data from at least one other client
				// for a frame that we just sent our data for. Clients don't like sending their data earlier than necessary
				// (or rather, they only simulate far enough ahead to get their data in on time).
				list<char> *netInputs = frameData.peek(outboundSize).items;
				range(i, playerDatas.num) {
					if (netInputs[i].num) playerDatas[i] = netInputs[i];
					// It's also possible that the netInputs we're reading are actually from a finalized frame,
					// in which case it would be more accurate to completely reset `playerDatas` to those values
					// and ignore `outboundData` completely (since finalized frames are authoritative).
					// However, that's even more unlikely - it means our client is way behind -
					// and I don't want to bother adding complexity for that case.
				}
			}
			doWholeStep(phantomState, &playerDatas, 0);

			gamestate *disposeMe = NULL;

			// Doing this outside the mutex probably doesn't matter much,
			// but waiting for the lock could be a bit inconsistent,
			// so this is marginally better I think
			long now = nowNanos();
			lock(renderMutex);
			if (renderData.dropoff) {
				disposeMe = renderData.dropoff;
				renderData.dropoff = NULL;
			} else if (renderData.pickup) {
				disposeMe = renderData.pickup;
			}
			renderData.pickup = phantomState;
			renderData.nanos = now;
			unlock(renderMutex);

			if (disposeMe) {
				doCleanup(disposeMe);
				free(disposeMe);
			}
		}

		clock_gettime(CLOCK_MONOTONIC_RAW, &t3);

		// Now we can consider the prospect of re-simulating from the rootState
		if (finalizedFrames > 1) {
			int toAdvance = finalizedFrames - 1;
			int outboundSize = outboundData.size();
			if (outboundSize < toAdvance) toAdvance = outboundSize; // Unlikely
			range(i, toAdvance) {
				// Bookkeeping for automatic sync of new clients.
				// We have to wait a bit to make sure they're up-to-date with any early-submitted inputs.
				// This part is rare.
				if (syncNeeded && syncNeeded <= 2*MAX_AHEAD) {
					syncNeeded++;
					if (syncNeeded == MAX_AHEAD && isLoader && !*loopbackCommandBuffer) {
						strcpy(loopbackCommandBuffer, "/sync");
					}
				}
				// An "official" step, all clients expect to agree on the state here
				doWholeStep(rootState, &frameData.peek(i+1), 1);
			}
			frameData.multipop(toAdvance);
			outboundData.multipop(toAdvance);
			outboundSize -= toAdvance;
			finalizedFrames -= toAdvance;

			char clockOk = behindClock; // If we're behind the clock, then don't blame issues on the clock; we just need to catch up
			playerDatas.num = 0;
			playerDatas.addAll(frameData.peek(0));
			newPhantom(rootState);
			int frameDataSize = frameData.size();
			range(outboundIx, outboundSize) {
				insertOutbound(&playerDatas[myPlayer], &outboundData.peek(outboundIx));
				if (outboundIx+1 < frameDataSize) {
					list<char> *netInputs = frameData.peek(outboundIx+1).items;
					range(i, playerDatas.num) {
						if (netInputs[i].num) {
							playerDatas[i] = netInputs[i];
							// If the server has any input from us ahead of time, we're going fast enough.
							if (i == myPlayer) clockOk = 1;
						}
					}
				}
				doWholeStep(phantomState, &playerDatas, 0);
			}
			if (!clockOk) fasterFrames = PENALTY_FRAMES;
		} else {
			if (outboundData.size() >= MAX_AHEAD) {
				puts("Game thread: Server is way behind, going to sleep until we hear something");
				asleep = 1;
				while (globalRunning && finalizedFrames <= 1) {
					wait(netCond, netMutex);
				}
				asleep = 0;
				puts("Game thread: Waking up");
				destNanos = nowNanos();
			}
			// We still need to make a copy since the rendering stuff is probably working with
			// the current phantomState.
			newPhantom(phantomState);
		}

		if (fasterFrames) {
			fasterFrames--;
			destNanos += FASTER_NANOS;
		} else {
			destNanos += STEP_NANOS;
		}

		unlock(netMutex);

		clock_gettime(CLOCK_MONOTONIC_RAW, &t4);
		{
			inputs_nanos = BILLION * (t2.tv_sec - t1.tv_sec) + (t2.tv_nsec - t1.tv_nsec);
			update_nanos = BILLION * (t3.tv_sec - t2.tv_sec) + (t3.tv_nsec - t2.tv_nsec);
			follow_nanos = BILLION * (t4.tv_sec - t3.tv_sec) + (t4.tv_nsec - t3.tv_nsec);
			if (performanceFrames) {
				performanceFrames--;
				performanceTotal += follow_nanos;
				if (!performanceFrames) {
					printf("perf: %ld\n", performanceTotal);
					performanceTotal = 0;
					performanceIters--;
					if (performanceIters) performanceFrames = PERF_FRAMES_MAX;
					else puts("perf done");
				}
			}
		}
	}
	playerDatas.destroy();

	return NULL;
}

static char handleCtrlBind(int key) {
	char *const t = textBuffer;
	if (key == GLFW_KEY_R) {
		strcpy(t, "/sync");
	} else if (key == GLFW_KEY_E) {
		strcpy(t, "/p");
	} else if (key == GLFW_KEY_Q) {
		strcpy(t, "/save");
	} else if (key == GLFW_KEY_L) {
		strcpy(t, "/load");
	} else if (key == GLFW_KEY_C) {
		strcpy(t, "/copy");
	} else if (key == GLFW_KEY_F) {
		strcpy(t, "/hl");
	} else if (key == GLFW_KEY_B) {
		strcpy(t, "/b 400 400 400");
	} else if (key == GLFW_KEY_I) {
		// TODO Really this should be `shiftPressed`, since keys[5] is only *left* shift
		if (activeInputs.basic.p1Keys[5]) {
			strcpy(t, "/inside 2");
		} else {
			strcpy(t, "/inside");
		}
	} else {
		return 0;
	}
	textSendInd = 1;
	typingLen = -1;
	return 1;
}

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
	if (typingLen >= 0) {
		if (action == GLFW_PRESS) {
			if (key == GLFW_KEY_ESCAPE) {
				typingLen = -1;
			} else if (key == GLFW_KEY_BACKSPACE && typingLen) {
				textBuffer[typingLen] = '\0';
				textBuffer[typingLen-1] = '_';
				typingLen--;
			} else if (key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER) {
				textBuffer[typingLen] = '\0';
				textSendInd = 1;
				typingLen = -1;
			}
		}
		// Typing mode primarily handles char input, not key input.
		// Nothing else is handled here.
		return;
	}

	if (action == GLFW_PRESS) {
		if (ctrlPressed && handleCtrlBind(key)) return;
		if (key == GLFW_KEY_ESCAPE) {
			glfwSetInputMode(display, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
			mouseGrabbed = 0;
		} else {
			handleKey(key, 1);
		}
	} else if (action == GLFW_RELEASE) {
		handleKey(key, 0);
	} else if (action == GLFW_REPEAT) {
		if (ctrlPressed) handleCtrlBind(key);
	}
}

static void character_callback(GLFWwindow* window, unsigned int c) {
	if (typingLen < 0) {
		// Maybe we start typing, but nothing else
		if (!textSendInd) {
			char *const t = textBuffer;
			if (c == 't') {
				t[1] = '\0';
				t[0] = '_';
				typingLen = 0;
			} else if (c == '/') {
				t[2] = '\0';
				t[1] = '_';
				t[0] = '/';
				typingLen = 1;
			}
		}
		return;
	}

	char *const t = textBuffer;
	// `c` is the unicode codepoint
	if (c >= 0x20 && c <= 0xFE && typingLen+2 < TEXT_BUF_LEN) {
		t[typingLen+2] = '\0';
		t[typingLen+1] = '_';
		t[typingLen] = c;
		typingLen++;
	}
}

static void cursor_position_callback(GLFWwindow* window, double xpos, double ypos) {
	if (mouseGrabbed) {
		if (mouseDragMode == 0) {
			handleMouseMove(xpos - mouseX, ypos - mouseY);
		} else if (mouseDragMode == -1) {
			// This state just eats one frame of captured mouse input
			// so your view doesn't jump. I could do the same with
			// an extra set of x/y variables, but that sounds like hassle.
			mouseDragMode = 0;
		} else {
			handleMouseDrag(xpos, ypos);
			// return early, do not update mouseX / mouseY with default logic
			return;
		}
		// GLFW has glfwSetCursorPos,
		// but the docs are very adamant that you not mess with that when you're
		// locking the cursor. I suppose it's unlikely that we'll reach the limits
		// on where a double can't represent a single unit of change, anyway.
	}
	mouseX = xpos;
	mouseY = ypos;
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
	if (!mouseGrabbed) {
		if (action == GLFW_PRESS) {
			glfwSetInputMode(display, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
			mouseGrabbed = 1;
		}
		return;
	}
	char press = (action == GLFW_PRESS);
	if (button == GLFW_MOUSE_BUTTON_LEFT) {

		// Some checks related to mouseDragMode.
		// Specific structure of these conditions is a probably pointless optimization.
		if ((ctrlPressed || altPressed) && press) {
			mouseDragMode = 1;
			return;
		}
		if (mouseDragMode && !press) mouseDragMode = -1;

		activeInputs.lmbBuf.push(press*(1+activeInputs.basic.p1Keys[5]));
	} else if (button == GLFW_MOUSE_BUTTON_RIGHT) {
		activeInputs.rmbBuf.push(press*(1+activeInputs.basic.p1Keys[5]));
	}
}

void scroll_callback(GLFWwindow* window, double _x, double _y) {
	int yoffset = (int)-_y;
	if (yoffset && !textSendInd && (ctrlPressed || altPressed)) {
		int axis;
		if (activeInputs.basic.viewPitch > M_PI_4) {
			axis = -3;
		} else if (activeInputs.basic.viewPitch < -M_PI_4) {
			axis = 2;
		} else {
			axis = yawToAxis(0);
		}
		if ((yoffset < 0) ^ activeInputs.basic.p1Keys[5]) {
			axis = ~axis;
			yoffset = -yoffset;
		}
		snprintf(
			textBuffer,
			TEXT_BUF_LEN,
			"/%s %d %d",
			ctrlPressed ? "p" : "s",
			axis,
			yoffset * wheelIncr
		);
		textSendInd = 1;
		typingLen = -1;
	}
}

static void window_focus_callback(GLFWwindow* window, int focused) {
	if (!focused) {
		glfwSetInputMode(display, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
		mouseGrabbed = 0;
	}
}

static void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
	// Usually I avoid locking mutexes directly in callbacks,
	// since they could be called a lot,
	// but this one's quite rare
	lock(renderMutex);
	framebuffer.width = width;
	framebuffer.height = height;
	framebuffer.changed = 1;
	unlock(renderMutex);
}

static void checkRenderData() {
	lock(renderMutex);
	if (renderData.pickup) {
		renderData.dropoff = renderedState;
		renderedState = renderData.pickup;
		renderData.pickup = NULL;
		renderStartNanos = renderData.nanos;
	}
	updateResolution();
	unlock(renderMutex);
}

static void* renderThreadFunc(void *_arg) {
	glfwMakeContextCurrent(display);
	// Explicitly request v-sync;
	// otherwise GLFW leaves it up to the driver default
	glfwSwapInterval(1);
	long drawingNanos = 0;
	long totalNanos = 0;
	long time0 = nowNanos();
	while (globalRunning) {
		checkRenderData();

		// Todo would this be better with STEP_NANOS, FASTER_NANOS, or something in between (like the idealized server frame nanos)?
		float interpRatio = (float)(time0 - renderStartNanos) / STEP_NANOS;
		if (interpRatio > 1.1) interpRatio = 1.1; // Ideally it would be somewhere in (0, 1]

		int32_t oldPos[3], newPos[3];
		list<player> *players = &renderedState->players;
		outerSetupFrame(players, renderedState, oldPos, newPos, interpRatio);

		ent *root = (*players)[myPlayer].entity;
		if (root) root = root->holdRoot;
		doDrawing(renderedState, root, thirdPerson, oldPos, newPos, interpRatio);

		drawTags(players, oldPos, newPos, interpRatio);
		drawOverlay(players);

		if (showFps) {
			drawFps(drawingNanos, totalNanos);
		}
		long time1 = nowNanos();

		glfwSwapBuffers(display);
		if (manualGlFinish) {
			glFinish();
		}
		long time2 = nowNanos();

		drawingNanos = time1-time0;
		totalNanos = time2-time0;
		time0 = time2;

		/*
		// Usually we spend most of the frame between `time1` and `time2`.
		// This is expected since vsync is on. However, sometimes we'll spend
		// most of 2 frames there, for no discernable reason.
		if (totalNanos > 20'000'000) {
			printf("Frame nanos = %ld (drawing for %ld)\n", totalNanos, drawingNanos);
		}
		*/
	}
	return NULL;
}

static void* inputThreadFunc(void *_arg) {
	glfwSetKeyCallback(display, key_callback);
	glfwSetCharCallback(display, character_callback);
	glfwSetCursorPosCallback(display, cursor_position_callback);
	glfwSetMouseButtonCallback(display, mouse_button_callback);
	glfwSetScrollCallback(display, scroll_callback);
	glfwSetWindowFocusCallback(display, window_focus_callback);
	glfwSetFramebufferSizeCallback(display, framebuffer_size_callback);

	timespec t;
	t.tv_sec = 0;
	t.tv_nsec = 10'000'000;

	while (!glfwWindowShouldClose(display)) {

		// glfwWaitEvents() should be exactly what we want, but in practice it would occasionally be
		// way too slow for unknown reasons (looks like dropped frames, but it's not a graphics issue;
		// the mouse data just doesn't update for few frames or so).
		// Haven't investigated further, but for now we just poll at 100 Hz,
		// which should be fast enough for a human and slow enough for a computer.
		glfwPollEvents();
		nanosleep(&t, NULL);

		shareInputs();
	}
	return NULL;
}

static char waitForThread(pthread_t thread) {
	timespec t;
	if (clock_gettime(CLOCK_REALTIME, &t) == -1) {
		printf("clock_gettime has errno %d, so can't wait.\n", errno);
		return 0;
	}
	// 1 second should be enough time here, we're not doing anything
	// really intense in these threads. Most likely reason we'd miss
	// this window would be if we're blocked on a socket operation,
	// which is rare enough I'm fine just killing the thread in that
	// case.
	t.tv_sec += 1;
	int ret = pthread_timedjoin_np(thread, NULL, &t);
	if (!ret) {
		return 1;
	}
	if (ret == ETIMEDOUT) {
		puts("Waited too long.");
	} else {
		printf("pthread_timedjoin_np returned error %d.\n", ret);
	}
	return 0;
}

static void cleanupThread(pthread_t thread, char const * const descr) {
	printf("Waiting for %s thread...\n", descr);
	if (waitForThread(thread)) {
		puts("Done.");
		return;
	}

	printf("Killing %s thread instead.\n", descr);
	pthread_cancel(thread);
	puts("Waiting for killed thread to complete...");
	if (waitForThread(thread)) {
		puts("Done.");
	} else {
		puts("Guess we're going to just move on then.");
	}
}

const char* usage = "Arguments: server_addr [port]\n\tport default is 15000";

int main(int argc, char **argv) {
	// Parse args
	if (argc < 2 || argc > 3) {
		puts(usage);
		return 1;
	}
	char *srvAddr = argv[1];
	const char *port = "15000";
	if (argc > 2) {
		port = argv[2];
		printf("Using specified port of %s\n", port);
	}

	velbox_init();
	rootState = mkGamestate();

	init_registrar();
	init_entFuncs();

	if (!glfwInit()) {
		fputs("Couldn't init GLFW\n", stderr);
		return 1;
	}
	// glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
	display = glfwCreateWindow(1000, 700, "Bittoss", NULL, NULL);
	if (!display) {
		fputs("Couldn't create our display\n", stderr);
		return 1;
	}

	{
		// Framebuffer size is not guaranteed to be equal to window size
		int fbWidth, fbHeight;
		glfwGetFramebufferSize(display, &fbWidth, &fbHeight);
		setDisplaySize(fbWidth, fbHeight);
	}

	glfwMakeContextCurrent(display);
	initGraphics(); // OpenGL Setup (calls initFont())
	glfwMakeContextCurrent(NULL); // Give up control so other thread can take it

	gamestring_init();
	file_init();
	config_init(); // "config" should be after "file"
	colors_init();
	edit_init();
	initMods(); //Set up modules
	ent_init();
	hud_init();
	outboundTextQueue.init();
	outboundData.init();
	syncData.init();
	seat_tick = tickHandlers.get(TICK_SEAT);
	seat_tick_old = tickHandlers.get(TICK_SEAT_OLD);

	// Other general game setup, including networking
	puts("Connecting to host...");
	if (initSocket(srvAddr, port)) return 1;
	puts("Done.");
	puts("Awaiting setup info...");
	char initNetData[7];
	if (readData(initNetData, 7)) {
		puts("Error, aborting!");
		return 1;
	}
	if (initNetData[0] != (char)0x81) {
		printf("Bad initial byte %hhd, aborting!\n", initNetData[0]);
		return 1;
	}
	myPlayer = initNetData[1];
	int numPlayers = initNetData[2];
	int32_t startFrame = ntohl(*(int32_t*)(initNetData+3));
	printf("Done, I am client #%d out of %d\n", myPlayer, numPlayers);
	setupPlayers(rootState, numPlayers);
	isLoader = (numPlayers == 1);

	net2_init(numPlayers);

	// Map loading
	mkMap(rootState);
	// Loading from file is one thing, but programatically defined maps may need a `flush` so things aren't weird the first time around.
	// `flush` isn't exposed since this is the one time we need it, and it's not like it really matters,
	// so we just do the second half of a normal step to get the same effect.
	prepPhysics(rootState);
	doPhysics(rootState);
	finishStep(rootState);
	// init phantomState
	newPhantom(rootState);
	renderedState = dup(rootState); // Give us something to render, so we can skip null checks

	// Setup text buffers.
	// We make it so the player automatically sends the "syncme" command on their first frame,
	// which just displays a message to anybody else already in-game
	textBuffer[TEXT_BUF_LEN-1] = '\0';
	strcpy(textBuffer, "/syncme");
	textSendInd = 1;

	chatBuffer[0] = chatBuffer[TEXT_BUF_LEN-1] = '\0';
	loadedFilename[0] = loadedFilename[TEXT_BUF_LEN-1] = '\0';
	loopbackCommandBuffer[0] = '\0';

	// set up timing stuff
	{
		timespec now;
		clock_gettime(CLOCK_MONOTONIC_RAW, &now);
		startSec = now.tv_sec;
	}

	//Main loop
	pthread_t gameThread;
	pthread_t netThread;
	pthread_t renderThread;
	{
		int ret = pthread_create(&gameThread, NULL, gameThreadFunc, &startFrame);
		if (ret) {
			printf("pthread_create returned %d for gameThread\n", ret);
			return 1;
		}
		ret = pthread_create(&netThread, NULL, netThreadFunc, &startFrame);
		if (ret) {
			printf("pthread_create returned %d for netThread\n", ret);
			pthread_cancel(gameThread); // No idea if this works, this is a failure case anyway
			return 1;
		}
		ret = pthread_create(&renderThread, NULL, renderThreadFunc, NULL);
		if (ret) {
			printf("pthread_create returned %d for renderThread\n", ret);
			pthread_cancel(gameThread); // No idea if this works, this is a failure case anyway
			pthread_cancel(netThread);
			return 1;
		}
	}
	// Main thread lives in here until the program exits
	inputThreadFunc(NULL);

	// Generally signal that there's a shutdown in progress
	// (if the inputThread isn't the main thread we'd want to set the GLFW "should exit" flag and push an empty event through)
	globalRunning = 0;
	// Game thread could potentially be waiting on this condition
	lock(netMutex);
	signal(netCond);
	unlock(netMutex);

	puts("Writing config file...");
	config_write();
	puts("Done.");
	puts("Beginning cleanup.");
	closeSocket();
	cleanupThread(netThread, "network");
	cleanupThread(gameThread, "game");
	cleanupThread(renderThread, "render");
	puts("Cleaning up game objects...");
	int32_t rand = rootState->rand;
	doCleanup(rootState);
	free(rootState);
	doCleanup(phantomState);
	free(phantomState);
	if (renderData.pickup) renderData.dropoff = renderData.pickup;
	if (renderData.dropoff) {
		doCleanup(renderData.dropoff);
		free(renderData.dropoff);
	}
	puts("Done.");
	puts("Cleaning up simple interal components...");
	syncData.destroy();
	outboundData.destroy();
	outboundTextQueue.destroy();
	net2_destroy();
	destroyFont();
	destroy_registrar();
	hud_destroy();
	ent_destroy();
	edit_destroy();
	velbox_destroy();
	colors_destroy();
	config_destroy();
	file_destroy();
	gamestring_destroy();
	puts("Done.");
	puts("Cleaning up GLFW...");
	glfwTerminate();
	puts("Done.");
	char const * x[17] = {"goodbye", "so long", "thanks for playing", "peace", "dude", "man", "wow", "adios", "but you owe me", "heck yeah", "I think", "toodles", "and how", "all done", "hooray", "for now", "allegedly"};
	printf("Cleanup complete, %s!\n", x[rand%17]);
	return 0;
}
