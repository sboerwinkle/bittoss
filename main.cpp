#include <stdio.h>
#include <string.h>
#include <allegro5/allegro.h>
#include <allegro5/allegro_image.h>
#include <allegro5/allegro_primitives.h>
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

#define displayWidth 1024
#define displayHeight 1024

static ALLEGRO_VERTEX vertices[4];
//const int indices[6] = {0, 1, 2, 3, 2, 1};
//static int32_t cx = (displayWidth / 2) * PTS_PER_PX;
//static int32_t cy = (displayHeight / 2) * PTS_PER_PX;

void rect(int32_t X, int32_t Y, float z, int32_t RX, int32_t RY, float r, float g, float b) {
	if (X < 0) X -= PTS_PER_PX - 1;
	if (Y < 0) Y -= PTS_PER_PX - 1;
	int32_t x = X / PTS_PER_PX;
	int32_t y = Y / PTS_PER_PX;
	int32_t rx = RX / PTS_PER_PX;
	int32_t ry = RY / PTS_PER_PX;
	ALLEGRO_COLOR c = al_map_rgb_f(r, g, b);

	vertices[0].color = vertices[1].color = vertices[2].color = vertices[3].color = c;
	vertices[0].z = vertices[1].z = vertices[2].z = vertices[3].z = z;

	vertices[0].x = x - rx;
	vertices[0].y = y - ry;

	vertices[1].x = x + rx;
	vertices[1].y = y - ry;

	vertices[2].x = x - rx;
	vertices[2].y = y + ry;

	vertices[3].x = x + rx;
	vertices[3].y = y + ry;

	al_draw_prim(vertices, NULL, NULL, 0, 4, ALLEGRO_PRIM_TRIANGLE_STRIP);
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
			dv[i] = axisMaxis * (p1Keys[2*i+1] - p1Keys[2*i]);
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
		}
		if (p->entity->dead) {
			p->entity = NULL;
			p->reviveCounter = FRAMERATE * 3;
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

static void doGravity() {
	int32_t grav[2] = {0, 8};
	ent *i;
	for (i = rootEnts; i; i = i->LL.n) {
		if (!(i->typeMask & T_WEIGHTLESS)) uVel(i, grav);
	}
}

static void doLava() {
	ent *i;
	for (i = ents; i; i = i->ll.n) {
		if (i->center[1] > 2000 * PTS_PER_PX) crushEnt(i);
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
	if (al_install_joystick()) {
		numPlayers = al_get_num_joysticks() + 1;;
	} else {
		numPlayers = 1;
		fputs("Couldn't install joystick driver\n", stderr);
	}
	ALLEGRO_DISPLAY *display = al_create_display(displayWidth, displayHeight);
	if (!display) {
		fputs("Couldn't create our display\n", stderr);
		return 1;
	}
	if (!al_init_primitives_addon()) {
		fputs("Couldn't init Allegro primitives addon\n", stderr);
		return 1;
	}
	if (!al_install_keyboard()) {
		fputs("No keyboard support, which would make the game unplayable\n", stderr);
		return 1;
	}
	if (!al_install_mouse()) {
		fputs("No mouse support.\n", stderr);
	}
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
	if (al_is_mouse_installed()) {
		al_register_event_source(queue, al_get_mouse_event_source());
	}
	//Main loop
	al_start_timer(timer);
	ALLEGRO_EVENT evnt;
	ent *selectedEnt = NULL;
	while (1) {
		al_wait_for_event(queue, &evnt);
		switch(evnt.type) {
			case ALLEGRO_EVENT_DISPLAY_CLOSE:
				return 0;
				break;
#ifndef MANUAL_STEP
			case ALLEGRO_EVENT_TIMER:
				doGravity();
				doLava();
				// Heroes must be after Lava, or they can be killed and then cleaned up immediately
				doHeroes();
				doPhysics();
				al_clear_to_color(al_map_rgb(0, 0, 0));
				doDrawing();
				al_flip_display();
				break;
#endif
			case ALLEGRO_EVENT_KEY_UP:
				handleKey(evnt.keyboard.keycode, 0);
				break;
			case ALLEGRO_EVENT_KEY_DOWN:
				handleKey(evnt.keyboard.keycode, 1);
#ifdef MANUAL_STEP
... Then this all needs to be updated
				doPhysics();
				doDrawing(p1.entity);
				al_clear_to_color(al_map_rgb(0, 0, 0));
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
				break;
			case ALLEGRO_EVENT_MOUSE_LEAVE_DISPLAY:
				selectedEnt = NULL;
				break;
			case ALLEGRO_EVENT_MOUSE_BUTTON_UP:
				if (selectedEnt) {
					save_from_C_call(sc);
					scheme_eval(sc,
						cons(sc, mk_symbol(sc, "move-to-test"),
						cons(sc, mk_c_ptr(sc, selectedEnt, 0),
						cons(sc,
							cons(sc, mk_symbol(sc, "list"),
							cons(sc, mk_integer(sc, evnt.mouse.x*PTS_PER_PX),
							cons(sc, mk_integer(sc, evnt.mouse.y*PTS_PER_PX),
							sc->NIL))),
						sc->NIL)))
					);
					selectedEnt = NULL;
				}
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
