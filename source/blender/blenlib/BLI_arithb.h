#undef TEST_ACTIVE
//#define ACTIVE 1
/**
 * blenlib/BLI_arithb.h    mar 2001 Nzc
 *
 * $Id$ 
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
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
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 *
 * The old math stuff from Ton. These will slowly phase out in favour
 * of MTC calls. (or even MoTO :) )
 * */

#ifndef BLI_ARITHB_H
#define BLI_ARITHB_H

#ifdef __cplusplus
extern "C" {
#endif

#define MAT4_UNITY {{ 1.0, 0.0, 0.0, 0.0},\
					{ 0.0, 1.0, 0.0, 0.0},\
					{ 0.0, 0.0, 1.0, 0.0},\
					{ 0.0, 0.0, 0.0, 1.0}}

#define MAT3_UNITY {{ 1.0, 0.0, 0.0},\
					{ 0.0, 1.0, 0.0},\
					{ 0.0, 0.0, 1.0}}


/* matrix operations */
/* void Mat4MulMat4(float m1[][4], float m2[][4], float m3[][4]); */
/* void Mat3MulVecfl(float mat[][3], float *vec);  */
/* or **mat, but it's the same */
/*void Mat3MulVecd(float mat[][3], double *vec); */

/* void Mat4MulVecfl(float mat[][4], float *vec); */
/* void Mat4MulSerie(float answ[][4], float m1[][4], float m2[][4],  */
/*                   float m3[][4], float m4[][4], float m5[][4],  */
/*                   float m6[][4], float m7[][4], float m8[][4]); */
/* int Mat4Invert(float inverse[][4], float mat[][4]); */

/* m2 to m1 */
/*  void Mat3CpyMat4(float m1p[][3], float m2p[][4]); */
/* void Mat3CpyMat4(float *m1p, float *m2p); */

/* m1 to m2 */
/*  void Mat3CpyMat3(float m1p[][3], float m2p[][3]); */
/* void Mat3CpyMat3(float *m1p, float *m2p); */

/* m2 to m1 */
/* void Mat4CpyMat3(float m1p[][4], float m2p[][3]); */

/* M1 = M3*M2 */
/*  void Mat3MulMat3(float m1[][3], float m2[][3], float m3[][3]); */
/*void Mat3MulMat3(float *m1, float *m3, float *m2); */

/* m1 = m2 * m3, ignore the elements on the 4th row/column of m3 */
/*void Mat3IsMat3MulMat4(float m1[][3], float m2[][3], float m3[][4]); */

/* m1 to m2 */
/*  void Mat4CpyMat4(float m1[][4], float m2[][4]); */
/* void Mat4CpyMat4(float *m1, float *m2); */


/* void Mat4Ortho(float mat[][4]); */
/* void Mat4Mul3Vecfl(float mat[][4], float *vec); */
/* void Mat4MulVec4fl(float mat[][4], float *vec); */
/* void Mat4SwapMat4(float *m1, float *m2); */

/* void Mat3Inv(float m1[][3], float m2[][3]); */
/* void Mat4One(float m[][4]); */
/* void Mat3One(float m[][3]); */


	void 
CalcCent3f(
	float *cent, const float *v1, const float *v2, const float *v3
);

	void 
CalcCent4f(
	float *cent, const float *v1, 
	const float *v2, const float *v3,
	const float *v4
);

	void 
Crossf(
	float *c, const float *a, const float *b
);

/**
 * Euler conversion routines
 */

	void 
EulToMat3(
	const float *eul, 
	float mat[][3]
);
	void 
EulToMat4(
	const float* eul, 
	float mat[][4]
);

	void 
Mat3ToEul(
	const float tmat[][3],
	float *eul
);

/**
 * @section Quaternion arithmetic routines
 */

	void 
QuatToEul(
	const float *quat, 
	float *eul
);
	void 
QuatOne(
	float *
);
	void 
QuatMul(
	float *, 
	const float *, 
	const float *
);
	void 
NormalQuat(
	float *
);
	void 
VecRotToQuat(
	const float *vec,
	float phi,
	float *quat
);
	void 
QuatSub(
	float *q, 
	const float *q1, 
	float *q2
);

/**
 * @section matrix multiplication can copying routines
 */

	void 
Mat3MulFloat(
	float *m, 
	float f
);
	void 
Mat4MulFloat(
	float *m, 
	float f
);
	void 
Mat4MulFloat3(
	float *m, 
	float f
);
	void 
Mat3Transp(
	float mat[][3]
);
	void 
Mat4Transp(
	float mat[][4]
);
	int 
Mat4Invert(
	float inverse[][4], 
	const float mat[][4]
);
	void 
Mat4InvertSimp(
	float inverse[][4],
	const float mat[][4]
);
	void 
Mat4Inv(
	float *m1, 
	const float *m2
);
	void 
Mat4InvGG(
	float out[][4], 
	const float in[][4]
);
	void 
Mat3CpyMat4(
	float m1[][3],
	const float m2[][4]
);

	void 
Mat3Inv(
	float m1[][3], 
	const float m2[][3]
);

	void 
Mat4CpyMat3(
	float m1[][4], 
	const float m2[][3]
); 

	float 
Det2x2(
	float a,float b,float c,float d
);

	float 
Det3x3(
	float a1, float a2, float a3,
	float b1, float b2, float b3,
	float c1, float c2, float c3 
);

	float 
Det4x4(
	const float m[][4]
);

	void 
Mat4Adj(
	float out[][4], 
	const float in[][4]
);
	void 
Mat3Adj(
	float m1[][3], 
	const float m[][3]
);
	void 
Mat4MulMat4(
	float m1[][4], 
	const float m2[][4], 
	const float m3[][4]
);
	void 
subMat4MulMat4(
	float *m1, 
	const float *m2, 
	const float *m3
);
#ifndef TEST_ACTIVE
	void 
Mat3MulMat3(
	float m1[][3], 
	const float m3[][3], 
	const float m2[][3]
);
#else
	void 
Mat3MulMat3(
	float *m1, 
	const float *m3, 
	const float *m2
);
#endif
	void 
Mat4MulMat34(
	float (*m1)[4], 
	const float (*m3)[3], 
	const float (*m2)[4]
);
	void 
Mat4CpyMat4(
	float m1[][4], 
	const float m2[][4]
);
	void 
Mat4SwapMat4(
	float *m1, 
	float *m2
);
	void 
Mat3CpyMat3(
	float m1[][3], 
	const float m2[][3]
);
	void 
Mat3MulSerie(
	float answ[][3],
	const float m1[][3], const float m2[][3], const float m3[][3],
	const float m4[][3], const float m5[][3], const float m6[][3],
	const float m7[][3], const float m8[][3]
);
	void 
Mat4MulSerie(
	float answ[][4], 
	const float m1[][4],
	const float m2[][4], const float m3[][4], const float m4[][4],
	const float m5[][4], const float m6[][4], const float m7[][4],
	const float m8[][4]
);
	void 
Mat4Clr(
	float *m
);
	void 
Mat3Clr(
	float *m
);
	void 
Mat3One(
	float m[][3]
);
	void 
Mat4MulVec(
	const float mat[][4],
	int *vec
);
	void 
VecMat4MulVecfl(
	float *in, 
	const float mat[][4], 
	const float *vec
);
	void 
Mat4MulMat43(
	float (*m1)[4], 
	const float (*m3)[4], 
	const float (*m2)[3]
);

	void 
Mat3IsMat3MulMat4(
	float m1[][3], 
	const float m2[][3], 
	const float m3[][4]
);
	void 
Mat4One(
	float m[][4]
);
	void 
Mat4Mul3Vecfl(
	const float mat[][4], 
	float *vec
);
	void 
Mat4MulVec4fl(
	const float mat[][4], 
	float *vec
);
	void 
Mat3MulVec(
	const float mat[][3],
	int *vec
);
	void 
Mat4MulVecfl(
	const float mat[][4], 
	float *vec
);
	void 
Mat4ToQuat(
	const float m[][4], 
	float *q
);
	void 
VecUpMat3old(
	const float *vec, 
	float mat[][3], 
	short axis
);
	int 
FloatCompare(
	const float *v1, 
	const float *v2, 
	float limit
);
	float 
Normalise(
	float *n
);
	float 
CalcNormFloat(
	const float *v1,
	const float *v2,
	const float *v3,
	float *n
);

	float 
CalcNormFloat4(
	const float *v1,
	const float *v2,
	const float *v3,
	const float *v4,
	float *n
);
	float 
VecLenf(
	const float *v1, 
	const float *v2
);
	void 
VecMulf(
	float *v1, 
	float f
);
	int 
VecCompare(
	const float *v1, 
	const float *v2, 
	float limit
);
	float 
Sqrt3f(
	float f
);
	double 
Sqrt3d(
	double d
);

	void 
euler_rot(
	float *beul, 
	float ang, 
	char axis
);
	float 
saacos(
	float fac
);
	float 
sasqrt(
	float fac
);
	float 
Inpf(
	const float *v1, 
	const float *v2
);
	void 
VecSubf(
	float *v, 
	const float *v1, 
	const float *v2
);
	void 
VecAddf(
	float *v, 
	const float *v1, 
	const float *v2
);
	void 
VecUpMat3(
	float *vec, 
	float mat[][3], 
	short axis
);
	float 
DistVL2Dfl(
	const float *v1,
	const float *v2,
	const float *v3
);
	float 
PdistVL2Dfl(
	const float *v1,
	const float *v2,
	const float *v3
);
	float 
AreaF2Dfl(
	const float *v1, 
	const float *v2, 
	const float *v3
);
	float 
AreaQ3Dfl(
	const float *v1, 
	const float *v2, 
	const float *v3, 
	const float *v4
);
	float 
AreaT3Dfl(
	const float *v1, 
	const float *v2, 
	const float *v3
);
	float 
AreaPoly3Dfl(
	int nr, 
	const float *verts, 
	const float *normal
);
	void 
VecRotToMat3(
	const float *vec, 
	float phi, 
	float mat[][3]
);
	float *
vectoquat(
	const float *vec, 
	short axis, 
	short upflag
);

	void 
i_lookat(
	float vx, float vy, 
	float vz, float px, 
	float py, float pz, 
	float twist, float mat[][4]
);
	void 
i_window(
	float left, float right,
	float bottom, float top,
	float nearClip, float farClip,
	float mat[][4]
);

	void 
hsv_to_rgb(
	float h, float s, 
	float v, float *r, 
	float *g, float *b
);

	void 
rgb_to_hsv(
	float r, float g, float b, 
	float *lh, float *ls, float *lv
);
	unsigned int 
hsv_to_cpack(
	float h, float s, float v
);
	unsigned int 
rgb_to_cpack(
	float r, float g, float b
);
	void 
cpack_to_rgb(
	unsigned int col, 
	float *r, float *g, float *b
);

	void 
EulToQuat(
	const float *eul, 
	float *quat
);

	void 
Mat3MulVecfl(
	const float mat[][3], 
	float *vec
);
	void 
Mat3MulVecd(
	const float mat[][3], 
	double *vec
);
	void 
Mat3TransMulVecfl(
	const float mat[][3], 
	float *vec
);
	void 
VecStar(
	float mat[][3],
	const float *vec
);
	short 
EenheidsMat(
	float mat[][3]
);
	void 
printmatrix3(
	const char *str, const float m[][3]
);
	void 
QuatToMat3(
	const float *q, 
	float m[][3]
);
	void 
QuatToMat4(
	const float *q, 
	float m[][4]
);
	void 
QuatToSpher(
	const float *quat, 
	float *sph
);
	void 
Mat3ToSpher(
	const float *mat, 
	float *sph
);
	void 
Mat3ToQuat_is_ok(
	const float wmat[][3], 
	float *q
);
	void 
i_ortho(
	float left, float right, 
	float bottom, float top, 
	float nearClip, float farClip, 
	float matrix[][4]
);
	void 
i_polarview(
	float dist, float azimuth, float incidence, float twist, 
	float Vm[][4]
);
	void 
Mat3Ortho(
	float mat[][3]
);
	void 
Mat4Ortho(
	float mat[][4]
);
	void 
VecCopyf(
	float *v1, 
	const float *v2
);
	int 
VecLen(
	const int *v1, 
	const int *v2
);
	void 
CalcNormShort(
	const short *v1, 
	const short *v2, 
	const short *v3, 
	float *n
) /* is ook uitprodukt */;

	void 
CalcNormLong(
	const int* v1, 
	const int*v2, 
	const int*v3, 
	float *n
);
	void 
MinMax3(
	float *min, 
	float *max, 
	const float *vec
);
	void 
Mat3ToEuln(
	const float tmat[][3],
	float *eul
);
	void 
SizeToMat3(
	const float *size, 
	float mat[][3]
);
	void 
printmatrix4(
	const char *str, 
	const float m[][4]
);
/* uit Sig.Proc.85 pag 253 */
	void 
Mat3ToQuat(
	const float wmat[][3],
	float *q
);
	void 
i_translate(
	float Tx, 
	float Ty, 
	float Tz, 
	float mat[][4]
);
	void 
i_multmatrix(
	const float icand[][4], 
	float Vm[][4]
);
	void 
i_rotate(
	float angle, 
	char axis, 
	float mat[][4]
);
	void 
VecMidf(
	float *v, const float *v1, const float *v2
);
	void 
Mat3ToSize(
	const float mat[][3], float *size
);
	void 
Mat4ToSize(
	const float mat[][4], float *size
);
	void 
triatoquat(
	const float *v1, 
	const float *v2, 
	const float *v3, float *quat
);
	void 
MinMaxRGB(
	short c[]
);


#ifdef __cplusplus
}
#endif
	
#endif

