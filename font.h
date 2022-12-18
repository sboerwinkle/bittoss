
struct font{
	float invaspect;
	float spacing;
	GLuint vertex_buffer;
	GLuint ref_buffer;
	short *letterStart;
	short *letterLen;
};

extern font myfont;

extern void initFont();
extern void destroyFont();
