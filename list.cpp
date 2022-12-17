#include "list.h"

template <>
void list<int>::display(FILE* f) const {
	fputc('[', f);
	int i;
	for (i = 0; i < num; i++) {
		if (i) fputs(", ", f);
		fprintf(f, "%d", items[i]);
	}
	fputs("]\n", f);
}

