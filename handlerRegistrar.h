
extern void init_registrar();

extern void destroy_registrar();


extern void regWhoMovesHandler(const char* name, whoMoves_t handler);

extern whoMoves_t getWhoMovesHandler(int ix);

extern void regPushedHandler(const char* name, pushed_t handler);

extern pushed_t getPushedHandler(int ix);
