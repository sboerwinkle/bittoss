#include <math.h>
#include <stdio.h>
#include <GL/glew.h>
#include <GL/gl.h>
#include "ExternLinAlg.h"
#include "Quaternion.h"
#include "graphics.h"

int frameOffset[3];

// Might improve this later, for now we just send the updated coords to the graphics card
// once per box being drawn. Each box has 6 faces * 2 triangles/face * 3 vertices/triangle * 6 attributes/vertex (XYZRGB)
GLfloat boxData[216];

static GLint u_camera_id = -1;
static GLuint stream_buffer_id;

static char glMsgBuf[3000]; // Is allocating all of this statically a bad idea? IDK
static void printGLProgErrors(GLuint prog){
	GLint ret = 0;
	glGetProgramiv(prog, GL_LINK_STATUS, &ret);
	printf("Link status %d ", ret);
	glGetProgramiv(prog, GL_ATTACHED_SHADERS, &ret);
	printf("Attached Shaders: %d ", ret);

	glMsgBuf[0] = 0;
	int len;
	glGetProgramInfoLog(prog, 3000, &len, glMsgBuf);
	printf("Len: %d\n", len);
	glMsgBuf[len] = 0;
	printf("GL Info Log: %s\n", glMsgBuf);
}
static void printGLShaderErrors(GLuint shader) {
	glMsgBuf[0] = 0;
	int len;
	glGetShaderInfoLog(shader, 3000, &len, glMsgBuf);
	printf("Len: %d\n", len);
	glMsgBuf[len] = 0;
	printf("GL Info Log: %s\n", glMsgBuf);
}

static void cerr(const char* msg){
	while(1) {
		int err = glGetError();
		if (!err) return;
		printf("Error (%s): %d\n", msg, err);
	}
}

static char* readFileContents(const char* path){
	FILE* fp = fopen(path, "r");
	if(!fp){
		printf("Failed to open file: %s\n", path);
		return NULL;
	}
	fseek(fp, 0, SEEK_END);
	long fsize = ftell(fp);
	rewind(fp);  /* same as rewind(f); */
	char *ret = (char*) malloc(fsize + 1);
	fread(ret, 1, fsize, fp);
	fclose(fp);
	ret[fsize] = 0;
	return ret;
}

static GLuint mkShader(GLenum type, const char* path) {
	GLuint shader = glCreateShader(type);
	char* src = readFileContents(path);
	glShaderSource(shader, 1, &src, NULL);
	free(src);
	glCompileShader(shader);
	printGLShaderErrors(shader);
	return shader;
}

void initGraphics() {
	if (GLEW_OK != glewInit()) {
		puts("GLEW failed to init, whups");
		exit(1);
	}
	GLuint vertexShader = mkShader(GL_VERTEX_SHADER, "shaders/solid.vert");

	GLuint fragShader = mkShader(GL_FRAGMENT_SHADER, "shaders/color.frag");

	GLuint prog = glCreateProgram();
	glAttachShader(prog, vertexShader);
	glAttachShader(prog, fragShader);
	glLinkProgram(prog);
	cerr("Post link");

	/* TODO:
	 * / Offset is handled in CPU code (ints are fun)
	 * / set u_camera appropriately (ExternLinAlg.h is big friend-o here)
	 * / Populate drawing buffer
	 * - Consider putting cube-drawing instructions in a static buffer which we then scale and shift? IDK
	 * - Maybe even a geometry shader??? Not sure if that would have a significant improvement or not.
	 * - As a third option, leverage the indexing thingy for a slight reduction in vertex data needing to be written?
	 */
	u_camera_id = glGetUniformLocation(prog, "u_camera");
	GLint a_loc_id = glGetAttribLocation(prog, "a_loc");
	GLint a_color_id = glGetAttribLocation(prog, "a_color");

	glEnableVertexAttribArray(a_loc_id);
	glEnableVertexAttribArray(a_color_id);
	printGLProgErrors(prog);

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glFrontFace(GL_CW); // Apparently I'm bad at working out windings in my head, easier to flip this than fix everything else
	glUseProgram(prog);

	glGenBuffers(1, &stream_buffer_id);
	glBindBuffer(GL_ARRAY_BUFFER, stream_buffer_id);
	// Position data is first
	glVertexAttribPointer(a_loc_id, 3, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 6, (void*) 0);
	// Followed by color data
	glVertexAttribPointer(a_color_id, 3, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 6, (void*) (sizeof(GLfloat) * 3));
}

void setupFrame(float pitch, float yaw, float up, float forward) {
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	float quat[4] = {1,0,0,0};
	quat_rotY(quat, NULL, yaw);
	quat_rotX(quat, NULL, pitch);
	quat_norm(quat);

	float mat_quat[16];
	float mat_a[16];
	float mat_final[16];
	mat4x4fromQuat(mat_quat, quat);
	mat4x4Transf(mat_quat, 0, up, forward);
	perspective(mat_a, 1, (float)displayWidth/displayHeight, nearPlane, farPlane);
	mat4x4Multf(mat_final, mat_a, mat_quat);

	glUniformMatrix4fv(u_camera_id, 1, GL_FALSE, mat_final);
}

void rect(int32_t *p, int32_t *r, float red, float grn, float blu) {
	// TODO This could be improved in a number of ways, see other TODO higher in this file
	GLfloat L = (float)(p[0] + frameOffset[0] - r[0]);
	GLfloat R = (float)(p[0] + frameOffset[0] + r[0]);
	GLfloat U = (float)(p[2] + frameOffset[2] - r[2]); // World coordinate axes don't line up with GL axes, really should fix this.
	GLfloat D = (float)(p[2] + frameOffset[2] + r[2]);
	GLfloat F = (float)(p[1] + frameOffset[1] - r[1]);
	GLfloat B = (float)(p[1] + frameOffset[1] + r[1]);
	GLfloat r1 = red;
	GLfloat g1 = grn;
	GLfloat b1 = blu;
	GLfloat r2 = red * 0.85;
	GLfloat g2 = grn * 0.85;
	GLfloat b2 = blu * 0.85;
	GLfloat r3 = red * 0.75;
	GLfloat g3 = grn * 0.75;
	GLfloat b3 = blu * 0.75;
	GLfloat r4 = red * 0.65;
	GLfloat g4 = grn * 0.65;
	GLfloat b4 = blu * 0.65;
	GLfloat r5 = red * 0.50;
	GLfloat g5 = grn * 0.50;
	GLfloat b5 = blu * 0.50;
	// Hopefully the compiler will optimize the counter out.
	// As previously stated, needs work.
	int counter = 0;
#define vtx(x, y, z, r, g, b) \
	boxData[counter++] = x; \
	boxData[counter++] = y; \
	boxData[counter++] = z; \
	boxData[counter++] = r; \
	boxData[counter++] = g; \
	boxData[counter++] = b;
	// Top is full color
	vtx(L,U,F,r1,g1,b1);
	vtx(R,U,F,r1,g1,b1);
	vtx(L,U,B,r1,g1,b1);
	vtx(L,U,B,r1,g1,b1);
	vtx(R,U,F,r1,g1,b1);
	vtx(R,U,B,r1,g1,b1);
	// Front is dimmer
	vtx(L,U,F,r2,g2,b2);
	vtx(L,D,F,r2,g2,b2);
	vtx(R,U,F,r2,g2,b2);
	vtx(R,U,F,r2,g2,b2);
	vtx(L,D,F,r2,g2,b2);
	vtx(R,D,F,r2,g2,b2);
	// Sides are a bit dimmer
	vtx(L,U,B,r3,g3,b3);
	vtx(L,D,B,r3,g3,b3);
	vtx(L,U,F,r3,g3,b3);
	vtx(L,U,F,r3,g3,b3);
	vtx(L,D,B,r3,g3,b3);
	vtx(L,D,F,r3,g3,b3);
	vtx(R,D,F,r3,g3,b3);
	vtx(R,D,B,r3,g3,b3);
	vtx(R,U,F,r3,g3,b3);
	vtx(R,U,F,r3,g3,b3);
	vtx(R,D,B,r3,g3,b3);
	vtx(R,U,B,r3,g3,b3);
	// Back is dimmer still
	vtx(L,U,B,r4,g4,b4);
	vtx(R,U,B,r4,g4,b4);
	vtx(L,D,B,r4,g4,b4);
	vtx(L,D,B,r4,g4,b4);
	vtx(R,U,B,r4,g4,b4);
	vtx(R,D,B,r4,g4,b4);
	// Bottom is dimmest
	vtx(L,D,F,r5,g5,b5);
	vtx(L,D,B,r5,g5,b5);
	vtx(R,D,F,r5,g5,b5);
	vtx(R,D,F,r5,g5,b5);
	vtx(L,D,B,r5,g5,b5);
	vtx(R,D,B,r5,g5,b5);
	glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat)*216, boxData, GL_STREAM_DRAW);
	glDrawArrays(GL_TRIANGLES, 0, 36);
}
