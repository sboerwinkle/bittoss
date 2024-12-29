
extern char writeFile(const char *name, const list<char> *data);
extern char readFile(const char *name, list<char> *out);

// Like `writeFile`, but doesn't restrict to `data/` directory.
extern char writeFileArbitraryPath(const char *name, const list<char> *data);

extern void file_init();
extern void file_destroy();
