/*  implicit.c      
* 
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
* The Original Code is Copyright (C) Blender Foundation
* All rights reserved.
*
* The Original Code is: all of this file.
*
* Contributor(s): none yet.
*
* ***** END GPL/BL DUAL LICENSE BLOCK *****
*/
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "MEM_guardedalloc.h"
/* types */
#include "DNA_curve_types.h"
#include "DNA_object_types.h"
#include "DNA_object_force.h"	
#include "DNA_cloth_types.h"	
#include "DNA_key_types.h"
#include "DNA_mesh_types.h"
#include "DNA_modifier_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_lattice_types.h"
#include "DNA_scene_types.h"
#include "DNA_modifier_types.h"
#include "BLI_arithb.h"
#include "BLI_blenlib.h"
#include "BLI_edgehash.h"
#include "BLI_threads.h"
#include "BKE_collisions.h"
#include "BKE_curve.h"
#include "BKE_displist.h"
#include "BKE_effect.h"
#include "BKE_global.h"
#include "BKE_key.h"
#include "BKE_object.h"
#include "BKE_cloth.h"
#include "BKE_modifier.h"
#include "BKE_utildefines.h"
#include "BKE_global.h"
#include  "BIF_editdeform.h"

#include "Bullet-C-Api.h"

#include <emmintrin.h>
#include <xmmintrin.h>
#include <pmmintrin.h>

#ifdef _WIN32
#include <windows.h>
static LARGE_INTEGER _itstart, _itend;
static LARGE_INTEGER ifreq;
void itstart(void)
{
	static int first = 1;
	if(first) {
		QueryPerformanceFrequency(&ifreq);
		first = 0;
	}
	QueryPerformanceCounter(&_itstart);
}
void itend(void)
{
	QueryPerformanceCounter(&_itend);
}
double itval()
{
	return ((double)_itend.QuadPart -
		(double)_itstart.QuadPart)/((double)ifreq.QuadPart);
}
#else
#include <sys/time.h>

static struct timeval _itstart, _itend;
static struct timezone itz;
void itstart(void)
{
	gettimeofday(&_itstart, &itz);
}
void itend(void)
{
	gettimeofday(&_itend,&itz);
}
double itval()
{
	double t1, t2;
	t1 =  (double)_itstart.tv_sec + (double)_itstart.tv_usec/(1000*1000);
	t2 =  (double)_itend.tv_sec + (double)_itend.tv_usec/(1000*1000);
	return t2-t1;
}
#endif

struct Cloth;

//////////////////////////////////////////
/* fast vector / matrix library, enhancements are welcome :) -dg */
/////////////////////////////////////////

/* DEFINITIONS */
#ifdef __GNUC__
typedef float lfVector[4] __attribute__ ((aligned (16)));
#else
typedef __declspec(align(16)) lfVector[4];
#endif

#ifdef __GNUC__
typedef struct fmatrix3x3 {
	float m[3][4] __attribute__ ((aligned (16))); /* 3x3 matrix */
	unsigned int c,r; /* column and row number */
	int pinned; /* is this vertex allowed to move? */
	float n1,n2,n3; /* three normal vectors for collision constrains */
	unsigned int vcount; /* vertex count */
	unsigned int scount; /* spring count */ 
} fmatrix3x3;
#else
typedef struct fmatrix3x3 {
	__declspec(align(16))
	float m[3][4]; /* 3x3 matrix */
	unsigned int c,r; /* column and row number */
	int pinned; /* is this vertex allowed to move? */
	float n1,n2,n3; /* three normal vectors for collision constrains */
	unsigned int vcount; /* vertex count */
	unsigned int scount; /* spring count */ 
} fmatrix3x3;
#endif


///////////////////////////
// float[3] vector
///////////////////////////
/* simple vector code */

/* STATUS: verified */
DO_INLINE void mul_fvector_S(float to[3], float from[3], float scalar)
{
	to[0] = from[0] * scalar;
	to[1] = from[1] * scalar;
	to[2] = from[2] * scalar;
}
/* simple cross product */
/* STATUS: verified */
DO_INLINE void cross_fvector(float to[3], float vectorA[3], float vectorB[3])
{
	float temp[3];
	
	temp[0] = vectorA[1] * vectorB[2] - vectorA[2] * vectorB[1];
	temp[1] = vectorA[2] * vectorB[0] - vectorA[0] * vectorB[2];
	temp[2] = vectorA[0] * vectorB[1] - vectorA[1] * vectorB[0];
	
	VECCOPY(to, temp);
}

/* simple v^T * v product ("outer product") */
/* STATUS: HAS TO BE verified (*should* work) */
DO_INLINE void mul_fvectorT_fvector(float to[3][4], float vectorA[3], float vectorB[3])
{
	mul_fvector_S(to[0], vectorB, vectorA[0]);
	mul_fvector_S(to[1], vectorB, vectorA[1]);
	mul_fvector_S(to[2], vectorB, vectorA[2]);
}
/* simple v^T * v product with scalar ("outer product") */
/* STATUS: HAS TO BE verified (*should* work) */
DO_INLINE void mul_fvectorT_fvectorS(float to[3][4], float vectorA[3], float vectorB[3], float aS)
{
	mul_fvector_S(to[0], vectorB, vectorA[0]* aS);
	mul_fvector_S(to[1], vectorB, vectorA[1]* aS);
	mul_fvector_S(to[2], vectorB, vectorA[2]* aS);
}

/* printf vector[3] on console: for debug output */
void print_fvector(float m3[3])
{
	printf("%f\n%f\n%f\n\n",m3[0],m3[1],m3[2]);
}

///////////////////////////
// long float vector float (*)[3]
///////////////////////////
/* print long vector on console: for debug output */
DO_INLINE void print_lfvector(lfVector *fLongVector, unsigned int verts)
{
	unsigned int i = 0;
	for(i = 0; i < verts; i++)
	{
		print_fvector(fLongVector[i]);
	}
}
/* create long vector */
DO_INLINE lfVector *create_lfvector(unsigned int verts)
{
	// TODO: check if memory allocation was successfull */
	return  (lfVector *)MEM_callocN (verts * sizeof(lfVector), "cloth_implicit_alloc_vector");
	// return (lfVector *)cloth_aligned_malloc(&MEMORY_BASE, verts * sizeof(lfVector));
}
/* delete long vector */
DO_INLINE void del_lfvector(lfVector *fLongVector)
{
	if (fLongVector != NULL)
	{
		MEM_freeN (fLongVector);
		// cloth_aligned_free(&MEMORY_BASE, fLongVector);
	}
}
/* copy long vector */
DO_INLINE void cp_lfvector(lfVector *to, lfVector *from, unsigned int verts)
{
	memcpy(to, from, verts * sizeof(lfVector));
}
/* init long vector with float[3] */
DO_INLINE void init_lfvector(lfVector *fLongVector, float vector[3], unsigned int verts)
{
	unsigned int i = 0;
	for(i = 0; i < verts; i++)
	{
		VECCOPY(fLongVector[i], vector);
	}
}
/* zero long vector with float[3] */
DO_INLINE void zero_lfvector(lfVector *to, unsigned int verts)
{
	memset(to, 0.0f, verts * sizeof(lfVector));
}
/* multiply long vector with scalar*/
DO_INLINE void mul_lfvectorS(lfVector *to, lfVector *fLongVector, float scalar, unsigned int verts)
{
	unsigned int i = 0;

	for(i = 0; i < verts; i++)
	{
		mul_fvector_S(to[i], fLongVector[i], scalar);
	}
}
/* multiply long vector with scalar*/
/* A -= B * float */
DO_INLINE void submul_lfvectorS(lfVector *to, lfVector *fLongVector, float scalar, unsigned int verts)
{
	unsigned int i = 0;
	for(i = 0; i < verts; i++)
	{
		VECSUBMUL(to[i], fLongVector[i], scalar);
	}
}
/* dot product for big vector */
DO_INLINE float dot_lfvector(lfVector *fLongVectorA, lfVector *fLongVectorB, unsigned int verts)
{
	unsigned int i = 0;
	float temp = 0.0;
// schedule(guided, 2)
#pragma omp parallel for reduction(+: temp) schedule(static)
	for(i = 0; i < verts; i++)
	{
		temp += INPR(fLongVectorA[i], fLongVectorB[i]);
	}
	return temp;
}
/* A = B + C  --> for big vector */
DO_INLINE void add_lfvector_lfvector(lfVector *to, lfVector *fLongVectorA, lfVector *fLongVectorB, unsigned int verts)
{
	unsigned int i = 0;

	for(i = 0; i < verts; i++)
	{
		VECADD(to[i], fLongVectorA[i], fLongVectorB[i]);
	}

}
/* A = B + C * float --> for big vector */
DO_INLINE void add_lfvector_lfvectorS(lfVector *to, lfVector *fLongVectorA, lfVector *fLongVectorB, float bS, unsigned int verts)
{
	unsigned int i = 0;

	for(i = 0; i < verts; i++)
	{
		VECADDS(to[i], fLongVectorA[i], fLongVectorB[i], bS);

	}
}
/* A = B * float + C * float --> for big vector */
DO_INLINE void add_lfvectorS_lfvectorS(lfVector *to, lfVector *fLongVectorA, float aS, lfVector *fLongVectorB, float bS, unsigned int verts)
{
	unsigned int i = 0;

	for(i = 0; i < verts; i++)
	{
		VECADDSS(to[i], fLongVectorA[i], aS, fLongVectorB[i], bS);
	}
}
/* A = B - C * float --> for big vector */
DO_INLINE void sub_lfvector_lfvectorS(lfVector *to, lfVector *fLongVectorA, lfVector *fLongVectorB, float bS, unsigned int verts)
{
	unsigned int i = 0;
	for(i = 0; i < verts; i++)
	{
		VECSUBS(to[i], fLongVectorA[i], fLongVectorB[i], bS);
	}

}
/* A = B - C --> for big vector */
DO_INLINE void sub_lfvector_lfvector(lfVector *to, lfVector *fLongVectorA, lfVector *fLongVectorB, unsigned int verts)
{
	unsigned int i = 0;

	for(i = 0; i < verts; i++)
	{
		VECSUB(to[i], fLongVectorA[i], fLongVectorB[i]);
	}

}
///////////////////////////
// 3x3 matrix
///////////////////////////
/* printf 3x3 matrix on console: for debug output */
void print_fmatrix(float m3[3][4])
{
	printf("%f\t%f\t%f\n",m3[0][0],m3[0][1],m3[0][2]);
	printf("%f\t%f\t%f\n",m3[1][0],m3[1][1],m3[1][2]);
	printf("%f\t%f\t%f\n\n",m3[2][0],m3[2][1],m3[2][2]);
}
/* copy 3x3 matrix */
DO_INLINE void cp_fmatrix(float to[3][4], float from[3][4])
{
	memcpy(to, from, sizeof (float) * 12);
	/*
	VECCOPY(to[0], from[0]);
	VECCOPY(to[1], from[1]);
	VECCOPY(to[2], from[2]);
	*/
}
/* calculate determinant of 3x3 matrix */
DO_INLINE float det_fmatrix(float m[3][4])
{
	return  m[0][0]*m[1][1]*m[2][2] + m[1][0]*m[2][1]*m[0][2] + m[0][1]*m[1][2]*m[2][0] 
	-m[0][0]*m[1][2]*m[2][1] - m[0][1]*m[1][0]*m[2][2] - m[2][0]*m[1][1]*m[0][2];
}
DO_INLINE void inverse_fmatrix(float to[3][4], float from[3][4])
{
	unsigned int i, j;
	float d;

	if((d=det_fmatrix(from))==0)
	{
		printf("can't build inverse");
		exit(0);
	}
	for(i=0;i<3;i++) 
	{
		for(j=0;j<3;j++) 
		{
			int i1=(i+1)%3;
			int i2=(i+2)%3;
			int j1=(j+1)%3;
			int j2=(j+2)%3;
			// reverse indexs i&j to take transpose
			to[j][i] = (from[i1][j1]*from[i2][j2]-from[i1][j2]*from[i2][j1])/d;
			/*
			if(i==j)
			to[i][j] = 1.0f / from[i][j];
			else
			to[i][j] = 0;
			*/
		}
	}

}

/* 3x3 matrix multiplied by a scalar */
/* STATUS: verified */
DO_INLINE void mul_fmatrix_S(float matrix[3][4], float scalar)
{
	mul_fvector_S(matrix[0], matrix[0],scalar);
	mul_fvector_S(matrix[1], matrix[1],scalar);
	mul_fvector_S(matrix[2], matrix[2],scalar);
}

/* a vector multiplied by a 3x3 matrix */
/* STATUS: verified */
DO_INLINE void mul_fvector_fmatrix(float *to, float *from, float matrix[3][4])
{
	float temp[3];
	
	VECCOPY(temp, from);
	
	to[0] = matrix[0][0]*temp[0] + matrix[1][0]*temp[1] + matrix[2][0]*temp[2];
	to[1] = matrix[0][1]*temp[0] + matrix[1][1]*temp[1] + matrix[2][1]*temp[2];
	to[2] = matrix[0][2]*temp[0] + matrix[1][2]*temp[1] + matrix[2][2]*temp[2];
}

/* 3x3 matrix multiplied by a vector */
/* STATUS: verified */
#ifdef __SSE3__
DO_INLINE void mul_fmatrix_fvector(float *to, float matrix[3][4], float *from) {
	float temp[4];
	__m128 v1, v2, v3, v4;
	
	v1 = _mm_loadu_ps(&matrix[0][0]);
	v2 = _mm_loadu_ps(&matrix[1][0]);
	v3 = _mm_loadu_ps(&matrix[2][0]);
	v4 = _mm_loadu_ps(from);

 	// stuff
	v1 = _mm_mul_ps(v1, v4);
	v2 = _mm_mul_ps(v2, v4);
	v3 = _mm_mul_ps(v3, v4);
	v1 = _mm_hadd_ps(v1, v2);
	v3 = _mm_hadd_ps(v3, _mm_setzero_ps());
	v4 = _mm_hadd_ps(v1, v3);
	
	_mm_storeu_ps(to, v4);
}
#else
DO_INLINE void mul_fmatrix_fvector(float *to, float matrix[3][4], float *from)
{
	to[0] = INPR(matrix[0],from);
	to[1] = INPR(matrix[1],from);
	to[2] = INPR(matrix[2],from);
}
#endif
		
/* 3x3 matrix multiplied by a 3x3 matrix */
/* STATUS: verified */
DO_INLINE void mul_fmatrix_fmatrix(float to[3][4], float matrixA[3][4], float matrixB[3][4])
{
	mul_fvector_fmatrix(to[0], matrixA[0],matrixB);
	mul_fvector_fmatrix(to[1], matrixA[1],matrixB);
	mul_fvector_fmatrix(to[2], matrixA[2],matrixB);
}
/* 3x3 matrix addition with 3x3 matrix */
DO_INLINE void add_fmatrix_fmatrix(float to[3][4], float matrixA[3][4], float matrixB[3][4])
{
	VECADD(to[0], matrixA[0], matrixB[0]);
	VECADD(to[1], matrixA[1], matrixB[1]);
	VECADD(to[2], matrixA[2], matrixB[2]);
}
/* 3x3 matrix add-addition with 3x3 matrix */
DO_INLINE void addadd_fmatrix_fmatrix(float to[3][4], float matrixA[3][4], float matrixB[3][4])
{
	VECADDADD(to[0], matrixA[0], matrixB[0]);
	VECADDADD(to[1], matrixA[1], matrixB[1]);
	VECADDADD(to[2], matrixA[2], matrixB[2]);
}
/* 3x3 matrix sub-addition with 3x3 matrix */
DO_INLINE void addsub_fmatrixS_fmatrixS(float to[3][4], float matrixA[3][4], float aS, float matrixB[3][4], float bS)
{
	VECADDSUBSS(to[0], matrixA[0], aS, matrixB[0], bS);
	VECADDSUBSS(to[1], matrixA[1], aS, matrixB[1], bS);
	VECADDSUBSS(to[2], matrixA[2], aS, matrixB[2], bS);
}
/* A -= B + C (3x3 matrix sub-addition with 3x3 matrix) */
DO_INLINE void subadd_fmatrix_fmatrix(float to[3][4], float matrixA[3][4], float matrixB[3][4])
{
	VECSUBADD(to[0], matrixA[0], matrixB[0]);
	VECSUBADD(to[1], matrixA[1], matrixB[1]);
	VECSUBADD(to[2], matrixA[2], matrixB[2]);
}
/* A -= B*x + C*y (3x3 matrix sub-addition with 3x3 matrix) */
DO_INLINE void subadd_fmatrixS_fmatrixS(float to[3][4], float matrixA[3][4], float aS, float matrixB[3][4], float bS)
{
	VECSUBADDSS(to[0], matrixA[0], aS, matrixB[0], bS);
	VECSUBADDSS(to[1], matrixA[1], aS, matrixB[1], bS);
	VECSUBADDSS(to[2], matrixA[2], aS, matrixB[2], bS);
}
/* A = B - C (3x3 matrix subtraction with 3x3 matrix) */
DO_INLINE void sub_fmatrix_fmatrix(float to[3][4], float matrixA[3][4], float matrixB[3][4])
{
	VECSUB(to[0], matrixA[0], matrixB[0]);
	VECSUB(to[1], matrixA[1], matrixB[1]);
	VECSUB(to[2], matrixA[2], matrixB[2]);
}
/* A += B - C (3x3 matrix add-subtraction with 3x3 matrix) */
DO_INLINE void addsub_fmatrix_fmatrix(float to[3][4], float matrixA[3][4], float matrixB[3][4])
{
	VECADDSUB(to[0], matrixA[0], matrixB[0]);
	VECADDSUB(to[1], matrixA[1], matrixB[1]);
	VECADDSUB(to[2], matrixA[2], matrixB[2]);
}
/////////////////////////////////////////////////////////////////
// special functions
/////////////////////////////////////////////////////////////////
/* a vector multiplied and added to/by a 3x3 matrix */
/*
DO_INLINE void muladd_fvector_fmatrix(float to[3], float from[3], float matrix[3][3])
{
	to[0] += matrix[0][0]*from[0] + matrix[1][0]*from[1] + matrix[2][0]*from[2];
	to[1] += matrix[0][1]*from[0] + matrix[1][1]*from[1] + matrix[2][1]*from[2];
	to[2] += matrix[0][2]*from[0] + matrix[1][2]*from[1] + matrix[2][2]*from[2];
}
*/
/* 3x3 matrix multiplied and added  to/by a 3x3 matrix  and added to another 3x3 matrix */
/*
DO_INLINE void muladd_fmatrix_fmatrix(float to[3][3], float matrixA[3][3], float matrixB[3][3])
{
	muladd_fvector_fmatrix(to[0], matrixA[0],matrixB);
	muladd_fvector_fmatrix(to[1], matrixA[1],matrixB);
	muladd_fvector_fmatrix(to[2], matrixA[2],matrixB);
}
*/
/* a vector multiplied and sub'd to/by a 3x3 matrix */
/*
DO_INLINE void mulsub_fvector_fmatrix(float to[3], float from[3], float matrix[3][3])
{
	to[0] -= matrix[0][0]*from[0] + matrix[1][0]*from[1] + matrix[2][0]*from[2];
	to[1] -= matrix[0][1]*from[0] + matrix[1][1]*from[1] + matrix[2][1]*from[2];
	to[2] -= matrix[0][2]*from[0] + matrix[1][2]*from[1] + matrix[2][2]*from[2];
}
*/
/* 3x3 matrix multiplied and sub'd  to/by a 3x3 matrix  and added to another 3x3 matrix */
/*
DO_INLINE void mulsub_fmatrix_fmatrix(float to[3][3], float matrixA[3][3], float matrixB[3][3])
{
	mulsub_fvector_fmatrix(to[0], matrixA[0],matrixB);
	mulsub_fvector_fmatrix(to[1], matrixA[1],matrixB);
	mulsub_fvector_fmatrix(to[2], matrixA[2],matrixB);
}
*/
/* 3x3 matrix multiplied+added by a vector */
/* STATUS: verified */

#ifdef __SSE3__
DO_INLINE void muladd_fmatrix_fvector(float to[3], float matrix[3][4], float from[3]) {
	__m128 v1, v2, v3, v4;
	
	v1 = _mm_load_ps(&matrix[0]);
	v2 = _mm_load_ps(&matrix[1]);
	v3 = _mm_load_ps(&matrix[2]);
	v4 = _mm_load_ps(from);

 	// stuff
	v1 = _mm_mul_ps(v1, v4);
	v2 = _mm_mul_ps(v2, v4);
	v3 = _mm_mul_ps(v3, v4);
	v1 = _mm_hadd_ps(v1, v2);
	v3 = _mm_hadd_ps(v3, _mm_setzero_ps());
	v1 = _mm_hadd_ps(v1, v3);
	
	v4 = _mm_load_ps(to);
	v4 = _mm_add_ps(v4,v1);

	_mm_store_ps(to, v4);
}
#else
DO_INLINE void muladd_fmatrix_fvector(float to[3], float matrix[3][4], float from[3])
{
	float temp[3] = { 0,0,0 };
	
	temp[0] = INPR(matrix[0],from);
	temp[1] = INPR(matrix[1],from);
	temp[2] = INPR(matrix[2],from);	
	
	VECADD(to, to, temp);
}
#endif

/* 3x3 matrix multiplied+sub'ed by a vector */
/*
DO_INLINE void mulsub_fmatrix_fvector(float to[3], float matrix[3][3], float from[3])
{
	to[0] -= INPR(matrix[0],from);
	to[1] -= INPR(matrix[1],from);
	to[2] -= INPR(matrix[2],from);
}
*/
/////////////////////////////////////////////////////////////////

///////////////////////////
// SPARSE SYMMETRIC big matrix with 3x3 matrix entries
///////////////////////////
/* printf a big matrix on console: for debug output */
void print_bfmatrix(fmatrix3x3 *m3)
{
	unsigned int i = 0;

	for(i = 0; i < m3[0].vcount + m3[0].scount; i++)
	{
		print_fmatrix(m3[i].m);
	}
}
/* create big matrix */
DO_INLINE fmatrix3x3 *create_bfmatrix(unsigned int verts, unsigned int springs)
{
	// TODO: check if memory allocation was successfull */
	fmatrix3x3 *temp = (fmatrix3x3 *)MEM_callocN (sizeof (fmatrix3x3) * (verts + springs), "cloth_implicit_alloc_matrix");
	temp[0].vcount = verts;
	temp[0].scount = springs;
	return temp;
}
/* delete big matrix */
DO_INLINE void del_bfmatrix(fmatrix3x3 *matrix)
{
	if (matrix != NULL)
	{
		MEM_freeN (matrix);
	}
}
/* copy big matrix */
DO_INLINE void cp_bfmatrix(fmatrix3x3 *to, fmatrix3x3 *from)
{	
	// TODO bounds checking	
	memcpy(to, from, sizeof(fmatrix3x3) * (from[0].vcount+from[0].scount) );
}
/* init the diagonal of big matrix */
// slow in parallel
DO_INLINE void initdiag_bfmatrix(fmatrix3x3 *matrix, float m3[3][4])
{
	unsigned int i,j;
	float tmatrix[3][4] = {{0,0,0,0},{0,0,0,0},{0,0,0,0}};

	for(i = 0; i < matrix[0].vcount; i++)
	{		
		cp_fmatrix(matrix[i].m, m3); 
	}
	for(j = matrix[0].vcount; j < matrix[0].vcount+matrix[0].scount; j++)
	{
		cp_fmatrix(matrix[j].m, tmatrix); 
	}
}
/* init big matrix */
DO_INLINE void init_bfmatrix(fmatrix3x3 *matrix, float m3[3][4])
{
	unsigned int i;

	for(i = 0; i < matrix[0].vcount+matrix[0].scount; i++)
	{
		cp_fmatrix(matrix[i].m, m3); 
	}
}
/* multiply big matrix with scalar*/
DO_INLINE void mul_bfmatrix_S(fmatrix3x3 *matrix, float scalar)
{
	unsigned int i = 0;
	for(i = 0; i < matrix[0].vcount+matrix[0].scount; i++)
	{
		mul_fmatrix_S(matrix[i].m, scalar);
	}
}
/* SPARSE SYMMETRIC multiply big matrix with long vector*/
/* STATUS: verified */
void mul_bfmatrix_lfvector( lfVector *to, fmatrix3x3 *from, lfVector *fLongVector)
{
	unsigned int i = 0;
	float *tflongvector;
	float temp[4]={0,0,0,0};
	
	zero_lfvector(to, from[0].vcount);
	
	/* process diagonal elements */	
	for(i = 0; i < from[0].vcount; i++)
	{
		mul_fmatrix_fvector(to[from[i].r], from[i].m, fLongVector[from[i].c]);
	}

	/* process off-diagonal entries (every off-diagonal entry needs to be symmetric) */
	// TODO: pragma below is wrong, correct it!
// #pragma omp parallel for shared(to,from, fLongVector) private(i) 
	
	for(i = from[0].vcount; i < from[0].vcount+from[0].scount; i++)
	{
		muladd_fmatrix_fvector(to[from[i].c], from[i].m, fLongVector[from[i].r]);
		muladd_fmatrix_fvector(to[from[i].r], from[i].m, fLongVector[from[i].c]);
	}
}

/* SPARSE SYMMETRIC add big matrix with big matrix: A = B + C*/
DO_INLINE void add_bfmatrix_bfmatrix( fmatrix3x3 *to, fmatrix3x3 *from,  fmatrix3x3 *matrix)
{
	unsigned int i = 0;

	/* process diagonal elements */
	for(i = 0; i < matrix[0].vcount+matrix[0].scount; i++)
	{
		add_fmatrix_fmatrix(to[i].m, from[i].m, matrix[i].m);	
	}

}
/* SPARSE SYMMETRIC add big matrix with big matrix: A += B + C */
DO_INLINE void addadd_bfmatrix_bfmatrix( fmatrix3x3 *to, fmatrix3x3 *from,  fmatrix3x3 *matrix)
{
	unsigned int i = 0;

	/* process diagonal elements */
	for(i = 0; i < matrix[0].vcount+matrix[0].scount; i++)
	{
		addadd_fmatrix_fmatrix(to[i].m, from[i].m, matrix[i].m);	
	}

}
/* SPARSE SYMMETRIC subadd big matrix with big matrix: A -= B + C */
DO_INLINE void subadd_bfmatrix_bfmatrix( fmatrix3x3 *to, fmatrix3x3 *from,  fmatrix3x3 *matrix)
{
	unsigned int i = 0;

	/* process diagonal elements */
	for(i = 0; i < matrix[0].vcount+matrix[0].scount; i++)
	{
		subadd_fmatrix_fmatrix(to[i].m, from[i].m, matrix[i].m);	
	}

}
/*  A = B - C (SPARSE SYMMETRIC sub big matrix with big matrix) */
DO_INLINE void sub_bfmatrix_bfmatrix( fmatrix3x3 *to, fmatrix3x3 *from,  fmatrix3x3 *matrix)
{
	unsigned int i = 0;

	/* process diagonal elements */
	for(i = 0; i < matrix[0].vcount+matrix[0].scount; i++)
	{
		sub_fmatrix_fmatrix(to[i].m, from[i].m, matrix[i].m);	
	}

}
/* SPARSE SYMMETRIC sub big matrix with big matrix S (special constraint matrix with limited entries) */
DO_INLINE void sub_bfmatrix_Smatrix( fmatrix3x3 *to, fmatrix3x3 *from,  fmatrix3x3 *matrix)
{
	unsigned int i = 0;

	/* process diagonal elements */
	for(i = 0; i < matrix[0].vcount; i++)
	{
		sub_fmatrix_fmatrix(to[matrix[i].c].m, from[matrix[i].c].m, matrix[i].m);	
	}

}
/* A += B - C (SPARSE SYMMETRIC addsub big matrix with big matrix) */
DO_INLINE void addsub_bfmatrix_bfmatrix( fmatrix3x3 *to, fmatrix3x3 *from,  fmatrix3x3 *matrix)
{
	unsigned int i = 0;

	/* process diagonal elements */
	for(i = 0; i < matrix[0].vcount+matrix[0].scount; i++)
	{
		addsub_fmatrix_fmatrix(to[i].m, from[i].m, matrix[i].m);	
	}

}
/* SPARSE SYMMETRIC sub big matrix with big matrix*/
/* A -= B * float + C * float --> for big matrix */
/* VERIFIED */
DO_INLINE void subadd_bfmatrixS_bfmatrixS( fmatrix3x3 *to, fmatrix3x3 *from, float aS,  fmatrix3x3 *matrix, float bS)
{
	unsigned int i = 0;

	/* process diagonal elements */
	for(i = 0; i < matrix[0].vcount+matrix[0].scount; i++)
	{
		subadd_fmatrixS_fmatrixS(to[i].m, from[i].m, aS, matrix[i].m, bS);	
	}

}

///////////////////////////////////////////////////////////////////
// simulator start
///////////////////////////////////////////////////////////////////
static float I[3][4] = {{1,0,0,0},{0,1,0,0},{0,0,1,0}};
static float ZERO[3][4] = {{0,0,0,0},{0,0,0,0},{0,0,0,0}};
typedef struct Implicit_Data 
{
	lfVector *X, *V, *Xnew, *Vnew, *F, *B, *dV, *z;
	fmatrix3x3 *A, *dFdV, *dFdX, *S, *P, *Pinv, *bigI; 
} Implicit_Data;

int implicit_init (Object *ob, ClothModifierData *clmd)
{
	unsigned int i = 0;
	unsigned int pinned = 0;
	Cloth *cloth = NULL;
	ClothVertex *verts = NULL;
	ClothSpring *spring = NULL;
	Implicit_Data *id = NULL;
	LinkNode *search = NULL;

	// init memory guard
	// MEMORY_BASE.first = MEMORY_BASE.last = NULL;

	cloth = (Cloth *)clmd->clothObject;
	verts = cloth->verts;

	// create implicit base
	id = (Implicit_Data *)MEM_callocN (sizeof(Implicit_Data), "implicit vecmat");
	cloth->implicit = id;

	/* process diagonal elements */		
	id->A = create_bfmatrix(cloth->numverts, cloth->numsprings);
	id->dFdV = create_bfmatrix(cloth->numverts, cloth->numsprings);
	id->dFdX = create_bfmatrix(cloth->numverts, cloth->numsprings);
	id->S = create_bfmatrix(cloth->numverts, 0);
	id->Pinv = create_bfmatrix(cloth->numverts, cloth->numsprings);
	id->P = create_bfmatrix(cloth->numverts, cloth->numsprings);
	id->bigI = create_bfmatrix(cloth->numverts, cloth->numsprings); // TODO 0 springs
	id->X = create_lfvector(cloth->numverts);
	id->Xnew = create_lfvector(cloth->numverts);
	id->V = create_lfvector(cloth->numverts);
	id->Vnew = create_lfvector(cloth->numverts);
	id->F = create_lfvector(cloth->numverts);
	id->B = create_lfvector(cloth->numverts);
	id->dV = create_lfvector(cloth->numverts);
	zero_lfvector(id->dV, cloth->numverts);
	id->z = create_lfvector(cloth->numverts);
	
	for(i=0;i<cloth->numverts;i++) 
	{
		id->A[i].r = id->A[i].c = id->dFdV[i].r = id->dFdV[i].c = id->dFdX[i].r = id->dFdX[i].c = id->P[i].c = id->P[i].r = id->Pinv[i].c = id->Pinv[i].r = id->bigI[i].c = id->bigI[i].r = i;

		if(verts [i].goal >= SOFTGOALSNAP)
		{
			id->S[pinned].pinned = 1;
			id->S[pinned].c = id->S[pinned].r = i;
			pinned++;
		}
	}

	// S is special and needs specific vcount and scount
	id->S[0].vcount = pinned; id->S[0].scount = 0;

	// init springs */
	search = cloth->springs;
	for(i=0;i<cloth->numsprings;i++) 
	{
		spring = search->link;
		
		// dFdV_start[i].r = big_I[i].r = big_zero[i].r = 
		id->A[i+cloth->numverts].r = id->dFdV[i+cloth->numverts].r = id->dFdX[i+cloth->numverts].r = 
				id->P[i+cloth->numverts].r = id->Pinv[i+cloth->numverts].r = id->bigI[i+cloth->numverts].r = spring->ij;

		// dFdV_start[i].c = big_I[i].c = big_zero[i].c = 
		id->A[i+cloth->numverts].c = id->dFdV[i+cloth->numverts].c = id->dFdX[i+cloth->numverts].c = 
			id->P[i+cloth->numverts].c = id->Pinv[i+cloth->numverts].c = id->bigI[i+cloth->numverts].c = spring->kl;

		spring->matrix_index = i + cloth->numverts;
		
		search = search->next;
	}

	for(i = 0; i < cloth->numverts; i++)
	{		
		VECCOPY(id->X[i], cloth->x[i]);
	}

	return 1;
}
int	implicit_free (ClothModifierData *clmd)
{
	Implicit_Data *id;
	Cloth *cloth;
	cloth = (Cloth *)clmd->clothObject;

	if(cloth)
	{
		id = cloth->implicit;

		if(id)
		{
			del_bfmatrix(id->A);
			del_bfmatrix(id->dFdV);
			del_bfmatrix(id->dFdX);
			del_bfmatrix(id->S);
			del_bfmatrix(id->P);
			del_bfmatrix(id->Pinv);
			del_bfmatrix(id->bigI);

			del_lfvector(id->X);
			del_lfvector(id->Xnew);
			del_lfvector(id->V);
			del_lfvector(id->Vnew);
			del_lfvector(id->F);
			del_lfvector(id->B);
			del_lfvector(id->dV);
			del_lfvector(id->z);

			MEM_freeN(id);
		}
	}

	return 1;
}

void cloth_bending_mode(ClothModifierData *clmd, int enabled)
{
	Cloth *cloth = clmd->clothObject;
	Implicit_Data *id;
	
	if(cloth)
	{
		id = cloth->implicit;
		
		if(id)
		{
			if(enabled)
			{
				cloth->numsprings = cloth->numspringssave;
			}
			else
			{
				cloth->numsprings = cloth->numothersprings;
			}
			
			id->A[0].scount = id->dFdV[0].scount = id->dFdX[0].scount = id->P[0].scount = id->Pinv[0].scount = id->bigI[0].scount = cloth->numsprings;
		}	
	}
}

DO_INLINE float fb(float length, float L)
{
	float x = length/L;
	return (-11.541f*pow(x,4)+34.193f*pow(x,3)-39.083f*pow(x,2)+23.116f*x-9.713f);
}

DO_INLINE float fbderiv(float length, float L)
{
	float x = length/L;

	return (-46.164f*pow(x,3)+102.579f*pow(x,2)-78.166f*x+23.116f);
}

DO_INLINE float fbstar(float length, float L, float kb, float cb)
{
	float tempfb = kb * fb(length, L);

	float fbstar = cb * (length - L);

	if(tempfb < fbstar)
		return fbstar;
	else
		return tempfb;		
}

DO_INLINE float fbstar_jacobi(float length, float L, float kb, float cb)
{
	float tempfb = kb * fb(length, L);
	float fbstar = cb * (length - L);

	if(tempfb < fbstar)
	{		
		return cb;
	}
	else
	{
		return kb * fbderiv(length, L);	
	}	
}

DO_INLINE void filter(lfVector *V, fmatrix3x3 *S)
{
	unsigned int i=0;

	for(i=0;i<S[0].vcount;i++)
	{
		mul_fvector_fmatrix(V[S[i].r], V[S[i].r], S[i].m);
	}
}

int  cg_filtered(lfVector *ldV, fmatrix3x3 *lA, lfVector *lB, lfVector *z, fmatrix3x3 *S)
{
	// Solves for unknown X in equation AX=B
	unsigned int conjgrad_loopcount=0, conjgrad_looplimit=100;
	float conjgrad_epsilon=0.0001f, conjgrad_lasterror=0;
	lfVector *q, *d, *tmp, *r; 
	float s, starget, a, s_prev;
	unsigned int numverts = lA[0].vcount;
	q = create_lfvector(numverts);
	d = create_lfvector(numverts);
	tmp = create_lfvector(numverts);
	r = create_lfvector(numverts);
	
	// zero_lfvector(ldV, numverts);
	filter(ldV, S);
	add_lfvector_lfvector(ldV, ldV, z, numverts);

	// r = B - Mul(tmp,A,X);    // just use B if X known to be zero
	mul_bfmatrix_lfvector(r, lA, ldV);
	sub_lfvector_lfvector(r, lB, r, numverts);
	filter(r, S);

	cp_lfvector(d, r, numverts);

	s = dot_lfvector(r, r, numverts);
	starget = s * conjgrad_epsilon;
	
	// itstart();
	while((s>starget && conjgrad_loopcount < conjgrad_looplimit))
	{	
		// Mul(q,A,d); // q = A*d;
		mul_bfmatrix_lfvector(q, lA, d);

		filter(q,S);

		a = s/dot_lfvector(d, q, numverts);

		// X = X + d*a;
		add_lfvector_lfvectorS(ldV, ldV, d, a, numverts);

		// r = r - q*a;
		sub_lfvector_lfvectorS(r, r, q, a, numverts);

		s_prev = s;
		s = dot_lfvector(r, r, numverts);

		//d = r+d*(s/s_prev);
		add_lfvector_lfvectorS(d, r, d, (s/s_prev), numverts);

		filter(d,S);

		conjgrad_loopcount++;
	}
	// itend();
	// printf("cg_filtered time: %f\n", (float)itval());
	
	conjgrad_lasterror = s;

	del_lfvector(q);
	del_lfvector(d);
	del_lfvector(tmp);
	del_lfvector(r);
	
	// printf("W/O conjgrad_loopcount: %d\n", conjgrad_loopcount);

	return conjgrad_loopcount<conjgrad_looplimit;  // true means we reached desired accuracy in given time - ie stable
}

// block diagonalizer
DO_INLINE void BuildPPinv(fmatrix3x3 *lA, fmatrix3x3 *P, fmatrix3x3 *Pinv)
{
	unsigned int i = 0;
	
	// Take only the diagonal blocks of A
// #pragma omp parallel for private(i)
	for(i = 0; i<lA[0].vcount; i++)
	{
		// block diagonalizer
		cp_fmatrix(P[i].m, lA[i].m);
		inverse_fmatrix(Pinv[i].m, P[i].m);
		
	}
}

// version 1.3
int cg_filtered_pre(lfVector *dv, fmatrix3x3 *lA, lfVector *lB, lfVector *z, fmatrix3x3 *S, fmatrix3x3 *P, fmatrix3x3 *Pinv)
{
	unsigned int numverts = lA[0].vcount, iterations = 0, conjgrad_looplimit=100;
	float delta0 = 0, deltaNew = 0, deltaOld = 0, alpha = 0;
	float conjgrad_epsilon=0.0001; // 0.2 is dt for steps=5
	lfVector *r = create_lfvector(numverts);
	lfVector *p = create_lfvector(numverts);
	lfVector *s = create_lfvector(numverts);
	lfVector *h = create_lfvector(numverts);
	
	BuildPPinv(lA, P, Pinv);
	
	filter(dv, S);
	add_lfvector_lfvector(dv, dv, z, numverts);
	
	mul_bfmatrix_lfvector(r, lA, dv);
	sub_lfvector_lfvector(r, lB, r, numverts);
	filter(r, S);
	
	mul_bfmatrix_lfvector(p, Pinv, r);
	filter(p, S);
	
	deltaNew = delta0 = dot_lfvector(r, p, numverts);
	
	// itstart();
	
	while ((deltaNew > (conjgrad_epsilon*delta0)) && (iterations < conjgrad_looplimit))
	{
		iterations++;
		
		mul_bfmatrix_lfvector(s, lA, p);
		filter(s, S);
		
		alpha = deltaNew / dot_lfvector(p, s, numverts);
		
		add_lfvector_lfvectorS(dv, dv, p, alpha, numverts);
		
		sub_lfvector_lfvectorS(r, r, s, alpha, numverts);
		
		mul_bfmatrix_lfvector(h, Pinv, r);
		filter(h, S);
		
		deltaOld = deltaNew;
		
		deltaNew = dot_lfvector(r, h, numverts);
		
		add_lfvector_lfvectorS(p, h, p, deltaNew / deltaOld, numverts);
		filter(p, S);
		
	}
	
	// itend();
	// printf("cg_filtered_pre time: %f\n", (float)itval());
	
	del_lfvector(h);
	del_lfvector(s);
	del_lfvector(p);
	del_lfvector(r);
	
	// printf("iterations: %d\n", iterations);
	
	return iterations<conjgrad_looplimit;
}

// outer product is NOT cross product!!!
DO_INLINE void dfdx_spring_type1(float to[3][4], float dir[3],float length,float L,float k)
{
	// dir is unit length direction, rest is spring's restlength, k is spring constant.
	// return  (outerprod(dir,dir)*k + (I - outerprod(dir,dir))*(k - ((k*L)/length)));
	float temp[3][4];
	mul_fvectorT_fvector(temp, dir, dir);
	sub_fmatrix_fmatrix(to, I, temp);
	mul_fmatrix_S(to, k* (1.0f-(L/length)));
	mul_fmatrix_S(temp, k);
	add_fmatrix_fmatrix(to, temp, to);
}

DO_INLINE void dfdx_spring_type2(float to[3][4], float dir[3],float length,float L,float k, float cb)
{
	// return  outerprod(dir,dir)*fbstar_jacobi(length, L, k, cb);
	mul_fvectorT_fvectorS(to, dir, dir, fbstar_jacobi(length, L, k, cb));
}

DO_INLINE void dfdv_damp(float to[3][4], float dir[3], float damping)
{
	// derivative of force wrt velocity.  
	// return outerprod(dir,dir) * damping;
	mul_fvectorT_fvectorS(to, dir, dir, damping);
}

DO_INLINE void dfdx_spring(float to[3][4],  float dir[3],float length,float L,float k)
{
	// dir is unit length direction, rest is spring's restlength, k is spring constant.
	//return  ( (I-outerprod(dir,dir))*Min(1.0f,rest/length) - I) * -k;
	mul_fvectorT_fvector(to, dir, dir);
	sub_fmatrix_fmatrix(to, I, to);
	mul_fmatrix_S(to, (((L/length)> 1.0f) ? (1.0f): (L/length))); 
	sub_fmatrix_fmatrix(to, to, I);
	mul_fmatrix_S(to, -k);
}

DO_INLINE void dfdx_damp(float to[3][4],  float dir[3],float length,const float vel[3],float rest,float damping)
{
	// inner spring damping   vel is the relative velocity  of the endpoints.  
	// 	return (I-outerprod(dir,dir)) * (-damping * -(dot(dir,vel)/Max(length,rest)));
	mul_fvectorT_fvector(to, dir, dir);
	sub_fmatrix_fmatrix(to, I, to);
	mul_fmatrix_S(to,  (-damping * -(INPR(dir,vel)/MAX2(length,rest)))); 

}

DO_INLINE void cloth_calc_spring_force(ClothModifierData *clmd, ClothSpring *s, lfVector *lF, lfVector *X, lfVector *V, fmatrix3x3 *dFdV, fmatrix3x3 *dFdX, float dt)
{
	float extent[3];
	float length = 0;
	float dir[3] = {0,0,0};
	float vel[3] = {0,0,0};
	float k = 0.0f;
	float L = s->restlen;
	float cb = clmd->sim_parms->structural;

	float nullf[3] = {0,0,0};
	float stretch_force[3] = {0,0,0};
	float bending_force[3] = {0,0,0};
	float damping_force[3] = {0,0,0};
	float nulldfdx[3][4]={ {0,0,0,0}, {0,0,0,0}, {0,0,0,0}};
	
	VECCOPY(s->f, nullf);
	cp_fmatrix(s->dfdx, nulldfdx);
	cp_fmatrix(s->dfdv, nulldfdx);

	// calculate elonglation
	VECSUB(extent, X[s->kl], X[s->ij]);
	VECSUB(vel, V[s->kl], V[s->ij]);
	length = sqrt(INPR(extent, extent));
	
	s->flags &= ~CLOTH_SPRING_FLAG_NEEDED;
	
	if(length > ABS(ALMOST_ZERO))
	{
		/*
		if(length>L)
		{
			if((clmd->sim_parms->flags & CSIMSETT_FLAG_TEARING_ENABLED) 
			&& ((((length-L)*100.0f/L) > clmd->sim_parms->maxspringlen))) // cut spring!
			{
				s->flags |= CSPRING_FLAG_DEACTIVATE;
				return;
			}
		} 
		*/
		mul_fvector_S(dir, extent, 1.0f/length);
	}
	else	
	{
		mul_fvector_S(dir, extent, 0.0f);
	}
	
	
	// calculate force of structural + shear springs
	if(s->type != CLOTH_SPRING_TYPE_BENDING)
	{
		if(length > L) // only on elonglation
		{
			s->flags |= CLOTH_SPRING_FLAG_NEEDED;

			k = (clmd->sim_parms->structural*(length-L));	

			mul_fvector_S(stretch_force, dir, k); 

			VECADD(s->f, s->f, stretch_force);

			// Ascher & Boxman, p.21: Damping only during elonglation
			mul_fvector_S(damping_force, extent, clmd->sim_parms->Cdis * 0.01 * ((INPR(vel,extent)/length))); 
			VECADD(s->f, s->f, damping_force);
			
			// Formula from Ascher / Boxman, Speeding up cloth simulation
			// if((dt * (k*dt + 2 * clmd->sim_parms->Cdis * 0.01)) > 0.01 )
			{
				dfdx_spring_type1(s->dfdx, dir,length,L,clmd->sim_parms->structural);
				dfdv_damp(s->dfdv, dir,clmd->sim_parms->Cdis * 0.01);
			}
			// printf("(dt*k*dt) ): %f, k: %f\n", (dt * (k*dt + 2 * clmd->sim_parms->Cdis * 0.01) ), k);
		}
	}
	else // calculate force of bending springs
	{
		if(length < L)
		{
			// clmd->sim_parms->flags |= CLOTH_SIMSETTINGS_FLAG_BIG_FORCE;
			
			s->flags |= CLOTH_SPRING_FLAG_NEEDED;
			
			k = fbstar(length, L, clmd->sim_parms->bending, cb);	

			mul_fvector_S(bending_force, dir, k);
			VECADD(s->f, s->f, bending_force);
			
			// DG: My formula to handle bending for the AIMEX scheme 
			// multiply with 1000 because of numerical problems
			// if( ((k*1000)*dt*dt) < -0.18 )
			{
				dfdx_spring_type2(s->dfdx, dir,length,L,clmd->sim_parms->bending, cb);
				clmd->sim_parms->flags |= CLOTH_SIMSETTINGS_FLAG_BIG_FORCE;
			}
			// printf("(dt*k*dt) ): %f, k: %f\n", (dt*dt*k*-1.0), k);
		}
	}
}

DO_INLINE int cloth_apply_spring_force(ClothModifierData *clmd, ClothSpring *s, lfVector *lF, lfVector *X, lfVector *V, fmatrix3x3 *dFdV, fmatrix3x3 *dFdX)
{
	if(s->flags & CLOTH_SPRING_FLAG_NEEDED)
	{
		VECADD(lF[s->ij], lF[s->ij], s->f);
		VECSUB(lF[s->kl], lF[s->kl], s->f);	
			
		if(s->type != CLOTH_SPRING_TYPE_BENDING)
		{
			sub_fmatrix_fmatrix(dFdV[s->ij].m, dFdV[s->ij].m, s->dfdv);
			sub_fmatrix_fmatrix(dFdV[s->kl].m, dFdV[s->kl].m, s->dfdv);
			add_fmatrix_fmatrix(dFdV[s->matrix_index].m, dFdV[s->matrix_index].m, s->dfdv);	
		}
		else if(!(clmd->sim_parms->flags & CLOTH_SIMSETTINGS_FLAG_BIG_FORCE))
			return 0;
		
		sub_fmatrix_fmatrix(dFdX[s->ij].m, dFdX[s->ij].m, s->dfdx);
		sub_fmatrix_fmatrix(dFdX[s->kl].m, dFdX[s->kl].m, s->dfdx);

		add_fmatrix_fmatrix(dFdX[s->matrix_index].m, dFdX[s->matrix_index].m, s->dfdx);
	}
	
	return 1;
}

DO_INLINE void calculateTriangleNormal(float to[3], lfVector *X, MFace mface)
{
	float v1[3], v2[3];

	VECSUB(v1, X[mface.v2], X[mface.v1]);
	VECSUB(v2, X[mface.v3], X[mface.v1]);
	cross_fvector(to, v1, v2);
}

DO_INLINE void calculatQuadNormal(float to[3], lfVector *X, MFace mface)
{
	float temp = CalcNormFloat4(X[mface.v1],X[mface.v2],X[mface.v3],X[mface.v4],to);
	mul_fvector_S(to, to, temp);
}

void calculateWeightedVertexNormal(ClothModifierData *clmd, MFace *mfaces, float to[3], int index, lfVector *X)
{
	float temp[3]; 
	int i;
	Cloth *cloth = clmd->clothObject;

	for(i = 0; i < cloth->numfaces; i++)
	{
		// check if this triangle contains the selected vertex
		if(mfaces[i].v1 == index || mfaces[i].v2 == index || mfaces[i].v3 == index || mfaces[i].v4 == index)
		{
			calculatQuadNormal(temp, X, mfaces[i]);
			VECADD(to, to, temp);
		}
	}
}
float calculateVertexWindForce(float wind[3], float vertexnormal[3])  
{
	return fabs(INPR(wind, vertexnormal) * 0.5f);
}

DO_INLINE void calc_triangle_force(ClothModifierData *clmd, MFace mface, lfVector *F, lfVector *X, lfVector *V, fmatrix3x3 *dFdV, fmatrix3x3 *dFdX, ListBase *effectors)
{	

}

void cloth_calc_force(ClothModifierData *clmd, lfVector *lF, lfVector *lX, lfVector *lV, fmatrix3x3 *dFdV, fmatrix3x3 *dFdX, ListBase *effectors, float time, float dt)
{
	/* Collect forces and derivatives:  F,dFdX,dFdV */
	Cloth 		*cloth 		= clmd->clothObject;
	unsigned int 	i 		= 0;
	float 		spring_air 	= clmd->sim_parms->Cvi * 0.01f; /* viscosity of air scaled in percent */
	float 		gravity[3];
	float 		tm2[3][4] 	= {{-spring_air,0,0,0}, {0,-spring_air,0,0},{0,0,-spring_air,0}};
	ClothVertex *verts = cloth->verts;
	MFace 		*mfaces 	= cloth->mfaces;
	float wind_normalized[3];
	unsigned int numverts = cloth->numverts;
	float auxvect[3], velgoal[3], tvect[3];
	float kd, ks;
	LinkNode *search = cloth->springs;

	VECCOPY(gravity, clmd->sim_parms->gravity);
	mul_fvector_S(gravity, gravity, 0.001f); /* scale gravity force */

	/* set dFdX jacobi matrix to zero */
	init_bfmatrix(dFdX, ZERO);
	/* set dFdX jacobi matrix diagonal entries to -spring_air */ 
	initdiag_bfmatrix(dFdV, tm2);

	init_lfvector(lF, gravity, numverts);

	submul_lfvectorS(lF, lV, spring_air, numverts);

	/* do goal stuff */
	if(clmd->sim_parms->flags & CLOTH_SIMSETTINGS_FLAG_GOAL) 
	{	
		for(i = 0; i < numverts; i++)
		{			
			if(verts [i].goal < SOFTGOALSNAP)
			{			
				// current_position = xold + t * (newposition - xold)
				VECSUB(tvect, cloth->xconst[i], cloth->xold[i]);
				mul_fvector_S(tvect, tvect, time);
				VECADD(tvect, tvect, cloth->xold[i]);

				VECSUB(auxvect, tvect, lX[i]);
				ks  = 1.0f/(1.0f- verts [i].goal*clmd->sim_parms->goalspring)-1.0f ;
				VECADDS(lF[i], lF[i], auxvect, -ks);

				// calulate damping forces generated by goals				
				VECSUB(velgoal, cloth->xold[i], cloth->xconst[i]);
				kd =  clmd->sim_parms->goalfrict * 0.01f; // friction force scale taken from SB
				VECSUBADDSS(lF[i], velgoal, kd, lV[i], kd);
				
			}
		}	
	}

	/* handle external forces like wind */
	if(effectors)
	{
		float speed[3] = {0.0f, 0.0f,0.0f};
		float force[3]= {0.0f, 0.0f, 0.0f};
		
		#pragma omp parallel for private (i) shared(lF)
		for(i = 0; i < cloth->numverts; i++)
		{
			float vertexnormal[3]={0,0,0};
			float fieldfactor = 1000.0f, windfactor  = 250.0f; // from sb
			
			pdDoEffectors(effectors, lX[i], force, speed, (float)G.scene->r.cfra, 0.0f, PE_WIND_AS_SPEED);		
			
			// TODO apply forcefields here
			VECADDS(lF[i], lF[i], force, fieldfactor*0.01f);

			VECCOPY(wind_normalized, speed);
			Normalize(wind_normalized);
			
			calculateWeightedVertexNormal(clmd, mfaces, vertexnormal, i, lX);
			VECADDS(lF[i], lF[i], wind_normalized, -calculateVertexWindForce(speed, vertexnormal));
		}
	}
	
	// calculate spring forces
	search = cloth->springs;
	while(search)
	{
		// only handle active springs
		// if(((clmd->sim_parms->flags & CSIMSETT_FLAG_TEARING_ENABLED) && !(springs[i].flags & CSPRING_FLAG_DEACTIVATE))|| !(clmd->sim_parms->flags & CSIMSETT_FLAG_TEARING_ENABLED)){}
		cloth_calc_spring_force(clmd, search->link, lF, lX, lV, dFdV, dFdX, dt);

		search = search->next;
	}
	
	if(clmd->sim_parms->flags & CLOTH_SIMSETTINGS_FLAG_BIG_FORCE)
	{	
		if(cloth->numspringssave != cloth->numsprings)
		{
			cloth_bending_mode(clmd, 1);
		}
	}
	else
	{
		if(cloth->numspringssave == cloth->numsprings)
		{
			cloth_bending_mode(clmd, 0);
		}
	}
	
	// apply spring forces
	search = cloth->springs;
	while(search)
	{
		// only handle active springs
		// if(((clmd->sim_parms->flags & CSIMSETT_FLAG_TEARING_ENABLED) && !(springs[i].flags & CSPRING_FLAG_DEACTIVATE))|| !(clmd->sim_parms->flags & CSIMSETT_FLAG_TEARING_ENABLED))	
		if(!cloth_apply_spring_force(clmd, search->link, lF, lX, lV, dFdV, dFdX))
			break;
		search = search->next;
	}
	
	clmd->sim_parms->flags &= ~CLOTH_SIMSETTINGS_FLAG_BIG_FORCE;
	
}


void simulate_implicit_euler(lfVector *Vnew, lfVector *lX, lfVector *lV, lfVector *lF, fmatrix3x3 *dFdV, fmatrix3x3 *dFdX, float dt, fmatrix3x3 *A, lfVector *B, lfVector *dV, fmatrix3x3 *S, lfVector *z, fmatrix3x3 *P, fmatrix3x3 *Pinv)
{
	unsigned int numverts = dFdV[0].vcount;

	lfVector *dFdXmV = create_lfvector(numverts);
	
	initdiag_bfmatrix(A, I);
	
	subadd_bfmatrixS_bfmatrixS(A, dFdV, dt, dFdX, (dt*dt));

	mul_bfmatrix_lfvector(dFdXmV, dFdX, lV);

	add_lfvectorS_lfvectorS(B, lF, dt, dFdXmV, (dt*dt), numverts);
	
	// cg_filtered(dV, A, B, z, S); // conjugate gradient algorithm to solve Ax=b 
	cg_filtered_pre(dV, A, B, z, S, P, Pinv);
	
	// advance velocities
	add_lfvector_lfvector(Vnew, lV, dV, numverts);
	
	del_lfvector(dFdXmV);
}

/*
// this version solves for the new velocity
void simulate_implicit_euler(lfVector *Vnew, lfVector *lX, lfVector *lV, lfVector *lF, fmatrix3x3 *dFdV, fmatrix3x3 *dFdX, float dt, fmatrix3x3 *A, lfVector *B, lfVector *dV, fmatrix3x3 *S, lfVector *z, fmatrix3x3 *P, fmatrix3x3 *Pinv)
{
	unsigned int numverts = dFdV[0].vcount;

	lfVector *dFdXmV = create_lfvector(numverts);
	
	initdiag_bfmatrix(A, I);
	
	subadd_bfmatrixS_bfmatrixS(A, dFdV, dt, dFdX, (dt*dt));

	mul_bfmatrix_lfvector(dFdXmV, dFdV, lV);

	add_lfvectorS_lfvectorS(B, lF, dt, dFdXmV, -dt, numverts);
	add_lfvector_lfvector(B, B, lV, numverts);

	cg_filtered_pre(Vnew, A, B, z, S, P, Pinv);
	
	del_lfvector(dFdXmV);
}
*/
int implicit_solver (Object *ob, float frame, ClothModifierData *clmd, ListBase *effectors)
{ 	 	
	unsigned int i=0;
	float step=0.0f, tf=1.0f;
	Cloth *cloth = clmd->clothObject;
	ClothVertex *verts = cloth->verts;
	unsigned int numverts = cloth->numverts;
	float dt = 1.0f / clmd->sim_parms->stepsPerFrame;
	Implicit_Data *id = cloth->implicit;
	int result = 0;
	float force = 0, lastforce = 0;
	lfVector *dx;
	
	if(clmd->sim_parms->flags & CLOTH_SIMSETTINGS_FLAG_GOAL) /* do goal stuff */
	{
		for(i = 0; i < numverts; i++)
		{			
			// update velocities with constrained velocities from pinned verts
			if(verts [i].goal >= SOFTGOALSNAP)
			{			
				VECSUB(id->V[i], cloth->xconst[i], cloth->xold[i]);
				// VecMulf(id->V[i], 1.0 / dt);
			}
		}	
	}

	while(step < tf)
	{ 		
		effectors= pdInitEffectors(ob,NULL);
		
		// calculate 
		cloth_calc_force(clmd, id->F, id->X, id->V, id->dFdV, id->dFdX, effectors, step, dt );
		
		// check for sleeping
		// if(!(clmd->coll_parms->flags & CLOTH_SIMSETTINGS_FLAG_SLEEP))
		{
			simulate_implicit_euler(id->Vnew, id->X, id->V, id->F, id->dFdV, id->dFdX, dt, id->A, id->B, id->dV, id->S, id->z, id->P, id->Pinv);
		
			add_lfvector_lfvectorS(id->Xnew, id->X, id->Vnew, dt, numverts);
		}
		
		dx = create_lfvector(numverts);
		sub_lfvector_lfvector(dx, id->Xnew, id->X, numverts);
		force = dot_lfvector(dx, dx, numverts);
		del_lfvector(dx);
		
		/*
		if((force < 0.00001) && (lastforce >= force))
			clmd->coll_parms->flags |= CLOTH_SIMSETTINGS_FLAG_SLEEP;
		else if((lastforce*2 < force))
		*/
			clmd->coll_parms->flags &= ~CLOTH_SIMSETTINGS_FLAG_SLEEP;
		
		lastforce = force;
		
		if(clmd->coll_parms->flags & CLOTH_COLLISIONSETTINGS_FLAG_ENABLED)
		{
			// collisions 
			// itstart();
			
			// update verts to current positions
			for(i = 0; i < numverts; i++)
			{		
				if(clmd->sim_parms->flags & CLOTH_SIMSETTINGS_FLAG_GOAL) /* do goal stuff */
				{			
					if(verts [i].goal >= SOFTGOALSNAP)
					{			
						float tvect[3] = {.0,.0,.0};
						// VECSUB(tvect, id->Xnew[i], verts[i].xold);
						mul_fvector_S(tvect, id->V[i], step+dt);
						VECADD(tvect, tvect, cloth->xold[i]);
						VECCOPY(id->Xnew[i], tvect);
					}
						
				}
				
				VECCOPY(cloth->current_x[i], id->Xnew[i]);
				VECSUB(cloth->current_v[i], cloth->current_x[i], cloth->current_xold[i]);
				VECCOPY(cloth->v[i], cloth->current_v[i]);
			}
			
			// call collision function
			result = cloth_bvh_objcollision(clmd, step + dt, step, dt);
			
			// copy corrected positions back to simulation
			if(result)
			{
				memcpy(cloth->current_xold, cloth->current_x, sizeof(lfVector) * numverts);
				memcpy(id->Xnew, cloth->current_x, sizeof(lfVector) * numverts);
				
				for(i = 0; i < numverts; i++)
				{	
					VECCOPY(id->Vnew[i], cloth->current_v[i]);
					VecMulf(id->Vnew[i], 1.0f / dt);
				}
			}
			else
			{
				memcpy(cloth->current_xold, id->Xnew, sizeof(lfVector) * numverts);
			}
			
			// X = Xnew;
			cp_lfvector(id->X, id->Xnew, numverts);
			
			// if there were collisions, advance the velocity from v_n+1/2 to v_n+1
			if(result)
			{
				// V = Vnew;
				cp_lfvector(id->V, id->Vnew, numverts);
				
				// calculate 
				cloth_calc_force(clmd, id->F, id->X, id->V, id->dFdV, id->dFdX, effectors, step, dt);	
				simulate_implicit_euler(id->Vnew, id->X, id->V, id->F, id->dFdV, id->dFdX, dt / 2.0f, id->A, id->B, id->dV, id->S, id->z, id->P, id->Pinv);
			}
			
		}
		else
		{
			// X = Xnew;
			cp_lfvector(id->X, id->Xnew, numverts);
		}
		
		// itend();
		// printf("collision time: %f\n", (float)itval());
		
		// V = Vnew;
		cp_lfvector(id->V, id->Vnew, numverts);
		
		step += dt;

		if(effectors) pdEndEffectors(effectors);
	}
	
	if(clmd->sim_parms->flags & CLOTH_SIMSETTINGS_FLAG_GOAL)
	{
		for(i = 0; i < numverts; i++)
		{
			if(verts [i].goal < SOFTGOALSNAP)
			{
				VECCOPY(cloth->current_xold[i], id->X[i]);
				VECCOPY(cloth->x[i], id->X[i]);
			}
			else
			{
				VECCOPY(cloth->current_xold[i], cloth->xconst[i]);
				VECCOPY(cloth->x[i], cloth->xconst[i]);
			}
		}
	}
	else
	{
		for(i = 0; i < numverts; i++)
		{
			VECCOPY(cloth->current_xold[i], id->X[i]);
			VECCOPY(cloth->x[i], id->X[i]);
		}
		// memcpy(cloth->current_xold, id->X, sizeof(lfVector) * numverts);
		// memcpy(cloth->x, id->X, sizeof(lfVector) * numverts);
	}
	
	for(i = 0; i < numverts; i++)
		VECCOPY(cloth->v[i], id->V[i]);
	
	// memcpy(cloth->v, id->V, sizeof(lfVector) * numverts);
	
	return 1;
}

void implicit_set_positions (ClothModifierData *clmd)
{ 	 	
	Cloth *cloth = clmd->clothObject;
	unsigned int numverts = cloth->numverts, i = 0;
	Implicit_Data *id = cloth->implicit;
	
	
	for(i = 0; i < numverts; i++)
	{
		VECCOPY(id->X[i], cloth->x[i]);
		VECCOPY(id->V[i], cloth->v[i]);
	}
	
	// memcpy(id->X, cloth->x, sizeof(lfVector) * numverts);	
	// memcpy(id->V, cloth->v, sizeof(lfVector) * numverts);	
}


int collisions_collision_response_static(ClothModifierData *clmd, ClothModifierData *coll_clmd)
{
	/*
	unsigned int i = 0;
	int result = 0;
	LinkNode *search = NULL;
	CollPair *collpair = NULL;
	Cloth *cloth1, *cloth2;
	float w1, w2, w3, u1, u2, u3;
	float v1[3], v2[3], relativeVelocity[3];
	float magrelVel;
	
	cloth1 = clmd->clothObject;
	cloth2 = coll_clmd->clothObject;

	// search = clmd->coll_parms->collision_list;
	
	while(search)
	{
	collpair = search->link;
		
		// compute barycentric coordinates for both collision points
	collisions_compute_barycentric(collpair->pa,
	cloth1->verts[collpair->ap1].txold,
	cloth1->verts[collpair->ap2].txold,
	cloth1->verts[collpair->ap3].txold, 
	&w1, &w2, &w3);
	
	collisions_compute_barycentric(collpair->pb,
	cloth2->verts[collpair->bp1].txold,
	cloth2->verts[collpair->bp2].txold,
	cloth2->verts[collpair->bp3].txold,
	&u1, &u2, &u3);
	
		// Calculate relative "velocity".
	interpolateOnTriangle(v1, cloth1->verts[collpair->ap1].tv, cloth1->verts[collpair->ap2].tv, cloth1->verts[collpair->ap3].tv, w1, w2, w3);
		
	interpolateOnTriangle(v2, cloth2->verts[collpair->bp1].tv, cloth2->verts[collpair->bp2].tv, cloth2->verts[collpair->bp3].tv, u1, u2, u3);
		
	VECSUB(relativeVelocity, v1, v2);
			
		// Calculate the normal component of the relative velocity (actually only the magnitude - the direction is stored in 'normal').
	magrelVel = INPR(relativeVelocity, collpair->normal);
		
		// printf("magrelVel: %f\n", magrelVel);
				
		// Calculate masses of points.
		
		// If v_n_mag < 0 the edges are approaching each other.
	if(magrelVel < -ALMOST_ZERO) 
	{
			// Calculate Impulse magnitude to stop all motion in normal direction.
			// const double I_mag = v_n_mag / (1/m1 + 1/m2);
	float magnitude_i = magrelVel / 2.0f; // TODO implement masses
	float tangential[3], magtangent, magnormal, collvel[3];
	float vrel_t_pre[3];
	float vrel_t[3];
	double impulse;
	float epsilon = clmd->coll_parms->epsilon;
	float overlap = (epsilon + ALMOST_ZERO-collpair->distance);
			
			// calculateFrictionImpulse(tangential, relativeVelocity, collpair->normal, magrelVel, clmd->coll_parms->friction*0.01, magrelVel);
			
			// magtangent = INPR(tangential, tangential);
			
			// Apply friction impulse.
	if (magtangent < -ALMOST_ZERO) 
	{
				
				// printf("friction applied: %f\n", magtangent);
				// TODO check original code 
}
			

	impulse = -2.0f * magrelVel / ( 1.0 + w1*w1 + w2*w2 + w3*w3);
			
			// printf("impulse: %f\n", impulse);
			
			// face A
	VECADDMUL(cloth1->verts[collpair->ap1].impulse, collpair->normal, w1 * impulse); 
	cloth1->verts[collpair->ap1].impulse_count++;
			
	VECADDMUL(cloth1->verts[collpair->ap2].impulse, collpair->normal, w2 * impulse); 
	cloth1->verts[collpair->ap2].impulse_count++;
			
	VECADDMUL(cloth1->verts[collpair->ap3].impulse, collpair->normal, w3 * impulse); 
	cloth1->verts[collpair->ap3].impulse_count++;
			
			// face B
	VECADDMUL(cloth2->verts[collpair->bp1].impulse, collpair->normal, u1 * impulse); 
	cloth2->verts[collpair->bp1].impulse_count++;
			
	VECADDMUL(cloth2->verts[collpair->bp2].impulse, collpair->normal, u2 * impulse); 
	cloth2->verts[collpair->bp2].impulse_count++;
			
	VECADDMUL(cloth2->verts[collpair->bp3].impulse, collpair->normal, u3 * impulse); 
	cloth2->verts[collpair->bp3].impulse_count++;
			
			
	result = 1;	
		
			// printf("magnitude_i: %f\n", magnitude_i); // negative before collision in my case
			
			// Apply the impulse and increase impulse counters.
	
			
}
		
	search = search->next;
}
	
	
	return result;
	*/
	return 0;
}


int collisions_collision_response_moving_tris(ClothModifierData *clmd, ClothModifierData *coll_clmd)
{
	return 0;
}


int collisions_collision_response_moving_edges(ClothModifierData *clmd, ClothModifierData *coll_clmd)
{
	return 0;
}

void cloth_collision_static(ClothModifierData *clmd, LinkNode *collision_list)
{
	/*
	CollPair *collpair = NULL;
	Cloth *cloth1=NULL, *cloth2=NULL;
	MFace *face1=NULL, *face2=NULL;
	ClothVertex *verts1=NULL, *verts2=NULL;
	double distance = 0;
	float epsilon = clmd->coll_parms->epsilon;
	unsigned int i = 0;

	for(i = 0; i < 4; i++)
	{
	collpair = (CollPair *)MEM_callocN(sizeof(CollPair), "cloth coll pair");	
		
	cloth1 = clmd->clothObject;
	cloth2 = coll_clmd->clothObject;
		
	verts1 = cloth1->verts;
	verts2 = cloth2->verts;
	
	face1 = &(cloth1->mfaces[tree1->tri_index]);
	face2 = &(cloth2->mfaces[tree2->tri_index]);
		
		// check all possible pairs of triangles
	if(i == 0)
	{
	collpair->ap1 = face1->v1;
	collpair->ap2 = face1->v2;
	collpair->ap3 = face1->v3;
			
	collpair->bp1 = face2->v1;
	collpair->bp2 = face2->v2;
	collpair->bp3 = face2->v3;
			
}
		
	if(i == 1)
	{
	if(face1->v4)
	{
	collpair->ap1 = face1->v3;
	collpair->ap2 = face1->v4;
	collpair->ap3 = face1->v1;
				
	collpair->bp1 = face2->v1;
	collpair->bp2 = face2->v2;
	collpair->bp3 = face2->v3;
}
	else
	i++;
}
		
	if(i == 2)
	{
	if(face2->v4)
	{
	collpair->ap1 = face1->v1;
	collpair->ap2 = face1->v2;
	collpair->ap3 = face1->v3;
				
	collpair->bp1 = face2->v3;
	collpair->bp2 = face2->v4;
	collpair->bp3 = face2->v1;
}
	else
	i+=2;
}
		
	if(i == 3)
	{
	if((face1->v4)&&(face2->v4))
	{
	collpair->ap1 = face1->v3;
	collpair->ap2 = face1->v4;
	collpair->ap3 = face1->v1;
				
	collpair->bp1 = face2->v3;
	collpair->bp2 = face2->v4;
	collpair->bp3 = face2->v1;
}
	else
	i++;
}
	
		
	if(i < 4)
	{
			// calc distance + normal 	
	distance = plNearestPoints(
	verts1[collpair->ap1].txold, verts1[collpair->ap2].txold, verts1[collpair->ap3].txold, verts2[collpair->bp1].txold, verts2[collpair->bp2].txold, verts2[collpair->bp3].txold, collpair->pa,collpair->pb,collpair->vector);
			
	if (distance <= (epsilon + ALMOST_ZERO))
	{
				// printf("dist: %f\n", (float)distance);
				
				// collpair->face1 = tree1->tri_index;
				// collpair->face2 = tree2->tri_index;
				
				// VECCOPY(collpair->normal, collpair->vector);
				// Normalize(collpair->normal);
				
				// collpair->distance = distance;
				
}
	else
	{
	MEM_freeN(collpair);
}
}
	else
	{
	MEM_freeN(collpair);
}
}
	*/
}

int collisions_are_edges_adjacent(ClothModifierData *clmd, ClothModifierData *coll_clmd, EdgeCollPair *edgecollpair)
{
	Cloth *cloth1, *cloth2;
	ClothVertex *verts1, *verts2;
	float temp[3];
	 /*
	cloth1 = clmd->clothObject;
	cloth2 = coll_clmd->clothObject;
	
	verts1 = cloth1->verts;
	verts2 = cloth2->verts;
	
	VECSUB(temp, verts1[edgecollpair->p11].xold, verts2[edgecollpair->p21].xold);
	if(ABS(INPR(temp, temp)) < ALMOST_ZERO)
		return 1;
	
	VECSUB(temp, verts1[edgecollpair->p11].xold, verts2[edgecollpair->p22].xold);
	if(ABS(INPR(temp, temp)) < ALMOST_ZERO)
		return 1;
	
	VECSUB(temp, verts1[edgecollpair->p12].xold, verts2[edgecollpair->p21].xold);
	if(ABS(INPR(temp, temp)) < ALMOST_ZERO)
		return 1;
	
	VECSUB(temp, verts1[edgecollpair->p12].xold, verts2[edgecollpair->p22].xold);
	if(ABS(INPR(temp, temp)) < ALMOST_ZERO)
		return 1;
		*/
	return 0;
}


void collisions_collision_moving_edges(ClothModifierData *clmd, ClothModifierData *coll_clmd, CollisionTree *tree1, CollisionTree *tree2)
{
	/*
	EdgeCollPair edgecollpair;
	Cloth *cloth1=NULL, *cloth2=NULL;
	MFace *face1=NULL, *face2=NULL;
	ClothVertex *verts1=NULL, *verts2=NULL;
	double distance = 0;
	float epsilon = clmd->coll_parms->epsilon;
	unsigned int i = 0, j = 0, k = 0;
	int numsolutions = 0;
	float a[3], b[3], c[3], d[3], e[3], f[3], solution[3];
	
	cloth1 = clmd->clothObject;
	cloth2 = coll_clmd->clothObject;
	
	verts1 = cloth1->verts;
	verts2 = cloth2->verts;

	face1 = &(cloth1->mfaces[tree1->tri_index]);
	face2 = &(cloth2->mfaces[tree2->tri_index]);
	
	for( i = 0; i < 5; i++)
	{
	if(i == 0) 
	{
	edgecollpair.p11 = face1->v1;
	edgecollpair.p12 = face1->v2;
}
	else if(i == 1) 
	{
	edgecollpair.p11 = face1->v2;
	edgecollpair.p12 = face1->v3;
}
	else if(i == 2) 
	{
	if(face1->v4) 
	{
	edgecollpair.p11 = face1->v3;
	edgecollpair.p12 = face1->v4;
}
	else 
	{
	edgecollpair.p11 = face1->v3;
	edgecollpair.p12 = face1->v1;
	i+=5; // get out of here after this edge pair is handled
}
}
	else if(i == 3) 
	{
	if(face1->v4) 
	{
	edgecollpair.p11 = face1->v4;
	edgecollpair.p12 = face1->v1;
}	
	else
	continue;
}
	else
	{
	edgecollpair.p11 = face1->v3;
	edgecollpair.p12 = face1->v1;
}

		
	for( j = 0; j < 5; j++)
	{
	if(j == 0)
	{
	edgecollpair.p21 = face2->v1;
	edgecollpair.p22 = face2->v2;
}
	else if(j == 1)
	{
	edgecollpair.p21 = face2->v2;
	edgecollpair.p22 = face2->v3;
}
	else if(j == 2)
	{
	if(face2->v4) 
	{
	edgecollpair.p21 = face2->v3;
	edgecollpair.p22 = face2->v4;
}
	else 
	{
	edgecollpair.p21 = face2->v3;
	edgecollpair.p22 = face2->v1;
}
}
	else if(j == 3)
	{
	if(face2->v4) 
	{
	edgecollpair.p21 = face2->v4;
	edgecollpair.p22 = face2->v1;
}
	else
	continue;
}
	else
	{
	edgecollpair.p21 = face2->v3;
	edgecollpair.p22 = face2->v1;
}
			
			
	if(!collisions_are_edges_adjacent(clmd, coll_clmd, &edgecollpair))
	{
	VECSUB(a, verts1[edgecollpair.p12].xold, verts1[edgecollpair.p11].xold);
	VECSUB(b, verts1[edgecollpair.p12].v, verts1[edgecollpair.p11].v);
	VECSUB(c, verts1[edgecollpair.p21].xold, verts1[edgecollpair.p11].xold);
	VECSUB(d, verts1[edgecollpair.p21].v, verts1[edgecollpair.p11].v);
	VECSUB(e, verts2[edgecollpair.p22].xold, verts1[edgecollpair.p11].xold);
	VECSUB(f, verts2[edgecollpair.p22].v, verts1[edgecollpair.p11].v);
				
	numsolutions = collisions_get_collision_time(a, b, c, d, e, f, solution);
				
	for (k = 0; k < numsolutions; k++) 
	{								
	if ((solution[k] >= 0.0) && (solution[k] <= 1.0)) 
	{
	float out_collisionTime = solution[k];
						
						// TODO: check for collisions 
						
						// TODO: put into (edge) collision list
						
	printf("Moving edge found!\n");
}
}
}
}
}	
	*/	
}

void collisions_collision_moving_tris(ClothModifierData *clmd, ClothModifierData *coll_clmd, CollisionTree *tree1, CollisionTree *tree2)
{
	/*
	CollPair collpair;
	Cloth *cloth1=NULL, *cloth2=NULL;
	MFace *face1=NULL, *face2=NULL;
	ClothVertex *verts1=NULL, *verts2=NULL;
	double distance = 0;
	float epsilon = clmd->coll_parms->epsilon;
	unsigned int i = 0, j = 0, k = 0;
	int numsolutions = 0;
	float a[3], b[3], c[3], d[3], e[3], f[3], solution[3];

	for(i = 0; i < 2; i++)
	{		
	cloth1 = clmd->clothObject;
	cloth2 = coll_clmd->clothObject;
		
	verts1 = cloth1->verts;
	verts2 = cloth2->verts;
	
	face1 = &(cloth1->mfaces[tree1->tri_index]);
	face2 = &(cloth2->mfaces[tree2->tri_index]);
		
		// check all possible pairs of triangles
	if(i == 0)
	{
	collpair.ap1 = face1->v1;
	collpair.ap2 = face1->v2;
	collpair.ap3 = face1->v3;
			
	collpair.pointsb[0] = face2->v1;
	collpair.pointsb[1] = face2->v2;
	collpair.pointsb[2] = face2->v3;
	collpair.pointsb[3] = face2->v4;
}
		
	if(i == 1)
	{
	if(face1->v4)
	{
	collpair.ap1 = face1->v3;
	collpair.ap2 = face1->v4;
	collpair.ap3 = face1->v1;
				
	collpair.pointsb[0] = face2->v1;
	collpair.pointsb[1] = face2->v2;
	collpair.pointsb[2] = face2->v3;
	collpair.pointsb[3] = face2->v4;
}
	else
	i++;
}
		
		// calc SIPcode (?)
		
	if(i < 2)
	{
	VECSUB(a, verts1[collpair.ap2].xold, verts1[collpair.ap1].xold);
	VECSUB(b, verts1[collpair.ap2].v, verts1[collpair.ap1].v);
	VECSUB(c, verts1[collpair.ap3].xold, verts1[collpair.ap1].xold);
	VECSUB(d, verts1[collpair.ap3].v, verts1[collpair.ap1].v);
				
	for(j = 0; j < 4; j++)
	{					
	if((j==3) && !(face2->v4))
	break;
				
	VECSUB(e, verts2[collpair.pointsb[j]].xold, verts1[collpair.ap1].xold);
	VECSUB(f, verts2[collpair.pointsb[j]].v, verts1[collpair.ap1].v);
				
	numsolutions = collisions_get_collision_time(a, b, c, d, e, f, solution);
				
	for (k = 0; k < numsolutions; k++) 
	{								
	if ((solution[k] >= 0.0) && (solution[k] <= 1.0)) 
	{
	float out_collisionTime = solution[k];
						
						// TODO: check for collisions 
						
						// TODO: put into (point-face) collision list
						
	printf("Moving found!\n");
						
}
}
				
				// TODO: check borders for collisions
}
			
}
}
	*/
}

void collisions_collision_moving(ClothModifierData *clmd, ClothModifierData *coll_clmd, CollisionTree *tree1, CollisionTree *tree2)
{
	/*
	// TODO: check for adjacent
	collisions_collision_moving_edges(clmd, coll_clmd, tree1, tree2);
	
	collisions_collision_moving_tris(clmd, coll_clmd, tree1, tree2);
	collisions_collision_moving_tris(coll_clmd, clmd, tree2, tree1);
	*/
}

// cloth - object collisions
int cloth_bvh_objcollision(ClothModifierData * clmd, float step, float prevstep, float dt)
{
	
	Base *base = NULL;
	CollisionModifierData *collmd = NULL;
	Cloth *cloth = NULL;
	Object *ob2 = NULL;
	BVH *bvh1 = NULL, *bvh2 = NULL, *self_bvh;
	LinkNode *collision_list = NULL; 
	unsigned int i = 0, j = 0;
	int collisions = 0, count = 0;
	float (*current_x)[4];

	if (!(((Cloth *)clmd->clothObject)->tree))
	{
		printf("No BVH found\n");
		return 0;
	}
	
	cloth = clmd->clothObject;
	bvh1 = cloth->tree;
	self_bvh = cloth->selftree;
	
	////////////////////////////////////////////////////////////
	// static collisions
	////////////////////////////////////////////////////////////
	
	// update cloth bvh
	bvh_update_from_float4(bvh1, cloth->current_xold, cloth->numverts, cloth->current_x, 0); // 0 means STATIC, 1 means MOVING (see later in this function)
/*
	// check all collision objects
	for (base = G.scene->base.first; base; base = base->next)
	{
		ob2 = base->object;
		collmd = (CollisionModifierData *) modifiers_findByType (ob2, eModifierType_Collision);
		
		if (!collmd)
			continue;
		
		// check if there is a bounding volume hierarchy
		if (collmd->tree) 
		{			
			bvh2 = collmd->tree;
			
			// update position + bvh of collision object
			collision_move_object(collmd, step, prevstep);
			bvh_update_from_mvert(collmd->tree, collmd->current_x, collmd->numverts, NULL, 0);
			
			// fill collision list 
			collisions += bvh_traverse(bvh1->root, bvh2->root, &collision_list);
			
			// call static collision response
			
			// free collision list
			if(collision_list)
			{
				LinkNode *search = collision_list;
				
				while(search)
				{
					CollisionPair *coll_pair = search->link;
					
					MEM_freeN(coll_pair);
					search = search->next;
				}
				BLI_linklist_free(collision_list,NULL);
	
				collision_list = NULL;
			}
		}
	}
	
	//////////////////////////////////////////////
	// update velocities + positions
	//////////////////////////////////////////////
	for(i = 0; i < cloth->numverts; i++)
	{
		VECADD(cloth->current_x[i], cloth->current_xold[i], cloth->current_v[i]);
	}
	//////////////////////////////////////////////
*/	
	/*
	// fill collision list 
	collisions += bvh_traverse(self_bvh->root, self_bvh->root, &collision_list);
	
	// call static collision response
	
	// free collision list
	if(collision_list)
	{
		LinkNode *search = collision_list;
		
		while(search)
		{
			float distance = 0;
			float mindistance = cloth->selftree->epsilon;
			CollisionPair *collpair = (CollisionPair *)search->link;
			
			// get distance of faces
			distance = plNearestPoints(
					cloth->current_x[collpair->point_indexA[0]], cloth->current_x[collpair->point_indexA[1]], cloth->current_x[collpair->point_indexA[2]], cloth->current_x[collpair->point_indexB[0]], cloth->current_x[collpair->point_indexB[1]], cloth->current_x[collpair->point_indexB[2]], collpair->pa,collpair->pb,collpair->vector);
					
			if(distance < mindistance)
			{
				///////////////////////////////////////////
				// TODO: take velocity of the collision points into account!
				///////////////////////////////////////////
				
				float correction = mindistance - distance;
				float temp[3];
				
				VECCOPY(temp, collpair->vector);
				Normalize(temp);
				VecMulf(temp, -correction*0.5);
				
				if(!((clmd->sim_parms->flags & CLOTH_SIMSETTINGS_FLAG_GOAL) && (cloth->verts [collpair->point_indexA[0]].goal >= SOFTGOALSNAP)))
					VECSUB(cloth->current_x[collpair->point_indexA[0]], cloth->current_x[collpair->point_indexA[0]], temp);	
				
				if(!((clmd->sim_parms->flags & CLOTH_SIMSETTINGS_FLAG_GOAL) && (cloth->verts [collpair->point_indexA[1]].goal >= SOFTGOALSNAP)))
					VECSUB(cloth->current_x[collpair->point_indexA[1]], cloth->current_x[collpair->point_indexA[1]], temp);
				
				if(!((clmd->sim_parms->flags & CLOTH_SIMSETTINGS_FLAG_GOAL) && (cloth->verts [collpair->point_indexA[2]].goal >= SOFTGOALSNAP)))
					VECSUB(cloth->current_x[collpair->point_indexA[2]], cloth->current_x[collpair->point_indexA[2]], temp);
				
				
				if(!((clmd->sim_parms->flags & CLOTH_SIMSETTINGS_FLAG_GOAL) && (cloth->verts [collpair->point_indexB[0]].goal >= SOFTGOALSNAP)))
					VECSUB(cloth->current_x[collpair->point_indexB[0]], cloth->current_x[collpair->point_indexB[0]], temp);	
				
				if(!((clmd->sim_parms->flags & CLOTH_SIMSETTINGS_FLAG_GOAL) && (cloth->verts [collpair->point_indexB[1]].goal >= SOFTGOALSNAP)))
					VECSUB(cloth->current_x[collpair->point_indexB[1]], cloth->current_x[collpair->point_indexB[1]], temp);
				
				if(!((clmd->sim_parms->flags & CLOTH_SIMSETTINGS_FLAG_GOAL) && (cloth->verts [collpair->point_indexB[2]].goal >= SOFTGOALSNAP)))
					VECSUB(cloth->current_x[collpair->point_indexB[2]], cloth->current_x[collpair->point_indexB[2]], temp);
					
				collisions = 1;
				
			}
			
		}
		
		search = collision_list;
		while(search)
		{
			CollisionPair *coll_pair = search->link;
			
			MEM_freeN(coll_pair);
			search = search->next;
		}
		BLI_linklist_free(collision_list,NULL);

		collision_list = NULL;
	}
	*/
	// Test on *simple* selfcollisions
	collisions = 1;
	count = 0;
	current_x = cloth->current_x; // needed for openMP
/*
#pragma omp parallel for private(i,j, collisions) shared(current_x)
	for(count = 0; count < 6; count++)
	{
		collisions = 0;
		
		for(i = 0; i < cloth->numverts; i++)
		{
			for(j = i + 1; j < cloth->numverts; j++)
			{
				float temp[3];
				float length = 0;
				float mindistance = cloth->selftree->epsilon;
					
				if(clmd->sim_parms->flags & CLOTH_SIMSETTINGS_FLAG_GOAL)
				{			
					if((cloth->verts [i].goal >= SOFTGOALSNAP)
					&& (cloth->verts [j].goal >= SOFTGOALSNAP))
					{
						continue;
					}
				}
					
					// check for adjacent points
				if(BLI_edgehash_haskey ( cloth->edgehash, i, j ))
				{
					continue;
				}
					
				VECSUB(temp, current_x[i], current_x[j]);
					
				length = Normalize(temp);
					
				if(length < mindistance)
				{
					float correction = mindistance - length;
						
					if((clmd->sim_parms->flags & CLOTH_SIMSETTINGS_FLAG_GOAL) && (cloth->verts [i].goal >= SOFTGOALSNAP))
					{
						VecMulf(temp, -correction);
						VECADD(current_x[j], current_x[j], temp);
					}
					else if((clmd->sim_parms->flags & CLOTH_SIMSETTINGS_FLAG_GOAL) && (cloth->verts [j].goal >= SOFTGOALSNAP))
					{
						VecMulf(temp, correction);
						VECADD(current_x[i], current_x[i], temp);
					}
					else
					{
						VecMulf(temp, -correction*0.5);
						VECADD(current_x[j], current_x[j], temp);
						
						VECSUB(current_x[i], current_x[i], temp);	
					}
					
					collisions = 1;
				}
			}
		}
	}

	
	//////////////////////////////////////////////
	// SELFCOLLISIONS: update velocities
	//////////////////////////////////////////////
	for(i = 0; i < cloth->numverts; i++)
	{
		VECSUB(cloth->current_v[i], cloth->current_x[i], cloth->current_xold[i]);
	}
	//////////////////////////////////////////////
*/	
	return 1;
}
