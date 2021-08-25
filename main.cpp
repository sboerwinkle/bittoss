#include <math.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <allegro5/allegro.h>
#include <allegro5/allegro_opengl.h>
#include <pthread.h>
#include "ent.h"
#include "main.h"
#include "modules.h"
#include "net.h"

#include "entFuncs.h"
#include "entUpdaters.h"
#include "entGetters.h"

// Allegro's "custom events" are kinda a trashfire, but oh well.
// Eventually I should probably abandon Allegro entirely...
#define CUSTOM_EVT_TYPE 1025
static ALLEGRO_EVENT_SOURCE customSrc;

#define numKeys 5
typedef struct player {
	ent *entity;
	int reviveCounter;
	//ALLEGRO_JOYSTICK *js;
	float center[2];
} player;

static player *players;
static int myPlayer;
static int numPlayers;

//TODO: New control scheme. For now, stick with the idea that every player has a single ent (no entering other things)

int p1Codes[numKeys] = {ALLEGRO_KEY_A, ALLEGRO_KEY_D, ALLEGRO_KEY_W, ALLEGRO_KEY_S, ALLEGRO_KEY_SPACE};
char p1Keys[numKeys];

scheme *sc;

static ALLEGRO_DISPLAY *display;
static ALLEGRO_EVENT_QUEUE *queue;

#define displayWidth 1000
#define displayHeight 700
#define mouseSensitivity 0.15

// Person cube radius is 16, and with a 90 deg FOV the closest
// something touching our surface could be while still being onscreen
// is 16/sqrt(2) = approx 11.3
#define nearPlane (11*PTS_PER_PX)
// Pretty arbitrary; remember that increasing the ratio of far/near decreases z-buffer accuracy
#define farPlane (1500*PTS_PER_PX)

static double viewYaw = 0;
static double viewPitch = 0;
static int mouseX = 0;
static int mouseY = 0;
static char thirdPerson = 0;

//static ALLEGRO_VERTEX vertices[4];
//const int indices[6] = {0, 1, 2, 3, 2, 1};
//static int32_t cx = (displayWidth / 2) * PTS_PER_PX;
//static int32_t cy = (displayHeight / 2) * PTS_PER_PX;

#define micro_hist_num 5
static int historical_micros[micro_hist_num];
static int micro_hist_ix = micro_hist_num-1;

static void setupFrame() {
	// https://github.com/liballeg/allegro5/blob/b70f37412a082293f26e86ff9c0b6ac7c151d2d0/examples/ex_gldepth.c
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glFrustum(
		// 90 deg horizontal FOV
		-nearPlane, nearPlane,
		// Adjust for aspect ratio
		nearPlane*displayHeight/displayWidth, -nearPlane*displayHeight/displayWidth,
		nearPlane, farPlane
	);
	ent *p = players[myPlayer].entity;
	if (p && thirdPerson) {
		glTranslatef(0, 32*PTS_PER_PX, -64*PTS_PER_PX);
	}
	glRotated(viewPitch, -1, 0, 0);
	glRotated(viewYaw, 0, 1, 0);
	if (p) {
		glTranslatef(-p->center[0], -p->center[2], -p->center[1]);
	} else {
		glTranslatef(0, 0, -1000*PTS_PER_PX);
	}
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
}

void rect(int32_t *p, int32_t *r, float red, float grn, float blu) {
	// TODO If we want to get really fancy this could probably all go
	//      in a geometry shader or something, or maybe attach color
	//      information and do it all as one big TRIANGLES call?
	//      At the very least we only have to send 3 faces per cube if we're smart
#define L glVertex3i(p[0]-r[0],
#define R glVertex3i(p[0]+r[0],
#define U p[2]-r[2],
#define D p[2]+r[2],
#define F p[1]-r[1])
#define B p[1]+r[1])
	// Top is full color
	glColor3f(red, grn, blu);
	glBegin(GL_TRIANGLE_STRIP);
	L U F;
	R U F;
	L U B;
	R U B;
	glEnd();
	// Front is dimmer
	glColor3f(.85 * red, .85 * grn, .85 * blu);
	glBegin(GL_TRIANGLE_STRIP);
	L U F;
	L D F;
	R U F;
	R D F;
	glEnd();
	// Sides are a bit dimmer
	glColor3f(.75 * red, .75 * grn, .75 * blu);
	glBegin(GL_TRIANGLE_STRIP);
	L U B;
	L D B;
	L U F;
	L D F;
	glEnd();
	glBegin(GL_TRIANGLE_STRIP);
	R D F;
	R D B;
	R U F;
	R U B;
	glEnd();
	// Back is dimmer still
	glColor3f(.65 * red, .65 * grn, .65 * blu);
	glBegin(GL_TRIANGLE_STRIP);
	L U B;
	R U B;
	L D B;
	R D B;
	glEnd();
	// Bottom is dimmest
	glColor3f(.50 * red, .50 * grn, .50 * blu);
	glBegin(GL_TRIANGLE_STRIP);
	L D F;
	L D B;
	R D F;
	R D B;
	glEnd();
}

#define micros_per_frame (1000000 / FRAMERATE)
/*
static void drawMicroHist() {
	int sum = 0;
	int max = 0;
	int i;
	for (i = 0; i < micro_hist_num; i++) {
		int x = historical_micros[i];
		sum += x;
		if (x > max) max = x;
	}
	ALLEGRO_COLOR c = al_map_rgb_f(1, 1, 1);
	// TODO This used to be so nice! Even if I was drawing half the bars offscreen.
	//      Maybe some other visual tick time indicator could be good?
	//rect_inner(0, displayHeight-15, 0, displayWidth*sum/(micros_per_frame * micro_hist_num), 5, c);
	//rect_inner(0, displayHeight-5, 0, displayWidth*max/micros_per_frame, 5, c);
}
*/

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
	if (data[0]) pushBtn1(e);
	if (data[1] || data[2]) {
		int axis[2] = {data[1], data[2]};
		pushAxis1(e, axis);
	}
	// data[3] - data[5] unused for the moment, represent look direction
}

static void sendControls(int frame) {
	char data[8];
	data[0] = (char) frame;
	data[1] = 6;
	data[2] = p1Keys[4]; // Other buttons also go here, once they exist; bitfield

	int axis1 = p1Keys[1] - p1Keys[0];
	int axis2 = p1Keys[3] - p1Keys[2];
	double angle = viewYaw * M_PI / 180;
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
	double pitchRadians = viewPitch * M_PI / 180;
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
	data[5] = round(cosine / divisor);
	data[6] = round(sine / divisor);
	data[7] = round(pitchSine / divisor);
	sendData(data, 8);
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
				viewPitch = 0;
				viewYaw = 90;
			}
		}
		if (p->entity->dead) {
			p->entity = NULL;
			p->reviveCounter = FRAMERATE * 3;
			if (i == myPlayer) {
				viewPitch = viewYaw = 0;
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
	if (viewYaw >= 180) viewYaw -= 360;
	else if (viewYaw < -180) viewYaw += 360;
	viewPitch += dy * mouseSensitivity;
	if (viewPitch > 90) viewPitch = 90;
	else if (viewPitch < -90) viewPitch = -90;
}

static void doGravity() {
	int32_t grav[3] = {0, 0, 8};
	ent *i;
	for (i = rootEnts; i; i = i->LL.n) {
		if (!(i->typeMask & T_WEIGHTLESS)) uVel(i, grav);
	}
}

static void doLava() {
	ent *i;
	for (i = ents; i; i = i->ll.n) {
		if (i->center[2] > 2000 * PTS_PER_PX) crushEnt(i);
	}
}

static void* gameEngineThread(void *arg) {
	char mouse_grabbed = 0;
	ALLEGRO_EVENT evnt;
	while (1) {
		al_wait_for_event(queue, &evnt);
		switch(evnt.type) {
			case ALLEGRO_EVENT_DISPLAY_CLOSE:
				// Forces main thread to exit as well
				closeSocket();
				return NULL;
#ifndef MANUAL_STEP
			case ALLEGRO_EVENT_TIMER:
				break;
#endif
			case ALLEGRO_EVENT_KEY_UP:
				handleKey(evnt.keyboard.keycode, 0);
				break;
			case ALLEGRO_EVENT_KEY_DOWN:
				if (evnt.keyboard.keycode == ALLEGRO_KEY_ESCAPE) {
					al_ungrab_mouse();
					al_show_mouse_cursor(display);
					mouse_grabbed = 0;
				}
				handleKey(evnt.keyboard.keycode, 1);
				break;
			case ALLEGRO_EVENT_MOUSE_BUTTON_DOWN:
				if (!mouse_grabbed) {
					if ( (mouse_grabbed = al_grab_mouse(display)) ) {
						al_hide_mouse_cursor(display);
					}
					break;
				}
				break;
			case ALLEGRO_EVENT_MOUSE_LEAVE_DISPLAY:
				mouse_grabbed = 0;
				al_show_mouse_cursor(display);
				break;
			case ALLEGRO_EVENT_MOUSE_BUTTON_UP:
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
				sendControls(frame);
				break;
		}
	}
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
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glFrontFace(GL_CW); // Apparently I'm bad at working out windings in my head, easier to flip this than fix everything else
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

	//Main loop
	pthread_t engineThread;
	{
		int ret = pthread_create(&engineThread, NULL, &gameEngineThread, display);
		if (ret) {
			printf("pthread_create returned %d\n", ret);
			return 1;
		}
	}
	// TODO: If view direction ever matters, that should carry over rather than have a constant default
	// (buttons, move_x, move_y, look_x, look_y, look_z)
	char defaultData[6] = {0, 0, 0, 32, 0, 0};
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
		//gettimeofday(&t1, NULL);
		doGravity();
		doLava();
		// Heroes must be after Lava, or they can be killed and then cleaned up immediately
		doHeroes();

		for (int i = 0; i < numPlayers; i++) {
			char size;
			if (readData(&size, 1)) goto done;
			char *data;
			if (size == 0) {
				data = defaultData;
			} else if (size == 6) {
				data = actualData;
				if (readData(data, 6)) goto done;
			} else {
				printf("Fatal error, can't handle player net input of size %d\n", size);
				goto done;
			}
			if (players[i].entity != NULL) {
				doInputs(players[i].entity, data);
			}
		}
		doPhysics();
		setupFrame();
		doDrawing();
		//drawMicroHist();
		al_flip_display();
		//gettimeofday(&t2, NULL);
		/*{
			int micros = 1000000 * (t2.tv_sec - t1.tv_sec) + t2.tv_usec - t1.tv_usec;
			historical_micros[micro_hist_ix = (micro_hist_ix+1)%micro_hist_num] = micros;
		}*/
	}
	done:;
	puts("Beginning cleanup.");
	puts("Cancelling main engine thread...");
	{
		int ret = pthread_cancel(engineThread);
		if (ret) printf("Error cancelling engine thread: %d\n", ret);
		ret = pthread_join(engineThread, NULL);
		if (ret) printf("Error while joining engine thread: %d\n", ret);
	}
	puts("Done.");
	closeSocket();
	al_destroy_user_event_source(&customSrc);
	al_destroy_event_queue(queue);
	al_destroy_display(display);
	scheme_deinit(sc);
	free(sc);
	delete[] players;
	puts("Cleanup complete, goodbye!");
	return 0;
}
