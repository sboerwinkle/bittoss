/////////////////////////////////////
// Copied from work by mboerwinkle
/////////////////////////////////////

enum elementType{OBJECT,ARRAY,STRING,NUMBER,TRUE,FALSE,NUL,INVALID};

/*
 * data
 * OBJECT	Pointer to jsonObject
 * ARRAY	Pointer to jsonArray
 * STRING	Null terminated character array representing value in ascii. Unicode and escapes are not expanded automatically.
 * NUMBER	Null terminated character array representing value in ascii
 * TRUE		Unused
 * FALSE	Unused
 * NUL		Unused
 */
typedef struct jsonValue{
	enum elementType type;
	void* data;
} jsonValue;

typedef struct jsonArray{
	int len;
	jsonValue** data;
} jsonArray;

typedef struct jsonKeypair{
	char* key;
	jsonValue* data;
} jsonKeypair;

typedef struct jsonObject{
	int len;
	jsonKeypair* data;
} jsonObject;
typedef struct jsonString{
	char* d;
	int len;//size of data
	int size;//size of allocation
}jsonString;
//Getter functions
extern double jsonGetDouble(jsonValue j);
extern int jsonGetInt(jsonValue j);
extern char* jsonGetString(jsonValue j);
extern char** jsonGetKeys(jsonValue j);
extern enum elementType jsonGetType(jsonValue j);
extern int jsonGetLen(jsonValue j);
extern jsonValue jsonGetObj(jsonValue j, const char* key);
extern jsonValue jsonGetArr(jsonValue j, int idx);


extern jsonValue* jsonInterpret(char* data, int dataLen);
extern jsonValue* jsonLoad(FILE* fp);
extern void jsonFree(jsonValue target);
