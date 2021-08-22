#include <math.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <allegro5/allegro.h>
#include <allegro5/allegro_opengl.h>
#include "ent.h"
#include "main.h"
#include "modules.h"

#include "entFuncs.h"
#include "entUpdaters.h"
#include "entGetters.h"

#define numKeys 5
typedef struct player {
	ent *entity;
	int reviveCounter;
	ALLEGRO_JOYSTICK *js;
	float center[2];
} player;

static player *players;
static int numPlayers;

//TODO: New control scheme. For now, stick with the idea that every player has a single ent (no entering other things)

int p1Codes[numKeys] = {ALLEGRO_KEY_A, ALLEGRO_KEY_D, ALLEGRO_KEY_W, ALLEGRO_KEY_S, ALLEGRO_KEY_SPACE};
char p1Keys[numKeys];

scheme *sc;

int32_t gravity;

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
	glRotated(viewPitch, -1, 0, 0);
	glRotated(viewYaw, 0, 1, 0);
	ent *p = players[0].entity;
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

static void doInputs(player *p) {
	int dv[2];
	if (p->js) {
		ALLEGRO_JOYSTICK_STATE status;
		al_get_joystick_state(p->js, &status);
		if (status.button[0]) pushBtn1(p->entity);
		int i;
		for (i = 0; i < 2; i++) {
			dv[i] = axisMaxis * 1.25 * (status.stick[0].axis[i] - p->center[i]);
			if (dv[i] > axisMaxis) dv[i] = axisMaxis;
			else if (dv[i] < -axisMaxis) dv[i] = -axisMaxis;
		}
	} else {
		if (p1Keys[4]) pushBtn1(p->entity);
		int i;
		for (i = 0; i < 2; i++) {
			dv[i] = p1Keys[2*i+1] - p1Keys[2*i];
		}
		if (dv[0] || dv[1]) {
			double angle = viewYaw * M_PI / 180;
			double cosine = cos(angle);
			double sine = sin(angle);
			double r_x = cosine * dv[0] - sine * dv[1];
			double r_y = cosine * dv[1] + sine * dv[0];
			double mag_x = abs(r_x);
			double mag_y = abs(r_y);
			// Normally messy properties of floating point division
			// are mitigated by the fact that `axisMaxis` is a power of 2
			double divisor = (mag_x > mag_y ? mag_x : mag_y) / axisMaxis;
			dv[0] = round(r_x / divisor);
			dv[1] = round(r_y / divisor);
		}
	}
	if (dv[0] == 0 && dv[1] == 0) return;
	pushAxis1(p->entity, dv);
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
			viewPitch = 0;
			viewYaw = 90;
		}
		if (p->entity->dead) {
			p->entity = NULL;
			p->reviveCounter = FRAMERATE * 3;
			viewPitch = viewYaw = 0;
			continue;
		}
		doInputs(p);
	}
}

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

char handleKey(int code, char pressed) {
	int i;
	for (i = numKeys-1; i >= 0; i--) {
		if (p1Codes[i] == code) {
			p1Keys[i] = pressed;
			return true;
		}
	}
	if (pressed && code == ALLEGRO_KEY_BACKSLASH) centerJoysticks();
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

int main(int argc, char **argv) {
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
	ALLEGRO_DISPLAY *display = al_create_display(displayWidth, displayHeight);
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
	//Player setup
	players = new player[numPlayers];
	int i;
	players[0].js = NULL;
	memset(p1Keys, 0, sizeof(p1Keys));
	for (i = 1; i < numPlayers; i++) {
		players[i].js = al_get_joystick(i-1);
	}
	for (i = 0; i < numPlayers; i++) {
		players[i].reviveCounter = 0;
		players[i].entity = NULL;
	}
	centerJoysticks();
	//Set up other modules
	initMods();
	//map loading
	loadFile("map.scm");
	//Events
	ALLEGRO_TIMER *timer = al_create_timer(ALLEGRO_BPS_TO_SECS(FRAMERATE));
	ALLEGRO_EVENT_QUEUE *queue = al_create_event_queue();
	al_register_event_source(queue, al_get_display_event_source(display));
	al_register_event_source(queue, al_get_timer_event_source(timer));
	al_register_event_source(queue, al_get_keyboard_event_source());
	char mouse_grabbed = 0;
	if (al_is_mouse_installed()) {
		al_register_event_source(queue, al_get_mouse_event_source());
		// For now, don't capture mouse on startup. Might prevent mouse warpiness in first frame, idk, feel free to mess around
		/*
		if ( (mouse_grabbed = al_grab_mouse(display)) ) {
			al_hide_mouse_cursor(display);
		}
		*/
	}

	//Main loop
	al_start_timer(timer);
	ALLEGRO_EVENT evnt;
	ent *selectedEnt = NULL;
	struct timeval t1, t2;
	while (1) {
		al_wait_for_event(queue, &evnt);
		switch(evnt.type) {
			case ALLEGRO_EVENT_DISPLAY_CLOSE:
				return 0;
				break;
#ifndef MANUAL_STEP
			case ALLEGRO_EVENT_TIMER:
				gettimeofday(&t1, NULL);
				doGravity();
				doLava();
				// Heroes must be after Lava, or they can be killed and then cleaned up immediately
				doHeroes();
				doPhysics();
				setupFrame();
				doDrawing();
				//drawMicroHist();
				al_flip_display();
				gettimeofday(&t2, NULL);
				{
					int micros = 1000000 * (t2.tv_sec - t1.tv_sec) + t2.tv_usec - t1.tv_usec;
					historical_micros[micro_hist_ix = (micro_hist_ix+1)%micro_hist_num] = micros;
				}
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
#ifdef MANUAL_STEP
... Then this all needs to be updated
				doPhysics();
				doDrawing(p1.entity);
				for (i = 0; i < numLayers; i++) {
					for (j = layerNums[i]-1; j >= 0; j--) {
						struct sprite *s = layers[i] + j;
						al_draw_bitmap(s->img, s->x, s->y, 0);
					}
					layerNums[i] = 0;
				}
				al_flip_display();
#endif
				break;
			case ALLEGRO_EVENT_MOUSE_BUTTON_DOWN:
				if (!mouse_grabbed) {
					if ( (mouse_grabbed = al_grab_mouse(display)) ) {
						al_hide_mouse_cursor(display);
					}
					break;
				}
				/*
				{
					int x = evnt.mouse.x * PTS_PER_PX;
					int y = evnt.mouse.y * PTS_PER_PX;
					for (ent* e = ents; e; e = e->ll.n) {
						// TODO we can get more accuate sub-pixel selection if we want
						if (abs(e->center[0] - x) < e->radius[0] && abs(e->center[1] - y) < e->radius[1]) {
							selectedEnt = e;
							break;
						}
					}
				}
				*/
				break;
			case ALLEGRO_EVENT_MOUSE_LEAVE_DISPLAY:
				selectedEnt = NULL;
				mouse_grabbed = 0;
				al_show_mouse_cursor(display);
				break;
			case ALLEGRO_EVENT_MOUSE_BUTTON_UP:
				/*
				{
					save_from_C_call(sc);
					pointer owner;
					if (selectedEnt) {
						owner = mk_c_ptr(sc, selectedEnt, 0);
						selectedEnt = NULL;
					} else {
						sc->NIL->references++;
						owner = sc->NIL;
					}
					sc->NIL->references += 2;
					scheme_eval(sc,
						cons(sc, mk_symbol(sc, "move-to-test"),
						cons(sc, owner,
						cons(sc,
							cons(sc, mk_symbol(sc, "list"),
							cons(sc, mk_integer(sc, evnt.mouse.x*PTS_PER_PX),
							cons(sc, mk_integer(sc, evnt.mouse.y*PTS_PER_PX),
							sc->NIL))),
						sc->NIL)))
					);
				}
				*/
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
		}
	}
	al_destroy_timer(timer);
	al_destroy_event_queue(queue);
	al_destroy_display(display);
	scheme_deinit(sc);
	free(sc);
	delete[] players;
	return 0;
}
