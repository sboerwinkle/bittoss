#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <allegro5/allegro.h>
#include <allegro5/allegro_opengl.h>
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
static char ctrlPressed = 0;
static int wheelIncr = 100;

//static ALLEGRO_VERTEX vertices[4];
//const int indices[6] = {0, 1, 2, 3, 2, 1};
//static int32_t cx = (displayWidth / 2) * PTS_PER_PX;
//static int32_t cy = (displayHeight / 2) * PTS_PER_PX;

// At the moment we use a byte to give the network message length, so this has to be fairly small
#define TEXT_BUF_LEN 200
static char chatBuffer[TEXT_BUF_LEN];
static char inputTextBuffer[TEXT_BUF_LEN];
static char loopbackCommandBuffer[TEXT_BUF_LEN];
static int bufferedTextLen = 0;
static int textInputMode = 0; // 0 - idle; 1 - typing; 2 - queued; 3 - queued + wants to type again

#define micro_hist_num 5
static int historical_phys_micros[micro_hist_num];
static int historical_draw_micros[micro_hist_num];
static int historical_flip_micros[micro_hist_num];
static int micro_hist_ix = micro_hist_num-1;
static struct timespec t1, t2, t3, t4;

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
char syncReady;
char syncNeeded = 0;
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

static float hudColor[3] = {0.0, 0.5, 0.5};
static float grnColor[3] = {0.0, 1.0, 0.0};
static float bluColor[3] = {0.0, 0.0, 1.0};
static float redColor[3] = {1.0, 0.0, 0.0};
static void drawHud(list<player> *ps) {
	setupText();
	const char* drawMe = syncNeeded ? "CTRL+R TO SYNC" : chatBuffer;
	drawHudText(drawMe, 1, 1, 1, hudColor);
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
	int charge = p->sliders[3].v;
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

static char isCmd(const char* input, const char *cmd) {
	int l = strlen(cmd);
	return !strncmp(input, cmd, l) && (input[l] == ' ' || input[l] == '\0');
}

static void sendControls(int frame) {
	// This is a cheap trick which I need to codify as a method.
	// It's a threadsafe way to get what the next `add` will be, assuming we're the only
	// thread that ever calls `add`.
	list<char> &out = outboundData.items[outboundData.end];

	out.setMaxUp(11); // 4 size + 1 frame + 6 input data
	out.num = 11;

	// Size will go in 0-3, we populate it in a minute
	out[4] = (char) frame;
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

	char loopbackCommand = 0;
	if (syncReady == 2) {
		out.add((char)BIN_CMD_LOAD);
		out.addAll(syncData);
	} else if (textInputMode >= 2) {
		if (isCmd(inputTextBuffer, "/help")) {
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
		} else if (isCmd(inputTextBuffer, "/incr")) {
			int32_t x;
			const char *c = inputTextBuffer;
			if (getNum(&c, &x)) wheelIncr = x;
			else printf("incr: %d\n", wheelIncr);
		} else if (isCmd(inputTextBuffer, "/load")) {
			const char *file = "savegame";
			if (bufferedTextLen > 6) file = inputTextBuffer + 6;
			out.add((char)BIN_CMD_LOAD);
			readFile(file, &out);
		} else if (!strncmp(inputTextBuffer, "/import ", 8)) {
			out.add((char)BIN_CMD_IMPORT);
			readFile(inputTextBuffer + 8, &out);
		} else if (isCmd(inputTextBuffer, "/save") || isCmd(inputTextBuffer, "/export")) {
			// These commands should only affect the local filesystem, and not game state -
			// therefore, they don't need to be synchronized.
			// What's more, we don't really want to trust the server about such things.
			// So, those commands are only processed from the loopbackCommandBuffer,
			// which is our threadsafe way to communicate from the input thread to the paced thread.
			loopbackCommand = 1;
		} else {
			int start = out.num;
			out.num += bufferedTextLen;
			out.setMaxUp(out.num);
			memcpy(out.items + start, inputTextBuffer, bufferedTextLen);
		}
	}
	*(int32_t*)out.items = htonl(out.num - 4);
	sendData(out.items, out.num);

	lock(outboundMutex);
	if (loopbackCommand && !*loopbackCommandBuffer) {
		strcpy(loopbackCommandBuffer, inputTextBuffer);
	}
	if (syncReady == 2) syncReady = 0;
	outboundData.add();
	signal(outboundCond);
	unlock(outboundMutex);
	// Todo: Maybe `pop` doesn't make it immediately available for reclamation, so both ends have some wiggle room?

	if (textInputMode >= 2) {
		// loopbackCommandBuffer still needs to look at the inputTextBuffer inside the locked section,
		// so this last bit of cleanup has to wait until after.
		textInputMode -= 2;
		if (textInputMode == 1) setupTyping();
	}
}

static void updateColor(player *p) {
	if (p->entity) {
		p->entity->color = p->color;
	}
}

static void mkHeroes(gamestate *gs) {
	int numPlayers = gs->players->num;
	range(i, numPlayers) {
		player *p = &(*gs->players)[i];
		if (p->entity == NULL) {
			if (p->reviveCounter++ < FRAMERATE * 3) continue;
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
			p->reviveCounter = 0;
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
	else if (code == ALLEGRO_KEY_LCTRL || code == ALLEGRO_KEY_RCTRL) ctrlPressed = pressed;
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

	cmd("/weight", edit_m_weight(gs, me));
	cmd("/paper", edit_m_paper(gs, me));
	cmd("/wood", edit_m_wood(gs, me));
	cmd("/stone", edit_m_stone(gs, me));
	cmd("/wall", edit_m_wall(gs, me));
	cmd("/ghost", edit_m_ghost(gs, me));

	cmd("/dumb", edit_t_dumb(gs, me));
	cmd("/cursed", edit_t_cursed(gs, me));
	cmd("/logic", edit_t_logic(gs, me));
	cmd("/logic_debug", edit_t_logic_debug(gs, me));
	cmd("/door", edit_t_door(gs, me));

	cmd("/copy", edit_copy(gs, me));
	cmd("/flip", edit_flip(gs, me));
	cmd("/turn", edit_rotate(gs, me, verbose));
	cmd("/scale", edit_scale(gs, me, chatBuffer + 6, verbose));
	cmd("/scale!", edit_scale_force(gs, me, chatBuffer + 6, verbose));

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
	// Message could get lost here if multiple people send on the same frame,
	// but it will at least be consistent? Maybe fixing this is a TODO
	if (chars < TEXT_BUF_LEN) {
		memcpy(chatBuffer, data, chars);
		chatBuffer[chars] = '\0';
		char wasCommand = 1;
		if (isCmd(chatBuffer, "/sync")) {
			if (isMe && isReal && !syncReady) {
				syncData.num = 0;
				serialize(rootState, &syncData);
				syncReady = 1;
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
				int edit = !getSlider(e, 6);
				// Reach in and tweak internal state to toggle edit mode
				if (!edit || (gs->gamerules & RULE_EDIT)) {
					uSlider(e, 6, edit);
					if (isReal && isMe) {
						printf("Your edit toolset is %s\n", edit ? "ON" : "OFF");
					}
				}
			}
		} else if (isCmd(chatBuffer, "/i")) { // TODO This is rapidly getting unwieldy, move to another file? Maybe even make a lookup table??? Or at least check for '/' and bypass all this otherwise...
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
				range(i, 6) {
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
	// TODO: If view direction ever matters, that should carry over rather than have a constant default
	// (buttons, move_x, move_y, look_x, look_y, look_z)
	static char defaultData[6] = {0, 0, 0, 0, 0, 0};

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
		char *data;
		if (size == 0) {
			data = defaultData;
			if (isMe) clientLate = 1;
		} else {
			data = toProcess + 5;
			if (isMe && expectedFrame != toProcess[4]) clientLate = 1;
		}
		if (players[i].entity != NULL) {
			doInputs(players[i].entity, data);
		}
		// Some edit commands depend on the player's view direction,
		// so especially if they missed last frame we really want to
		// process the command *after* we process their inputs.
		if (size > 7 && (isMe || !data2)) {
			processCmd(state, &players[i], toProcess + 11, size - 7, isMe, !data2);
		}
	}

	doUpdates(state);

	// Hero creation happens here, right after all the ticks.
	// This means all the creation stuff will be flushed out before
	// the new ents process any ticks themselves, and they'll see
	// correctly initialized state.
	mkHeroes(state);

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
		al_emit_user_event(&customSrc, &customEvent, NULL);
		reqdOutboundSize++;

		// Begin setting up the tick, including some hard-coded things like gravity / lava
		clock_gettime(CLOCK_MONOTONIC_RAW, &t1);
		outerSetupFrame(&phantomPlayers);
		ent *inhabit = thirdPerson ? NULL : phantomPlayers[myPlayer].entity;
		doDrawing(phantomState, inhabit);
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
			if (doWholeStep(rootState, data, NULL, expectedFrame)) {
				clientLateCount++;
				padding -= paddingAdj;
				printLateStats();
			}
			expectedFrame = (expectedFrame+1)%FRAME_ID_MAX;
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
		if (*loopbackCommandBuffer) {
			// This part is kind of slow for us to keep the mutex locked;
			// maybe fix this later if it's actually a problem
			processLoopbackCommand(rootState);
			*loopbackCommandBuffer = '\0';
		}
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
			doWholeStep(phantomState, latestFrameData.items, myData, 0);
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

	paced_exit:;
	latestFrameData.destroy();
	return NULL;
}

static void* inputThreadFunc(void *_arg) {
	char mouse_grabbed = 0;
	char running = 1;
	int frame = startFrame;
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
				if (evnt.keyboard.modifiers & ALLEGRO_KEYMOD_CTRL) {
					if (evnt.keyboard.keycode == ALLEGRO_KEY_R) {
						strcpy(inputTextBuffer, "/sync");
					} else if (evnt.keyboard.keycode == ALLEGRO_KEY_E) {
						strcpy(inputTextBuffer, "/p");
					} else if (evnt.keyboard.keycode == ALLEGRO_KEY_K) {
						strcpy(inputTextBuffer, "/save");
					} else if (evnt.keyboard.keycode == ALLEGRO_KEY_L) {
						strcpy(inputTextBuffer, "/load");
					} else if (evnt.keyboard.keycode == ALLEGRO_KEY_C) {
						strcpy(inputTextBuffer, "/copy");
					} else if (evnt.keyboard.keycode == ALLEGRO_KEY_F) {
						strcpy(inputTextBuffer, "/hl");
					} else if (evnt.keyboard.keycode == ALLEGRO_KEY_B) {
						strcpy(inputTextBuffer, "/b 200 200 200");
					} else if (evnt.keyboard.keycode == ALLEGRO_KEY_I) {
						if (p1Keys[5]) {
							strcpy(inputTextBuffer, "/inside 2");
						} else {
							strcpy(inputTextBuffer, "/inside");
						}
					} else {
						break;
					}
					bufferedTextLen = strlen(inputTextBuffer);
					textInputMode |= 2;
				} else if (textInputMode == 1) {
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
			case ALLEGRO_EVENT_DISPLAY_SWITCH_OUT:
				// This doesn't actually fire if you switch out via keyboard,
				// because Allegro is a useless POS I should have abandoned a long time ago.
				// However, that sounds like work, so we'll just do this for now and hope
				// it helps a little somehow.
				// (fall-thru into next block)
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
					if (evnt.mouse.dz && !(textInputMode & 2) && ctrlPressed) {
						const char *c;
						char pos;
						if (p1Keys[5]) {
							// Pressing LShift changes it from "resize" to "move"
							c = "p";
							pos = evnt.mouse.dz > 0;
						} else {
							c = "s";
							pos = evnt.mouse.dz < 0;
						}
						snprintf(inputTextBuffer, TEXT_BUF_LEN, "/%s %d", c, pos ? wheelIncr : -wheelIncr);
						bufferedTextLen = strlen(inputTextBuffer);
						textInputMode |= 2;
					}
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
	edit_init();
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

	// Setup text buffers.
	// We make it so the player automatically sends the "syncme" command on their first frame,
	// which is how we're eventually get "sync on join" to work
	inputTextBuffer[TEXT_BUF_LEN-1] = '\0';
	strcpy(inputTextBuffer, "/syncme");
	bufferedTextLen = strlen(inputTextBuffer);
	textInputMode = 2;
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
	customEvent.user.type = CUSTOM_EVT_TYPE;
	customEvent.user.data1 = 0;


	//Main loop
	pthread_t inputThread;
	pthread_t netThread;
	{
		int ret = pthread_create(&inputThread, NULL, inputThreadFunc, NULL);
		if (ret) {
			printf("pthread_create returned %d for inputThread\n", ret);
			return 1;
		}
		ret = pthread_create(&netThread, NULL, netThreadFunc, NULL);
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
	edit_destroy();
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
	puts("Done.");
	puts("Cleanup complete, goodbye!");
	return 0;
}
