
// This is the stuff we're going to mutex lock
// ===========================================

extern queue<list<list<char>>> frameData;
extern int finalizedFrames;

// ============= End mutex lock ==============

extern void* netThreadFunc(void *startFrame);

extern void net2_init();
extern void net2_destroy();
