/////////////////////////////////
// Most work by mboerwinkle
/////////////////////////////////

#ifndef EXTERNLINALG_H
#define EXTERNLINALG_H
/* Unused for now
#include <string.h>

static float distv3f(float* v){
	float d = 0;
	for(int idx = 0; idx < 3; idx++){
		d += v[idx]*v[idx];
	}
	return sqrt(d);
}

static void norm3f(float* v){
	float d = distv3f(v);
	v[0] /= d;
	v[1] /= d;
	v[2] /= d;
}

static void cross(float* res, float* a, float* b){
	res[0]=a[1]*b[2]-a[2]*b[1];
	res[1]=a[2]*b[0]-a[0]*b[2];
	res[2]=a[0]*b[1]-a[1]*b[0];
}

static void quatMult(float* a, float* b, float* r){
	float ret[4];
	ret[0]=(b[0] * a[0] - b[1] * a[1] - b[2] * a[2] - b[3] * a[3]);
	ret[1]=(b[0] * a[1] + b[1] * a[0] + b[2] * a[3] - b[3] * a[2]);
	ret[2]=(b[0] * a[2] - b[1] * a[3] + b[2] * a[0] + b[3] * a[1]);
	ret[3]=(b[0] * a[3] + b[1] * a[2] - b[2] * a[1] + b[3] * a[0]);
	memcpy(r, ret, 4*sizeof(float));
}

static void rotateVec(float* t, float* r, float* ret){
	float p[4] = {0, t[0], t[1], t[2]};
	float revRot[4] = {r[0], -r[1], -r[2], -r[3]};
	quatMult(r, p, p);
	quatMult(p, revRot, p);
	memcpy(ret, &(p[1]), 3*sizeof(float));
}
*/
static void mat4x4Multf(float* res, float* m1, float* m2){
	for(int x = 0; x < 4; x++){
		for(int y = 0; y < 4; y++){
			float v = 0;
			for(int i = 0; i < 4; i++){
				v += m1[y+4*i]*m2[i+4*x];
			}
			res[y+4*x] = v;
		}
	}
}
/* Unused for now
static void mat4x4By4x1Multf(float* res, float* m1, float* m2){
	for(int y = 0; y < 4; y++){
		float v = 0;
		for(int i = 0; i < 4; i++){
			v += m1[y+4*i]*m2[i];
		}
		res[y] = v;
	}
}
*/
static void mat4x4fromQuat(float* M, float *rot){
	M[0] = 1-2*rot[2]*rot[2]-2*rot[3]*rot[3];
	M[1] = 2*rot[1]*rot[2]+2*rot[0]*rot[3];
	M[2] = 2*rot[1]*rot[3]-2*rot[0]*rot[2];
	M[3] = 0;
	M[4] = 2*rot[1]*rot[2]-2*rot[0]*rot[3];
	M[5] = 1-2*rot[1]*rot[1]-2*rot[3]*rot[3];
	M[6] = 2*rot[2]*rot[3]+2*rot[0]*rot[1];
	M[7] = 0;
	M[8] = 2*rot[1]*rot[3]+2*rot[0]*rot[2];
	M[9] = 2*rot[2]*rot[3]-2*rot[0]*rot[1];
	M[10] = 1-2*rot[1]*rot[1]-2*rot[2]*rot[2];
	M[11] = 0;
	M[12] = 0;
	M[13] = 0;
	M[14] = 0;
	M[15] = 1;
}
/* Unused for now
static void mat4x4Scalef(float* res, float xs, float ys, float zs){
	res[0] *= xs;
	res[5] *= ys;
	res[9] *= zs;
}
*/
static void mat4x4Transf(float* res, float x, float y, float z){
	res[12] += x;
	res[13] += y;
	res[14] += z;
}
/* Unused for now
static void mat4x4idenf(float* res){
	for(int i = 0; i < 16; i++) res[i] = 0.0;
	res[0] = 1.0;
	res[5] = 1.0;
	res[10] = 1.0;
	res[15] = 1.0;
}
*/

//https://www.khronos.org/registry/OpenGL-Refpages/gl2.1/xhtml/gluPerspective.xml
static void perspective(float* m, float invSlopeX, float invSlopeY, float zNear, float zFar){
	m[0] = invSlopeX;
	m[1] = 0;
	m[2] = 0;
	m[3] = 0;
	m[4] = 0;
	m[5] = -invSlopeY; // This typically isn't negative, but my coords are all messed up
	m[6] = 0;
	m[7] = 0;
	m[8] = 0;
	m[9] = 0;
	m[10] = (zFar+zNear)/(zNear-zFar);
	m[11] = -1;
	m[12] = 0;
	m[13] = 0;
	m[14] = (2.0*zFar*zNear)/(zNear-zFar);
	m[15] = 0;
}
//https://www.khronos.org/registry/OpenGL-Refpages/gl2.1/xhtml/glOrtho.xml
static void glOrthoEquiv(float* m, float left, float right, float bottom, float top, float nearval, float farval){
	m[0] = 2.0/(right-left);
	m[1] = 0;
	m[2] = 0;
	m[3] = 0;
	m[4] = 0;
	m[5] = 2.0/(top-bottom);
	m[6] = 0;
	m[7] = 0;
	m[8] = 0;
	m[9] = 0;
	m[10] = -2.0/(farval-nearval);
	m[11] = 0;
	m[12] = -((right+left)/(right-left));
	m[13] = -((top+bottom)/(top-bottom));
	m[14] = -((farval+nearval)/(farval-nearval));
	m[15] = 1;
}
/* Unused for now
//https://www.khronos.org/opengl/wiki/GluLookAt_code
static void glhLookAtf2( float *matrix, float *eyePosition3D, float *center3D, float *upVector3D ){
	float forward[3], side[3], up[3];
	float matrix2[16], resultMatrix[16];
	// --------------------
	forward[0] = center3D[0] - eyePosition3D[0];
	forward[1] = center3D[1] - eyePosition3D[1];
	forward[2] = center3D[2] - eyePosition3D[2];
	norm3f(forward);
	// --------------------
	// Side = forward x up
	cross(side, forward, upVector3D);
	norm3f(side);
	// Recompute up as: up = side x forward
	cross(up, side, forward);
	// --------------------
	matrix2[0] = side[0];
	matrix2[4] = side[1];
	matrix2[8] = side[2];
	matrix2[12] = 0.0;
	// --------------------
	matrix2[1] = up[0];
	matrix2[5] = up[1];
	matrix2[9] = up[2];
	matrix2[13] = 0.0;
	// --------------------
	matrix2[2] = -forward[0];
	matrix2[6] = -forward[1];
	matrix2[10] = -forward[2];
	matrix2[14] = 0.0;
	// --------------------
	matrix2[3] = matrix2[7] = matrix2[11] = 0.0;
	matrix2[15] = 1.0;
	// --------------------
	mat4x4Multf(resultMatrix, matrix, matrix2);
	mat4x4Transf(resultMatrix, -eyePosition3D[0], -eyePosition3D[1], -eyePosition3D[2]);
	// --------------------
	memcpy(matrix, resultMatrix, 16*sizeof(float));
}
*/
#endif
