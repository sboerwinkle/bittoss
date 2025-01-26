#include <math.h>
#include <stdio.h>
#include <GL/gl.h>
#include <GLES3/gl3.h>
#include <string.h>

#include "ExternLinAlg.h"
#include "mat3.h"
#include "Quaternion.h"
#include "graphics.h"
#include "font.h"
#include "util.h"

int displayWidth = 0;
int displayHeight = 0;

// Might improve this later, for now we just send the updated coords to the graphics card
// once per box being drawn. Each box has 6 faces * 2 triangles/face * 3 vertices/triangle * 8 attributes/vertex (XYZRGBUV)
static GLfloat boxData[288];

static GLuint main_prog, stipple_prog, tag_prog, flat_prog;
static GLuint vaos[2];

static GLint u_camera_id = -1;
static GLint u_tag_camera_id = -1;
static GLint u_tag_loc_id = -1;
static GLint u_tag_color_id = -1;
static GLint u_tag_scale_id = -1;
static GLint u_tag_offset_id = -1;
static GLint u_flat_camera_id = -1;
static GLint u_flat_offset_id = -1;
static GLint u_flat_scale_id = -1;
static GLint u_flat_color_id = -1;
static GLuint stream_buffer_id;
static GLfloat camera_uniform_mat[16];

static GLuint tex_noise;

static char glMsgBuf[3000]; // Is allocating all of this statically a bad idea? IDK
static void printGLProgErrors(GLuint prog, const char *name){
	GLint ret = 0;
	glGetProgramiv(prog, GL_LINK_STATUS, &ret);
	if (ret != GL_TRUE) printf("Link status for program \"%s\": %d\n", name, ret);
	//glGetProgramiv(prog, GL_ATTACHED_SHADERS, &ret);
	//printf("Attached Shaders: %d\n", ret);

	glGetProgramInfoLog(prog, 3000, NULL, glMsgBuf);
	// If the returned string is non-empty, print it
	if (glMsgBuf[0]) printf("GL Info Log for program \"%s\":\n%s\n", name, glMsgBuf);
}
static void printGLShaderErrors(GLuint shader, const char *path) {
	glGetShaderInfoLog(shader, 3000, NULL, glMsgBuf);
	// If the returned string is non-empty, print it
	if (glMsgBuf[0]) printf("GL Info Log for shader at %s:\n%s\n", path, glMsgBuf);
}
static char progFailed(GLuint prog) {
	GLint out;
	glGetProgramiv(prog, GL_LINK_STATUS, &out);
	return out != GL_TRUE;
}

static void cerr(const char* msg){
	while(1) {
		int err = glGetError();
		if (!err) return;
		printf("Error (%s): %d\n", msg, err);
	}
}

void setDisplaySize(int width, int height){
	displayWidth = width;
	displayHeight = height;
	glViewport(0, 0, displayWidth, displayHeight);
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
	long numRead = fread(ret, 1, fsize, fp);
	if (numRead != fsize) {
		fprintf(stderr, "File allegedly has size %ld, but only read %ld bytes\n", fsize, numRead);
	}
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
	printGLShaderErrors(shader, path);
	return shader;
}

void initGraphics() {
	GLuint vertexShader = mkShader(GL_VERTEX_SHADER, "shaders/solid.vert");
	GLuint vertexShader2d = mkShader(GL_VERTEX_SHADER, "shaders/flat.vert");
	GLuint vertexShaderTags = mkShader(GL_VERTEX_SHADER, "shaders/tags.vert");
	GLuint fragShader = mkShader(GL_FRAGMENT_SHADER, "shaders/color.frag");
	GLuint fragShaderStipple = mkShader(GL_FRAGMENT_SHADER, "shaders/stipple.frag");

	main_prog = glCreateProgram();
	glAttachShader(main_prog, vertexShader);
	glAttachShader(main_prog, fragShader);
	glLinkProgram(main_prog);
	cerr("Post link");

	stipple_prog = glCreateProgram();
	glAttachShader(stipple_prog, vertexShader);
	glAttachShader(stipple_prog, fragShaderStipple);
	glLinkProgram(stipple_prog);
	cerr("Post link");

	tag_prog = glCreateProgram();
	glAttachShader(tag_prog, vertexShaderTags);
	glAttachShader(tag_prog, fragShader);
	glLinkProgram(tag_prog);
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
	// These will be the same attrib locations as stipple_prog, since they use the same vertex shader
	GLint a_loc_id = glGetAttribLocation(main_prog, "a_loc");
	GLint a_color_id = glGetAttribLocation(main_prog, "a_color");
	GLint a_uv_id = glGetAttribLocation(main_prog, "a_uv");
	u_camera_id = glGetUniformLocation(main_prog, "u_camera");

	if (u_camera_id != glGetUniformLocation(stipple_prog, "u_camera")) {
		puts("Camera uniform not co-located for 'main' / 'stipple' GLSL programs. Possibly a GLSL error?");
		exit(1);
	}

	u_tag_camera_id = glGetUniformLocation(tag_prog, "u_camera");
	u_tag_loc_id = glGetUniformLocation(tag_prog, "u_loc");
	u_tag_color_id = glGetUniformLocation(tag_prog, "u_color");
	u_tag_scale_id = glGetUniformLocation(tag_prog, "u_scale");
	u_tag_offset_id = glGetUniformLocation(tag_prog, "u_offset");
	GLint a_tag_offset_id = glGetAttribLocation(tag_prog, "a_offset");

	u_flat_camera_id = glGetUniformLocation(flat_prog, "u_camera");
	u_flat_offset_id = glGetUniformLocation(flat_prog, "u_offset");
	u_flat_scale_id = glGetUniformLocation(flat_prog, "u_scale");
	u_flat_color_id = glGetUniformLocation(flat_prog, "u_color");
	GLint a_flat_loc_id = glGetAttribLocation(flat_prog, "a_loc");

	if (a_tag_offset_id != a_flat_loc_id) {
		puts("Input attributes not co-located for 'tag' / 'flat' GLSL programs. Possibly a GLSL error?");
		exit(1);
	}

	printGLProgErrors(main_prog, "main");
	printGLProgErrors(stipple_prog, "stipple");
	printGLProgErrors(tag_prog, "tag");
	printGLProgErrors(flat_prog, "flat");

	if (progFailed(main_prog) || progFailed(stipple_prog) || progFailed(tag_prog) || progFailed(flat_prog)) {
		puts("Shaders failed, not proceeding further.");
		exit(1);
	}

	// glEnable(GL_DEPTH_TEST); Set per-program instead, see `setupFrame` / `setupText`
	glEnable(GL_CULL_FACE);
	//glFrontFace(GL_CW); // Apparently I'm bad at working out windings in my head, easier to flip this than fix everything else
	glGenVertexArrays(2, vaos);
	cerr("After GenVertexArrays");
	{// prepare va 0
	glBindVertexArray(vaos[0]);
	glEnableVertexAttribArray(a_loc_id);
	glEnableVertexAttribArray(a_color_id);
	glEnableVertexAttribArray(a_uv_id);
	glGenBuffers(1, &stream_buffer_id);
	glBindBuffer(GL_ARRAY_BUFFER, stream_buffer_id);
	// Position data is first
	glVertexAttribPointer(a_loc_id, 3, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 8, (void*) 0);
	// Followed by color data
	glVertexAttribPointer(a_color_id, 3, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 8, (void*) (sizeof(GLfloat) * 3));
	// Followed by uv data
	glVertexAttribPointer(a_uv_id, 2, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 8, (void*) (sizeof(GLfloat) * 6));
	}
	cerr("End of vao 0 prep");
	{// prepare va 1
	glBindVertexArray(vaos[1]);
	glEnableVertexAttribArray(a_flat_loc_id);
	initFont();
	glVertexAttribPointer(a_flat_loc_id, 2, GL_FLOAT, GL_FALSE, 2*sizeof(float), (void*) 0);
	}
	cerr("End of vao 1 prep");

	// This could really be in setupFrame, but it turns out the text processing never actually writes this again,
	// so we can leave it bound for the main_prog.
	glBindBuffer(GL_ARRAY_BUFFER, stream_buffer_id);
	// (Further note: Unlike GL_ELEMENT_ARRAY_BUFFER, the GL_ARRAY_BUFFER binding is _NOT_ part of VAO state.
	//  Its value is written to VAO state when glVertexAttribPointer (and kin) are called, though.)

	// Generate a random texture for mottling
	uint8_t tex_noise_data[MOTTLE_TEX_RESOLUTION*MOTTLE_TEX_RESOLUTION];
	uint32_t rstate = 59423;
	for(int idx = 0; idx < MOTTLE_TEX_RESOLUTION*MOTTLE_TEX_RESOLUTION; idx++){
		/* Algorithm "xor" from p. 4 of Marsaglia, "Xorshift RNGs" */
		rstate ^= rstate << 13;
		rstate ^= rstate >> 17;
		rstate ^= rstate << 5;
		tex_noise_data[idx] = rstate & 0xFF;
	}
	glGenTextures(1, &tex_noise);
	glBindTexture(GL_TEXTURE_2D, tex_noise);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	// When evaluating changes to the below filtering settings, you should at least evaluate the two scenarios:
	//  1. You are on a large, flat plane moving around.
	//    How crisp is it? How visible is the mipmap and sampling seam? How much does the sampling seam move as you move the camera only?
	//  2. There is a large object moving in the distance.
	//    How crisp is it? How does it look as it approaches the threshold of minification->magnification? Is there significant aliasing shimmer?
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, MOTTLE_TEX_RESOLUTION, MOTTLE_TEX_RESOLUTION, 0, GL_RED, GL_UNSIGNED_BYTE, tex_noise_data);
	// We do naive 'generateMipmap', but we should possibly consider forcing some of the extreme mipmap levels to be exactly neutral (rather than assuming it will filter to that)
	glGenerateMipmap(GL_TEXTURE_2D);
	cerr("End of graphics setup");
}

void getCameraDepthVector(float *out) {
	out[0] = camera_uniform_mat[3];
	out[1] = camera_uniform_mat[7];
	out[2] = camera_uniform_mat[11];
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
	mat4x4fromQuat(mat_quat, quat);
	mat4x4Transf(mat_quat, 0, up, forward);
	perspective(mat_a, 1, (float)displayWidth/displayHeight, nearPlane, farPlane);
	mat4x4Multf(camera_uniform_mat, mat_a, mat_quat);

	glUniformMatrix4fv(u_camera_id, 1, GL_FALSE, camera_uniform_mat);
	cerr("End of frame setup");
}

void setupStipple() {
	// This assumes that you've previously called `setupFrame`,
	// as it doesn't do all the camera math again, or re-bind the VAO
	glUseProgram(stipple_prog);
	glUniformMatrix4fv(u_camera_id, 1, GL_FALSE, camera_uniform_mat);
}

void setupTags() {
	// Relies on `setupFrame()` for the camera math
	glBindVertexArray(vaos[1]);
	glUseProgram(tag_prog);
	glUniformMatrix4fv(u_tag_camera_id, 1, GL_FALSE, camera_uniform_mat);
}

void setupText() {
	// Relies on `setupTags()` to have bound the correct VAO
	glUseProgram(flat_prog);
	glDisable(GL_DEPTH_TEST);

	flatCamDefault();
}

void flatCamDefault() {
	glUniformMatrix3fv(u_flat_camera_id, 1, GL_FALSE, mat3_default);
}

void flatCamRotated(float scale, float x, float y) {
	float mat[9];
	// Conveniently, camera X unit vector (horizontal screen space) will be flat and that's great for us I guess
	mat3_squared(mat, scale, x, y, camera_uniform_mat[0], camera_uniform_mat[4]);
	glUniformMatrix3fv(u_flat_camera_id, 1, GL_FALSE, mat);
}

#define TAG_MARGIN 0.2
void drawTag(const char* str, int32_t *pos, double scale, float *color) {
	// Not sure what this makes the units on the incoming `scale`,
	// but it would feel wrong to ignore `fontSizePx` altogether.
	scale *= fontSizePx;
	float fontMultX = scale / displayWidth;
	float fontMultY = scale * myfont.invaspect / displayHeight;

	float fpos[3] = {(float)pos[0], (float)pos[1], (float)pos[2]};
	glUniform3fv(u_tag_loc_id, 1, fpos);

	int len = strlen(str);
	float spacing = 1.0 + myfont.spacing;
	float width = spacing*(len-1)+1;

	// Draw actual text
	glUniform2f(u_tag_scale_id, fontMultX, -fontMultY);
	glUniform3fv(u_tag_color_id, 1, color);
	float offsetX = width / -2;
	range(idx, len) {
		glUniform2f(u_tag_offset_id, offsetX, -1 - TAG_MARGIN);
		offsetX += spacing;

		int fontIndex = str[idx]-33;
		if (fontIndex < 0 || fontIndex >= 94) continue;
		glDrawElements(GL_TRIANGLES, myfont.letterLen[fontIndex], GL_UNSIGNED_SHORT, (void*) (sizeof(short)*myfont.letterStart[fontIndex]));
	}

	// Draw background for text
	glUniform3f(u_tag_color_id, 0, 0, 0); // We could base this off of the provided `color`, but black is fine for now
	glUniform2f(u_tag_scale_id, fontMultX*(width+2*TAG_MARGIN), -fontMultY*(1+2*TAG_MARGIN));
	glUniform2f(u_tag_offset_id, -0.5, -1);
	glDrawElements(GL_TRIANGLES, myfont.letterLen[94], GL_UNSIGNED_SHORT, (void*)(sizeof(short)*myfont.letterStart[94]));
}

void drawHudText(const char* str, double xBase, double yBase, double x, double y, double scale, float* color){
	float fontMultX = (float) (scale * fontSizePx / displayWidth);
	float fontMultY = (float) (scale * myfont.invaspect * fontSizePx / displayHeight);
	glUniform2f(u_flat_scale_id, fontMultX, fontMultY);

	glUniform3fv(u_flat_color_id, 1, color);

	fontMultX *= 1.0 + myfont.spacing;

	x = xBase + x*fontMultX;
	y = yBase + y*fontMultY;

	for(int idx = 0;; idx++){
		glUniform2f(u_flat_offset_id, x+fontMultX*idx, y);
		int letter = str[idx];
		if(!letter) return;
		letter -= 33;
		if(letter < 0 || letter >= 94) continue;
		glDrawElements(GL_TRIANGLES, myfont.letterLen[letter], GL_UNSIGNED_SHORT, (void*) (sizeof(short)*myfont.letterStart[letter]));
	}
}

void drawHudRect(double x, double y, double w, double h, float *color) {
	// I've added one more font character (at postition 94, corresponding to ASCII 127 / DEL)
	// which is just 2 tris as a square. This is because I am lazy, and want to draw rectangles easily.

	glUniform2f(u_flat_offset_id, x, y);
	glUniform2f(u_flat_scale_id, w, h);
	glUniform3fv(u_flat_color_id, 1, color);

	glDrawElements(GL_TRIANGLES, myfont.letterLen[94], GL_UNSIGNED_SHORT, (void*)(sizeof(short)*myfont.letterStart[94]));
}

void rect(int32_t *p, int32_t *r, float red, float grn, float blu) {
	// TODO This could be improved in a number of ways, see other TODO higher in this file
	GLfloat L = (float)(p[0] - r[0]);
	GLfloat R = (float)(p[0] + r[0]);
	GLfloat F = (float)(p[1] - r[1]);
	GLfloat B = (float)(p[1] + r[1]);
	GLfloat U = (float)(p[2] - r[2]);
	GLfloat D = (float)(p[2] + r[2]);
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
	GLfloat xuv = ((float)(r[0]*2)) * MOTTLE_TEX_SCALE;
	GLfloat yuv = ((float)(r[1]*2)) * MOTTLE_TEX_SCALE;
	GLfloat zuv = ((float)(r[2]*2)) * MOTTLE_TEX_SCALE;
	// Hopefully the compiler will optimize the counter out.
	// As previously stated, needs work.
	int counter = 0;
#define vtx(x, y, z, r, g, b, u, v) \
	boxData[counter++] = x; \
	boxData[counter++] = y; \
	boxData[counter++] = z; \
	boxData[counter++] = r; \
	boxData[counter++] = g; \
	boxData[counter++] = b; \
	boxData[counter++] = u; \
	boxData[counter++] = v; \
	// END vtx
	// Top is full color
	vtx(L,F,U,r1,g1,b1,0.0,0.0);
	vtx(L,B,U,r1,g1,b1,0.0,yuv);
	vtx(R,F,U,r1,g1,b1,xuv,0.0);
	vtx(L,B,U,r1,g1,b1,0.0,yuv);
	vtx(R,B,U,r1,g1,b1,xuv,yuv);
	vtx(R,F,U,r1,g1,b1,xuv,0.0);
	// Front is dimmer
	vtx(L,F,U,r2,g2,b2,0.0,0.0);
	vtx(R,F,U,r2,g2,b2,0.0,xuv);
	vtx(L,F,D,r2,g2,b2,zuv,0.0);
	vtx(R,F,U,r2,g2,b2,0.0,xuv);
	vtx(R,F,D,r2,g2,b2,zuv,xuv);
	vtx(L,F,D,r2,g2,b2,zuv,0.0);
	// Sides are a bit dimmer
	vtx(L,B,U,r3,g3,b3,0.0,0.0);
	vtx(L,F,U,r3,g3,b3,0.0,yuv);
	vtx(L,B,D,r3,g3,b3,zuv,0.0);
	vtx(L,F,U,r3,g3,b3,0.0,yuv);
	vtx(L,F,D,r3,g3,b3,zuv,yuv);
	vtx(L,B,D,r3,g3,b3,zuv,0.0);
	vtx(R,F,D,r3,g3,b3,0.0,0.0);
	vtx(R,F,U,r3,g3,b3,0.0,zuv);
	vtx(R,B,D,r3,g3,b3,yuv,0.0);
	vtx(R,F,U,r3,g3,b3,0.0,zuv);
	vtx(R,B,U,r3,g3,b3,yuv,zuv);
	vtx(R,B,D,r3,g3,b3,yuv,0.0);
	// Back is dimmer still
	vtx(L,B,U,r4,g4,b4,0.0,0.0);
	vtx(L,B,D,r4,g4,b4,0.0,zuv);
	vtx(R,B,U,r4,g4,b4,xuv,0.0);
	vtx(L,B,D,r4,g4,b4,0.0,zuv);
	vtx(R,B,D,r4,g4,b4,xuv,zuv);
	vtx(R,B,U,r4,g4,b4,xuv,0.0);
	// Bottom is dimmest
	vtx(L,F,D,r5,g5,b5,0.0,0.0);
	vtx(R,F,D,r5,g5,b5,0.0,xuv);
	vtx(L,B,D,r5,g5,b5,yuv,0.0);
	vtx(R,F,D,r5,g5,b5,0.0,xuv);
	vtx(R,B,D,r5,g5,b5,yuv,xuv);
	vtx(L,B,D,r5,g5,b5,yuv,0.0);
	glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat)*288, boxData, GL_STREAM_DRAW);
	glDrawArrays(GL_TRIANGLES, 0, 36);
}
