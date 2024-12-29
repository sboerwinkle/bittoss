/////////////////////////////////////
// Modified from work by mboerwinkle
/////////////////////////////////////

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "json.h"

// Default build erases all jsonLog() calls, you can add them back if needed!
#define jsonLog(msg, lvl)
/*
#define ERROR_VERBOSITY 3
static void jsonLog(const char* msg, int lvl){
	//jsonLog("func jsonLog", 666);
	if(lvl == 1 && ERROR_VERBOSITY >= 1){
		printf("JSON ERROR: \"%s\"\n", msg);
		getchar();
	}
	if(lvl == 2 && ERROR_VERBOSITY >= 2){
		printf("JSON WARN: \"%s\"\n", msg);
	}
	if(lvl == 3 && ERROR_VERBOSITY >= 3){
		printf("JSON INFO: \"%s\"\n", msg);
	}
	if(lvl == 4 && ERROR_VERBOSITY >= 4){
		printf("JSON STAT: \"%s\"\n", msg);
	}
}
*/

jsonValue* jsonValue::get(char const *key) {
	if (type != J_OBJ) {
		fprintf(stderr, "JSON: `get` needs J_OBJ, got %s\n", typeStr(type));
		return NULL;
	}
	for (int i = 0; i < d.obj.num; i++) {
		if (!strcmp(key, d.obj[i].key)) return &d.obj[i].value;
	}
	return NULL;
}

jsonValue* jsonValue::set(char const *key) {
	if (type != J_OBJ) {
		fprintf(stderr, "JSON: `set` needs J_OBJ, got %s\n", typeStr(type));
		return NULL;
	}
	for (int i = 0; i < d.obj.num; i++) {
		if (!strcmp(key, d.obj[i].key)) {
			d.obj[i].value.destroy();
			return &d.obj[i].value;
		}
	}
	jsonEntry& newEntry = d.obj.add();
	newEntry.key = strdup(key);
	return &newEntry.value;
}

char jsonValue::rm(char const *key) {
	if (type != J_OBJ) {
		fprintf(stderr, "JSON: `rm` needs J_OBJ, got %s\n", typeStr(type));
		return 0;
	}
	for (int i = 0; i < d.obj.num; i++) {
		if (!strcmp(key, d.obj[i].key)) {
			free(d.obj[i].key);
			d.obj[i].value.destroy();
			d.obj.rmAt(i);
			return 1;
		}
	}
	return 0;
}

list<jsonValue>* jsonValue::getItems() {
	if (type != J_ARR) {
		fprintf(stderr, "JSON: `getItems` needs J_ARR, got %s\n", typeStr(type));
		return NULL;
	}
	return &d.arr;
}

char const* jsonValue::getString(){
	if (type != J_STR) {
		fprintf(stderr, "JSON: `getString` needs J_STR, got %s\n", typeStr(type));
		return NULL;
	}
	return d.str;
}

int jsonValue::getInt() {
	if (type != J_NUM) {
		fprintf(stderr, "JSON: `getInt` needs J_NUM, got %s\n", typeStr(type));
		return 0;
	}
	int ret;
	sscanf(d.str, "%d", &ret);
	return ret;
}

double jsonValue::getDouble() {
	if (type != J_NUM) {
		fprintf(stderr, "JSON: `getDouble` needs J_NUM, got %s\n", typeStr(type));
		return 0;
	}
	double ret;
	sscanf(d.str, "%lf", &ret);
	return ret;
}

char jsonValue::getBool() {
	if (type != J_BOL) {
		fprintf(stderr, "JSON: `getBool` needs J_BOL, got %s\n", typeStr(type));
		return 0;
	}
	return d.bol;
}

void jsonValue::initObj() {
	type = J_OBJ;
	d.obj.init();
}

void jsonValue::initArr() {
	type = J_ARR;
	d.arr.init();
}

void jsonValue::initStr(char *str) {
	type = J_STR;
	d.str = str;
}

void jsonValue::initNum(char *str) {
	type = J_NUM;
	d.str = str;
}

void jsonValue::initBol(char bol) {
	type = J_BOL;
	d.bol = bol;
}

void jsonValue::initNul() {
	type = J_NUL;
}

void jsonValue::destroy() {
	switch (type) {
		case J_OBJ:
			for (int i = 0; i < d.obj.num; i++) {
				free(d.obj[i].key);
				d.obj[i].value.destroy();
			}
			d.obj.destroy();
			break;
		case J_ARR:
			for (int i = 0; i < d.arr.num; i++) {
				d.arr[i].destroy();
			}
			d.arr.destroy();
			break;
		case J_STR:
		case J_NUM:
			free(d.str);
			break;
		// J_BOL / J_NUL do not need any special `destroy()` logic
	}
}

static void getNextValue(jsonValue *dest, char* data, int dataLen, int* pos);

#define NEXT(_c) (*pos < dataLen ? data[(*pos)++] : _c)
#define PEEK(_c) (*pos < dataLen ? data[(*pos)] : _c)

int isWhite(char c){
	if(c == 0x0020 || c == 0x000D || c == 0x000A || c == 0x0009){
		return 1;
	}
	return 0;
}

void skipWhite(char *data, int dataLen, int *pos) {
	while (isWhite(PEEK('x'))) (*pos)++;
}

#define SKIP_WHITE() skipWhite(data, dataLen, pos)

char* jsonReadString(char* data, int dataLen, int* pos){
	jsonLog("func jsonReadString", 4);
	list<char> l;
	l.init();

	char c;
	while ('"' != (c = NEXT('"'))) {
		if (c == '\\') {
			c = NEXT('\\');
		}
		l.add(c);
	}
	l.add('\0');

	// No need for `l.destroy()` since we're passing ownership of the memory out of this method
	return l.items;
}

static void jsonReadObject(jsonValue *dest, char* data, int dataLen, int* pos){
	jsonLog("func jsonReadObject", 4);
	dest->initObj();
	list<jsonEntry> &entries = dest->d.obj;
	while (1) {
		SKIP_WHITE();
		char c = NEXT('}');
		if (c == '"') {
			jsonLog("Getting keypair key", 4);
			jsonEntry &entry = entries.add();
			entry.key = jsonReadString(data, dataLen, pos);
			jsonLog("Done getting keypair key", 4);

			SKIP_WHITE();
			c = PEEK(':');
			if (c != ':') {
				fprintf(stderr, "JSON: Object key should be followed by ':', got '%c' (0x%hhx)\n", c, c);
			} else {
				jsonLog("Found ':'", 4);
				(*pos)++;
			}

			jsonLog("Getting keypair data", 4);
			getNextValue(&entry.value, data, dataLen, pos);
			jsonLog("Done getting keypair data", 4);
		} else if (c == '}') {
			return;
		} else if (c != ',') {
			fprintf(stderr, "JSON: Invalid character in Object. Expected one of '\"', '}', or ','; got '%c' (0x%hhx)\n", c, c);
		}
	}
}

static void jsonReadArray(jsonValue *dest, char* data, int dataLen, int* pos){
	jsonLog("func jsonReadArray", 4);
	dest->initArr();
	list<jsonValue> &items = dest->d.arr;

	while(1){
		SKIP_WHITE();
		char c = NEXT(']');
		if (c == ']') {
			return;
		} else if (c != ',') {
			(*pos)--;
			getNextValue(&items.add(), data, dataLen, pos);
		}
	}
}

static int isNumeric(char c){
	return (c >= '0' && c <= '9') || c == '.' || c == '+' || c == '-' || c == 'e' || c == 'E';
}

static void jsonReadNumber(jsonValue *dest, char* data, int dataLen, int* pos){
	jsonLog("func jsonReadNumber", 4);
	int start = *pos;
	while (1) {
		char c = PEEK(',');
		if (isNumeric(c)) (*pos)++;
		else break;
	}

	int size = (*pos)-start;
	char *ret = (char*)malloc(size+1);
	memcpy(ret, data+start, size);
	ret[size] = '\0';

	dest->initNum(ret);
}

static void jsonReadConstant(const char *goal, char *data, int dataLen, int *pos) {
	for (int ix = 0; goal[ix]; ix++) {
		char c = PEEK('_');
		if (c == goal[ix]) {
			(*pos)++;
			continue;
		}

		// Else, handle failure...
		fprintf(stderr, "JSON: Trying to read keyword \"%s\", but only processed %d characters before hitting unexpected char '%c' (0x%hhx)\n", goal, ix, c, c);
		// Clear any letters out of the input before we return to regular processing
		while ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
			(*pos)++;
			c = PEEK('_');
		}
		return;
	}
}

static void getNextValue(jsonValue *dest, char* data, int dataLen, int* pos){
	jsonLog("func getNextValue", 4);
	SKIP_WHITE();

	char c = NEXT('_');
	if (c == '{') { //OBJECT
		jsonReadObject(dest, data, dataLen, pos);
	} else if (c == '[') { //ARRAY
		jsonReadArray(dest, data, dataLen, pos);
	} else if (c == '"') { //STRING
		dest->initStr(jsonReadString(data, dataLen, pos));
	} else if (c == '-' || isNumeric(c)){ //NUMBER
		// Whatever we saw needs to be send to jsonReadNumber as well
		(*pos)--;
		jsonReadNumber(dest, data, dataLen, pos);
	} else if (c == 't') { //TRUE
		dest->initBol(1);
		(*pos)--;
		jsonReadConstant("true", data, dataLen, pos);
	} else if (c == 'f') { //FALSE
		dest->initBol(0);
		(*pos)--;
		jsonReadConstant("false", data, dataLen, pos);
	} else if (c == 'n') { //NULL
		dest->initNul();
		(*pos)--;
		jsonReadConstant("null", data, dataLen, pos);
	} else {
		fprintf(stderr, "JSON: Character is not a legal start to any JSON value: '%c' (0x%hhx)\n", c, c);
		dest->initNul();
	}
}

jsonValue* jsonInterpret(char* data, int dataLen){
	jsonValue *ret = (jsonValue*)malloc(sizeof(jsonValue));
	int pos = 0;
	getNextValue(ret, data, dataLen, &pos); //every nested jsonValue is on the heap.
	return ret;
}

jsonValue* jsonLoad(FILE* fp){
	rewind(fp);
	//datasize is the allocation. datalen is the actual length
	int dataSize = 10;
	int dataLen = 0;
	char* data = (char*)malloc(10);
	jsonLog("Loading file into memory", 3);
	while (1) {
		dataLen += fread(data+dataLen, 1, dataSize - dataLen, fp);
		if (dataLen != dataSize) break;
		dataSize *= 2;
		data = (char*)realloc(data, dataSize);
	}
	jsonLog("Interpreting JSON", 3);
	jsonValue* ret = jsonInterpret(data, dataLen);
	jsonLog("Done Interpreting JSON", 3);
	free(data);
	return ret;
}

///// Stuff for serializing JSON /////

static void doIndent(list<char> *data, int indentCount) {
	indentCount *= 2;
	data->setMaxUp(data->num + indentCount);
	char *dest = data->items + data->num;
	for (int i = 0; i < indentCount; i++) {
		dest[i] = ' ';
	}
	data->num += indentCount;
}

static void writeRawString(list<char> *data, char const *str) {
	const list<char> dummy = {.items = (char*)str, .num=(int)strlen(str), .max=0};
	data->addAll(dummy);
}

static void writeString(list<char> *data, char *str) {
	data->add('"');
	for (; *str; str++) {
		if (*str == '\\' || *str == '"') data->add('\\');
		data->add(*str);
	}
	data->add('"');
}

static void writeObj(list<char> *data, jsonValue *v, int indent) {
	char useIndent = 0;
	if (indent >= 0) {
		for (int i = 0; i < v->d.obj.num; i++) {
			elementType t = v->d.obj[i].value.type;
			if (t == J_OBJ || t == J_ARR) {
				indent++;
				useIndent = 1;
				break;
			}
		}
	}

	data->add('{');
	for (int i = 0; i < v->d.obj.num; i++) {
		if (i) data->add(',');
		if (useIndent) {
			data->add('\n');
			doIndent(data, indent);
		}
		jsonEntry &entry = v->d.obj[i];
		writeString(data, entry.key);
		data->add(':');
		jsonSerialize(data, &entry.value, indent);
	}
	if (useIndent) {
		data->add('\n');
		doIndent(data, indent-1);
	}
	data->add('}');
}

static void writeArr(list<char> *data, jsonValue *v, int indent) {
	char useIndent = 0;
	if (indent >= 0) {
		for (int i = 0; i < v->d.arr.num; i++) {
			elementType t = v->d.arr[i].type;
			if (t == J_OBJ || t == J_ARR) {
				indent++;
				useIndent = 1;
				break;
			}
		}
	}

	data->add('[');
	for (int i = 0; i < v->d.arr.num; i++) {
		if (i) data->add(',');
		if (useIndent) {
			data->add('\n');
			doIndent(data, indent);
		}
		jsonSerialize(data, &v->d.arr[i], indent);
	}
	if (useIndent) {
		data->add('\n');
		doIndent(data, indent-1);
	}
	data->add(']');
}

void jsonSerialize(list<char> *data, jsonValue *v, int indent) {
	switch (v->type) {
	case J_OBJ:
		writeObj(data, v, indent);
		break;
	case J_ARR:
		writeArr(data, v, indent);
		break;
	case J_STR:
		writeString(data, v->d.str);
		break;
	case J_NUM:
		writeRawString(data, v->d.str);
		break;
	case J_BOL:
		writeRawString(data, v->d.bol ? "true" : "false");
		break;
	case J_NUL:
		writeRawString(data, "null");
		break;
	}
}

///// Misc util stuff /////

char const* typeStr(elementType t) {
	switch (t) {
	case J_OBJ:
		return "J_OBJ";
	case J_ARR:
		return "J_ARR";
	case J_STR:
		return "J_STR";
	case J_NUM:
		return "J_NUM";
	case J_BOL:
		return "J_BOL";
	case J_NUL:
		return "J_NUL";
	default:
		return "UNKNOWN";
	}
}
