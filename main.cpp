#include <math.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <allegro5/allegro.h>
#include <allegro5/allegro_opengl.h>
#include <pthread.h>

#include <random>

#include "graphics.h"
#include "font.h"
#include "ent.h"
#include "main.h"
#include "modules.h"
#include "net.h"
#include "handlerRegistrar.h"
#include "effects.h"

#include "entFuncs.h"
#include "entUpdaters.h"
#include "entGetters.h"

// Allegro's "custom events" are kinda a trashfire, but oh well.
// Eventually I should probably abandon Allegro entirely...
#define CUSTOM_EVT_TYPE 1025
static ALLEGRO_EVENT_SOURCE customSrc;

#define numKeys 6
typedef struct player {
	ent *entity;
	int reviveCounter;
	//ALLEGRO_JOYSTICK *js;
	float center[2];
} player;

static player *players;
static int myPlayer;
static int numPlayers;

int p1Codes[numKeys] = {ALLEGRO_KEY_A, ALLEGRO_KEY_D, ALLEGRO_KEY_W, ALLEGRO_KEY_S, ALLEGRO_KEY_SPACE, ALLEGRO_KEY_LSHIFT};
char p1Keys[numKeys];
static char mouseBtnDown = 0;
static char mouseSecondaryDown = 0;

scheme *sc;

static std::minstd_rand *random_gen;
// rand_t is defined in main.h (to avoid including <random> everywhere).
// It is manually verified to be what std::minstd_rand returns.
static rand_t random_base;
rand_t random_max;

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
static char chatBuffer[TEXT_BUF_LEN];
static int bufferedTextLen = 0;
static int textInputMode = 0; // 0 - idle; 1 - typing; 2 - queued; 3 - queued + wants to type again

#define micro_hist_num 5
static int historical_phys_micros[micro_hist_num];
static int historical_draw_micros[micro_hist_num];
static int micro_hist_ix = micro_hist_num-1;
static struct timeval t1, t2, t3;

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
static void drawHud() {
	setupText();
	drawHudText(chatBuffer, 1, 1, 1, hudColor);
	if (textInputMode) drawHudText(inputTextBuffer, 1, 3, 1, hudColor);

	float f1 = (double) historical_phys_micros[micro_hist_ix] / micros_per_frame;
	float f2 = (double) historical_draw_micros[micro_hist_ix] / micros_per_frame;
	// Draw frame timing bars
	drawHudRect(0, 1 - 1.0/64, f1, 1.0/64, grnColor);
	drawHudRect(f1, 1 - 1.0/64, f2, 1.0/64, bluColor);

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

rand_t get_random() {
	return (*random_gen)() - random_base;
}

void loadFile(const char* file) {
	FILE *f = fopen(file, "r");
	if (!f) {
		fprintf(stderr, "Couldn't open %s\n", file);
		return;
	}
	scheme_load_file(sc, f);
	fclose(f);
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

static void doHeroes() {
	int i;
	for (i = 0; i < numPlayers; i++) {
		player *p = players + i;
		if (p->entity == NULL) {
			if (p->reviveCounter--) continue;
			save_from_C_call(sc);
			sc->NIL->references++;
			pointer hero = scheme_eval(sc,
				cons(sc, mk_symbol(sc, "mk-hero"),
				cons(sc, mk_integer(sc, i),
				cons(sc, mk_integer(sc, numPlayers),
				sc->NIL))));
			if (!is_c_ptr(hero, 0)) {
				fputs("mk-hero didn't give an ent*\n", stderr);
				p->reviveCounter = FRAMERATE * 3;
				continue;
			}
			p->entity = (ent*)c_ptr_value(hero);
			if (i == myPlayer) {
				viewPitch = M_PI_2;
				viewYaw = 0;
			}
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

/*
static void centerJoysticks() {
	ALLEGRO_JOYSTICK_STATE jsState;
	int i;
	for (i = 1; i < numPlayers; i++) {
		al_get_joystick_state(players[i].js, &jsState);
		int j;
		for (j = 0; j < 2; j++) {
			players[i].center[j] = jsState.stick[0].axis[j];
		}
	}
}
*/

char handleKey(int code, char pressed) {
	int i;
	for (i = numKeys-1; i >= 0; i--) {
		if (p1Codes[i] == code) {
			p1Keys[i] = pressed;
			return true;
		}
	}
	if (pressed && code == ALLEGRO_KEY_TAB) thirdPerson ^= 1;
	//if (pressed && code == ALLEGRO_KEY_BACKSLASH) centerJoysticks();
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

static void* inputThreadFunc(void *arg) {
	char mouse_grabbed = 0;
	char running = 1;
	ALLEGRO_EVENT evnt;
	while (running) {
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
				int frame = (int) evnt.user.data1;
				if (frame == -1) running = 0;
				else sendControls(frame);
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
	}
	//centerJoysticks();
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

	random_gen = new std::minstd_rand(0);
	random_base = random_gen->min();
	random_max = random_gen->max() - random_base;

	// set up scheme stuff
	if (!(sc = scheme_init_new())) {
		fputs("Couldn't init TinyScheme!\n", stderr);
		return 1;
	}
	scheme_set_output_port_file(sc, stdout);
	loadFile("myInit.scm");
	scheme_define(sc, sc->global_env, mk_symbol(sc, "draw"), mk_foreign_func(sc, ts_draw));
	registerTsUpdaters();
	registerTsGetters();
	registerTsFuncSetters();

	init_registrar();

	// Set up allegro stuff
	if (!al_init()) {
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
	//Set up other modules
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
	loadFile("map.scm");
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

	//Main loop
	pthread_t inputThread;
	{
		int ret = pthread_create(&inputThread, NULL, &inputThreadFunc, display);
		if (ret) {
			printf("pthread_create returned %d\n", ret);
			return 1;
		}
	}
	// TODO: If view direction ever matters, that should carry over rather than have a constant default
	// (buttons, move_x, move_y, look_x, look_y, look_z)
	char defaultData[6] = {0, 0, 0, 0, 0, 0};
	char actualData[6];
	ALLEGRO_EVENT customEvent;
	customEvent.user.type = CUSTOM_EVT_TYPE;
	char expectedFrame = 0;
	while (1) {
		char frame;
		if (readData(&frame, 1)) break;
		if (frame != expectedFrame) {
			printf("Didn't get right frame value, expected %d but got %d\n", expectedFrame, frame);
			break;
		}
		expectedFrame = (expectedFrame + 1) % 16;

		// Add custom event to Allegro event queue
		customEvent.user.data1 = frame;
		al_emit_user_event(&customSrc, &customEvent, NULL);

		// Begin setting up the tick, including some hard-coded things like gravity / lava
		gettimeofday(&t1, NULL);
		doCrushtainer();
		createDebris();
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
					if (readData(chatBuffer, size)) goto done;
					chatBuffer[size] = '\0';
				}
			}
			if (players[i].entity != NULL) {
				doInputs(players[i].entity, data);
			}
		}

		doTicks();
		doGravity();
		doPhysics();

		gettimeofday(&t2, NULL);

		outerSetupFrame();
		doDrawing();
		drawHud();
		al_flip_display();
		gettimeofday(&t3, NULL);
		{
			int physMicros = 1000000 * (t2.tv_sec - t1.tv_sec) + t2.tv_usec - t1.tv_usec;
			int drawMicros = 1000000 * (t3.tv_sec - t2.tv_sec) + t3.tv_usec - t2.tv_usec;
			micro_hist_ix = (micro_hist_ix+1)%micro_hist_num;
			historical_phys_micros[micro_hist_ix] = physMicros;
			historical_draw_micros[micro_hist_ix] = drawMicros;
		}
	}
	done:;
	puts("Beginning cleanup.");
	puts("Cleaning up simple interal components...");
	destroyFont();
	destroy_registrar();
	delete random_gen;
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
	doCleanup();
	puts("Done.");
	puts("Cleaning up Scheme...");
	scheme_deinit(sc);
	free(sc);
	puts("Done.");
	delete[] players;
	puts("Cleanup complete, goodbye!");
	return 0;
}
