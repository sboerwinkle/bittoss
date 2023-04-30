#pragma once

#include <stdlib.h>
#include <string.h>

// T should be init/destroy-able
template <typename T> struct queue {
	T *items;
	int start, end, max;
	void init(int size = 1);
	void destroy();
	T& add();
	T& pop();
	T& peek(int ix);
	void multipop(int num);
	int size() const;
private:
	void resize_full();
};

// Need to init the entries as they're created, which is a very different approach from how `list` handles things.

template <typename T>
void queue<T>::init(int size) {
	items = (T*)malloc(sizeof(T)*size);
	start = end = 0;
	max = size;
	for (int i = 0; i < max; i++) items[i].init();
}

template <typename T>
void queue<T>::destroy() {
	for (int i = 0; i < max; i++) items[i].destroy();
	free(items);
}

template <typename T>
T& queue<T>::add() {
	end = (end+1) % max;
	if (end == start) {
		resize_full();
	}
	if (end == 0) return items[max-1];
	return items[end-1];
}

template <typename T>
T& queue<T>::pop() {
#ifndef NODEBUG
	if (start == end) {
		fputs("Queue is empty but `pop()` was requested, this is a bug! Things will probably be corrupted now!\n", stderr);
	}
#endif
	T& ret = items[start];
	start = (start + 1) % max;
	return ret;
}

template <typename T>
T& queue<T>::peek(int ix) {
#ifndef NODEBUG
	if (ix >= size()) {
		fprintf(stderr, "Requested index %d, but size is only %d. Bad queue data returned\n", ix, size());
	}
#endif
	return items[(start + ix) % max];
}

template <typename T>
void queue<T>::multipop(int num) {
#ifndef NODEBUG
	if (num > size()) {
		fprintf(stderr, "Popped %d items off of queue, but size was only %d. Queue is now corrupted!\n", num, size());
	}
#endif
	start = (start + num) % max;
}

template <typename T>
int queue<T>::size() const {
	if (start <= end) return end - start;
	return end - start + max;
}

///// Private

template <typename T>
void queue<T>::resize_full() {
	int incr = max;
	int newMax = max + incr;
	items = (T*)realloc(items, newMax*sizeof(T));
	for (int i = max-1; i >= start; i--) {
		items[i + incr] = items[i];
	}
	start += incr;
	for (int i = end; i < start; i++) items[i].init();
	max = newMax;
}

