#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <GLFW/glfw3.h>
#include <arpa/inet.h>
#include <pthread.h>

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
#include "edit.h"
#include "file.h"

#include "entFuncs.h"
#include "entUpdaters.h"
#include "entGetters.h"

#include "modules/player.h"

#define numKeys 6

char globalRunning = 1;

static list<player> players, phantomPlayers;
static int myPlayer;

static int32_t defaultColors[8] = {0xFFAA00, 0x00AA00, 0xFF0055, 0xFFFF00, 0xAA0000, 0x00FFAA, 0xFF5555, 0x55FF55};

int p1Codes[numKeys] = {GLFW_KEY_A, GLFW_KEY_D, GLFW_KEY_W, GLFW_KEY_S, GLFW_KEY_SPACE, GLFW_KEY_LEFT_SHIFT};

#define TEXT_BUF_LEN 200
struct inputs {
	struct {
		char p1Keys[numKeys];
		char mouseBtnDown = 0;
		char mouseSecondaryDown = 0;
		double viewYaw = 0;
		double viewPitch = 0;
	} basic;
	char textBuffer[TEXT_BUF_LEN];
	char sendInd;
};
inputs activeInputs = {0}, sharedInputs = {0};

static int typingLen = -1;

static char chatBuffer[TEXT_BUF_LEN];
static char loopbackCommandBuffer[TEXT_BUF_LEN];

static gamestate *rootState, *phantomState;

static GLFWwindow *display;

#define mouseSensitivity 0.0025

static double mouseX = 0;
static double mouseY = 0;
static char thirdPerson = 1;
static char ctrlPressed = 0;
static int wheelIncr = 100;
static char mouseGrabbed = 0;

static tick_t seat_tick;


static int phys_micros = 0;
static int draw_micros = 0;
static int flip_micros = 0;
static struct timespec t1, t2, t3, t4;
static int performanceFrames = 0;

#define frame_nanos 33333333
// TODO I think this define is outdated, not sure what all even uses FRAMERATE anymore...
#define micros_per_frame (1000000 / FRAMERATE)
// How many frames of data are kept to help guess the server clock
#define frame_time_num 90
// How many frames have to agree that we're early (or late) before we start adjusting our clock
#define frame_time_majority 60
// If serverLead becomes negative, the server is late with its batch of frame data.
// It also means we'll send our data before we got the request for it, since it's probably just network lag.
// However, if the server gets far enough behind, we'll wait until we hear from it again
// ("far enough" = missing_server_factor * latency)
#define missing_server_factor -1
pthread_mutex_t timingMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t timingCond = PTHREAD_COND_INITIALIZER;
static time_t startSec;
static long frameTimes[frame_time_num];
static int frameTimeIx = 0;
static long medianTime;
static int serverLead = -1; // Starts at -1 since it hasn't provided our first frame yet (and we're mad about it)
static int latency;
static char startFrame;
#define FRAME_ID_MAX 128

#define BIN_CMD_LOAD 128
#define BIN_CMD_IMPORT 129
queue<list<char>> frameData;
queue<list<char>> outboundData;
list<char> syncData; // Temporary buffer for savegame data, for "/sync" command
char syncNeeded = 0;
pthread_mutex_t outboundMutex = PTHREAD_MUTEX_INITIALIZER;

#define lock(mtx) if (int __ret = pthread_mutex_lock(&mtx)) printf("Mutex lock failed with code %d\n", __ret)
#define unlock(mtx) if (int __ret = pthread_mutex_unlock(&mtx)) printf("Mutex unlock failed with code %d\n", __ret)
#define wait(cond, mtx) if (int __ret = pthread_cond_wait(&cond, &mtx)) printf("Mutex cond wait failed with code %d\n", __ret)
#define signal(cond) if (int __ret = pthread_cond_signal(&cond)) printf("Mutex cond signal failed with code %d\n", __ret)

static void outerSetupFrame(list<player> *ps) {
	ent *e = (*ps)[myPlayer].entity;
	float up = 0, forward = 0;
	if (e) {
		ent *h = e->holder;
		// Checking the values of handlers to determine type is sort of frowned upon for
		// interactions between ents, but I think it's more fine for UI stuff
		char piloting = (h && h->tick == seat_tick);
		if (piloting) e = h; // This centers the camera on the seat
		if (thirdPerson) {
			if (piloting) {
				up = getSlider(h, 0);
				forward = -getSlider(h, 1);
			} else {
				up = 32*PTS_PER_PX;
				forward = -64*PTS_PER_PX;
			}
		}
	}
	// It's a different thread that writes the pitch/yaw values, and I just realized access
	// has been unsynchronized for a while. It's possible that `double` writes are effectively
	// atomic on modern processors, but whatever the case:
	// A) Nobody's complained about weirdness here
	// B) If there was weirdness it would just manifest as weird visual frames, not a crash or desync
	setupFrame(-activeInputs.basic.viewPitch, activeInputs.basic.viewYaw, up, forward);
	if (e) {
		frameOffset[0] = -e->center[0];
		frameOffset[1] = -e->center[1];
		frameOffset[2] = -e->center[2];
	} else {
		frameOffset[0] = 0;
		frameOffset[1] = -15000;
		frameOffset[2] = 500*PTS_PER_PX;
	}
}

static float hudColor[3] = {0.0, 0.5, 0.5};
static float grnColor[3] = {0.0, 1.0, 0.0};
static float bluColor[3] = {0.0, 0.0, 1.0};
static float redColor[3] = {1.0, 0.0, 0.0};
static void drawHud(list<player> *ps) {
	setupText();
	const char* drawMe = syncNeeded ? "CTRL+R TO SYNC" : chatBuffer;
	drawHudText(drawMe, 1, 1, 1, hudColor);
	// The very last char of this buffer is always and forever '\0',
	// so while unsynchronized reads to data owned by another thread is bad this is probably actually okay
	if (typingLen >= 0) drawHudText(activeInputs.textBuffer, 1, 3, 1, hudColor);

	float f1 = (double) draw_micros / micros_per_frame;
	float f2 = (double) flip_micros / micros_per_frame;
	float f3 = (double) phys_micros / micros_per_frame;
	// Draw frame timing bars
	drawHudRect(0, 1 - 1.0/64, f1, 1.0/64, bluColor);
	drawHudRect(f1, 1 - 1.0/64, f2, 1.0/64, redColor);
	drawHudRect(f1+f2, 1 - 1.0/64, f3, 1.0/64, grnColor);

	// Draw ammo bars if applicable
	ent *p = (*ps)[myPlayer].entity;
	if (!p) return;
	int charge = p->sliders[8].v;
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
	// A really big number; new players should be considered as "dead a long time" so they spawn immediately
	p->reviveCounter = 100000;
	p->data = 0;
	p->entity = NULL;
	p->color = defaultColors[i % 8];
}

static void resetPlayers(gamestate *gs) {
	range(i, gs->players->num) resetPlayer(gs, i);
}

static void saveGame(const char *name) {
	list<char> data;
	data.init();
	serialize(rootState, &data);
	writeFile(name, &data);
	data.destroy();
}

static void doInputs(ent *e, char *data) {
	if (data[0] & 1) pushBtn(e, 0);
	if (data[0] & 2) pushBtn(e, 1);
	if (data[0] & 4) pushBtn(e, 2);
	if (data[0] & 8) pushBtn(e, 3);
	int32_t axis[2] = {data[1], data[2]};
	setAxis(e, axis);
	int32_t look[3] = {data[3], data[4], data[5]};
	setLook(e, look);
}

static void doDefaultInputs(ent *e) {
	range(i, 4) {
		if (getButton(e, i)) pushBtn(e, i);
	}
}

static char isCmd(const char* input, const char *cmd) {
	int l = strlen(cmd);
	return !strncmp(input, cmd, l) && (input[l] == ' ' || input[l] == '\0');
}

static void sendControls(int frame) {
	list<char> &out = outboundData.add();

	out.setMaxUp(11); // 4 size + 1 frame + 6 input data
	out.num = 11;

	lock(outboundMutex);

	const char *const k = sharedInputs.basic.p1Keys;

	// Size will go in 0-3, we populate it in a minute
	out[4] = (char) frame;
	// Other buttons also go here, once they exist; bitfield
	out[5] = k[4] + 2*k[5] + 4*sharedInputs.basic.mouseBtnDown + 8*sharedInputs.basic.mouseSecondaryDown;

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
		out[6] = round(r_x / divisor);
		out[7] = round(r_y / divisor);
	} else {
		out[6] = out[7] = 0;
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
	out[8] = round(sine / divisor);
	out[9] = round(-cosine / divisor);
	out[10] = round(pitchSine / divisor);

	if (syncData.num) {
		out.add((char)BIN_CMD_LOAD);
		out.addAll(syncData);
		syncData.num = 0;
	} else if (sharedInputs.sendInd) {
		sharedInputs.sendInd = 0;
		const char *const text = sharedInputs.textBuffer;
		if (isCmd(text, "/help")) {
			puts(
				"Available commands:\n"
				"/save [FILE] - save to file, default file is \"savegame\"\n"
				"/load [FILE] - load from file, as above\n"
				"/sync - send current state over network - also Ctrl+R\n"
				"/syncme - displays message to other clients requesting sync\n"
				"/tree - prints debug info about collision detection tree\n"
				"/rule [RULE] - list available gamerules, or toggle one\n"
				"/c COLOR - Sets the player color to COLOR, a 6-digit hex code or a CSS4 color name\n"
				"/help - display this help\n"
			);
		} else if (isCmd(text, "/incr")) {
			int32_t x;
			const char *c = text + 5;
			if (getNum(&c, &x)) wheelIncr = x;
			else printf("incr: %d\n", wheelIncr);
		} else if (isCmd(text, "/perf")) {
			performanceFrames = 60;
		} else if (isCmd(text, "/load")) {
			const char *file = "savegame";
			if (text[5]) file = text + 6;
			out.add((char)BIN_CMD_LOAD);
			readFile(file, &out);
		} else if (!strncmp(text, "/import ", 8)) {
			out.add((char)BIN_CMD_IMPORT);
			readFile(text + 8, &out);
		} else if (isCmd(text, "/save") || isCmd(text, "/export")) {
			// These commands should only affect the local filesystem, and not game state -
			// therefore, they don't need to be synchronized.
			// What's more, we don't really want to trust the server about such things.
			// So, those commands are only processed from the loopbackCommandBuffer.
			strcpy(loopbackCommandBuffer, text);
			// We could probably process them right here,
			// but it's less to worry about if the serialization out happens at the same
			// point in the tick cycle as the deserialization in.
		} else {
			int start = out.num;
			int len = strlen(text);
			out.num += len;
			out.setMaxUp(out.num);
			memcpy(out.items + start, text, len);
		}
	}
	unlock(outboundMutex);

	*(int32_t*)out.items = htonl(out.num - 4);
	sendData(out.items, out.num);

	// Todo: Maybe `pop` doesn't make it immediately available for reclamation, so both ends have some wiggle room?
}

static void mkHeroes(gamestate *gs) {
	int numPlayers = gs->players->num;
	range(i, numPlayers) {
		player *p = &(*gs->players)[i];
		if (p->entity == NULL) {
			if (p->reviveCounter < FRAMERATE * 3) continue;
			p->entity = mkHero(gs, i, numPlayers);
			p->entity->color = p->color;
			p->reviveCounter = 0;
		}
	}
}

static void cleanupDeadHeroes(gamestate *gs) {
	int numPlayers = gs->players->num;
	range(i, numPlayers) {
		player *p = &(*gs->players)[i];
		if (p->entity && p->entity->dead) {
			p->entity = NULL;
		}
	}
}

void shareInputs() {
	lock(outboundMutex);
	sharedInputs.basic = activeInputs.basic;
	if (!sharedInputs.sendInd && activeInputs.sendInd) {
		activeInputs.sendInd = 0;
		sharedInputs.sendInd = 1;
		strcpy(sharedInputs.textBuffer, activeInputs.textBuffer);
	}
	unlock(outboundMutex);
}

char handleKey(int code, char pressed) {
	int i;
	for (i = numKeys-1; i >= 0; i--) {
		if (p1Codes[i] == code) {
			activeInputs.basic.p1Keys[i] = pressed;
			return true;
		}
	}
	if (pressed && code == GLFW_KEY_TAB) thirdPerson ^= 1;
	else if (code == GLFW_KEY_LEFT_CONTROL || code == GLFW_KEY_RIGHT_CONTROL) ctrlPressed = pressed;
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

static void updateMedianTime() {
	int lt = 0;
	int gt = 0;
	long stepDown = 0;
	long stepUp = INT64_MAX;
	range(ix, frame_time_num) {
		long t = frameTimes[ix];
		if (t < medianTime) {
			lt++;
			if (t > stepDown) stepDown = t;
		} else if (t > medianTime) {
			gt++;
			if (t < stepUp) stepUp = t;
		}
	}
	long old = medianTime;
	if (gt >= frame_time_majority) {
		medianTime = stepUp;
	} else if (lt >= frame_time_majority) {
		medianTime = stepDown;
	} else {
		return;
	}
	long delt = medianTime - old;
	if (delt > 3000000 || delt < -3000000) { // Around 10% of frame time, definitely notable
		printf("Delay jumped by %ld ns\n", delt);
	}
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
		ent *me = (*gs->players)[myPlayer].entity;
		edit_export(gs, me, c + 8);
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

	cmd("/weight", edit_m_weight(gs, me, chatBuffer + 7));
	cmd("/fpdraw", edit_m_fpdraw(gs, me, chatBuffer + 7));
	cmd("/paper", edit_m_paper(gs, me));
	cmd("/wood", edit_m_wood(gs, me));
	cmd("/stone", edit_m_stone(gs, me));
	cmd("/metal", edit_m_metal(gs, me));
	cmd("/wall", edit_m_wall(gs, me));
	cmd("/ghost", edit_m_ghost(gs, me));

	cmd("/dumb", edit_t_dumb(gs, me));
	cmd("/cursed", edit_t_cursed(gs, me));
	cmd("/logic", edit_t_logic(gs, me));
	cmd("/logic_debug", edit_t_logic_debug(gs, me));
	cmd("/door", edit_t_door(gs, me));
	cmd("/legg", edit_t_legg(gs, me));
	cmd("/respawner", edit_t_respawn(gs, me));
	cmd("/seat", edit_t_seat(gs, me));
	cmd("/veheye", edit_m_t_veh_eye(gs, me));

	cmd("/copy", edit_copy(gs, me));
	cmd("/flip", edit_flip(gs, me));
	cmd("/turn", edit_rotate(gs, me, verbose));
	cmd("/scale", edit_scale(gs, me, chatBuffer + 6, verbose));
	cmd("/scale!", edit_scale_force(gs, me, chatBuffer + 7, verbose));

	cmd("/pickup", edit_pickup(gs, me, chatBuffer + 7));
	cmd("/drop", edit_drop(gs, me));
	cmd("/wire", edit_wire(gs, me));
	cmd("/unwire", edit_unwire(gs, me));

	cmd("/b", edit_create(gs, me, chatBuffer + 2, verbose));
	cmd("/p", edit_push(gs, me, chatBuffer + 2));
	cmd("/s", edit_stretch(gs, me, chatBuffer + 2, verbose));
	cmd("/d", edit_rm(gs, me));
	cmd("/slider", edit_slider(gs, me, chatBuffer + 7, verbose));
	cmd("/hl", edit_highlight(gs, me));
	cmd("/m", if (verbose) edit_measure(gs, me));
#undef cmd
	return 0;
}

static void processCmd(gamestate *gs, player *p, char *data, int chars, char isMe, char isReal) {
	if (chars && *(unsigned char*)data == BIN_CMD_LOAD) {
		if (!isReal) return;
		syncNeeded = 0;
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
	if (chars > 5 && *(unsigned char*)data == BIN_CMD_IMPORT) {
		if (!(gs->gamerules & RULE_EDIT)) {
			if (isReal && isMe) puts("/import only works with /rule 10 enabled");
			return;
		}

		list<char> fakeList;
		fakeList.items = data+1;
		fakeList.num = fakeList.max = chars - 1;
		edit_import(gs, p->entity, &fakeList);
		return;
	}
	if (chars < TEXT_BUF_LEN) {
		memcpy(chatBuffer, data, chars);
		chatBuffer[chars] = '\0';
		char wasCommand = 1;
		if (isCmd(chatBuffer, "/sync")) {
			if (isMe && isReal) {
				syncData.num = 0;
				serialize(rootState, &syncData);
			}
		} else if (isCmd(chatBuffer, "/data")) {
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
		// TODO All the edit command need to be documented in /help
		//      (maybe add /edithelp?)
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
		} else if (isCmd(chatBuffer, "/i")) {
			if (isMe && isReal) edit_info(p->entity);
		} else if (isCmd(chatBuffer, "/rule")) {
			if (chars >= 7) {
				int rule = atoi(chatBuffer+6);
				int32_t mask = 1 << rule;
				gs->gamerules ^= mask;
				if (isReal) {
					printf("Game rule %d is now %s\n", rule, (gs->gamerules & mask) ? "ON" : "OFF");
				}
			} else if (isMe && isReal) {
				puts(RULE_HELP_STR);
				range(i, 7) {
					putchar((gs->gamerules & (1<<i)) ? 'X' : '.');
				}
				putchar('\n');
				printf("Editing is %s\n", (gs->gamerules & RULE_EDIT) ? "ON" : "OFF");
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

static char doWholeStep(gamestate *state, char *inputData, char *data2, char expectedFrame) {
	unsigned char numPlayers = *inputData++;
	list<player> &players = *state->players;
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

	char clientLate = 0;

	const char isReal = !data2;

	range(i, numPlayers) {
		char isMe = i == myPlayer;
		char *toProcess;
		if (data2 && isMe) {
			toProcess = data2;
		} else {
			toProcess = inputData;
		}
		// regardless of what `toProcess` is, we need to know how much to advance `inputData`
		int32_t size = ntohl(*(int32_t*)inputData);
		inputData += 4 + size;

		size = ntohl(*(int32_t*)toProcess);
		if (isMe && (size == 0 || expectedFrame != toProcess[4])) clientLate = 1;
		ent *e = players[i].entity;
		if (e != NULL) {
			if (size) {
				doInputs(e, toProcess + 5);
			} else {
				doDefaultInputs(e);
			}
		} else {
			players[i].reviveCounter++;
		}
		// Some edit commands depend on the player's view direction,
		// so especially if they missed last frame we really want to
		// process the command *after* we process their inputs.
		if (size > 7 && (isMe || isReal)) {
			processCmd(state, &players[i], toProcess + 11, size - 7, isMe, isReal);
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
	//   This leaves a single correct point for gravity to apply, unless we go shuffling around basic stuff.
	// The division between prepPhysics and doPhysics is specifically so we can do stuff in that gap.
	if (state->gamerules & EFFECT_GRAV) doGravity(state);

	doPhysics(state); 

	// This happens just before finishing the step so we can be 100% sure whether the player's entity is dead or not
	cleanupDeadHeroes(state);
	finishStep(state);

	return clientLate;
}

static void cloneToPhantom() {
	doCleanup(phantomState);
	free(phantomState);
	phantomState = dup(rootState, &phantomPlayers);
}

static void* pacedThreadFunc(void *_arg) {
	glfwMakeContextCurrent(display);

	long destNanos;
	long performanceTotal = 0;
	timespec t;
	int outboundFrame = startFrame;
	
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
		range(j, 4) tmp.add(0); // 4 bytes size (all zero)
	}
	unlock(outboundMutex);

	// Lower values (> 0) are better at handling long network latency,
	// while higher values (< 1) are better at handling inconsistencies in the latency (specifically from the server to us).
	// This does get adjusted dynamically.
	float padding = 0.4;
	// This means it takes about 25 late events in the same direction to move the padding by 10%, which seems reasonable
	static const float paddingAdj = 1.0 / 256;
	int serverLateCount = 0;

	// The first few frames, it's impossible for the server to have any data for us.
	// Set expectations accordingly. There's probably a more elegant way to do this...
	int clientLateCount = -latency;
	padding += paddingAdj * latency;

#define printLateStats() printf("%.4f pad; %d vs %d (server late vs client late)\n", padding, serverLateCount, clientLateCount)
	int expectedFrame = (FRAME_ID_MAX + startFrame - latency) % FRAME_ID_MAX;

	while (1) {
		lock(timingMutex);
		while (serverLead <= latency * missing_server_factor && globalRunning) {
			puts("Server isn't responding, pausing pacing thread...");
			wait(timingCond, timingMutex);
		}
		destNanos = medianTime + (long)(frame_nanos * latency * padding);
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

		sendControls(outboundFrame);
		outboundFrame = (outboundFrame + 1) % FRAME_ID_MAX;

		clock_gettime(CLOCK_MONOTONIC_RAW, &t1);
		outerSetupFrame(&phantomPlayers);

		ent *inhabit = thirdPerson ? NULL : phantomPlayers[myPlayer].entity;
		if (inhabit) inhabit = inhabit->holdRoot;
		doDrawing(phantomState, inhabit);

		drawHud(&phantomPlayers);

		clock_gettime(CLOCK_MONOTONIC_RAW, &t2);
		glfwSwapBuffers(display);

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
			if (doWholeStep(rootState, data, NULL, expectedFrame)) {
				clientLateCount++;
				padding -= paddingAdj;
				printLateStats();
			}
			expectedFrame = (expectedFrame+1)%FRAME_ID_MAX;
		}

		outboundData.multipop(framesProcessed);

		int outboundSize = outboundData.size();
		int outboundStart;
		if (framesProcessed) {
			cloneToPhantom();
			outboundStart = 0;
		} else {
			outboundStart = outboundSize - 1;
		}
		while (outboundStart < outboundSize) {
			char *myData = outboundData.peek(outboundStart++).items;
			doWholeStep(phantomState, latestFrameData.items, myData, 0);
		}

		clock_gettime(CLOCK_MONOTONIC_RAW, &t4);
		{
			draw_micros = 1000000 * (t2.tv_sec - t1.tv_sec) + (t2.tv_nsec - t1.tv_nsec) / 1000;
			flip_micros = 1000000 * (t3.tv_sec - t2.tv_sec) + (t3.tv_nsec - t2.tv_nsec) / 1000;
			phys_micros = 1000000 * (t4.tv_sec - t3.tv_sec) + (t4.tv_nsec - t3.tv_nsec) / 1000;
			if (performanceFrames) {
				performanceFrames--;
				performanceTotal += phys_micros;
				if (!performanceFrames) {
					printf("perf: %ld\n", performanceTotal);
					performanceTotal = 0;
				}
			}
		}

		// if `serverLead == 0` here, I *think* that means the server has sent us the packet for this frame, but nothing extra yet
		lock(timingMutex);
		if (serverLead < 0) {
			serverLateCount++;
			padding += paddingAdj;
			printLateStats();
		}
		serverLead--;
		range(i, frame_time_num) frameTimes[i] += frame_nanos;
		medianTime += frame_nanos;
		unlock(timingMutex);
	}
#undef printLateStats

	latestFrameData.destroy();
	return NULL;
}

static char handleCtrlBind(int key) {
	char *const t = activeInputs.textBuffer;
	if (key == GLFW_KEY_R) {
		strcpy(t, "/sync");
	} else if (key == GLFW_KEY_E) {
		strcpy(t, "/p");
	} else if (key == GLFW_KEY_K) {
		strcpy(t, "/save");
	} else if (key == GLFW_KEY_L) {
		strcpy(t, "/load");
	} else if (key == GLFW_KEY_C) {
		strcpy(t, "/copy");
	} else if (key == GLFW_KEY_F) {
		strcpy(t, "/hl");
	} else if (key == GLFW_KEY_B) {
		strcpy(t, "/b 200 200 200");
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
	activeInputs.sendInd = 1;
	typingLen = -1;
	return 1;
}

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
	if (typingLen >= 0) {
		if (action == GLFW_PRESS) {
			if (key == GLFW_KEY_ESCAPE) {
				typingLen = -1;
			} else if (key == GLFW_KEY_BACKSPACE && typingLen) {
				activeInputs.textBuffer[typingLen] = '\0';
				activeInputs.textBuffer[typingLen-1] = '_';
				typingLen--;
			} else if (key == GLFW_KEY_ENTER) {
				activeInputs.textBuffer[typingLen] = '\0';
				activeInputs.sendInd = 1;
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
		if (!activeInputs.sendInd) {
			char *const t = activeInputs.textBuffer;
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

	char *const t = activeInputs.textBuffer;
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
		handleMouseMove(xpos - mouseX, ypos - mouseY);
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
	if (button == GLFW_MOUSE_BUTTON_LEFT) {
		activeInputs.basic.mouseBtnDown = (action == GLFW_PRESS);
	} else if (button == GLFW_MOUSE_BUTTON_RIGHT) {
		activeInputs.basic.mouseSecondaryDown = (action == GLFW_PRESS);
	}
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
	if (yoffset && !activeInputs.sendInd && ctrlPressed) {
		const char *c;
		char sign;
		if (activeInputs.basic.p1Keys[5]) {
			// Pressing LShift changes it from "resize" to "move"
			c = "p";
			sign = yoffset > 0;
		} else {
			c = "s";
			sign = yoffset < 0;
		}
		snprintf(
			activeInputs.textBuffer,
			TEXT_BUF_LEN,
			"/%s %d",
			c,
			sign ? wheelIncr : -wheelIncr
		);
		activeInputs.sendInd = 1;
		typingLen = -1;
	}
}

static void window_focus_callback(GLFWwindow* window, int focused) {
	if (!focused) {
		glfwSetInputMode(display, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
		mouseGrabbed = 0;
	}
}

static void* inputThreadFunc(void *_arg) {
	glfwSetKeyCallback(display, key_callback);
	glfwSetCharCallback(display, character_callback);
	glfwSetCursorPosCallback(display, cursor_position_callback);
	glfwSetMouseButtonCallback(display, mouse_button_callback);
	glfwSetScrollCallback(display, scroll_callback);
	glfwSetWindowFocusCallback(display, window_focus_callback);
	while (!glfwWindowShouldClose(display)) {
		glfwWaitEvents();
		shareInputs();
	}
	return NULL;
}

static void setupPlayers(gamestate *gs, int numPlayers) {
	gs->players->setMaxUp(numPlayers);
	gs->players->num = numPlayers;
	resetPlayers(gs);
}

static void* netThreadFunc(void *_arg) {
	char expectedFrame = startFrame;
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
		nanos = nanos - frame_nanos * serverLead;

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
			if (size && size < 7) {
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
		char asleep = (serverLead <= latency * missing_server_factor);
		serverLead++;
		frameTimes[frameTimeIx] = nanos;
		frameTimeIx = (frameTimeIx + 1) % frame_time_num;
		updateMedianTime();
		if (asleep) {
			signal(timingCond);
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

	players.init();
	phantomPlayers.init();

	rootState = mkGamestate(&players);
	phantomState = mkGamestate(&phantomPlayers);

	init_registrar();
	init_entFuncs();

	if (!glfwInit()) {
		fputs("Couldn't init GLFW\n", stderr);
		return 1;
	}
	// glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
	display = glfwCreateWindow(displayWidth, displayHeight, "Bittoss", NULL, NULL);
	if (!display) {
		fputs("Couldn't create our display\n", stderr);
		return 1;
	}

	glfwMakeContextCurrent(display);
	initGraphics(); // OpenGL Setup (calls initFont())
	glfwMakeContextCurrent(NULL);

	colors_init();
	velbox_init();
	ent_init();
	edit_init();
	initMods(); //Set up modules
	frameData.init();
	outboundData.init();
	syncData.init();
	seat_tick = tickHandlers.get(TICK_SEAT);

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
	startFrame = clientCounts[3];
	printf("Done, I am client #%d\n", myPlayer);

	// `myPlayer + 1` is the minimum number where our index makes sense,
	// try to avoid things catching on fire
	setupPlayers(rootState, myPlayer + 1);
	// Map loading
	mkMap(rootState);
	// Loading from file is one thing, but programatically defined maps may need a `flush` so things aren't weird the first time around.
	// `flush` isn't exposed since this is the one time we need it, and it's not like it really matters,
	// so we just do the second half of a normal step to get the same effect.
	prepPhysics(rootState);
	doPhysics(rootState);
	finishStep(rootState);
	// Not 100% sure this part is necessary, but it's simpler if we know the phantom state is also in a valid state.
	cloneToPhantom();

	// Setup text buffers.
	// We make it so the player automatically sends the "syncme" command on their first frame,
	// which just displays a message to anybody else already in-game
	activeInputs.textBuffer[TEXT_BUF_LEN-1] = '\0';
	sharedInputs.textBuffer[TEXT_BUF_LEN-1] = '\0';
	strcpy(sharedInputs.textBuffer, "/syncme");
	sharedInputs.sendInd = 1;

	chatBuffer[0] = chatBuffer[TEXT_BUF_LEN-1] = loopbackCommandBuffer[0] = '\0';

	// Pre-populate timing buffer, set `startSec`
	{
		timespec now;
		clock_gettime(CLOCK_MONOTONIC_RAW, &now);
		startSec = now.tv_sec;
		long firstFrame = now.tv_nsec + frame_nanos;
		range(i, frame_time_num) {
			frameTimes[i] = firstFrame;
		}
		medianTime = firstFrame;
	}


	//Main loop
	pthread_t pacedThread;
	pthread_t netThread;
	{
		int ret = pthread_create(&pacedThread, NULL, pacedThreadFunc, NULL);
		if (ret) {
			printf("pthread_create returned %d for pacedThread\n", ret);
			return 1;
		}
		ret = pthread_create(&netThread, NULL, netThreadFunc, NULL);
		if (ret) {
			printf("pthread_create returned %d for netThread\n", ret);
			pthread_cancel(pacedThread); // No idea if this works, this is a failure case anyway
			return 1;
		}
	}
	// Main thread lives in here until the program exits
	inputThreadFunc(NULL);

	// Generally signal that there's a shutdown in progress
	// (if the inputThread isn't the main thread we'd want to set the GLFW "should exit" flag and push an empty event through)
	globalRunning = 0;
	// Somebody could potentially be waiting on this condition
	lock(timingMutex);
	signal(timingCond);
	unlock(timingMutex);

	puts("Beginning cleanup.");
	puts("Cancelling network thread...");
	{
		closeSocket();
		int ret = pthread_join(netThread, NULL);
		if (ret) printf("Error while joining pacing thread: %d\n", ret);
	}
	puts("Done.");
	puts("Cancelling paced thread...");
	{
		//int ret = pthread_cancel(inputThread);
		//if (ret) printf("Error cancelling input thread: %d\n", ret);
		int ret = pthread_join(pacedThread, NULL);
		if (ret) printf("Error while joining paced thread: %d\n", ret);
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
	edit_destroy();
	ent_destroy();
	velbox_destroy();
	colors_destroy();
	puts("Done.");
	puts("Cleaning up GLFW...");
	glfwTerminate();
	puts("Done.");
	puts("Final misc cleanup...");
	players.destroy();
	phantomPlayers.destroy();
	puts("Done.");
	puts("Cleanup complete, goodbye!");
	return 0;
}
