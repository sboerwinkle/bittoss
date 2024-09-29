
#define CB_SIZE 3
struct controlBuffer {

public:
	char d[CB_SIZE];
	char latest;
	int s;

	// init() or = {0} maybe
	void push(char x);
	char pop();
	void consume(controlBuffer *o);
};
