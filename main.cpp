#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <allegro5/allegro.h>
#include <allegro5/allegro_opengl.h>
#include <pthread.h>

#include "util.h"
#include "graphics.h"
#include "font.h"
#include "ent.h"
#include "main.h"
#include "modules.h"
#include "net.h"
#include "handlerRegistrar.h"
#include "effects.h"
#include "map.h"

#include "entFuncs.h"
#include "entUpdaters.h"
#include "entGetters.h"

// Allegro's "custom events" are kinda a trashfire, but oh well.
// Eventually I should probably abandon Allegro entirely...
#define CUSTOM_EVT_TYPE 1025
static ALLEGRO_EVENT_SOURCE customSrc;
static ALLEGRO_EVENT customEvent;

#define numKeys 6
typedef struct player {
	ent *entity;
	int32_t color;
	int reviveCounter;
} player;

static player *players;
static int myPlayer;
static int numPlayers;

static int32_t defaultColors[8] = {11, 8, 19, 15, 2, 44, 23, 29};

int p1Codes[numKeys] = {ALLEGRO_KEY_A, ALLEGRO_KEY_D, ALLEGRO_KEY_W, ALLEGRO_KEY_S, ALLEGRO_KEY_SPACE, ALLEGRO_KEY_LSHIFT};
char p1Keys[numKeys];
static char mouseBtnDown = 0;
static char mouseSecondaryDown = 0;

static gamestate *rootState;
static volatile char doClone = 0; // Not sure if we need `volatile` here, but the whole thing is temporary anyway

static ALLEGRO_DISPLAY *display;
static ALLEGRO_EVENT_QUEUE *queue;

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
static char cmdBuffer[TEXT_BUF_LEN];
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

#define lock(mtx) if (int __ret = pthread_mutex_lock(&mtx)) printf("Mutex lock failed with code %d\n", __ret)
#define unlock(mtx) if (int __ret = pthread_mutex_unlock(&mtx)) printf("Mutex unlock failed with code %d\n", __ret)
#define wait(cond, mtx) if (int __ret = pthread_cond_wait(&cond, &mtx)) printf("Mutex cond wait failed with code %d\n", __ret)
#define signal(cond) if (int __ret = pthread_cond_signal(&cond)) printf("Mutex cond signal failed with code %d\n", __ret)

static void outerSetupFrame() {
	ent *p = players[myPlayer].entity;
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
static void drawHud() {
	setupText();
	drawHudText(chatBuffer, 1, 1, 1, hudColor);
	if (textInputMode) drawHudText(inputTextBuffer, 1, 3, 1, hudColor);

	float f1 = (double) historical_phys_micros[micro_hist_ix] / micros_per_frame;
	float f2 = (double) historical_draw_micros[micro_hist_ix] / micros_per_frame;
	float f3 = (double) historical_flip_micros[micro_hist_ix] / micros_per_frame;
	// Draw frame timing bars
	drawHudRect(0, 1 - 1.0/64, f1, 1.0/64, grnColor);
	drawHudRect(f1, 1 - 1.0/64, f2, 1.0/64, bluColor);
	drawHudRect(f1+f2, 1 - 1.0/64, f3, 1.0/64, redColor);

	// Draw ammo bars if applicable
	ent *p = players[myPlayer].entity;
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
	char data[8];
	data[0] = (char) frame;
	data[1] = 6;
	// Other buttons also go here, once they exist; bitfield
	data[2] = p1Keys[4] + 2*p1Keys[5] + 4*mouseBtnDown + 8*mouseSecondaryDown;

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
		data[3] = round(r_x / divisor);
		data[4] = round(r_y / divisor);
	} else {
		data[3] = data[4] = 0;
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
	data[5] = round(sine / divisor);
	data[6] = round(-cosine / divisor);
	data[7] = round(pitchSine / divisor);
	if (textInputMode >= 2) data[1] += bufferedTextLen;
	sendData(data, 8);
	if (textInputMode >= 2) {
		sendData(inputTextBuffer, bufferedTextLen);
		textInputMode -= 2;
		if (textInputMode == 1) setupTyping();
	}
}

static void updateColor(player *p) {
	if (p->entity) {
		uStateSlider(&p->entity->state, 6, p->color);
	}
}

static void doHeroes() {
	int i;
	for (i = 0; i < numPlayers; i++) {
		player *p = players + i;
		if (p->entity == NULL) {
			if (p->reviveCounter--) continue;
			p->entity = mkHero(rootState, i, numPlayers);
			if (i == myPlayer) {
				viewPitch = M_PI_2;
				viewYaw = 0;
			}
			updateColor(p);
		}
		if (p->entity->dead) {
			p->entity = NULL;
			p->reviveCounter = FRAMERATE * 3;
			if (i == myPlayer) {
				viewPitch = M_PI_4 / 2;
				viewYaw = 0;
			}
			continue;
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
	// TODO move this stuff into the KEY_DOWN section?
	if (pressed && code == ALLEGRO_KEY_TAB) thirdPerson ^= 1;
	else if (pressed && code == ALLEGRO_KEY_F5) doClone = 1;
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

static void* pacedThreadFunc(void *_arg) {
	long destNanos;
	timespec t;
	while (1) {
		lock(timingMutex);
		while (medianTime == INT64_MAX) {
			puts("Server isn't responding, pausing pacing thread...");
			wait(timingCond, timingMutex);
		}
		destNanos = medianTime;
		unlock(timingMutex);

		clock_gettime(CLOCK_MONOTONIC_RAW, &t);
		destNanos = destNanos - t.tv_nsec - 1000000000 * (t.tv_sec - startSec);
		if (destNanos > 999999999) {
			puts("Tried to wait for more than a second???");
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

		lock(timingMutex);
		serverLead--;
		range(i, frame_time_num) frameTimes[i] += frame_nanos;
		medianTime += frame_nanos;
		updateMedianTime();
		unlock(timingMutex);
	}
}

static void* inputThreadFunc(void *_arg) {
	char mouse_grabbed = 0;
	char running = 1;
	int frame = 0;
	// Could use an Allegro timer here, but we expect the delay to be updated moment-to-moment so this is easier
	//ALLEGRO_TIMEOUT timeout;
	ALLEGRO_EVENT evnt;
	evnt.type = 0;
	while (running) {
		/*
		if (evnt.type != CUSTOM_EVT_TYPE) al_init_timeout(&timeout, 1); // 1.0 seconds from now
		bool hasEvent = al_wait_for_event_until(queue, &evnt, &timeout);
		if (!hasEvent) {
			evnt.type = 0;
			puts("Boo");
			continue;
		}
		*/
		al_wait_for_event(queue, &evnt);
		switch(evnt.type) {
			case ALLEGRO_EVENT_DISPLAY_CLOSE:
				// Forces main thread to exit, it will then tell us to exit and we'll quit gracefully
				closeSocket();
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
				frame = (frame + 1) % 16;
				break;
		}
	}
	puts("\tCleaning up Allegro stuff...");
	al_unregister_event_source(queue, &customSrc);
	al_destroy_user_event_source(&customSrc);
	al_destroy_event_queue(queue);
	al_destroy_display(display);
	puts("\tDone.");
	return NULL;
}

static void setupPlayers() {
	memset(p1Keys, 0, sizeof(p1Keys));
	players = new player[numPlayers];
	for (int i = 0; i < numPlayers; i++) {
		players[i].reviveCounter = 0;
		players[i].entity = NULL;
		players[i].color = defaultColors[i % 8];
	}
}

static void processCmd(player *p, int chars) {
	if (chars >= 6 && !strncmp(cmdBuffer, "/c ", 3)) {
		int32_t color = (cmdBuffer[3] - '0') + 4 * (cmdBuffer[4] - '0') + 16 * (cmdBuffer[5] - '0');
		p->color = color;
		updateColor(p);
		return;
	}
	// Default case, just display it
	memcpy(chatBuffer, cmdBuffer, chars + 1);
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

	rootState = (gamestate*) calloc(1, sizeof(gamestate));
	rootState->rand = 1;

	init_registrar();

	// Set up allegro stuff
	if (!al_install_system(ALLEGRO_VERSION_INT, NULL)) {
		fputs("Couldn't init Allegro\n", stderr);
		return 1;
	}
	/* Joystick code is still laying around places, but only player one gets the view window,
	 * and additionally joysticks don't account for yaw when sending inputs. TODO maybe?
	if (al_install_joystick()) {
		numPlayers = al_get_num_joysticks() + 1;
	} else {
		numPlayers = 1;
		fputs("Couldn't install joystick driver\n", stderr);
	}
	 */
	numPlayers = 1;
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
	// OpenGL Setup
	initGraphics(); // calls initFont()
	ent_init();
	//Set up modules
	initMods();

	// Other general game setup, including networking
	puts("Connecting to host...");
	if (initSocket(srvAddr, port)) return 1;
	puts("Done.");
	puts("Awaiting other clients...");
	char clientCounts[3];
	if (readData(clientCounts, 3)) {
		puts("Error, aborting!");
		return 1;
	}
	if (clientCounts[0] != (char)0x80) {
		printf("Bad initial byte %hhd, aborting!\n", clientCounts[0]);
		return 1;
	}
	myPlayer = clientCounts[1];
	numPlayers = clientCounts[2];
	printf("Done, I am client #%d (%d total)\n", myPlayer, numPlayers);
	setupPlayers();
	//map loading
	mkMap(rootState);
	//Events
	//ALLEGRO_TIMER *timer = al_create_timer(ALLEGRO_BPS_TO_SECS(FRAMERATE));
	al_init_user_event_source(&customSrc);
	queue = al_create_event_queue();
	al_register_event_source(queue, &customSrc);
	al_register_event_source(queue, al_get_display_event_source(display));
	//al_register_event_source(queue, al_get_timer_event_source(timer));
	al_register_event_source(queue, al_get_keyboard_event_source());
	if (al_is_mouse_installed()) {
		al_register_event_source(queue, al_get_mouse_event_source());
		// For now, don't capture mouse on startup. Might prevent mouse warpiness in first frame, idk, feel free to mess around
	}

	inputTextBuffer[0] = inputTextBuffer[TEXT_BUF_LEN-1] = '\0';
	chatBuffer[0] = chatBuffer[TEXT_BUF_LEN-1] = '\0';

	timespec now;
	clock_gettime(CLOCK_MONOTONIC_RAW, &now);
	startSec = now.tv_sec;
	// Pre-populate timing buffer
	{
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
	pthread_t pacedThread;
	{
		int ret = pthread_create(&inputThread, NULL, &inputThreadFunc, NULL);
		if (ret) {
			printf("pthread_create returned %d for inputThread\n", ret);
			return 1;
		}
		ret = pthread_create(&pacedThread, NULL, pacedThreadFunc, NULL);
		if (ret) {
			printf("pthread_create returned %d for pacedThread\n", ret);
			pthread_cancel(inputThread); // No idea if this works, this is a failure case anyway
			return 1;
		}
	}
	// TODO: If view direction ever matters, that should carry over rather than have a constant default
	// (buttons, move_x, move_y, look_x, look_y, look_z)
	char defaultData[6] = {0, 0, 0, 0, 0, 0};
	char actualData[6];
	char expectedFrame = 0;
	while (1) {
		char frame;
		if (readData(&frame, 1)) break;
		if (frame != expectedFrame) {
			printf("Didn't get right frame value, expected %d but got %d\n", expectedFrame, frame);
			break;
		}
		expectedFrame = (expectedFrame + 1) % 16;

		clock_gettime(CLOCK_MONOTONIC_RAW, &now);
		// TODO: Read offset from message
		long nanos = 1000000000 * (now.tv_sec - startSec) + now.tv_nsec;

		lock(timingMutex);
		serverLead++;
		frameTimes[frameTimeIx] = nanos - frame_nanos * serverLead;
		frameTimeIx = (frameTimeIx + 1) % frame_time_num;
		if (serverLead <= 0) {
			char asleep = (medianTime == INT64_MAX);
			updateMedianTime();
			if (asleep && medianTime != INT64_MAX) {
				signal(timingCond);
			}
		}
		unlock(timingMutex);


		// Begin setting up the tick, including some hard-coded things like gravity / lava
		clock_gettime(CLOCK_MONOTONIC_RAW, &t1);
		if (doClone) {
			doClone = 0;
			fputs("Cloning...\n", stderr);
			gamestate* tmp = dup(rootState);
			range(i, numPlayers) if (players[i].entity != NULL) players[i].entity = players[i].entity->clone;
			doCleanup(rootState);
			free(rootState);
			rootState = tmp;
		}
		doCrushtainer(rootState);
		createDebris(rootState);
		// Heroes must be after environmental death effects, or they can be killed and then cleaned up immediately
		doHeroes();

		for (int i = 0; i < numPlayers; i++) {
			unsigned char size;
			if (readData((char*)&size, 1)) goto done;
			char *data;
			if (size == 0) {
				data = defaultData;
			} else if (size < 6) {
				printf("Fatal error, can't handle player net input of size %d\n", size);
				goto done;
			} else {
				data = actualData;
				if (readData(data, 6)) goto done;
				size -= 6;
				if (size >= TEXT_BUF_LEN) {
					printf("Fatal error, can't handle player chat of length %d\n", size);
					goto done;
				}
				if (size) {
					if (readData(cmdBuffer, size)) goto done;
					cmdBuffer[size] = '\0';
					processCmd(&players[i], size);
				}
			}
			if (players[i].entity != NULL) {
				doInputs(players[i].entity, data);
			}
		}

		doUpdates(rootState);
		doGravity(rootState);
		doPhysics(rootState);

		clock_gettime(CLOCK_MONOTONIC_RAW, &t2);

		outerSetupFrame();
		doDrawing(rootState);
		drawHud();
		clock_gettime(CLOCK_MONOTONIC_RAW, &t3);
		al_flip_display();
		clock_gettime(CLOCK_MONOTONIC_RAW, &t4);
		{
			int physMicros = 1000000 * (t2.tv_sec - t1.tv_sec) + (t2.tv_nsec - t1.tv_nsec) / 1000;
			int drawMicros = 1000000 * (t3.tv_sec - t2.tv_sec) + (t3.tv_nsec - t2.tv_nsec) / 1000;
			int flipMicros = 1000000 * (t4.tv_sec - t3.tv_sec) + (t4.tv_nsec - t3.tv_nsec) / 1000;
			micro_hist_ix = (micro_hist_ix+1)%micro_hist_num;
			historical_phys_micros[micro_hist_ix] = physMicros;
			historical_draw_micros[micro_hist_ix] = drawMicros;
			historical_flip_micros[micro_hist_ix] = flipMicros;
		}
	}
	done:;
	puts("Beginning cleanup.");
	puts("Cleaning up simple interal components...");
	destroyFont();
	destroy_registrar();
	ent_destroy();
	puts("Done.");
	puts("Cancelling pacing thread...");
	{
		int ret = pthread_cancel(pacedThread);
		if (ret) printf("Error while cancelling pacing thread: %d\n", ret);
		else {
			ret = pthread_join(pacedThread, NULL);
			if (ret) printf("Error while joining pacing thread: %d\n", ret);
		}
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
	puts("Closing socket...");
	closeSocket();
	puts("Done.");
	puts("Cleaning up game objects...");
	doCleanup(rootState);
	free(rootState);
	puts("Done.");
	delete[] players;
	puts("Cleaning up Allegro...");
	al_uninstall_system();
	puts("Done.");
	puts("Cleanup complete, goodbye!");
	return 0;
}
