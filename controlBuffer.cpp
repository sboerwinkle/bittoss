#include "util.h"

#include "controlBuffer.h"

void controlBuffer::push(char x) {
	if (x != latest && s != CB_SIZE) {
		d[s++] = x;
	}
	latest = x;
}

char controlBuffer::pop() {
	if (!s) return latest;

	char ret = d[0];
	// Could be `memmove` I guess
	range(i, s-1) d[i] = d[i+1];

	if (s == CB_SIZE && d[CB_SIZE-2] != latest) {
		d[CB_SIZE-1] = latest;
	} else {
		s--;
	}

	return ret;
}

void controlBuffer::consume(controlBuffer *o) {
	int avail = CB_SIZE - s;
	if (o->s < avail) avail = o->s;
	range(i, avail) {
		d[s+i] = o->d[i];
	}
	s += avail;
	o->s = 0;
	latest = o->latest;
}

// TODO
