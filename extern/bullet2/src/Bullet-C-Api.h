#ifndef Bullet_C_API_H
#define Bullet_C_API_H

#ifdef __cplusplus
extern "C"  {
#endif // __cplusplus

double plNearestPoints(float p[3][3], float q[3][3], float *pa, float *pb, float normal[3]);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif

