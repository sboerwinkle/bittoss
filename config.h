extern char const* config_getName();
extern char const* config_getColor();
extern char const* config_getHost();
extern void config_setName(char const* str);
extern void config_setColor(char const* str);
extern void config_setHost(char const* str);

extern void config_init();
extern void config_destroy();
extern void config_write();
