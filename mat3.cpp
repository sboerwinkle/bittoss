#include <GL/gl.h>
//#include <GL/glew.h>

#include "graphics.h"

GLfloat mat3_default[9] = {2, 0, 0, 0, -2, 0, -1, 1, 0};

void mat3_squared(float* m, float size, float x, float y, float cos, float sin) {
	x = 2*x - 1;
	y = 1 - 2*y;

	float width = 2*size/displayWidth;
	float height = -2*size/displayHeight;

	m[0] = width*cos;
	m[1] = height*-sin;
	m[2] = 0;
	m[3] = width*sin;
	m[4] = height*cos;
	m[5] = 0;
	m[6] = x;
	m[7] = y;
	m[8] = 1;
}
