#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "Quaternion.h"
void quat_norm(float* t){
	float len = t[0]*t[0]+t[1]*t[1]+t[2]*t[2]+t[3]*t[3];
	t[0]/=len;
	t[1]/=len;
	t[2]/=len;
	t[3]/=len;
}
void quat_rotateBy(float* ret, float* by){
	quat_mult(ret, ret, by);
}
void quat_rotX(float* ret, float* strt, float r){
	r /= 2.0;
	float tmp[4] = {cos(r), sin(r), 0, 0};
	quat_rotateBy(ret, tmp);
}
void quat_rotY(float* ret, float* strt, float r){
	r /= 2.0;
	float tmp[4] = {cos(r), 0, sin(r), 0};
	quat_rotateBy(ret, tmp);
}
void quat_rotZ(float* ret, float* strt, float r){
	r /= 2.0;
	float tmp[4] = {cos(r), 0, 0, sin(r)};
	quat_rotateBy(ret, tmp);
}
void quat_mult(float* ret, float* a, float* b){
	float t[4];//fixme dont have save. instead return an array
	t[0]=(b[0] * a[0] - b[1] * a[1] - b[2] * a[2] - b[3] * a[3]);
	t[1]=(b[0] * a[1] + b[1] * a[0] + b[2] * a[3] - b[3] * a[2]);
	t[2]=(b[0] * a[2] - b[1] * a[3] + b[2] * a[0] + b[3] * a[1]);
	t[3]=(b[0] * a[3] + b[1] * a[2] - b[2] * a[1] + b[3] * a[0]);
	memcpy(ret, t, sizeof(float)*4);
}
void quat_print(float* t){
	printf("%.2f %.2f %.2f %.2f\n", t[0],t[1],t[2],t[3]);
}
