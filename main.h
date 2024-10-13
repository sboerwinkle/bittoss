#pragma once

#include <cstdint>
#include "ent.h"

#define lock(mtx) if (int __ret = pthread_mutex_lock(&mtx)) printf("Mutex lock failed with code %d\n", __ret)
#define unlock(mtx) if (int __ret = pthread_mutex_unlock(&mtx)) printf("Mutex unlock failed with code %d\n", __ret)
#define wait(cond, mtx) if (int __ret = pthread_cond_wait(&cond, &mtx)) printf("Mutex cond wait failed with code %d\n", __ret)
#define signal(cond) if (int __ret = pthread_cond_signal(&cond)) printf("Mutex cond signal failed with code %d\n", __ret)

void showMessage(gamestate const * const gs, char const * const msg);
void requestReload(gamestate const * const gs);

extern char globalRunning;
extern gamestate *rootState;
