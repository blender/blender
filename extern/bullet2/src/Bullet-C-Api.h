#ifndef Bullet_C_API_H
#define Bullet_C_API_H

#ifdef __cplusplus
extern "C"  {
#endif // __cplusplus

double plNearestPoints(float p1[3], float p2[3], float p3[3], float q1[3], float q2[3], float q3[3], float *pa, float *pb, float normal[3]);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif

