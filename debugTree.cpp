#include <stdio.h>

#include "ent.h"
#include "box.h"
#include "handlerRegistrar.h"

static void indent(list<char> *i) {
	i->num--;
	i->add(' ');
	i->add(' ');
	i->add('\0');
}

static void unindent(list<char> *i) {
	i->num -= 3;
	i->add('\0');
}

static void f(box *b, list<char> *i) {
	fputs(i->items, stdout);
	printf("(%d)", b->intersects.num);
	if (b->data) {
		ent *e = (ent*)b->data;
		putchar(' ');
		fputs(drawHandlers.itemToName(e->draw), stdout);
	}
	putchar('\n');
	indent(i);
	range(x, b->kids.num) {
		f(b->kids[x], i);
	}
	unindent(i);
}

void printTree(gamestate *gs) {
	list<char> prefix;
	prefix.init();
	prefix.add('\0');
	box *b = gs->rootBox;
	f(b, &prefix);
	prefix.destroy();
}
