/////////////////////////////////////
// Copied from work by mboerwinkle
/////////////////////////////////////

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "loadJsonConfig.h"
#define BLOCK 5
#define ERROR_VERBOSITY 3
void jsonLog(const char* msg, int lvl){
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

double jsonGetDouble(jsonValue j){
	if(j.type != NUMBER) jsonLog("Wrong type. Not NUMBER", 1);
	double ret;
	sscanf((char*)(j.data), "%lf", &ret);
	return ret;
}
int jsonGetInt(jsonValue j){
	if(j.type != NUMBER) jsonLog("Wrong type. Not NUMBER", 1);
	int ret;
	sscanf((char*)(j.data), "%d", &ret);
	return ret;
}
char* jsonGetString(jsonValue j){
	if(j.type != STRING) jsonLog("Wrong type. Not STRING", 1);
	return (char*)(j.data);
}
char** jsonGetKeys(jsonValue j){
	if(j.type != OBJECT) jsonLog("Wrong type. Not OBJECT", 1);
	jsonObject* obj = (jsonObject*)(j.data);
	char** ret = (char**)calloc(obj->len, sizeof(char*));
	for(int idx = 0; idx < obj->len; idx++){
		ret[idx] = obj->data[idx].key;
	}
	return ret;
}
int jsonGetLen(jsonValue j){
	if(j.type == OBJECT){
		return ((jsonObject*)(j.data))->len;
	}else if(j.type == ARRAY){
		return ((jsonArray*)(j.data))->len;
	}else if(j.type == STRING){
		return strlen((char*)(j.data));
	}else{
		jsonLog("Type does not have length.", 1);
	}
	return -1;
}
enum elementType jsonGetType(jsonValue j){
	return j.type;
}
jsonValue jsonGetObj(jsonValue j, const char* key){
	if(j.type == OBJECT){
		jsonObject* obj = (jsonObject*)(j.data);
		for(int idx = 0; idx < obj->len; idx++){//linear search through elements
			if(!strcmp(obj->data[idx].key, key)){
				return *(obj->data[idx].data);
			}
		}
	}else{
		jsonLog("Cannot 'get'. Not OBJECT", 1);
	}
	jsonValue ret = {.type=INVALID, .data=NULL};
	return ret;
}
jsonValue jsonGetArr(jsonValue j, int idx){
	if(j.type == ARRAY){
		return *(((jsonArray*)(j.data))->data[idx]);
	}else{
		jsonLog("Cannot 'get'. Not ARRAY", 1);
	}
	jsonValue ret = {.type=INVALID, .data=NULL};
	return ret;
}

jsonValue* getNextValue(char* data, int dataLen, int* pos);


jsonString initJsonString(){
	jsonString ret = {.len = 0, .size = 10};
	ret.d = (char*)malloc(ret.size);
	ret.d[0] = 0;
	return ret;
}
void appendJsonString(jsonString* j, char c){
	if(j->len == j->size-1){//minus 1 because we null terminate after every character
		(j->size)+=10;
		j->d = (char*)realloc(j->d, j->size);
	}
	j->d[j->len] = c;//write character
	j->d[j->len+1] = 0;//null terminate
	(j->len)++;
}
char* jsonReadString(char* data, int dataLen, int* pos){
	jsonLog("func jsonReadString", 4);
	jsonString ret = initJsonString();
	int isEscaped = 0;
	char c;
	while(1){
		(*pos)++;
		c = data[*pos];
		if(c == '\\'){
			isEscaped = 1;
			appendJsonString(&ret, '\\');
			(*pos)++;
			c = data[*pos];
		}
		if(!isEscaped && c == '"'){
			(*pos)++;
			return ret.d;
		}
		isEscaped = 0;
		appendJsonString(&ret, c);
	}
}
jsonValue* jsonLoad(FILE* fp){
	if(fp == NULL){
		jsonLog("Filepointer is NULL", 1);
		return (jsonValue*)NULL;
	}
	rewind(fp);
	//datasize is the allocation. datalen is the actual length
	int dataSize = 0;
	int dataLen = 0;
	char* data = NULL;
	int readSize;
	jsonLog("Loading file into memory", 3);
	do{
		jsonLog("New block loading", 4);
		dataSize += BLOCK;
		data = (char*)realloc(data, dataSize);
		readSize = fread(data+dataLen, 1, BLOCK, fp);
		dataLen+=readSize;
	}while(readSize == BLOCK);
	//printf("JSON Data (len %d) is: %s\n", dataLen, data);
	jsonLog("Interpreting JSON", 3);
	jsonValue* ret = jsonInterpret(data, dataLen);
	jsonLog("Done Interpreting JSON", 3);
	free(data);
	return ret;
}

int isWhite(char c){
	if(c == 0x0020 || c == 0x000D || c == 0x000A || c == 0x0009){
		return 1;
	}
	return 0;
}
jsonObject* jsonReadObject(char* data, int dataLen, int* pos){
	jsonLog("func jsonReadObject", 4);
	jsonObject* ret = (jsonObject*)malloc(sizeof(jsonObject));
	ret->len = 0;
	int size = 10;
	ret->data = (jsonKeypair*)calloc(size, sizeof(jsonKeypair));//####
	(*pos)++;
	while(1){
		while(isWhite(data[*pos])) (*pos)++;
		if(data[*pos] == '"'){
			jsonLog("Getting keypair key", 4);
			ret->data[ret->len].key = jsonReadString(data, dataLen, pos);//####
			jsonLog("Done getting keypair key", 4);
			while(isWhite(data[*pos])) (*pos)++;
			if(data[*pos] != ':'){
				jsonLog("Invalid character in Object. Expected :", 1);
			}else{
				jsonLog("Found ':'", 4);
			}
			(*pos)++;
			jsonLog("Getting keypair data", 4);
			ret->data[ret->len].data = getNextValue(data, dataLen, pos);
			jsonLog("Done getting keypair data", 4);
			(ret->len)++;
			if(ret->len == size){
				size += 10;
				ret->data = (jsonKeypair*)realloc(ret->data, size*sizeof(jsonKeypair));
			}
			while(isWhite(data[*pos])) (*pos)++;
			if(data[*pos] == '}'){
				(*pos)++;
				return ret;
			}else if(data[*pos] == ','){
				(*pos)++;
			}
		}else if(data[*pos] == '}'){
			(*pos)++;
			return ret;
		}else{
			jsonLog("Invalid character in Object. Expected } or \"", 1);
		}
	}
}
jsonArray* jsonReadArray(char* data, int dataLen, int* pos){
	jsonLog("func jsonReadArray", 4);
	jsonArray* ret = (jsonArray*)malloc(sizeof(jsonArray));
	ret->len = 0;
	int size = 10;
	ret->data = (jsonValue**)calloc(size, sizeof(jsonValue*));
	(*pos)++;

	while(isWhite(data[*pos])) (*pos)++;
	if(data[*pos] == ']'){
			(*pos)++;
			return ret;
	}
	while(1){
		ret->data[ret->len] = getNextValue(data, dataLen, pos);
		(ret->len)++;
		while(isWhite(data[*pos])) (*pos)++;
		if(data[*pos] == ','){
			(*pos)++;
		}else if(data[*pos] == ']'){
			(*pos)++;
			return ret;
		}else{
			jsonLog("Invalid character in array. Expected ] or ,", 1);
		}
		if(ret->len == size){
			size += 10;
			ret->data = (jsonValue**)realloc(ret->data, size*sizeof(jsonValue*));
		}
	}
}
int isDecimal(char c){
	if(c >= '0' && c <= '9') return 1;
	return 0;
}
char* jsonReadNumber(char* data, int dataLen, int* pos){
	jsonLog("func jsonReadNumber", 4);
	jsonString ret = initJsonString();
	char c = data[*pos];
	appendJsonString(&ret, c);
	(*pos)++;
	c = data[*pos];
	while(isDecimal(c)){
		appendJsonString(&ret, c);
		(*pos)++;
		c = data[*pos];
	}
	if(c == '.'){
		appendJsonString(&ret, '.');
		(*pos)++;
		c = data[*pos];
		while(isDecimal(c)){
			appendJsonString(&ret, c);
			(*pos)++;
			c = data[*pos];
		}
	}
	if(c == 'e' || c == 'E'){
		appendJsonString(&ret, 'E');
		(*pos)++;
		c = data[*pos];
		if(c == '-' || c == '+'){
			appendJsonString(&ret, c);
			(*pos)++;
			c = data[*pos];
		}
		while(isDecimal(c)){
			appendJsonString(&ret, c);
			(*pos)++;
			c = data[*pos];
		}
	}
	return ret.d;
}

jsonValue* getNextValue(char* data, int dataLen, int* pos){
	jsonLog("func getNextValue", 4);
	jsonValue* ret = (jsonValue*)malloc(sizeof(jsonValue));
	//clear whitespace
	while(isWhite(data[*pos])) (*pos)++;
	
	char v = data[*pos];
	if(v == '{'){ //OBJECT
		ret->type = OBJECT;
		ret->data = jsonReadObject(data, dataLen, pos);
	}else if(v == '['){//ARRAY
		ret->type = ARRAY;
		ret->data = jsonReadArray(data, dataLen, pos);
	}else if(v == '"'){//STRING
		ret->type = STRING;
		ret->data = jsonReadString(data, dataLen, pos);
	}else if(v == '-' || isDecimal(v)){//NUMBER
		ret->type = NUMBER;
		ret->data = jsonReadNumber(data, dataLen, pos);
	}else if(v == 't'){//TRUE
		if(strncmp(&(data[*pos]), "true", 4)){
			jsonLog("Value starts with t, but is not true", 1);
		}
		ret->type = TRUE;
		ret->data = NULL;
		(*pos)+=4;
	}else if(v == 'f'){//FALSE
		if(strncmp(&(data[*pos]), "false", 5)){
			jsonLog("Value starts with f, but is not false", 1);
		}
		ret->type = FALSE;
		ret->data = NULL;
		(*pos)+=5;
	}else if(v == 'n'){//NULL
		if(strncmp(&(data[*pos]), "null", 4)){
			jsonLog("Value starts with n, but is not null", 1);
		}
		ret->type = NUL;
		ret->data = NULL;
		(*pos)+=4;
	}else{
		char msg[30];
		sprintf(msg, "Invalid character: \'%c\'", v);
		jsonLog(msg, 1);
	}
	return ret;
}
jsonValue* jsonInterpret(char* data, int dataLen){
	int pos = 0;
	return getNextValue(data, dataLen, &pos);//every nested jsonValue is on the heap.
}
void jsonFree(jsonValue target){
	if(target.type == OBJECT){
		jsonObject* o = (jsonObject*) target.data;
		for(int idx = 0; idx < o->len; idx++){//cycle through keypairs
			free(o->data[idx].key);//free key
			jsonFree(*(o->data[idx].data));//free data
			free(o->data[idx].data);
		}
		free(o->data);//free keypair array allocation
		free(o);//free the jsonobject
	}else if(target.type == ARRAY){
		jsonArray* a = (jsonArray*) target.data;
		for(int idx = 0; idx < a->len; idx++){
			jsonFree(*(a->data[idx]));
			free(a->data[idx]);
		}
		free(a->data);//free the array objects value array allocation
		free(a);//free the array object itself
	}else{
		free(target.data);//This might be null. If not, it is a char array.
	}
}
