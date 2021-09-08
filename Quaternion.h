#ifndef QUATERNION_H
#define QUATERNION_H
extern void quat_norm(float* t);
extern void quat_rotX(float* ret, float* strt, float r);
extern void quat_rotY(float* ret, float* strt, float r);
extern void quat_rotZ(float* ret, float* strt, float r);
extern void quat_mult(float* ret, float* a, float* b);
extern void quat_print(float* t);
#endif
