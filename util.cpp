#include <stdlib.h>

char getNum(const char **c, int32_t *out) {
	char *d;
	*out = strtol(*c, &d, 0);
	if (*c != d) {
		*c = d;
		return 1;
	}
	return 0;
}
