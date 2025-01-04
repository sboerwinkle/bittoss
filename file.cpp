#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include "list.h"
#include "file.h"

void file_init() {
	// All operations will be out of the `data/` directory from here on
	if (chdir("data")) {
		fprintf(stderr, "Failed to set working directory to ./data - `chdir` gave error %s (%s)\n", strerrorname_np(errno), strerror(errno));
		exit(1);
	}
	puts("Set working directory to 'data/'");
}

void file_destroy() {
}

static const char* resolvePath(const char* path) {
	if (*path == '/') {
		fprintf(stderr, "ERROR - path '%s' is absolute, but must be relative.\n", path);
		return NULL;
	}
	if (strstr(path, "..")) {
		fprintf(stderr, "ERROR - path '%s' may not contain the sequence '..'\n", path);
		return NULL;
	}
	// File contents are sometimes sent over the network (for /load), which is a
	// potential security issue. This was addressed by making commands that accept
	// path arguments be processed entirely locally - paths are never read from network data.
	//
	// Now we are considering certain game events triggering filesystem interactions.
	// While we are still not accepting arbitrary file names over the network in any capacity,
	// it seems prudent to add another level of protection.
	//
	// The checks above are simple, but simple is good. So long as someone hasn't blindly
	// ported this code to a system that doesn't use Unix paths, and so long as you haven't
	// put anything weird in `data/`, this should be watertight.
	// ("weird" in this context means symlinks or hard links to places you don't want
	//  arbirary reads/writes to.)
	// See also `man 7 path_resolution`.
	return path;
}

char writeFile(const char *name, const list<char> *data) {
	name = resolvePath(name);
	if (!name) return 1;
	return writeFileArbitraryPath(name, data);
}

char writeFileArbitraryPath(const char *name, const list<char> *data) {
	int fd = open(name, O_WRONLY | O_CREAT | O_TRUNC, 0664); // perms: rw-rw-r--
	if (fd == -1) {
		fprintf(stderr, "ERROR writing file '%s': `open` gave error %s (%s)\n", name, strerrorname_np(errno), strerror(errno));
		return 1;
	}
	int ret = write(fd, data->items, data->num);
	if (ret != data->num) {
		fprintf(stderr, "ERROR writing file '%s': `write` returned %d when %d was expected.\n", name, ret, data->num);
		// We could always retry a partial write but that's more effort that I'm not sure is necessary
		if (ret == -1) {
			fprintf(stderr, "Error is %s (%s)\n", strerrorname_np(errno), strerror(errno));
		}
		return 1;
	}
	close(fd);
	return 0;
}

char readFile(const char *name, list<char> *out) {
	name = resolvePath(name);
	if (!name) return 1;

	int fd = open(name, O_RDONLY);
	if (fd == -1) {
		fprintf(stderr, "ERROR reading file '%s': `open` gave error %s (%s)\n", name, strerrorname_np(errno), strerror(errno));
		return 1;
	}
	int ret;
	do {
		out->setMaxUp(out->num + 1000);
		ret = read(fd, out->items + out->num, 1000);
		if (ret == -1) {
			fprintf(stderr, "ERROR reading file '%s': `read` gave error %s (%s)\n", name, strerrorname_np(errno), strerror(errno));
			break;
		}
		out->num += ret;
	} while (ret);
	close(fd);
	return ret == -1;
}
