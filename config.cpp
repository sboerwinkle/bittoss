#include <errno.h>
#include <stdio.h>
#include <sys/stat.h>

#include "util.h"
#include "json.h"
#include "file.h"

#define BUF_LEN 200

static char configDir[BUF_LEN];
static char configFile[BUF_LEN];
static char modified;

static char name[BUF_LEN];
static char color[BUF_LEN];
static char host[BUF_LEN];
static char port[BUF_LEN];

#define NUM_KEYS 4
static struct {
	const char *jsonKey;
	char *dest;
} mappings[NUM_KEYS] = {
	{.jsonKey="name", .dest=name},
	{.jsonKey="color", .dest=color},
	{.jsonKey="host", .dest=host},
	{.jsonKey="port", .dest=port},
};

///// getters & setters for config properties /////
// (Getters just add a "const" qualifier)

char const* config_getName() {
	return name;
}

char const* config_getColor() {
	return color;
}

char const* config_getHost() {
	return host;
}

char const* config_getPort() {
	return port;
}

static void setConfigVal(char *dest, char const *src) {
	if (strcmp(dest, src)) {
		snprintf(dest, BUF_LEN, "%s", src);
		modified = 1;
	}
}

void config_setName(char const* str) {
	setConfigVal(name, str);
}

void config_setColor(char const* str) {
	setConfigVal(color, str);
}

void config_setHost(char const* str) {
	setConfigVal(host, str);
}

void config_setPort(char const* str) {
	setConfigVal(port, str);
}

///// Logic for the on-disk config file /////

static void consumeString(jsonValue *root, char const *key, char *destBuf) {
	jsonValue *v = root->get(key);
	if (!v) return;
	if (v->type != J_STR) {
		fprintf(stderr, "Config value for key '%s' should have type J_STR but is %s\n", key, typeStr(v->type));
		return;
	}
	snprintf(destBuf, BUF_LEN, "%s", v->getString());
	root->rm(key);
}

void config_init() {
	// Set up some defaults, so if anything goes awry we can safely just return.
	configDir[0] = '\0';
	configFile[0] = '\0';
	modified = 0;
	name[0] = color[0] = host[0] = port[0] = '\0';

	char const *xdg_config_home = getenv("XDG_CONFIG_HOME");
	if (xdg_config_home) {
		printf("XDG_CONFIG_HOME='%s'\n", xdg_config_home);
		snprintf(configDir, BUF_LEN, "%s/bittoss", xdg_config_home);
	} else {
		puts("XDG_CONFIG_HOME not set, checking HOME.");
		char const *home = getenv("HOME");
		if (home) {
			printf("HOME='%s'\n", home);
			snprintf(configDir, BUF_LEN, "%s/.config/bittoss", home);
		} else {
			puts("HOME not set either, config files will be stored one level above `data/`.");
			strcpy(configDir, "..");
		}
	}
	snprintf(configFile, 200, "%s/config.json", configDir);
	printf("Using config file path: '%s'\n", configFile);

	FILE* configStream = fopen(configFile, "r");
	if (!configStream) {
		// This doesn't go to `stderr` only because this is perfectly normal for a first launch
		printf("Couldn't open config file. No configs will be loaded, but this isn't fatal. Error is %s (%s).\n", strerrorname_np(errno), strerror(errno));
		return;
	}
	jsonValue *configJson = jsonLoad(configStream);
	fclose(configStream);

	if (configJson->type != J_OBJ) {
		// The methods in json.cpp do all the type checking for you,
		// but we're going to get our hands dirty later with the
		// struct internals, so we need an explicit type check here.
		fprintf(stderr, "Config file top-level item should be J_OBJ but is %s. No configs will be loaded.\n", typeStr(configJson->type));
	} else {
		range(i, NUM_KEYS) {
			consumeString(configJson, mappings[i].jsonKey, mappings[i].dest);
		}

		range(i, configJson->d.obj.num) {
			// This is phrased to be as informative and accurate as possible, but has to cover a few scenarios.
			// It could be an unknown key, or it could be a known key but the type was wrong (e.g. J_NUM not J_STR).
			// It will be erased if the file is written, but the file might not be written (if no config changes happen).
			fprintf(stderr, "Config key '%s' is unused\n", configJson->d.obj[i].key);
		}
		puts("Config loaded.");
	}

	configJson->destroy();
	free(configJson);
}

void config_destroy() {
	// We have some strings, but they're all statically allocated so nothing to do here!
}

void config_write() {
	if (!modified) {
		puts("No config options changed, not writing config file.");
		return;
	}
	modified = 0;

	if (!*configFile) {
		// Pretty sure this is unreachable currently.
		puts("Config file path somehow not set during startup, not writing config file.");
		return;
	}

	// Try to create the immediate parent dir,
	// since in a typical "~/.config/bittoss/config.json" setup it likely won't exist the first time.
	if (mkdir(configDir, 0755) && errno != EEXIST) {
		fprintf(stderr, "Tried to create parent dir at '%s' for config file, but `mkdir` gave error %s (%s). Not writing config file.\n", configDir, strerrorname_np(errno), strerror(errno));
		return;
	}

	jsonValue root;
	root.initObj();
	range(i, NUM_KEYS) {
		if (*mappings[i].dest) {
			root.set(mappings[i].jsonKey)->initStr(strdup(mappings[i].dest));
		}
	}

	list<char> buffer;
	buffer.init();
	jsonSerialize(&buffer, &root, 0);
	buffer.add('\n');
	root.destroy();

	writeFileArbitraryPath(configFile, &buffer);
	buffer.destroy();
}
