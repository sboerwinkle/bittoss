#pragma once

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/**
 * Just a thin wrapper around a pointer. Effectively an adapter for things that
 * expect to be able to `init`/`destroy` items (e.g. `queue`s).
 */
template <typename T, size_t S> class bloc {
public:
	T *items;
	void init();
	void destroy();
};

template <typename T, size_t S>
void bloc<T, S>::init() {
	items = (T*)malloc(S);
}

template <typename T, size_t S>
void bloc<T, S>::destroy() {
	free(items);
}
