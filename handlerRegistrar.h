
extern void init_registrar();
extern void destroy_registrar();

extern int handlerByName(const char* name);

extern void regWhoMovesHandler(const char* name, whoMoves_t handler);
extern whoMoves_t getWhoMovesHandler(int ix);

extern void regPushedHandler(const char* name, pushed_t handler);
extern pushed_t getPushedHandler(int ix);

extern void regPushHandler(const char* name, push_t handler);
extern push_t getPushHandler(int ix);
