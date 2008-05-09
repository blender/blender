#undef TEST_ACTIVE
//#define ACTIVE 1
/**
 * blenlib/BLI_arithb.h    mar 2001 Nzc
 *
 * $Id$ 
 *
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 * */

#ifndef BLI_ARITHB_H
#define BLI_ARITHB_H

#ifdef __cplusplus
extern "C" {
#endif

#ifndef M_PI
#define M_PI		3.14159265358979323846
#endif
#ifndef M_PI_2
#define M_PI_2		1.57079632679489661923
#endif
#ifndef M_SQRT2
#define M_SQRT2		1.41421356237309504880
#endif
#ifndef M_SQRT1_2
#define M_SQRT1_2	0.70710678118654752440
#endif

#ifdef WIN32
	#ifndef FREE_WINDOWS
		#define isnan(n) _isnan(n)
		#define finite _finite
	#endif
#endif

#define MAT4_UNITY {{ 1.0, 0.0, 0.0, 0.0},\
					{ 0.0, 1.0, 0.0, 0.0},\
					{ 0.0, 0.0, 1.0, 0.0},\
					{ 0.0, 0.0, 0.0, 1.0}}

#define MAT3_UNITY {{ 1.0, 0.0, 0.0},\
					{ 0.0, 1.0, 0.0},\
					{ 0.0, 0.0, 1.0}}


void CalcCent3f(float *cent,  float *v1, float *v2, float *v3);
void CalcCent4f(float *cent, float *v1, float *v2, float *v3, float *v4);

void Crossf(float *c, float *a, float *b);
void Projf(float *c, float *v1, float *v2);

float Inpf(float *v1, float *v2);
float Inp2f(float *v1, float *v2);

float Normalize(float *n);
float Normalize2(float *n);

float Sqrt3f(float f);
double Sqrt3d(double d);

float saacos(float fac);
float saasin(float fac);
float sasqrt(float fac);

int FloatCompare(float *v1, float *v2, float limit);
float FloatLerpf(float target, float origin, float fac);

float CalcNormFloat(float *v1, float *v2, float *v3, float *n);
float CalcNormFloat4(float *v1, float *v2, float *v3, float *v4, float *n);

void CalcNormLong(int *v1, int *v2, int *v3, float *n);
/* CalcNormShort: is ook uitprodukt - (translates as 'is also out/cross product') */
void CalcNormShort(short *v1, short *v2, short *v3, float *n);
float power_of_2(float val);

/**
 * @section Euler conversion routines
 */

void EulToMat3(float *eul, float mat[][3]);
void EulToMat4(float *eul, float mat[][4]);

void Mat3ToEul(float tmat[][3], float *eul);
void Mat4ToEul(float tmat[][4],float *eul);

void EulToQuat(float *eul, float *quat);

void compatible_eul(float *eul, float *oldrot);

void Mat3ToCompatibleEul(float mat[][3], float *eul, float *oldrot);


/**
 * @section Quaternion arithmetic routines
 */

void QuatToEul(float *quat, float *eul);
void QuatOne(float *);
void QuatMul(float *, float *, float *);
void QuatMulVecf(float *q, float *v);
void QuatMulf(float *q, float f);
void QuatMulFac(float *q, float fac);

void NormalQuat(float *);
void VecRotToQuat(float *vec, float phi, float *quat);

void QuatSub(float *q, float *q1, float *q2);
void QuatConj(float *q);
void QuatInv(float *q);
float QuatDot(float *q1, float *q2);
void QuatCopy(float *q1, float *q2);

void printquat(char *str, float q[4]);

void QuatInterpol(float *result, float *quat1, float *quat2, float t);
void QuatAdd(float *result, float *quat1, float *quat2, float t);


/**
 * @section matrix multiplication and copying routines
 */

void Mat3MulFloat(float *m, float f);
void Mat4MulFloat(float *m, float f);
void Mat4MulFloat3(float *m, float f);

void Mat3Transp(float mat[][3]);
void Mat4Transp(float mat[][4]);

int Mat4Invert(float inverse[][4], float mat[][4]);
void Mat4InvertSimp(float inverse[][4], float mat[][4]);
void Mat4Inv(float *m1, float *m2);
void Mat4InvGG(float out[][4], float in[][4]);
void Mat3Inv(float m1[][3], float m2[][3]);

void Mat3CpyMat4(float m1[][3],float m2[][4]);
void Mat4CpyMat3(float m1[][4], float m2[][3]); 

void Mat4BlendMat4(float out[][4], float dst[][4], float src[][4], float srcweight);

float Det2x2(float a,float b,float c, float d);

float Det3x3(
	float a1, float a2, float a3,
	float b1, float b2, float b3,
	float c1, float c2, float c3 
);

float Det4x4(float m[][4]);

void Mat3Adj(float m1[][3], float m[][3]);
void Mat4Adj(float out[][4], float in[][4]);

void Mat4MulMat4(float m1[][4], float m2[][4], float m3[][4]);
void subMat4MulMat4(float *m1, float *m2, float *m3);
#ifndef TEST_ACTIVE
void Mat3MulMat3(float m1[][3], float m3[][3], float m2[][3]);
#else
void Mat3MulMat3(float *m1, float *m3, float *m2);
#endif
void Mat4MulMat34(float (*m1)[4], float (*m3)[3], float (*m2)[4]);
void Mat4CpyMat4(float m1[][4], float m2[][4]);
void Mat4SwapMat4(float *m1, float *m2);
void Mat3CpyMat3(float m1[][3], float m2[][3]);

void Mat3MulSerie(float answ[][3],
	float m1[][3], float m2[][3], float m3[][3],
	float m4[][3], float m5[][3], float m6[][3],
	float m7[][3], float m8[][3]
);

void Mat4MulSerie(float answ[][4], float m1[][4],
	float m2[][4], float m3[][4], float m4[][4],
	float m5[][4], float m6[][4], float m7[][4],
	float m8[][4]
);
	
void Mat4Clr(float *m);
void Mat3Clr(float *m);
	
void Mat3One(float m[][3]);
void Mat4One(float m[][4]);

void Mat3Ortho(float mat[][3]);
void Mat4Ortho(float mat[][4]);

void VecMat4MulVecfl(float *in, float mat[][4], float *vec);
void Mat4MulMat43(float (*m1)[4], float (*m3)[4], float (*m2)[3]);
void Mat3IsMat3MulMat4(float m1[][3], float m2[][3], float m3[][4]);

void Mat4MulVec(float mat[][4],int *vec);
void Mat4MulVecfl(float mat[][4], float *vec);
void Mat4Mul3Vecfl(float mat[][4], float *vec);
void Mat4MulVec3Project(float mat[][4],float *vec);
void Mat4MulVec4fl(float mat[][4], float *vec);
void Mat3MulVec(float mat[][3],int *vec);
void Mat3MulVecfl(float mat[][3], float *vec);
void Mat3MulVecd(float mat[][3], double *vec);
void Mat3TransMulVecfl(float mat[][3], float *vec);

void Mat3AddMat3(float m1[][3], float m2[][3], float m3[][3]);
void Mat4AddMat4(float m1[][4], float m2[][4], float m3[][4]);

void VecUpMat3old(float *vec, float mat[][3], short axis);
void VecUpMat3(float *vec, float mat[][3], short axis);
void VecRotToMat3(float *vec, float phi, float mat[][3]);
void VecRotToMat4(float *vec, float phi, float mat[][4]);

void VecCopyf(float *v1, float *v2);
int VecLen(int *v1, int *v2);
float VecLenf(float *v1, float *v2);
float VecLength(float *v);
void VecMulf(float *v1, float f);

int VecLenCompare(float *v1, float *v2, float limit);
int VecCompare(float *v1, float *v2, float limit);
int VecEqual(float *v1, float *v2);

void printvecf(char *str,float v[3]);
void printvec4f(char *str, float v[4]);

void VecAddf(float *v, float *v1, float *v2);
void VecSubf(float *v, float *v1, float *v2);
void VecLerpf(float *target, float *a, float *b, float t);
void VecMidf(float *v, float *v1, float *v2);

void VecOrthoBasisf(float *v, float *v1, float *v2);

float Vec2Lenf(float *v1, float *v2);
float Vec2Length(float *v);
void Vec2Mulf(float *v1, float f);
void Vec2Addf(float *v, float *v1, float *v2);
void Vec2Subf(float *v, float *v1, float *v2);
void Vec2Copyf(float *v1, float *v2);

void vectoquat(float *vec, short axis, short upflag, float *q);

float VecAngle2(float *v1, float *v2);
float VecAngle3(float *v1, float *v2, float *v3);
float NormalizedVecAngle2(float *v1, float *v2);

float VecAngle3_2D(float *v1, float *v2, float *v3);
float NormalizedVecAngle2_2D(float *v1, float *v2);

void euler_rot(float *beul, float ang, char axis);
	

float DistVL2Dfl(float *v1, float *v2, float *v3);
float PdistVL2Dfl(float *v1, float *v2, float *v3);
float PdistVL3Dfl(float *v1, float *v2, float *v3);
void PclosestVL3Dfl(float *closest, float *v1, float *v2, float *v3);
float AreaF2Dfl(float *v1, float *v2, float *v3);
float AreaQ3Dfl(float *v1, float *v2, float *v3, float *v4);
float AreaT3Dfl(float *v1, float *v2, float *v3);
float AreaPoly3Dfl(int nr, float *verts, float *normal);

/* intersect Line-Line
	return:
	-1: colliniar
	 0: no intersection of segments
	 1: exact intersection of segments
	 2: cross-intersection of segments
*/
extern short IsectLL2Df(float *v1, float *v2, float *v3, float *v4);
extern short IsectLL2Ds(short *v1, short *v2, short *v3, short *v4);

/*point in tri,  0 no intersection, 1 intersect */
int IsectPT2Df(float pt[2], float v1[2], float v2[2], float v3[2]);
/* point in quad,  0 no intersection, 1 intersect */
int IsectPQ2Df(float pt[2], float v1[2], float v2[2], float v3[2], float v4[2]);

/* interpolation weights of point in a triangle or quad, v4 may be NULL */
void InterpWeightsQ3Dfl(float *v1, float *v2, float *v3, float *v4, float *co, float *w);
/* interpolation weights of point in a polygon with >= 3 vertices */
void MeanValueWeights(float v[][3], int n, float *co, float *w);

void i_lookat(
	float vx, float vy, 
	float vz, float px, 
	float py, float pz, 
	float twist, float mat[][4]
);

void i_window(
	float left, float right,
	float bottom, float top,
	float nearClip, float farClip,
	float mat[][4]
);

void hsv_to_rgb(float h, float s, float v, float *r, float *g, float *b);
void hex_to_rgb(char *hexcol, float *r, float *g, float *b);
void rgb_to_yuv(float r, float g, float b, float *ly, float *lu, float *lv);
void yuv_to_rgb(float y, float u, float v, float *lr, float *lg, float *lb);
void ycc_to_rgb(float y, float cb, float cr, float *lr, float *lg, float *lb);
void rgb_to_ycc(float r, float g, float b, float *ly, float *lcb, float *lcr);
void rgb_to_hsv(float r, float g, float b, float *lh, float *ls, float *lv);
unsigned int hsv_to_cpack(float h, float s, float v);
unsigned int rgb_to_cpack(float r, float g, float b);
void cpack_to_rgb(unsigned int col, float *r, float *g, float *b);
void MinMaxRGB(short c[]);



void VecStar(float mat[][3],float *vec);

short EenheidsMat(float mat[][3]);

void QuatToMat3(float *q, float m[][3]);
void QuatToMat4(float *q, float m[][4]);

void Mat3ToQuat_is_ok(float wmat[][3], float *q);

void i_ortho(float left, float right, float bottom, float top, float nearClip, float farClip, float matrix[][4]);
void i_polarview(float dist, float azimuth, float incidence, float twist, float Vm[][4]);
void i_translate(float Tx, float Ty, float Tz, float mat[][4]);
void i_multmatrix(float icand[][4], float Vm[][4]);
void i_rotate(float angle, char axis, float mat[][4]);





void MinMax3(float *min, float *max, float *vec);
void SizeToMat3(float *size, float mat[][3]);
void SizeToMat4(float *size, float mat[][4]);

float Mat3ToScalef(float mat[][3]);
float Mat4ToScalef(float mat[][4]);

void printmatrix3(char *str, float m[][3]);
void printmatrix4(char *str, float m[][4]);

/* uit Sig.Proc.85 pag 253 */
void Mat3ToQuat(float wmat[][3], float *q);
void Mat4ToQuat(float m[][4], float *q);

void Mat3ToSize(float mat[][3], float *size);
void Mat4ToSize(float mat[][4], float *size);

void triatoquat(float *v1, float *v2, float *v3, float *quat);

void LocEulSizeToMat4(float mat[][4], float loc[3], float eul[3], float size[3]);
void LocQuatSizeToMat4(float mat[][4], float loc[3], float quat[4], float size[3]);

void tubemap(float x, float y, float z, float *u, float *v);
void spheremap(float x, float y, float z, float *u, float *v);

int LineIntersectsTriangle(float p1[3], float p2[3], float v0[3], float v1[3], float v2[3], float *lambda, float *uv);
int SweepingSphereIntersectsTriangleUV(float p1[3], float p2[3], float radius, float v0[3], float v1[3], float v2[3], float *lambda, float *ipoint);
int AxialLineIntersectsTriangle(int axis, float co1[3], float co2[3], float v0[3], float v1[3], float v2[3], float *lambda);
int AabbIntersectAabb(float min1[3], float max1[3], float min2[3], float max2[3]);
void VecfCubicInterpol(float *x1, float *v1, float *x2, float *v2, float t, float *x, float *v);
void PointInQuad2DUV(float v0[2], float v1[2], float v2[2], float v3[2], float pt[2], float *uv);
void PointInFace2DUV(int isquad, float v0[2], float v1[2], float v2[2], float v3[2], float pt[2], float *uv);
int point_in_tri_prism(float p[3], float v1[3], float v2[3], float v3[3]);

float lambda_cp_line_ex(float p[3], float l1[3], float l2[3], float cp[3]);

typedef struct DualQuat {
	float quat[4];
	float trans[4];

	float scale[4][4];
	float scale_weight;
} DualQuat;

void Mat4ToDQuat(float basemat[][4], float mat[][4], DualQuat *dq);
void DQuatToMat4(DualQuat *dq, float mat[][4]);
void DQuatAddWeighted(DualQuat *dqsum, DualQuat *dq, float weight);
void DQuatNormalize(DualQuat *dq, float totweight);
void DQuatMulVecfl(DualQuat *dq, float *co, float mat[][3]);
void DQuatCpyDQuat(DualQuat *dq1, DualQuat *dq2);
			  
/* Tangent stuff */
typedef struct VertexTangent {
	float tang[3], uv[2];
	struct VertexTangent *next;
} VertexTangent;

void sum_or_add_vertex_tangent(void *arena, VertexTangent **vtang, float *tang, float *uv);
float *find_vertex_tangent(VertexTangent *vtang, float *uv);
void tangent_from_uv(float *uv1, float *uv2, float *uv3, float *co1, float *co2, float *co3, float *n, float *tang);

#ifdef __cplusplus
}
#endif
	
#endif

