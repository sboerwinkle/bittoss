
struct font{
        float invaspect;
        float spacing;
        GLuint vertex_buffer;
        GLuint ref_buffer;
        short letterStart[94];
        short letterLen[94];
};

extern font myfont;

extern void initFont();
