/////////////////////////////////////
// Modified from work by mboerwinkle
/////////////////////////////////////

#include "list.h"

enum elementType {J_OBJ,J_ARR,J_STR,J_NUM,J_BOL,J_NUL};

struct jsonEntry;

struct jsonValue {
	elementType type;
	union {
		list<jsonEntry> obj; // J_OBJ
		list<jsonValue> arr; // J_ARR
		char *str; // J_STR, but also J_NUM actually
		char bol; // J_BOL (boolean)
		// No data required for J_NUL (null)
	} d;

	jsonValue* get(char const *key);
	jsonValue* set(char const *key);
	char rm(char const *key);
	list<jsonValue>* getItems();
	char const* getString();
	int getInt();
	double getDouble();
	char getBool();

	void initObj();
	void initArr();
	void initStr(char *str);
	void initNum(char *str);
	void initBol(char bol);
	void initNul();

	void destroy();
};

struct jsonEntry {
	char *key;
	jsonValue value;
};

extern jsonValue* jsonInterpret(char* data, int dataLen);
extern jsonValue* jsonLoad(FILE* fp);
/**
 * Setting `indent` to 0 will result in slightly prettier JSON,
 * while setting it to -1 will result in dense JSON.
 */
extern void jsonSerialize(list<char> *data, jsonValue *v, int indent);
extern char const* typeStr(elementType t);
