#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>

#include "list.h"

char writeFile(const char *name, const list<char> *data) {
	int fd = open(name, O_WRONLY | O_CREAT, 0664); // perms: rw-rw-r--
	if (fd == -1) {
		fprintf(stderr, "ERROR - Save failed - Couldn't write file '%s'\n", name);
		return 1;
	}
	int ret = write(fd, data->items, data->num);
	if (ret != data->num) {
		fprintf(stderr, "ERROR - Save failed - `write` returned %d when %d was expected\n", ret, data->num);
		// We could always retry a partial write but that's more effort that I'm not sure is necessary
		if (ret == -1) {
			fprintf(stderr, "errno is %d\n", errno);
		}
		return 1;
	}
	close(fd);
	return 0;
}

char readFile(const char *name, list<char> *out) {
	int fd = open(name, O_RDONLY);
	if (fd == -1) {
		fprintf(stderr, "ERROR - Load failed - Couldn't read file '%s'\n", name);
		return 1;
	}
	int ret;
	do {
		out->setMaxUp(out->num + 1000);
		ret = read(fd, out->items + out->num, 1000);
		if (ret == -1) {
			fprintf(stderr, "Failed to read from file, errno is %d\n", errno);
			break;
		}
		out->num += ret;
	} while (ret);
	close(fd);
	return ret == -1;
}
