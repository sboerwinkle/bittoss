#include <math.h>
#include <stdio.h>
#include <GL/glew.h>
#include <GL/gl.h>
#include "ExternLinAlg.h"
#include "Quaternion.h"
#include "graphics.h"
#include "font.h"

int frameOffset[3];

// Might improve this later, for now we just send the updated coords to the graphics card
// once per box being drawn. Each box has 6 faces * 2 triangles/face * 3 vertices/triangle * 6 attributes/vertex (XYZRGB)
GLfloat boxData[216];

static GLuint main_prog, flat_prog;
static GLuint vaos[2];

static GLint u_camera_id = -1;
static GLint u_flat_camera_id = -1;
static GLint u_flat_offset_id = -1;
static GLint u_flat_scale_x_id = -1;
static GLint u_flat_scale_y_id = -1;
static GLint u_flat_color_id = -1;
static GLuint stream_buffer_id;
static GLfloat cam_lens_ortho[16];

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
	GLuint vertexShader2d = mkShader(GL_VERTEX_SHADER, "shaders/hud.vert");
	GLuint fragShader = mkShader(GL_FRAGMENT_SHADER, "shaders/color.frag");

	main_prog = glCreateProgram();
	glAttachShader(main_prog, vertexShader);
	glAttachShader(main_prog, fragShader);
	glLinkProgram(main_prog);
	cerr("Post link");

	flat_prog = glCreateProgram();
	glAttachShader(flat_prog, vertexShader2d);
	glAttachShader(flat_prog, fragShader);
	glLinkProgram(flat_prog);
	cerr("Post link");

	/* TODO:
	 * / Offset is handled in CPU code (ints are fun)
	 * / set u_camera appropriately (ExternLinAlg.h is big friend-o here)
	 * / Populate drawing buffer
	 * - Consider putting cube-drawing instructions in a static buffer which we then scale and shift? IDK
	 * - Maybe even a geometry shader??? Not sure if that would have a significant improvement or not.
	 * - As a third option, leverage the indexing thingy for a slight reduction in vertex data needing to be written?
	 */
	u_camera_id = glGetUniformLocation(main_prog, "u_camera");
	GLint a_loc_id = glGetAttribLocation(main_prog, "a_loc");
	GLint a_color_id = glGetAttribLocation(main_prog, "a_color");

	u_flat_camera_id = glGetUniformLocation(flat_prog, "u_camera");
	u_flat_offset_id = glGetUniformLocation(flat_prog, "u_offset");
	u_flat_scale_x_id = glGetUniformLocation(flat_prog, "u_scale_x");
	u_flat_scale_y_id = glGetUniformLocation(flat_prog, "u_scale_y");
	u_flat_color_id = glGetUniformLocation(flat_prog, "u_color");
	GLint a_flat_loc_id = glGetAttribLocation(flat_prog, "a_loc");

	printGLProgErrors(main_prog);
	printGLProgErrors(flat_prog);

	// glEnable(GL_DEPTH_TEST); Set per-program instead, see `setupFrame` / `setupText`
	glEnable(GL_CULL_FACE);
	//glFrontFace(GL_CW); // Apparently I'm bad at working out windings in my head, easier to flip this than fix everything else
	glGenVertexArrays(2, vaos);

	glBindVertexArray(vaos[0]);
	glEnableVertexAttribArray(a_loc_id);
	glEnableVertexAttribArray(a_color_id);
	glGenBuffers(1, &stream_buffer_id);
	glBindBuffer(GL_ARRAY_BUFFER, stream_buffer_id);
	// Position data is first
	glVertexAttribPointer(a_loc_id, 3, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 6, (void*) 0);
	// Followed by color data
	glVertexAttribPointer(a_color_id, 3, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 6, (void*) (sizeof(GLfloat) * 3));

	glBindVertexArray(vaos[1]);
	glEnableVertexAttribArray(a_flat_loc_id);
	initFont();
	glVertexAttribPointer(a_flat_loc_id, 2, GL_FLOAT, GL_FALSE, 2*sizeof(float), (void*) 0);

	glOrthoEquiv(cam_lens_ortho, 0, (float) displayWidth / fontSizePx, (float) displayHeight / fontSizePx, 0, -1, 1);

	// This could really be in setupFrame, but it turns out the text processing never actually writes this again,
	// so we can leave it bound for the main_prog.
	glBindBuffer(GL_ARRAY_BUFFER, stream_buffer_id);
	cerr("End of graphics setup");
}

void setupFrame(float pitch, float yaw, float up, float forward) {
	glUseProgram(main_prog);
	glBindVertexArray(vaos[0]);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glEnable(GL_DEPTH_TEST);

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
	cerr("End of frame setup");
}

void setupText() {
	glUseProgram(flat_prog);
	//cerr("After use program (flat_prog)");
	glBindVertexArray(vaos[1]);
	glDisable(GL_DEPTH_TEST);

	glUniformMatrix4fv(u_flat_camera_id, 1, GL_FALSE, cam_lens_ortho);
}

void drawHudText(const char* str, double x, double y, double scale, float* color){
	glUniform1f(u_flat_scale_x_id, (float)scale);
	glUniform1f(u_flat_scale_y_id, (float)(scale*myfont.invaspect));
	glUniform3fv(u_flat_color_id, 1, color);

	for(int idx = 0;; idx++){
		glUniform2f(u_flat_offset_id, x+(1.0+myfont.spacing)*scale*idx, y);
		int letter = str[idx];
		if(!letter) return;
		letter -= 33;
		if(letter < 0 || letter >= 94) continue;
		glDrawElements(GL_TRIANGLES, myfont.letterLen[letter], GL_UNSIGNED_SHORT, (void*) (sizeof(short)*myfont.letterStart[letter]));
	}
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
	vtx(L,U,B,r1,g1,b1);
	vtx(R,U,F,r1,g1,b1);
	vtx(L,U,B,r1,g1,b1);
	vtx(R,U,B,r1,g1,b1);
	vtx(R,U,F,r1,g1,b1);
	// Front is dimmer
	vtx(L,U,F,r2,g2,b2);
	vtx(R,U,F,r2,g2,b2);
	vtx(L,D,F,r2,g2,b2);
	vtx(R,U,F,r2,g2,b2);
	vtx(R,D,F,r2,g2,b2);
	vtx(L,D,F,r2,g2,b2);
	// Sides are a bit dimmer
	vtx(L,U,B,r3,g3,b3);
	vtx(L,U,F,r3,g3,b3);
	vtx(L,D,B,r3,g3,b3);
	vtx(L,U,F,r3,g3,b3);
	vtx(L,D,F,r3,g3,b3);
	vtx(L,D,B,r3,g3,b3);
	vtx(R,D,F,r3,g3,b3);
	vtx(R,U,F,r3,g3,b3);
	vtx(R,D,B,r3,g3,b3);
	vtx(R,U,F,r3,g3,b3);
	vtx(R,U,B,r3,g3,b3);
	vtx(R,D,B,r3,g3,b3);
	// Back is dimmer still
	vtx(L,U,B,r4,g4,b4);
	vtx(L,D,B,r4,g4,b4);
	vtx(R,U,B,r4,g4,b4);
	vtx(L,D,B,r4,g4,b4);
	vtx(R,D,B,r4,g4,b4);
	vtx(R,U,B,r4,g4,b4);
	// Bottom is dimmest
	vtx(L,D,F,r5,g5,b5);
	vtx(R,D,F,r5,g5,b5);
	vtx(L,D,B,r5,g5,b5);
	vtx(R,D,F,r5,g5,b5);
	vtx(R,D,B,r5,g5,b5);
	vtx(L,D,B,r5,g5,b5);
	glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat)*216, boxData, GL_STREAM_DRAW);
	glDrawArrays(GL_TRIANGLES, 0, 36);
}