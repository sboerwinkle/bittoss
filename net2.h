
// This should match server.py's `MAX_AHEAD`.
#define MAX_AHEAD 15
#define FRAME_ID_MAX (1<<29)

extern pthread_mutex_t netMutex;
extern pthread_cond_t netCond;

// This is the stuff we're going to mutex lock
// ===========================================

extern queue<list<list<char>>> frameData;
extern int finalizedFrames;
extern char asleep;

// ============= End mutex lock ==============

extern void* netThreadFunc(void *startFrame);

extern void net2_init(int numPlayers);
extern void net2_destroy();
