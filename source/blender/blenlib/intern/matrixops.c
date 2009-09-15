/*
 *
 * Some matrix operations.
 *
 * Always use
 * - vector with x components :   float x[3], int x[3], etc
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
 */

/* ------------------------------------------------------------------------- */
#include <string.h>
#include "MTC_matrixops.h"
#include "MTC_vectorops.h"
/* ------------------------------------------------------------------------- */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#if defined(__sun__) || defined( __sun ) || defined (__sparc) || defined (__sparc__)
#include <strings.h>
#endif

#define ABS(x)	((x) < 0 ? -(x) : (x))
#define SWAP(type, a, b)	{ type sw_ap; sw_ap=(a); (a)=(b); (b)=sw_ap; }

void MTC_Mat4CpyMat4(float m1[][4], float m2[][4])
{
	memcpy(m1, m2, 4*4*sizeof(float));
}

/* ------------------------------------------------------------------------- */

void MTC_Mat4MulSerie(float answ[][4],
				  float m1[][4], float m2[][4], float m3[][4],
				  float m4[][4], float m5[][4], float m6[][4],
				  float m7[][4], float m8[][4])
{
	float temp[4][4];
	
	if(m1==0 || m2==0) return;
	
	MTC_Mat4MulMat4(answ, m2, m1);
	if(m3) {
		MTC_Mat4MulMat4(temp, m3, answ);
		if(m4) {
			MTC_Mat4MulMat4(answ, m4, temp);
			if(m5) {
				MTC_Mat4MulMat4(temp, m5, answ);
				if(m6) {
					MTC_Mat4MulMat4(answ, m6, temp);
					if(m7) {
						MTC_Mat4MulMat4(temp, m7, answ);
						if(m8) {
							MTC_Mat4MulMat4(answ, m8, temp);
						}
						else MTC_Mat4CpyMat4(answ, temp);
					}
				}
				else MTC_Mat4CpyMat4(answ, temp);
			}
		}
		else MTC_Mat4CpyMat4(answ, temp);
	}
}

/* ------------------------------------------------------------------------- */
void MTC_Mat4MulMat4(float m1[][4], float m2[][4], float m3[][4])
{
  /* matrix product: c[j][k] = a[j][i].b[i][k] */

	m1[0][0] = m2[0][0]*m3[0][0] + m2[0][1]*m3[1][0] + m2[0][2]*m3[2][0] + m2[0][3]*m3[3][0];
	m1[0][1] = m2[0][0]*m3[0][1] + m2[0][1]*m3[1][1] + m2[0][2]*m3[2][1] + m2[0][3]*m3[3][1];
	m1[0][2] = m2[0][0]*m3[0][2] + m2[0][1]*m3[1][2] + m2[0][2]*m3[2][2] + m2[0][3]*m3[3][2];
	m1[0][3] = m2[0][0]*m3[0][3] + m2[0][1]*m3[1][3] + m2[0][2]*m3[2][3] + m2[0][3]*m3[3][3];

	m1[1][0] = m2[1][0]*m3[0][0] + m2[1][1]*m3[1][0] + m2[1][2]*m3[2][0] + m2[1][3]*m3[3][0];
	m1[1][1] = m2[1][0]*m3[0][1] + m2[1][1]*m3[1][1] + m2[1][2]*m3[2][1] + m2[1][3]*m3[3][1];
	m1[1][2] = m2[1][0]*m3[0][2] + m2[1][1]*m3[1][2] + m2[1][2]*m3[2][2] + m2[1][3]*m3[3][2];
	m1[1][3] = m2[1][0]*m3[0][3] + m2[1][1]*m3[1][3] + m2[1][2]*m3[2][3] + m2[1][3]*m3[3][3];

	m1[2][0] = m2[2][0]*m3[0][0] + m2[2][1]*m3[1][0] + m2[2][2]*m3[2][0] + m2[2][3]*m3[3][0];
	m1[2][1] = m2[2][0]*m3[0][1] + m2[2][1]*m3[1][1] + m2[2][2]*m3[2][1] + m2[2][3]*m3[3][1];
	m1[2][2] = m2[2][0]*m3[0][2] + m2[2][1]*m3[1][2] + m2[2][2]*m3[2][2] + m2[2][3]*m3[3][2];
	m1[2][3] = m2[2][0]*m3[0][3] + m2[2][1]*m3[1][3] + m2[2][2]*m3[2][3] + m2[2][3]*m3[3][3];

	m1[3][0] = m2[3][0]*m3[0][0] + m2[3][1]*m3[1][0] + m2[3][2]*m3[2][0] + m2[3][3]*m3[3][0];
	m1[3][1] = m2[3][0]*m3[0][1] + m2[3][1]*m3[1][1] + m2[3][2]*m3[2][1] + m2[3][3]*m3[3][1];
	m1[3][2] = m2[3][0]*m3[0][2] + m2[3][1]*m3[1][2] + m2[3][2]*m3[2][2] + m2[3][3]*m3[3][2];
	m1[3][3] = m2[3][0]*m3[0][3] + m2[3][1]*m3[1][3] + m2[3][2]*m3[2][3] + m2[3][3]*m3[3][3];

}
/* ------------------------------------------------------------------------- */

void MTC_Mat4MulVecfl(float mat[][4], float *vec)
{
	float x,y;

	x=vec[0]; 
	y=vec[1];
	vec[0]=x*mat[0][0] + y*mat[1][0] + mat[2][0]*vec[2] + mat[3][0];
	vec[1]=x*mat[0][1] + y*mat[1][1] + mat[2][1]*vec[2] + mat[3][1];
	vec[2]=x*mat[0][2] + y*mat[1][2] + mat[2][2]*vec[2] + mat[3][2];
}

/* ------------------------------------------------------------------------- */
void MTC_Mat3MulVecfl(float mat[][3], float *vec)
{
	float x,y;

	x=vec[0]; 
	y=vec[1];
	vec[0]= x*mat[0][0] + y*mat[1][0] + mat[2][0]*vec[2];
	vec[1]= x*mat[0][1] + y*mat[1][1] + mat[2][1]*vec[2];
	vec[2]= x*mat[0][2] + y*mat[1][2] + mat[2][2]*vec[2];
}

/* ------------------------------------------------------------------------- */

int MTC_Mat4Invert(float inverse[][4], float mat[][4])
{
	int i, j, k;
	double temp;
	float tempmat[4][4];
	float max;
	int maxj;

	/* Set inverse to identity */
	for (i=0; i<4; i++)
		for (j=0; j<4; j++)
			inverse[i][j] = 0;
	for (i=0; i<4; i++)
		inverse[i][i] = 1;

	/* Copy original matrix so we don't mess it up */
	for(i = 0; i < 4; i++)
		for(j = 0; j <4; j++)
			tempmat[i][j] = mat[i][j];

	for(i = 0; i < 4; i++) {
		/* Look for row with max pivot */
		max = ABS(tempmat[i][i]);
		maxj = i;
		for(j = i + 1; j < 4; j++) {
			if(ABS(tempmat[j][i]) > max) {
				max = ABS(tempmat[j][i]);
				maxj = j;
			}
		}
		/* Swap rows if necessary */
		if (maxj != i) {
			for( k = 0; k < 4; k++) {
				SWAP(float, tempmat[i][k], tempmat[maxj][k]);
				SWAP(float, inverse[i][k], inverse[maxj][k]);
			}
		}

		temp = tempmat[i][i];
		if (temp == 0)
			return 0;  /* No non-zero pivot */
		for(k = 0; k < 4; k++) {
			tempmat[i][k] /= temp;
			inverse[i][k] /= temp;
		}
		for(j = 0; j < 4; j++) {
			if(j != i) {
				temp = tempmat[j][i];
				for(k = 0; k < 4; k++) {
					tempmat[j][k] -= tempmat[i][k]*temp;
					inverse[j][k] -= inverse[i][k]*temp;
				}
			}
		}
	}
	return 1;
}

/* ------------------------------------------------------------------------- */
void MTC_Mat3CpyMat4(float m1[][3], float m2[][4])
{
	
	m1[0][0]= m2[0][0];
	m1[0][1]= m2[0][1];
	m1[0][2]= m2[0][2];

	m1[1][0]= m2[1][0];
	m1[1][1]= m2[1][1];
	m1[1][2]= m2[1][2];

	m1[2][0]= m2[2][0];
	m1[2][1]= m2[2][1];
	m1[2][2]= m2[2][2];
}

/* ------------------------------------------------------------------------- */

void MTC_Mat3CpyMat3(float m1[][3], float m2[][3])
{	
	memcpy(m1, m2, 3*3*sizeof(float));
}

/* ------------------------------------------------------------------------- */
/*  void Mat3MulMat3(float m1[][3], float m3[][3], float m2[][3]) */
void MTC_Mat3MulMat3(float m1[][3], float m3[][3], float m2[][3])
{
	/* be careful about this rewrite... */
	    /* m1[i][j] = m2[i][k]*m3[k][j], args are flipped! */
	m1[0][0]= m2[0][0]*m3[0][0] + m2[0][1]*m3[1][0] + m2[0][2]*m3[2][0];
	m1[0][1]= m2[0][0]*m3[0][1] + m2[0][1]*m3[1][1] + m2[0][2]*m3[2][1];
	m1[0][2]= m2[0][0]*m3[0][2] + m2[0][1]*m3[1][2] + m2[0][2]*m3[2][2];

	m1[1][0]= m2[1][0]*m3[0][0] + m2[1][1]*m3[1][0] + m2[1][2]*m3[2][0];
	m1[1][1]= m2[1][0]*m3[0][1] + m2[1][1]*m3[1][1] + m2[1][2]*m3[2][1];
	m1[1][2]= m2[1][0]*m3[0][2] + m2[1][1]*m3[1][2] + m2[1][2]*m3[2][2];

	m1[2][0]= m2[2][0]*m3[0][0] + m2[2][1]*m3[1][0] + m2[2][2]*m3[2][0];
	m1[2][1]= m2[2][0]*m3[0][1] + m2[2][1]*m3[1][1] + m2[2][2]*m3[2][1];
	m1[2][2]= m2[2][0]*m3[0][2] + m2[2][1]*m3[1][2] + m2[2][2]*m3[2][2];

/*  	m1[0]= m2[0]*m3[0] + m2[1]*m3[3] + m2[2]*m3[6]; */
/*  	m1[1]= m2[0]*m3[1] + m2[1]*m3[4] + m2[2]*m3[7]; */
/*  	m1[2]= m2[0]*m3[2] + m2[1]*m3[5] + m2[2]*m3[8]; */
/*  	m1+=3; */
/*  	m2+=3; */
/*  	m1[0]= m2[0]*m3[0] + m2[1]*m3[3] + m2[2]*m3[6]; */
/*  	m1[1]= m2[0]*m3[1] + m2[1]*m3[4] + m2[2]*m3[7]; */
/*  	m1[2]= m2[0]*m3[2] + m2[1]*m3[5] + m2[2]*m3[8]; */
/*  	m1+=3; */
/*  	m2+=3; */
/*  	m1[0]= m2[0]*m3[0] + m2[1]*m3[3] + m2[2]*m3[6]; */
/*  	m1[1]= m2[0]*m3[1] + m2[1]*m3[4] + m2[2]*m3[7]; */
/*  	m1[2]= m2[0]*m3[2] + m2[1]*m3[5] + m2[2]*m3[8]; */
} /* end of void Mat3MulMat3(float m1[][3], float m3[][3], float m2[][3]) */

/* ------------------------------------------------------------------------- */

void MTC_Mat4Ortho(float mat[][4])
{
	float len;
	
	len= MTC_normalize3DF(mat[0]);
	if(len!=0.0) mat[0][3]/= len;
	len= MTC_normalize3DF(mat[1]);
	if(len!=0.0) mat[1][3]/= len;
	len= MTC_normalize3DF(mat[2]);
	if(len!=0.0) mat[2][3]/= len;
}

/* ------------------------------------------------------------------------- */

void MTC_Mat4Mul3Vecfl(float mat[][4], float *vec)
{
	float x,y;
	/* vec = mat^T dot vec !!! or vec a row, then vec = vec dot mat*/

	x= vec[0]; 
	y= vec[1];
	vec[0]= x*mat[0][0] + y*mat[1][0] + mat[2][0]*vec[2];
	vec[1]= x*mat[0][1] + y*mat[1][1] + mat[2][1]*vec[2];
	vec[2]= x*mat[0][2] + y*mat[1][2] + mat[2][2]*vec[2];
}

/* ------------------------------------------------------------------------- */

void MTC_Mat4One(float m[][4])
{

	m[0][0]= m[1][1]= m[2][2]= m[3][3]= 1.0;
	m[0][1]= m[0][2]= m[0][3]= 0.0;
	m[1][0]= m[1][2]= m[1][3]= 0.0;
	m[2][0]= m[2][1]= m[2][3]= 0.0;
	m[3][0]= m[3][1]= m[3][2]= 0.0;
}


/* ------------------------------------------------------------------------- */
/* Result is a 3-vector!*/
void MTC_Mat3MulVecd(float mat[][3], double *vec)
{
	double x,y;

	/* vec = mat^T dot vec !!! or vec a row, then vec = vec dot mat*/
	x=vec[0]; 
	y=vec[1];
	vec[0]= x * mat[0][0] + y * mat[1][0] + mat[2][0] * vec[2];
	vec[1]= x * mat[0][1] + y * mat[1][1] + mat[2][1] * vec[2];
	vec[2]= x * mat[0][2] + y * mat[1][2] + mat[2][2] * vec[2];
}

/* ------------------------------------------------------------------------- */

void MTC_Mat3Inv(float m1[][3], float m2[][3])
{
	short a,b;
	float det;

	/* first adjoint */
	MTC_Mat3Adj(m1,m2);

	/* then determinant old mat! */
	det= m2[0][0]* (m2[1][1]*m2[2][2] - m2[1][2]*m2[2][1])
	    -m2[1][0]* (m2[0][1]*m2[2][2] - m2[0][2]*m2[2][1])
	    +m2[2][0]* (m2[0][1]*m2[1][2] - m2[0][2]*m2[1][1]);

	if(det==0) det=1;
	det= 1/det;
	for(a=0;a<3;a++) {
		for(b=0;b<3;b++) {
			m1[a][b]*=det;
		}
	}
}

/* ------------------------------------------------------------------------- */

void MTC_Mat3Adj(float m1[][3], float m[][3])
{
	m1[0][0]=m[1][1]*m[2][2]-m[1][2]*m[2][1];
	m1[0][1]= -m[0][1]*m[2][2]+m[0][2]*m[2][1];
	m1[0][2]=m[0][1]*m[1][2]-m[0][2]*m[1][1];

	m1[1][0]= -m[1][0]*m[2][2]+m[1][2]*m[2][0];
	m1[1][1]=m[0][0]*m[2][2]-m[0][2]*m[2][0];
	m1[1][2]= -m[0][0]*m[1][2]+m[0][2]*m[1][0];

	m1[2][0]=m[1][0]*m[2][1]-m[1][1]*m[2][0];
	m1[2][1]= -m[0][0]*m[2][1]+m[0][1]*m[2][0];
	m1[2][2]=m[0][0]*m[1][1]-m[0][1]*m[1][0];
}

/* ------------------------------------------------------------------------- */

void MTC_Mat3One(float m[][3])
{

	m[0][0]= m[1][1]= m[2][2]= 1.0;
	m[0][1]= m[0][2]= 0.0;
	m[1][0]= m[1][2]= 0.0;
	m[2][0]= m[2][1]= 0.0;
}

/* ------------------------------------------------------------------------- */

void MTC_Mat4SwapMat4(float m1[][4], float m2[][4])
{
	float t;
	int i, j;

	for(i = 0; i < 4; i++) {
		for (j = 0; j < 4; j++) {
			t        = m1[i][j];
			m1[i][j] = m2[i][j];
			m2[i][j] = t;
		}
	}
}

/* ------------------------------------------------------------------------- */

void MTC_Mat4MulVec4fl(float mat[][4], float *vec)
{
	float x,y,z;

	x = vec[0]; 
	y = vec[1]; 
	z = vec[2];
	vec[0] = x*mat[0][0] + y*mat[1][0] + z*mat[2][0] + mat[3][0]*vec[3];
	vec[1] = x*mat[0][1] + y*mat[1][1] + z*mat[2][1] + mat[3][1]*vec[3];
	vec[2] = x*mat[0][2] + y*mat[1][2] + z*mat[2][2] + mat[3][2]*vec[3];
	vec[3] = x*mat[0][3] + y*mat[1][3] + z*mat[2][3] + mat[3][3]*vec[3];
}

/* ------------------------------------------------------------------------- */

void MTC_Mat4CpyMat3nc(float m1[][4], float m2[][3])	/* no clear */
{
	m1[0][0]= m2[0][0];
	m1[0][1]= m2[0][1];
	m1[0][2]= m2[0][2];

	m1[1][0]= m2[1][0];
	m1[1][1]= m2[1][1];
	m1[1][2]= m2[1][2];

	m1[2][0]= m2[2][0];
	m1[2][1]= m2[2][1];
	m1[2][2]= m2[2][2];
}

/* ------------------------------------------------------------------------- */

void MTC_Mat4MulMat33(float m1[][3], float m2[][4], float m3[][3])
{
	/* m1_i_j = m2_i_k * m3_k_j ? */
	
	m1[0][0] = m2[0][0]*m3[0][0] + m2[0][1]*m3[1][0] + m2[0][2]*m3[2][0];
	m1[0][1] = m2[0][0]*m3[0][1] + m2[0][1]*m3[1][1] + m2[0][2]*m3[2][1];
	m1[0][2] = m2[0][0]*m3[0][2] + m2[0][1]*m3[1][2] + m2[0][2]*m3[2][2];

	m1[1][0] = m2[1][0]*m3[0][0] + m2[1][1]*m3[1][0] + m2[1][2]*m3[2][0];
	m1[1][1] = m2[1][0]*m3[0][1] + m2[1][1]*m3[1][1] + m2[1][2]*m3[2][1];
	m1[1][2] = m2[1][0]*m3[0][2] + m2[1][1]*m3[1][2] + m2[1][2]*m3[2][2];

	m1[2][0] = m2[2][0]*m3[0][0] + m2[2][1]*m3[1][0] + m2[2][2]*m3[2][0];
	m1[2][1] = m2[2][0]*m3[0][1] + m2[2][1]*m3[1][1] + m2[2][2]*m3[2][1];
	m1[2][2] = m2[2][0]*m3[0][2] + m2[2][1]*m3[1][2] + m2[2][2]*m3[2][2];

}

/* ------------------------------------------------------------------------- */

/* eof */
