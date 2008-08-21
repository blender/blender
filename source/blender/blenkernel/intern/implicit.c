/*  implicit.c      
* 
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
* The Original Code is Copyright (C) Blender Foundation
* All rights reserved.
*
* The Original Code is: all of this file.
*
* Contributor(s): none yet.
*
* ***** END GPL LICENSE BLOCK *****
*/

#include "MEM_guardedalloc.h"

#include "BKE_cloth.h"

#include "DNA_cloth_types.h"	
#include "DNA_scene_types.h"

#include "BKE_effect.h"
#include "BKE_global.h"
#include "BKE_cloth.h"
#include "BKE_utildefines.h"

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
// intrinsics need better compile flag checking
// #include <xmmintrin.h>
// #include <pmmintrin.h>
// #include <pthread.h>

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

static float I[3][3] = {{1,0,0},{0,1,0},{0,0,1}};
static float ZERO[3][3] = {{0,0,0}, {0,0,0}, {0,0,0}};

/*
#define C99
#ifdef C99
#defineDO_INLINE inline 
#else 
#defineDO_INLINE static 
#endif
*/
struct Cloth;

//////////////////////////////////////////
/* fast vector / matrix library, enhancements are welcome :) -dg */
/////////////////////////////////////////

/* DEFINITIONS */
typedef float lfVector[3];
typedef struct fmatrix3x3 {
	float m[3][3]; /* 3x3 matrix */
	unsigned int c,r; /* column and row number */
	int pinned; /* is this vertex allowed to move? */
	float n1,n2,n3; /* three normal vectors for collision constrains */
	unsigned int vcount; /* vertex count */
	unsigned int scount; /* spring count */ 
} fmatrix3x3;

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
	to[0] = vectorA[1] * vectorB[2] - vectorA[2] * vectorB[1];
	to[1] = vectorA[2] * vectorB[0] - vectorA[0] * vectorB[2];
	to[2] = vectorA[0] * vectorB[1] - vectorA[1] * vectorB[0];
}
/* simple v^T * v product ("outer product") */
/* STATUS: HAS TO BE verified (*should* work) */
DO_INLINE void mul_fvectorT_fvector(float to[3][3], float vectorA[3], float vectorB[3])
{
	mul_fvector_S(to[0], vectorB, vectorA[0]);
	mul_fvector_S(to[1], vectorB, vectorA[1]);
	mul_fvector_S(to[2], vectorB, vectorA[2]);
}
/* simple v^T * v product with scalar ("outer product") */
/* STATUS: HAS TO BE verified (*should* work) */
DO_INLINE void mul_fvectorT_fvectorS(float to[3][3], float vectorA[3], float vectorB[3], float aS)
{	
	mul_fvectorT_fvector(to, vectorA, vectorB);
	
	mul_fvector_S(to[0], to[0], aS);
	mul_fvector_S(to[1], to[1], aS);
	mul_fvector_S(to[2], to[2], aS);
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
DO_INLINE void print_lfvector(float (*fLongVector)[3], unsigned int verts)
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
DO_INLINE void del_lfvector(float (*fLongVector)[3])
{
	if (fLongVector != NULL)
	{
		MEM_freeN (fLongVector);
		// cloth_aligned_free(&MEMORY_BASE, fLongVector);
	}
}
/* copy long vector */
DO_INLINE void cp_lfvector(float (*to)[3], float (*from)[3], unsigned int verts)
{
	memcpy(to, from, verts * sizeof(lfVector));
}
/* init long vector with float[3] */
DO_INLINE void init_lfvector(float (*fLongVector)[3], float vector[3], unsigned int verts)
{
	unsigned int i = 0;
	for(i = 0; i < verts; i++)
	{
		VECCOPY(fLongVector[i], vector);
	}
}
/* zero long vector with float[3] */
DO_INLINE void zero_lfvector(float (*to)[3], unsigned int verts)
{
	memset(to, 0.0f, verts * sizeof(lfVector));
}
/* multiply long vector with scalar*/
DO_INLINE void mul_lfvectorS(float (*to)[3], float (*fLongVector)[3], float scalar, unsigned int verts)
{
	unsigned int i = 0;

	for(i = 0; i < verts; i++)
	{
		mul_fvector_S(to[i], fLongVector[i], scalar);
	}
}
/* multiply long vector with scalar*/
/* A -= B * float */
DO_INLINE void submul_lfvectorS(float (*to)[3], float (*fLongVector)[3], float scalar, unsigned int verts)
{
	unsigned int i = 0;
	for(i = 0; i < verts; i++)
	{
		VECSUBMUL(to[i], fLongVector[i], scalar);
	}
}
/* dot product for big vector */
DO_INLINE float dot_lfvector(float (*fLongVectorA)[3], float (*fLongVectorB)[3], unsigned int verts)
{
	long i = 0;
	float temp = 0.0;
// schedule(guided, 2)
#pragma omp parallel for reduction(+: temp)
	for(i = 0; i < (long)verts; i++)
	{
		temp += INPR(fLongVectorA[i], fLongVectorB[i]);
	}
	return temp;
}
/* A = B + C  --> for big vector */
DO_INLINE void add_lfvector_lfvector(float (*to)[3], float (*fLongVectorA)[3], float (*fLongVectorB)[3], unsigned int verts)
{
	unsigned int i = 0;

	for(i = 0; i < verts; i++)
	{
		VECADD(to[i], fLongVectorA[i], fLongVectorB[i]);
	}

}
/* A = B + C * float --> for big vector */
DO_INLINE void add_lfvector_lfvectorS(float (*to)[3], float (*fLongVectorA)[3], float (*fLongVectorB)[3], float bS, unsigned int verts)
{
	unsigned int i = 0;

	for(i = 0; i < verts; i++)
	{
		VECADDS(to[i], fLongVectorA[i], fLongVectorB[i], bS);

	}
}
/* A = B * float + C * float --> for big vector */
DO_INLINE void add_lfvectorS_lfvectorS(float (*to)[3], float (*fLongVectorA)[3], float aS, float (*fLongVectorB)[3], float bS, unsigned int verts)
{
	unsigned int i = 0;

	for(i = 0; i < verts; i++)
	{
		VECADDSS(to[i], fLongVectorA[i], aS, fLongVectorB[i], bS);
	}
}
/* A = B - C * float --> for big vector */
DO_INLINE void sub_lfvector_lfvectorS(float (*to)[3], float (*fLongVectorA)[3], float (*fLongVectorB)[3], float bS, unsigned int verts)
{
	unsigned int i = 0;
	for(i = 0; i < verts; i++)
	{
		VECSUBS(to[i], fLongVectorA[i], fLongVectorB[i], bS);
	}

}
/* A = B - C --> for big vector */
DO_INLINE void sub_lfvector_lfvector(float (*to)[3], float (*fLongVectorA)[3], float (*fLongVectorB)[3], unsigned int verts)
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
void print_fmatrix(float m3[3][3])
{
	printf("%f\t%f\t%f\n",m3[0][0],m3[0][1],m3[0][2]);
	printf("%f\t%f\t%f\n",m3[1][0],m3[1][1],m3[1][2]);
	printf("%f\t%f\t%f\n\n",m3[2][0],m3[2][1],m3[2][2]);
}

/* copy 3x3 matrix */
DO_INLINE void cp_fmatrix(float to[3][3], float from[3][3])
{
	// memcpy(to, from, sizeof (float) * 9);
	VECCOPY(to[0], from[0]);
	VECCOPY(to[1], from[1]);
	VECCOPY(to[2], from[2]);
}

/* copy 3x3 matrix */
DO_INLINE void initdiag_fmatrixS(float to[3][3], float aS)
{
	cp_fmatrix(to, ZERO);
	
	to[0][0] = aS;
	to[1][1] = aS;
	to[2][2] = aS;
}

/* calculate determinant of 3x3 matrix */
DO_INLINE float det_fmatrix(float m[3][3])
{
	return  m[0][0]*m[1][1]*m[2][2] + m[1][0]*m[2][1]*m[0][2] + m[0][1]*m[1][2]*m[2][0] 
			-m[0][0]*m[1][2]*m[2][1] - m[0][1]*m[1][0]*m[2][2] - m[2][0]*m[1][1]*m[0][2];
}

DO_INLINE void inverse_fmatrix(float to[3][3], float from[3][3])
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
DO_INLINE void mul_fmatrix_S(float matrix[3][3], float scalar)
{
	mul_fvector_S(matrix[0], matrix[0],scalar);
	mul_fvector_S(matrix[1], matrix[1],scalar);
	mul_fvector_S(matrix[2], matrix[2],scalar);
}

/* a vector multiplied by a 3x3 matrix */
/* STATUS: verified */
DO_INLINE void mul_fvector_fmatrix(float *to, float *from, float matrix[3][3])
{
	to[0] = matrix[0][0]*from[0] + matrix[1][0]*from[1] + matrix[2][0]*from[2];
	to[1] = matrix[0][1]*from[0] + matrix[1][1]*from[1] + matrix[2][1]*from[2];
	to[2] = matrix[0][2]*from[0] + matrix[1][2]*from[1] + matrix[2][2]*from[2];
}

/* 3x3 matrix multiplied by a vector */
/* STATUS: verified */
DO_INLINE void mul_fmatrix_fvector(float *to, float matrix[3][3], float *from)
{
	to[0] = INPR(matrix[0],from);
	to[1] = INPR(matrix[1],from);
	to[2] = INPR(matrix[2],from);
}
/* 3x3 matrix multiplied by a 3x3 matrix */
/* STATUS: verified */
DO_INLINE void mul_fmatrix_fmatrix(float to[3][3], float matrixA[3][3], float matrixB[3][3])
{
	mul_fvector_fmatrix(to[0], matrixA[0],matrixB);
	mul_fvector_fmatrix(to[1], matrixA[1],matrixB);
	mul_fvector_fmatrix(to[2], matrixA[2],matrixB);
}
/* 3x3 matrix addition with 3x3 matrix */
DO_INLINE void add_fmatrix_fmatrix(float to[3][3], float matrixA[3][3], float matrixB[3][3])
{
	VECADD(to[0], matrixA[0], matrixB[0]);
	VECADD(to[1], matrixA[1], matrixB[1]);
	VECADD(to[2], matrixA[2], matrixB[2]);
}
/* 3x3 matrix add-addition with 3x3 matrix */
DO_INLINE void addadd_fmatrix_fmatrix(float to[3][3], float matrixA[3][3], float matrixB[3][3])
{
	VECADDADD(to[0], matrixA[0], matrixB[0]);
	VECADDADD(to[1], matrixA[1], matrixB[1]);
	VECADDADD(to[2], matrixA[2], matrixB[2]);
}
/* 3x3 matrix sub-addition with 3x3 matrix */
DO_INLINE void addsub_fmatrixS_fmatrixS(float to[3][3], float matrixA[3][3], float aS, float matrixB[3][3], float bS)
{
	VECADDSUBSS(to[0], matrixA[0], aS, matrixB[0], bS);
	VECADDSUBSS(to[1], matrixA[1], aS, matrixB[1], bS);
	VECADDSUBSS(to[2], matrixA[2], aS, matrixB[2], bS);
}
/* A -= B + C (3x3 matrix sub-addition with 3x3 matrix) */
DO_INLINE void subadd_fmatrix_fmatrix(float to[3][3], float matrixA[3][3], float matrixB[3][3])
{
	VECSUBADD(to[0], matrixA[0], matrixB[0]);
	VECSUBADD(to[1], matrixA[1], matrixB[1]);
	VECSUBADD(to[2], matrixA[2], matrixB[2]);
}
/* A -= B*x + C*y (3x3 matrix sub-addition with 3x3 matrix) */
DO_INLINE void subadd_fmatrixS_fmatrixS(float to[3][3], float matrixA[3][3], float aS, float matrixB[3][3], float bS)
{
	VECSUBADDSS(to[0], matrixA[0], aS, matrixB[0], bS);
	VECSUBADDSS(to[1], matrixA[1], aS, matrixB[1], bS);
	VECSUBADDSS(to[2], matrixA[2], aS, matrixB[2], bS);
}
/* A = B - C (3x3 matrix subtraction with 3x3 matrix) */
DO_INLINE void sub_fmatrix_fmatrix(float to[3][3], float matrixA[3][3], float matrixB[3][3])
{
	VECSUB(to[0], matrixA[0], matrixB[0]);
	VECSUB(to[1], matrixA[1], matrixB[1]);
	VECSUB(to[2], matrixA[2], matrixB[2]);
}
/* A += B - C (3x3 matrix add-subtraction with 3x3 matrix) */
DO_INLINE void addsub_fmatrix_fmatrix(float to[3][3], float matrixA[3][3], float matrixB[3][3])
{
	VECADDSUB(to[0], matrixA[0], matrixB[0]);
	VECADDSUB(to[1], matrixA[1], matrixB[1]);
	VECADDSUB(to[2], matrixA[2], matrixB[2]);
}
/////////////////////////////////////////////////////////////////
// special functions
/////////////////////////////////////////////////////////////////
/* a vector multiplied and added to/by a 3x3 matrix */
DO_INLINE void muladd_fvector_fmatrix(float to[3], float from[3], float matrix[3][3])
{
	to[0] += matrix[0][0]*from[0] + matrix[1][0]*from[1] + matrix[2][0]*from[2];
	to[1] += matrix[0][1]*from[0] + matrix[1][1]*from[1] + matrix[2][1]*from[2];
	to[2] += matrix[0][2]*from[0] + matrix[1][2]*from[1] + matrix[2][2]*from[2];
}
/* 3x3 matrix multiplied and added  to/by a 3x3 matrix  and added to another 3x3 matrix */
DO_INLINE void muladd_fmatrix_fmatrix(float to[3][3], float matrixA[3][3], float matrixB[3][3])
{
	muladd_fvector_fmatrix(to[0], matrixA[0],matrixB);
	muladd_fvector_fmatrix(to[1], matrixA[1],matrixB);
	muladd_fvector_fmatrix(to[2], matrixA[2],matrixB);
}
/* a vector multiplied and sub'd to/by a 3x3 matrix */
DO_INLINE void mulsub_fvector_fmatrix(float to[3], float from[3], float matrix[3][3])
{
	to[0] -= matrix[0][0]*from[0] + matrix[1][0]*from[1] + matrix[2][0]*from[2];
	to[1] -= matrix[0][1]*from[0] + matrix[1][1]*from[1] + matrix[2][1]*from[2];
	to[2] -= matrix[0][2]*from[0] + matrix[1][2]*from[1] + matrix[2][2]*from[2];
}
/* 3x3 matrix multiplied and sub'd  to/by a 3x3 matrix  and added to another 3x3 matrix */
DO_INLINE void mulsub_fmatrix_fmatrix(float to[3][3], float matrixA[3][3], float matrixB[3][3])
{
	mulsub_fvector_fmatrix(to[0], matrixA[0],matrixB);
	mulsub_fvector_fmatrix(to[1], matrixA[1],matrixB);
	mulsub_fvector_fmatrix(to[2], matrixA[2],matrixB);
}
/* 3x3 matrix multiplied+added by a vector */
/* STATUS: verified */
DO_INLINE void muladd_fmatrix_fvector(float to[3], float matrix[3][3], float from[3])
{
	to[0] += INPR(matrix[0],from);
	to[1] += INPR(matrix[1],from);
	to[2] += INPR(matrix[2],from);	
}
/* 3x3 matrix multiplied+sub'ed by a vector */
DO_INLINE void mulsub_fmatrix_fvector(float to[3], float matrix[3][3], float from[3])
{
	to[0] -= INPR(matrix[0],from);
	to[1] -= INPR(matrix[1],from);
	to[2] -= INPR(matrix[2],from);
}
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

/* init big matrix */
// slow in parallel
DO_INLINE void init_bfmatrix(fmatrix3x3 *matrix, float m3[3][3])
{
	unsigned int i;

	for(i = 0; i < matrix[0].vcount+matrix[0].scount; i++)
	{		
		cp_fmatrix(matrix[i].m, m3); 
	}
}

/* init the diagonal of big matrix */
// slow in parallel
DO_INLINE void initdiag_bfmatrix(fmatrix3x3 *matrix, float m3[3][3])
{
	unsigned int i,j;
	float tmatrix[3][3] = {{0,0,0},{0,0,0},{0,0,0}};

	for(i = 0; i < matrix[0].vcount; i++)
	{		
		cp_fmatrix(matrix[i].m, m3); 
	}
	for(j = matrix[0].vcount; j < matrix[0].vcount+matrix[0].scount; j++)
	{
		cp_fmatrix(matrix[j].m, tmatrix); 
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
DO_INLINE void mul_bfmatrix_lfvector( float (*to)[3], fmatrix3x3 *from, lfVector *fLongVector)
{
	unsigned int i = 0;
	lfVector *temp = create_lfvector(from[0].vcount);
	
	zero_lfvector(to, from[0].vcount);

#pragma omp parallel sections private(i)
	{
#pragma omp section
		{
			for(i = from[0].vcount; i < from[0].vcount+from[0].scount; i++)
			{
				muladd_fmatrix_fvector(to[from[i].c], from[i].m, fLongVector[from[i].r]);
			}
		}	
#pragma omp section
		{
			for(i = 0; i < from[0].vcount+from[0].scount; i++)
			{
				muladd_fmatrix_fvector(temp[from[i].r], from[i].m, fLongVector[from[i].c]);
			}
		}
	}
	add_lfvector_lfvector(to, to, temp, from[0].vcount);
	
	del_lfvector(temp);
	
	
}

/* SPARSE SYMMETRIC multiply big matrix with long vector (for diagonal preconditioner) */
/* STATUS: verified */
DO_INLINE void mul_prevfmatrix_lfvector( float (*to)[3], fmatrix3x3 *from, lfVector *fLongVector)
{
	unsigned int i = 0;
	
	for(i = 0; i < from[0].vcount; i++)
	{
		mul_fmatrix_fvector(to[from[i].r], from[i].m, fLongVector[from[i].c]);
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
typedef struct Implicit_Data 
{
	lfVector *X, *V, *Xnew, *Vnew, *olddV, *F, *B, *dV, *z;
	fmatrix3x3 *A, *dFdV, *dFdX, *S, *P, *Pinv, *bigI, *M; 
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
	
	if(G.rt > 0)
		printf("implicit_init\n");

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
	id->M = create_bfmatrix(cloth->numverts, cloth->numsprings);
	id->X = create_lfvector(cloth->numverts);
	id->Xnew = create_lfvector(cloth->numverts);
	id->V = create_lfvector(cloth->numverts);
	id->Vnew = create_lfvector(cloth->numverts);
	id->olddV = create_lfvector(cloth->numverts);
	zero_lfvector(id->olddV, cloth->numverts);
	id->F = create_lfvector(cloth->numverts);
	id->B = create_lfvector(cloth->numverts);
	id->dV = create_lfvector(cloth->numverts);
	id->z = create_lfvector(cloth->numverts);
	
	for(i=0;i<cloth->numverts;i++) 
	{
		id->A[i].r = id->A[i].c = id->dFdV[i].r = id->dFdV[i].c = id->dFdX[i].r = id->dFdX[i].c = id->P[i].c = id->P[i].r = id->Pinv[i].c = id->Pinv[i].r = id->bigI[i].c = id->bigI[i].r = id->M[i].r = id->M[i].c = i;

		if(verts [i].flags & CLOTH_VERT_FLAG_PINNED)
		{
			id->S[pinned].pinned = 1;
			id->S[pinned].c = id->S[pinned].r = i;
			pinned++;
		}
		
		initdiag_fmatrixS(id->M[i].m, verts[i].mass);
	}

	// S is special and needs specific vcount and scount
	id->S[0].vcount = pinned; id->S[0].scount = 0;

	// init springs 
	search = cloth->springs;
	for(i=0;i<cloth->numsprings;i++) 
	{
		spring = search->link;
		
		// dFdV_start[i].r = big_I[i].r = big_zero[i].r = 
		id->A[i+cloth->numverts].r = id->dFdV[i+cloth->numverts].r = id->dFdX[i+cloth->numverts].r = 
				id->P[i+cloth->numverts].r = id->Pinv[i+cloth->numverts].r = id->bigI[i+cloth->numverts].r = id->M[i+cloth->numverts].r = spring->ij;

		// dFdV_start[i].c = big_I[i].c = big_zero[i].c = 
		id->A[i+cloth->numverts].c = id->dFdV[i+cloth->numverts].c = id->dFdX[i+cloth->numverts].c = 
				id->P[i+cloth->numverts].c = id->Pinv[i+cloth->numverts].c = id->bigI[i+cloth->numverts].c = id->M[i+cloth->numverts].c = spring->kl;

		spring->matrix_index = i + cloth->numverts;
		
		search = search->next;
	}
	
	initdiag_bfmatrix(id->bigI, I);

	for(i = 0; i < cloth->numverts; i++)
	{		
		VECCOPY(id->X[i], verts[i].x);
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
			del_bfmatrix(id->M);

			del_lfvector(id->X);
			del_lfvector(id->Xnew);
			del_lfvector(id->V);
			del_lfvector(id->Vnew);
			del_lfvector(id->olddV);
			del_lfvector(id->F);
			del_lfvector(id->B);
			del_lfvector(id->dV);
			del_lfvector(id->z);

			MEM_freeN(id);
		}
	}

	return 1;
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

// function to calculae bending spring force (taken from Choi & Co)
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

	// zero_lfvector(ldV, CLOTHPARTICLES);
	filter(ldV, S);

	add_lfvector_lfvector(ldV, ldV, z, numverts);

	// r = B - Mul(tmp,A,X);    // just use B if X known to be zero
	cp_lfvector(r, lB, numverts);
	mul_bfmatrix_lfvector(tmp, lA, ldV);
	sub_lfvector_lfvector(r, r, tmp, numverts);

	filter(r,S);

	cp_lfvector(d, r, numverts);

	s = dot_lfvector(r, r, numverts);
	starget = s * sqrt(conjgrad_epsilon);

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
/*
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
	
	mul_prevfmatrix_lfvector(p, Pinv, r);
	filter(p, S);
	
	deltaNew = dot_lfvector(r, p, numverts);
	
	delta0 = deltaNew * sqrt(conjgrad_epsilon);
	
	// itstart();
	
	while ((deltaNew > delta0) && (iterations < conjgrad_looplimit))
	{
		iterations++;
		
		mul_bfmatrix_lfvector(s, lA, p);
		filter(s, S);
		
		alpha = deltaNew / dot_lfvector(p, s, numverts);
		
		add_lfvector_lfvectorS(dv, dv, p, alpha, numverts);
		
		add_lfvector_lfvectorS(r, r, s, -alpha, numverts);
		
		mul_prevfmatrix_lfvector(h, Pinv, r);
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
	
	printf("iterations: %d\n", iterations);
	
	return iterations<conjgrad_looplimit;
}
*/
// version 1.4
int cg_filtered_pre(lfVector *dv, fmatrix3x3 *lA, lfVector *lB, lfVector *z, fmatrix3x3 *S, fmatrix3x3 *P, fmatrix3x3 *Pinv, fmatrix3x3 *bigI)
{
	unsigned int numverts = lA[0].vcount, iterations = 0, conjgrad_looplimit=100;
	float delta0 = 0, deltaNew = 0, deltaOld = 0, alpha = 0, tol = 0;
	lfVector *r = create_lfvector(numverts);
	lfVector *p = create_lfvector(numverts);
	lfVector *s = create_lfvector(numverts);
	lfVector *h = create_lfvector(numverts);
	lfVector *bhat = create_lfvector(numverts);
	lfVector *btemp = create_lfvector(numverts);
	
	BuildPPinv(lA, P, Pinv);
	
	initdiag_bfmatrix(bigI, I);
	sub_bfmatrix_Smatrix(bigI, bigI, S);
	
	// x = Sx_0+(I-S)z
	filter(dv, S);
	add_lfvector_lfvector(dv, dv, z, numverts);
	
	// b_hat = S(b-A(I-S)z)
	mul_bfmatrix_lfvector(r, lA, z);
	mul_bfmatrix_lfvector(bhat, bigI, r);
	sub_lfvector_lfvector(bhat, lB, bhat, numverts);
	
	// r = S(b-Ax)
	mul_bfmatrix_lfvector(r, lA, dv);
	sub_lfvector_lfvector(r, lB, r, numverts);
	filter(r, S);
	
	// p = SP^-1r
	mul_prevfmatrix_lfvector(p, Pinv, r);
	filter(p, S);
	
	// delta0 = bhat^TP^-1bhat
	mul_prevfmatrix_lfvector(btemp, Pinv, bhat);
	delta0 = dot_lfvector(bhat, btemp, numverts);
	
	// deltaNew = r^TP
	deltaNew = dot_lfvector(r, p, numverts);
	
	/*
	filter(dv, S);
	add_lfvector_lfvector(dv, dv, z, numverts);
	
	mul_bfmatrix_lfvector(r, lA, dv);
	sub_lfvector_lfvector(r, lB, r, numverts);
	filter(r, S);
	
	mul_prevfmatrix_lfvector(p, Pinv, r);
	filter(p, S);
	
	deltaNew = dot_lfvector(r, p, numverts);
	
	delta0 = deltaNew * sqrt(conjgrad_epsilon);
	*/
	
	// itstart();
	
	tol = (0.01*0.2);
	
	while ((deltaNew > delta0*tol*tol) && (iterations < conjgrad_looplimit))
	{
		iterations++;
		
		mul_bfmatrix_lfvector(s, lA, p);
		filter(s, S);
		
		alpha = deltaNew / dot_lfvector(p, s, numverts);
		
		add_lfvector_lfvectorS(dv, dv, p, alpha, numverts);
		
		add_lfvector_lfvectorS(r, r, s, -alpha, numverts);
		
		mul_prevfmatrix_lfvector(h, Pinv, r);
		filter(h, S);
		
		deltaOld = deltaNew;
		
		deltaNew = dot_lfvector(r, h, numverts);
		
		add_lfvector_lfvectorS(p, h, p, deltaNew / deltaOld, numverts);
		
		filter(p, S);
		
	}
	
	// itend();
	// printf("cg_filtered_pre time: %f\n", (float)itval());
	
	del_lfvector(btemp);
	del_lfvector(bhat);
	del_lfvector(h);
	del_lfvector(s);
	del_lfvector(p);
	del_lfvector(r);
	
	// printf("iterations: %d\n", iterations);
	
	return iterations<conjgrad_looplimit;
}

// outer product is NOT cross product!!!
DO_INLINE void dfdx_spring_type1(float to[3][3], float extent[3], float length, float L, float dot, float k)
{
	// dir is unit length direction, rest is spring's restlength, k is spring constant.
	// return  (outerprod(dir,dir)*k + (I - outerprod(dir,dir))*(k - ((k*L)/length)));
	float temp[3][3];
	float temp1 = k*(1.0 - (L/length));	
	
	mul_fvectorT_fvectorS(temp, extent, extent, 1.0 / dot);
	sub_fmatrix_fmatrix(to, I, temp);
	mul_fmatrix_S(to, temp1);
	
	mul_fvectorT_fvectorS(temp, extent, extent, k/ dot);
	add_fmatrix_fmatrix(to, to, temp);
	
	/*
	mul_fvectorT_fvector(temp, dir, dir);
	sub_fmatrix_fmatrix(to, I, temp);
	mul_fmatrix_S(to, k* (1.0f-(L/length)));
	mul_fmatrix_S(temp, k);
	add_fmatrix_fmatrix(to, temp, to);
	*/
}

DO_INLINE void dfdx_spring_type2(float to[3][3], float dir[3], float length, float L, float k, float cb)
{
	// return  outerprod(dir,dir)*fbstar_jacobi(length, L, k, cb);
	mul_fvectorT_fvectorS(to, dir, dir, fbstar_jacobi(length, L, k, cb));
}

DO_INLINE void dfdv_damp(float to[3][3], float dir[3], float damping)
{
	// derivative of force wrt velocity.  
	mul_fvectorT_fvectorS(to, dir, dir, damping);
	
}

DO_INLINE void dfdx_spring(float to[3][3],  float dir[3],float length,float L,float k)
{
	// dir is unit length direction, rest is spring's restlength, k is spring constant.
	//return  ( (I-outerprod(dir,dir))*Min(1.0f,rest/length) - I) * -k;
	mul_fvectorT_fvector(to, dir, dir);
	sub_fmatrix_fmatrix(to, I, to);
	mul_fmatrix_S(to, (((L/length)> 1.0f) ? (1.0f): (L/length))); 
	sub_fmatrix_fmatrix(to, to, I);
	mul_fmatrix_S(to, -k);
}

// unused atm
DO_INLINE void dfdx_damp(float to[3][3],  float dir[3],float length,const float vel[3],float rest,float damping)
{
	// inner spring damping   vel is the relative velocity  of the endpoints.  
	// 	return (I-outerprod(dir,dir)) * (-damping * -(dot(dir,vel)/Max(length,rest)));
	mul_fvectorT_fvector(to, dir, dir);
	sub_fmatrix_fmatrix(to, I, to);
	mul_fmatrix_S(to,  (-damping * -(INPR(dir,vel)/MAX2(length,rest)))); 

}

DO_INLINE void cloth_calc_spring_force(ClothModifierData *clmd, ClothSpring *s, lfVector *lF, lfVector *X, lfVector *V, fmatrix3x3 *dFdV, fmatrix3x3 *dFdX, float time)
{
	Cloth *cloth = clmd->clothObject;
	ClothVertex *verts = cloth->verts;
	float extent[3];
	float length = 0, dot = 0;
	float dir[3] = {0,0,0};
	float vel[3];
	float k = 0.0f;
	float L = s->restlen;
	float cb = clmd->sim_parms->structural;

	float nullf[3] = {0,0,0};
	float stretch_force[3] = {0,0,0};
	float bending_force[3] = {0,0,0};
	float damping_force[3] = {0,0,0};
	float nulldfdx[3][3]={ {0,0,0}, {0,0,0}, {0,0,0}};
	
	float scaling = 0.0;
	
	VECCOPY(s->f, nullf);
	cp_fmatrix(s->dfdx, nulldfdx);
	cp_fmatrix(s->dfdv, nulldfdx);

	// calculate elonglation
	VECSUB(extent, X[s->kl], X[s->ij]);
	VECSUB(vel, V[s->kl], V[s->ij]);
	dot = INPR(extent, extent);
	length = sqrt(dot);
	
	s->flags &= ~CLOTH_SPRING_FLAG_NEEDED;
	
	if(length > ALMOST_ZERO)
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
	if((s->type & CLOTH_SPRING_TYPE_STRUCTURAL) || (s->type & CLOTH_SPRING_TYPE_SHEAR))
	{
		if(length > L) // only on elonglation
		{
			s->flags |= CLOTH_SPRING_FLAG_NEEDED;
			
			k = clmd->sim_parms->structural;
				
			scaling = k + s->stiffness * ABS(clmd->sim_parms->max_struct-k);
			
			k = scaling / (clmd->sim_parms->avg_spring_len + FLT_EPSILON);
			
			// TODO: verify, half verified (couldn't see error)
			mul_fvector_S(stretch_force, dir, k*(length-L)); 

			VECADD(s->f, s->f, stretch_force);

			// Ascher & Boxman, p.21: Damping only during elonglation
			// something wrong with it...
			mul_fvector_S(damping_force, dir, clmd->sim_parms->Cdis * INPR(vel,dir));
			VECADD(s->f, s->f, damping_force);
			
			/* VERIFIED */
			dfdx_spring(s->dfdx, dir, length, L, k);
			
			/* VERIFIED */
			dfdv_damp(s->dfdv, dir, clmd->sim_parms->Cdis);
			
		}
	}
	else if(s->type & CLOTH_SPRING_TYPE_GOAL)
	{
		float tvect[3];
		
		s->flags |= CLOTH_SPRING_FLAG_NEEDED;
		
		// current_position = xold + t * (newposition - xold)
		VECSUB(tvect, verts[s->ij].xconst, verts[s->ij].xold);
		mul_fvector_S(tvect, tvect, time);
		VECADD(tvect, tvect, verts[s->ij].xold);

		VECSUB(extent, X[s->ij], tvect);
		
		dot = INPR(extent, extent);
		length = sqrt(dot);
		
		k = clmd->sim_parms->goalspring;
		
		scaling = k + s->stiffness * ABS(clmd->sim_parms->max_struct-k);
			
		k = verts [s->ij].goal * scaling / (clmd->sim_parms->avg_spring_len + FLT_EPSILON);
		
		VECADDS(s->f, s->f, extent, -k);
		
		mul_fvector_S(damping_force, dir, clmd->sim_parms->goalfrict * 0.01 * INPR(vel,dir));
		VECADD(s->f, s->f, damping_force);
		
		// HERE IS THE PROBLEM!!!!
		// dfdx_spring(s->dfdx, dir, length, 0.0, k);
		// dfdv_damp(s->dfdv, dir, MIN2(1.0, (clmd->sim_parms->goalfrict/100.0)));
	}
	else // calculate force of bending springs
	{
		if(length < L)
		{
			s->flags |= CLOTH_SPRING_FLAG_NEEDED;
			
			k = clmd->sim_parms->bending;	
			
			scaling = k + s->stiffness * ABS(clmd->sim_parms->max_bend-k);			
			cb = k = scaling / (20.0*(clmd->sim_parms->avg_spring_len + FLT_EPSILON));

			mul_fvector_S(bending_force, dir, fbstar(length, L, k, cb));
			VECADD(s->f, s->f, bending_force);

			dfdx_spring_type2(s->dfdx, dir, length,L, k, cb);
		}
	}
}

DO_INLINE void cloth_apply_spring_force(ClothModifierData *clmd, ClothSpring *s, lfVector *lF, lfVector *X, lfVector *V, fmatrix3x3 *dFdV, fmatrix3x3 *dFdX)
{
	if(s->flags & CLOTH_SPRING_FLAG_NEEDED)
	{
		if(!(s->type & CLOTH_SPRING_TYPE_BENDING))
		{
			sub_fmatrix_fmatrix(dFdV[s->ij].m, dFdV[s->ij].m, s->dfdv);
			sub_fmatrix_fmatrix(dFdV[s->kl].m, dFdV[s->kl].m, s->dfdv);
			add_fmatrix_fmatrix(dFdV[s->matrix_index].m, dFdV[s->matrix_index].m, s->dfdv);	
		}

		VECADD(lF[s->ij], lF[s->ij], s->f);
		
		if(!(s->type & CLOTH_SPRING_TYPE_GOAL))
			VECSUB(lF[s->kl], lF[s->kl], s->f);
		
		sub_fmatrix_fmatrix(dFdX[s->kl].m, dFdX[s->kl].m, s->dfdx);
		sub_fmatrix_fmatrix(dFdX[s->ij].m, dFdX[s->ij].m, s->dfdx);
		add_fmatrix_fmatrix(dFdX[s->matrix_index].m, dFdX[s->matrix_index].m, s->dfdx);
	}	
}


static void CalcFloat( float *v1, float *v2, float *v3, float *n)
{
	float n1[3],n2[3];

	n1[0]= v1[0]-v2[0];
	n2[0]= v2[0]-v3[0];
	n1[1]= v1[1]-v2[1];
	n2[1]= v2[1]-v3[1];
	n1[2]= v1[2]-v2[2];
	n2[2]= v2[2]-v3[2];
	n[0]= n1[1]*n2[2]-n1[2]*n2[1];
	n[1]= n1[2]*n2[0]-n1[0]*n2[2];
	n[2]= n1[0]*n2[1]-n1[1]*n2[0];
}

static void CalcFloat4( float *v1, float *v2, float *v3, float *v4, float *n)
{
	/* real cross! */
	float n1[3],n2[3];

	n1[0]= v1[0]-v3[0];
	n1[1]= v1[1]-v3[1];
	n1[2]= v1[2]-v3[2];

	n2[0]= v2[0]-v4[0];
	n2[1]= v2[1]-v4[1];
	n2[2]= v2[2]-v4[2];

	n[0]= n1[1]*n2[2]-n1[2]*n2[1];
	n[1]= n1[2]*n2[0]-n1[0]*n2[2];
	n[2]= n1[0]*n2[1]-n1[1]*n2[0];
}

float calculateVertexWindForce(float wind[3], float vertexnormal[3])  
{
	return (INPR(wind, vertexnormal));
}

void cloth_calc_force(ClothModifierData *clmd, lfVector *lF, lfVector *lX, lfVector *lV, fmatrix3x3 *dFdV, fmatrix3x3 *dFdX, ListBase *effectors, float time, fmatrix3x3 *M)
{
	/* Collect forces and derivatives:  F,dFdX,dFdV */
	Cloth 		*cloth 		= clmd->clothObject;
	int		i 		= 0;
	float 		spring_air 	= clmd->sim_parms->Cvi * 0.01f; /* viscosity of air scaled in percent */
	float 		gravity[3];
	float 		tm2[3][3] 	= {{-spring_air,0,0}, {0,-spring_air,0},{0,0,-spring_air}};
	MFace 		*mfaces 	= cloth->mfaces;
	unsigned int numverts = cloth->numverts;
	LinkNode *search = cloth->springs;
	lfVector *winvec;

	VECCOPY(gravity, clmd->sim_parms->gravity);
	mul_fvector_S(gravity, gravity, 0.001f); /* scale gravity force */

	/* set dFdX jacobi matrix to zero */
	init_bfmatrix(dFdX, ZERO);
	/* set dFdX jacobi matrix diagonal entries to -spring_air */ 
	initdiag_bfmatrix(dFdV, tm2);

	init_lfvector(lF, gravity, numverts);
	
	/* multiply lF with mass matrix
	// force = mass * acceleration (in this case: gravity)
	*/
	for(i = 0; i < numverts; i++)
	{
		float temp[3];
		VECCOPY(temp, lF[i]);
		mul_fmatrix_fvector(lF[i], M[i].m, temp);
	}

	submul_lfvectorS(lF, lV, spring_air, numverts);
	
	/* handle external forces like wind */
	if(effectors)
	{	
		// 0 = force, 1 = normalized force
		winvec = create_lfvector(cloth->numverts);
		
		if(!winvec)
			printf("winvec: out of memory in implicit.c\n");
		
		// precalculate wind forces
		for(i = 0; i < cloth->numverts; i++)
		{
			float speed[3] = {0.0f, 0.0f,0.0f};
			
			pdDoEffectors(effectors, lX[i], winvec[i], speed, (float)G.scene->r.cfra, 0.0f, 0);
		}
		
		for(i = 0; i < cloth->numfaces; i++)
		{
			float trinormal[3]={0,0,0}; // normalized triangle normal
			float triunnormal[3]={0,0,0}; // not-normalized-triangle normal
			float tmp[3]={0,0,0};
			float factor = (mfaces[i].v4) ? 0.25 : 1.0 / 3.0;
			factor *= 0.02;
			
			// calculate face normal
			if(mfaces[i].v4)
				CalcFloat4(lX[mfaces[i].v1],lX[mfaces[i].v2],lX[mfaces[i].v3],lX[mfaces[i].v4],triunnormal);
			else
				CalcFloat(lX[mfaces[i].v1],lX[mfaces[i].v2],lX[mfaces[i].v3],triunnormal);
			
			VECCOPY(trinormal, triunnormal);
			Normalize(trinormal);
			
			// add wind from v1
			VECCOPY(tmp, trinormal);
			VecMulf(tmp, calculateVertexWindForce(winvec[mfaces[i].v1], triunnormal));
			VECADDS(lF[mfaces[i].v1], lF[mfaces[i].v1], tmp, factor);
			
			// add wind from v2
			VECCOPY(tmp, trinormal);
			VecMulf(tmp, calculateVertexWindForce(winvec[mfaces[i].v2], triunnormal));
			VECADDS(lF[mfaces[i].v2], lF[mfaces[i].v2], tmp, factor);
			
			// add wind from v3
			VECCOPY(tmp, trinormal);
			VecMulf(tmp, calculateVertexWindForce(winvec[mfaces[i].v3], triunnormal));
			VECADDS(lF[mfaces[i].v3], lF[mfaces[i].v3], tmp, factor);
			
			// add wind from v4
			if(mfaces[i].v4)
			{
				VECCOPY(tmp, trinormal);
				VecMulf(tmp, calculateVertexWindForce(winvec[mfaces[i].v4], triunnormal));
				VECADDS(lF[mfaces[i].v4], lF[mfaces[i].v4], tmp, factor);
			}
		}
		del_lfvector(winvec);
	}
		
	// calculate spring forces
	search = cloth->springs;
	while(search)
	{
		// only handle active springs
		// if(((clmd->sim_parms->flags & CSIMSETT_FLAG_TEARING_ENABLED) && !(springs[i].flags & CSPRING_FLAG_DEACTIVATE))|| !(clmd->sim_parms->flags & CSIMSETT_FLAG_TEARING_ENABLED)){}
		cloth_calc_spring_force(clmd, search->link, lF, lX, lV, dFdV, dFdX, time);

		search = search->next;
	}
	
	// apply spring forces
	search = cloth->springs;
	while(search)
	{
		// only handle active springs
		// if(((clmd->sim_parms->flags & CSIMSETT_FLAG_TEARING_ENABLED) && !(springs[i].flags & CSPRING_FLAG_DEACTIVATE))|| !(clmd->sim_parms->flags & CSIMSETT_FLAG_TEARING_ENABLED))	
		cloth_apply_spring_force(clmd, search->link, lF, lX, lV, dFdV, dFdX);
		search = search->next;
	}
	// printf("\n");
}

void simulate_implicit_euler(lfVector *Vnew, lfVector *lX, lfVector *lV, lfVector *lF, fmatrix3x3 *dFdV, fmatrix3x3 *dFdX, float dt, fmatrix3x3 *A, lfVector *B, lfVector *dV, fmatrix3x3 *S, lfVector *z, lfVector *olddV, fmatrix3x3 *P, fmatrix3x3 *Pinv, fmatrix3x3 *M, fmatrix3x3 *bigI)
{
	unsigned int numverts = dFdV[0].vcount;

	lfVector *dFdXmV = create_lfvector(numverts);
	zero_lfvector(dV, numverts);
	
	cp_bfmatrix(A, M);
	
	subadd_bfmatrixS_bfmatrixS(A, dFdV, dt, dFdX, (dt*dt));

	mul_bfmatrix_lfvector(dFdXmV, dFdX, lV);

	add_lfvectorS_lfvectorS(B, lF, dt, dFdXmV, (dt*dt), numverts);
	
	itstart();
	
	cg_filtered(dV, A, B, z, S); /* conjugate gradient algorithm to solve Ax=b */
	// cg_filtered_pre(dV, A, B, z, S, P, Pinv, bigI);
	
	itend();
	// printf("cg_filtered calc time: %f\n", (float)itval());
	
	cp_lfvector(olddV, dV, numverts);

	// advance velocities
	add_lfvector_lfvector(Vnew, lV, dV, numverts);
	

	del_lfvector(dFdXmV);
}

int implicit_solver (Object *ob, float frame, ClothModifierData *clmd, ListBase *effectors)
{ 	 	
	unsigned int i=0;
	float step=0.0f, tf=clmd->sim_parms->timescale;
	Cloth *cloth = clmd->clothObject;
	ClothVertex *verts = cloth->verts;
	unsigned int numverts = cloth->numverts;
	float dt = clmd->sim_parms->timescale / clmd->sim_parms->stepsPerFrame;
	Implicit_Data *id = cloth->implicit;
	int result = 0;
	
	if(clmd->sim_parms->flags & CLOTH_SIMSETTINGS_FLAG_GOAL) /* do goal stuff */
	{
		for(i = 0; i < numverts; i++)
		{			
			// update velocities with constrained velocities from pinned verts
			if(verts [i].flags & CLOTH_VERT_FLAG_PINNED)
			{			
				VECSUB(id->V[i], verts[i].xconst, verts[i].xold);
				// VecMulf(id->V[i], clmd->sim_parms->stepsPerFrame);
			}
		}	
	}
	
	while(step < tf)
	{	
		// calculate forces
		effectors= pdInitEffectors(ob,NULL);
		cloth_calc_force(clmd, id->F, id->X, id->V, id->dFdV, id->dFdX, effectors, step, id->M);
		if(effectors) pdEndEffectors(effectors);
		
		// calculate new velocity
		simulate_implicit_euler(id->Vnew, id->X, id->V, id->F, id->dFdV, id->dFdX, dt, id->A, id->B, id->dV, id->S, id->z, id->olddV, id->P, id->Pinv, id->M, id->bigI);
		
		// advance positions
		add_lfvector_lfvectorS(id->Xnew, id->X, id->Vnew, dt, numverts);
		
		/* move pinned verts to correct position */
		for(i = 0; i < numverts; i++)
		{	
			if(clmd->sim_parms->flags & CLOTH_SIMSETTINGS_FLAG_GOAL) 
			{			
				if(verts [i].flags & CLOTH_VERT_FLAG_PINNED)
				{			
					float tvect[3] = {.0,.0,.0};
					VECSUB(tvect, verts[i].xconst, verts[i].xold);
					mul_fvector_S(tvect, tvect, step+dt);
					VECADD(tvect, tvect, verts[i].xold);
					VECCOPY(id->Xnew[i], tvect);
				}	
			}
			
			VECCOPY(verts[i].txold, id->X[i]);
		}
		
		if(clmd->coll_parms->flags & CLOTH_COLLSETTINGS_FLAG_ENABLED)
		{
			// collisions 
			// itstart();
			
			// update verts to current positions
			for(i = 0; i < numverts; i++)
			{	
				VECCOPY(verts[i].tx, id->Xnew[i]);
				
				VECSUB(verts[i].tv, verts[i].tx, verts[i].txold);
				VECCOPY(verts[i].v, verts[i].tv);
			}
			
			// call collision function
			// TODO: check if "step" or "step+dt" is correct - dg
			result = cloth_bvh_objcollision(ob, clmd, step, dt);
			
			// correct velocity again, just to be sure we had to change it due to adaptive collisions
			for(i = 0; i < numverts; i++)
			{
				VECSUB(verts[i].tv, verts[i].tx, id->X[i]);
			}
			
			// copy corrected positions back to simulation
			for(i = 0; i < numverts; i++)
			{		
				if(result)
				{
					
					if((clmd->sim_parms->flags & CLOTH_SIMSETTINGS_FLAG_GOAL) && (verts [i].flags & CLOTH_VERT_FLAG_PINNED))
						continue;
					
					VECCOPY(id->Xnew[i], verts[i].tx);
					VECCOPY(id->Vnew[i], verts[i].tv);
					VecMulf(id->Vnew[i], clmd->sim_parms->stepsPerFrame);
				}
			}
			
			// X = Xnew;
			cp_lfvector(id->X, id->Xnew, numverts);
			
			// if there were collisions, advance the velocity from v_n+1/2 to v_n+1
			
			if(result)
			{
				// V = Vnew;
				cp_lfvector(id->V, id->Vnew, numverts);
				
				// calculate 
				effectors= pdInitEffectors(ob,NULL);
				cloth_calc_force(clmd, id->F, id->X, id->V, id->dFdV, id->dFdX, effectors, step+dt, id->M);	
				if(effectors) pdEndEffectors(effectors);
				
				simulate_implicit_euler(id->Vnew, id->X, id->V, id->F, id->dFdV, id->dFdX, dt / 2.0f, id->A, id->B, id->dV, id->S, id->z, id->olddV, id->P, id->Pinv, id->M, id->bigI);
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
		
	}

	for(i = 0; i < numverts; i++)
	{				
		if((clmd->sim_parms->flags & CLOTH_SIMSETTINGS_FLAG_GOAL) && (verts [i].flags & CLOTH_VERT_FLAG_PINNED))
		{
			VECCOPY(verts[i].txold, verts[i].xconst); // TODO: test --> should be .x 
			VECCOPY(verts[i].x, verts[i].xconst);
			VECCOPY(verts[i].v, id->V[i]);
		}
		else
		{
			VECCOPY(verts[i].txold, id->X[i]);
			VECCOPY(verts[i].x, id->X[i]);
			VECCOPY(verts[i].v, id->V[i]);
		}
	}
	
	return 1;
}

void implicit_set_positions (ClothModifierData *clmd)
{ 	 	
	Cloth *cloth = clmd->clothObject;
	ClothVertex *verts = cloth->verts;
	unsigned int numverts = cloth->numverts, i;
	Implicit_Data *id = cloth->implicit;
	
	for(i = 0; i < numverts; i++)
	{				
		VECCOPY(id->X[i], verts[i].x);
		VECCOPY(id->V[i], verts[i].v);
	}
	if(G.rt > 0)
		printf("implicit_set_positions\n");	
}

