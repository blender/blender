/*
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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: some of this file.
 *
 * ***** END GPL LICENSE BLOCK *****
 */


#include "BLI_math.h"

/********************************* Init **************************************/

void zero_m3(float m[3][3])
{
	memset(m, 0, 3*3*sizeof(float));
}

void zero_m4(float m[4][4])
{
	memset(m, 0, 4*4*sizeof(float));
}

void unit_m3(float m[][3])
{
	m[0][0]= m[1][1]= m[2][2]= 1.0;
	m[0][1]= m[0][2]= 0.0;
	m[1][0]= m[1][2]= 0.0;
	m[2][0]= m[2][1]= 0.0;
}

void unit_m4(float m[][4])
{
	m[0][0]= m[1][1]= m[2][2]= m[3][3]= 1.0;
	m[0][1]= m[0][2]= m[0][3]= 0.0;
	m[1][0]= m[1][2]= m[1][3]= 0.0;
	m[2][0]= m[2][1]= m[2][3]= 0.0;
	m[3][0]= m[3][1]= m[3][2]= 0.0;
}

void copy_m3_m3(float m1[][3], float m2[][3]) 
{	
	/* destination comes first: */
	memcpy(&m1[0], &m2[0], 9*sizeof(float));
}

void copy_m4_m4(float m1[][4], float m2[][4]) 
{
	memcpy(m1, m2, 4*4*sizeof(float));
}

void copy_m3_m4(float m1[][3], float m2[][4])
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

void copy_m4_m3(float m1[][4], float m2[][3])	/* no clear */
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

	/*	Reevan's Bugfix */
	m1[0][3]=0.0F;
	m1[1][3]=0.0F;
	m1[2][3]=0.0F;

	m1[3][0]=0.0F;	
	m1[3][1]=0.0F;	
	m1[3][2]=0.0F;	
	m1[3][3]=1.0F;

}

void swap_m4m4(float m1[][4], float m2[][4])
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

/******************************** Arithmetic *********************************/

void mul_m4_m4m4(float m1[][4], float m2[][4], float m3[][4])
{
  /* matrix product: m1[j][k] = m2[j][i].m3[i][k] */

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

void mul_m3_m3m3(float m1[][3], float m3[][3], float m2[][3])
{
   /*  m1[i][j] = m2[i][k]*m3[k][j], args are flipped!  */
  	m1[0][0]= m2[0][0]*m3[0][0] + m2[0][1]*m3[1][0] + m2[0][2]*m3[2][0]; 
  	m1[0][1]= m2[0][0]*m3[0][1] + m2[0][1]*m3[1][1] + m2[0][2]*m3[2][1]; 
  	m1[0][2]= m2[0][0]*m3[0][2] + m2[0][1]*m3[1][2] + m2[0][2]*m3[2][2]; 

  	m1[1][0]= m2[1][0]*m3[0][0] + m2[1][1]*m3[1][0] + m2[1][2]*m3[2][0]; 
  	m1[1][1]= m2[1][0]*m3[0][1] + m2[1][1]*m3[1][1] + m2[1][2]*m3[2][1]; 
  	m1[1][2]= m2[1][0]*m3[0][2] + m2[1][1]*m3[1][2] + m2[1][2]*m3[2][2]; 

  	m1[2][0]= m2[2][0]*m3[0][0] + m2[2][1]*m3[1][0] + m2[2][2]*m3[2][0]; 
  	m1[2][1]= m2[2][0]*m3[0][1] + m2[2][1]*m3[1][1] + m2[2][2]*m3[2][1]; 
  	m1[2][2]= m2[2][0]*m3[0][2] + m2[2][1]*m3[1][2] + m2[2][2]*m3[2][2]; 
}

void mul_m4_m4m3(float (*m1)[4], float (*m3)[4], float (*m2)[3])
{
	m1[0][0]= m2[0][0]*m3[0][0] + m2[0][1]*m3[1][0] + m2[0][2]*m3[2][0];
	m1[0][1]= m2[0][0]*m3[0][1] + m2[0][1]*m3[1][1] + m2[0][2]*m3[2][1];
	m1[0][2]= m2[0][0]*m3[0][2] + m2[0][1]*m3[1][2] + m2[0][2]*m3[2][2];
	m1[1][0]= m2[1][0]*m3[0][0] + m2[1][1]*m3[1][0] + m2[1][2]*m3[2][0];
	m1[1][1]= m2[1][0]*m3[0][1] + m2[1][1]*m3[1][1] + m2[1][2]*m3[2][1];
	m1[1][2]= m2[1][0]*m3[0][2] + m2[1][1]*m3[1][2] + m2[1][2]*m3[2][2];
	m1[2][0]= m2[2][0]*m3[0][0] + m2[2][1]*m3[1][0] + m2[2][2]*m3[2][0];
	m1[2][1]= m2[2][0]*m3[0][1] + m2[2][1]*m3[1][1] + m2[2][2]*m3[2][1];
	m1[2][2]= m2[2][0]*m3[0][2] + m2[2][1]*m3[1][2] + m2[2][2]*m3[2][2];
}

/* m1 = m2 * m3, ignore the elements on the 4th row/column of m3*/
void mul_m3_m3m4(float m1[][3], float m2[][3], float m3[][4])
{
    /* m1[i][j] = m2[i][k] * m3[k][j] */
    m1[0][0] = m2[0][0] * m3[0][0] + m2[0][1] * m3[1][0] +m2[0][2] * m3[2][0];
    m1[0][1] = m2[0][0] * m3[0][1] + m2[0][1] * m3[1][1] +m2[0][2] * m3[2][1];
    m1[0][2] = m2[0][0] * m3[0][2] + m2[0][1] * m3[1][2] +m2[0][2] * m3[2][2];

    m1[1][0] = m2[1][0] * m3[0][0] + m2[1][1] * m3[1][0] +m2[1][2] * m3[2][0];
    m1[1][1] = m2[1][0] * m3[0][1] + m2[1][1] * m3[1][1] +m2[1][2] * m3[2][1];
    m1[1][2] = m2[1][0] * m3[0][2] + m2[1][1] * m3[1][2] +m2[1][2] * m3[2][2];

    m1[2][0] = m2[2][0] * m3[0][0] + m2[2][1] * m3[1][0] +m2[2][2] * m3[2][0];
    m1[2][1] = m2[2][0] * m3[0][1] + m2[2][1] * m3[1][1] +m2[2][2] * m3[2][1];
    m1[2][2] = m2[2][0] * m3[0][2] + m2[2][1] * m3[1][2] +m2[2][2] * m3[2][2];
}

void mul_m4_m3m4(float (*m1)[4], float (*m3)[3], float (*m2)[4])
{
	m1[0][0]= m2[0][0]*m3[0][0] + m2[0][1]*m3[1][0] + m2[0][2]*m3[2][0];
	m1[0][1]= m2[0][0]*m3[0][1] + m2[0][1]*m3[1][1] + m2[0][2]*m3[2][1];
	m1[0][2]= m2[0][0]*m3[0][2] + m2[0][1]*m3[1][2] + m2[0][2]*m3[2][2];
	m1[1][0]= m2[1][0]*m3[0][0] + m2[1][1]*m3[1][0] + m2[1][2]*m3[2][0];
	m1[1][1]= m2[1][0]*m3[0][1] + m2[1][1]*m3[1][1] + m2[1][2]*m3[2][1];
	m1[1][2]= m2[1][0]*m3[0][2] + m2[1][1]*m3[1][2] + m2[1][2]*m3[2][2];
	m1[2][0]= m2[2][0]*m3[0][0] + m2[2][1]*m3[1][0] + m2[2][2]*m3[2][0];
	m1[2][1]= m2[2][0]*m3[0][1] + m2[2][1]*m3[1][1] + m2[2][2]*m3[2][1];
	m1[2][2]= m2[2][0]*m3[0][2] + m2[2][1]*m3[1][2] + m2[2][2]*m3[2][2];
}

void mul_serie_m3(float answ[][3],
				   float m1[][3], float m2[][3], float m3[][3],
				   float m4[][3], float m5[][3], float m6[][3],
				   float m7[][3], float m8[][3])
{
	float temp[3][3];
	
	if(m1==0 || m2==0) return;
	
	mul_m3_m3m3(answ, m2, m1);
	if(m3) {
		mul_m3_m3m3(temp, m3, answ);
		if(m4) {
			mul_m3_m3m3(answ, m4, temp);
			if(m5) {
				mul_m3_m3m3(temp, m5, answ);
				if(m6) {
					mul_m3_m3m3(answ, m6, temp);
					if(m7) {
						mul_m3_m3m3(temp, m7, answ);
						if(m8) {
							mul_m3_m3m3(answ, m8, temp);
						}
						else copy_m3_m3(answ, temp);
					}
				}
				else copy_m3_m3(answ, temp);
			}
		}
		else copy_m3_m3(answ, temp);
	}
}

void mul_serie_m4(float answ[][4], float m1[][4],
				float m2[][4], float m3[][4], float m4[][4],
				float m5[][4], float m6[][4], float m7[][4],
				float m8[][4])
{
	float temp[4][4];
	
	if(m1==0 || m2==0) return;
	
	mul_m4_m4m4(answ, m2, m1);
	if(m3) {
		mul_m4_m4m4(temp, m3, answ);
		if(m4) {
			mul_m4_m4m4(answ, m4, temp);
			if(m5) {
				mul_m4_m4m4(temp, m5, answ);
				if(m6) {
					mul_m4_m4m4(answ, m6, temp);
					if(m7) {
						mul_m4_m4m4(temp, m7, answ);
						if(m8) {
							mul_m4_m4m4(answ, m8, temp);
						}
						else copy_m4_m4(answ, temp);
					}
				}
				else copy_m4_m4(answ, temp);
			}
		}
		else copy_m4_m4(answ, temp);
	}
}

void mul_m4_v3(float mat[][4], float *vec)
{
	float x,y;

	x=vec[0]; 
	y=vec[1];
	vec[0]=x*mat[0][0] + y*mat[1][0] + mat[2][0]*vec[2] + mat[3][0];
	vec[1]=x*mat[0][1] + y*mat[1][1] + mat[2][1]*vec[2] + mat[3][1];
	vec[2]=x*mat[0][2] + y*mat[1][2] + mat[2][2]*vec[2] + mat[3][2];
}

void mul_v3_m4v3(float *in, float mat[][4], float *vec)
{
	float x,y;

	x=vec[0]; 
	y=vec[1];
	in[0]= x*mat[0][0] + y*mat[1][0] + mat[2][0]*vec[2] + mat[3][0];
	in[1]= x*mat[0][1] + y*mat[1][1] + mat[2][1]*vec[2] + mat[3][1];
	in[2]= x*mat[0][2] + y*mat[1][2] + mat[2][2]*vec[2] + mat[3][2];
}

void mul_mat3_m4_v3(float mat[][4], float *vec)
{
	float x,y;

	x= vec[0]; 
	y= vec[1];
	vec[0]= x*mat[0][0] + y*mat[1][0] + mat[2][0]*vec[2];
	vec[1]= x*mat[0][1] + y*mat[1][1] + mat[2][1]*vec[2];
	vec[2]= x*mat[0][2] + y*mat[1][2] + mat[2][2]*vec[2];
}

void mul_project_m4_v4(float mat[][4], float *vec)
{
	float w;

	w = vec[0]*mat[0][3] + vec[1]*mat[1][3] + vec[2]*mat[2][3] + mat[3][3];
	mul_m4_v3(mat, vec);

	vec[0] /= w;
	vec[1] /= w;
	vec[2] /= w;
}

void mul_m4_v4(float mat[][4], float *vec)
{
	float x,y,z;

	x=vec[0]; 
	y=vec[1]; 
	z= vec[2];
	vec[0]=x*mat[0][0] + y*mat[1][0] + z*mat[2][0] + mat[3][0]*vec[3];
	vec[1]=x*mat[0][1] + y*mat[1][1] + z*mat[2][1] + mat[3][1]*vec[3];
	vec[2]=x*mat[0][2] + y*mat[1][2] + z*mat[2][2] + mat[3][2]*vec[3];
	vec[3]=x*mat[0][3] + y*mat[1][3] + z*mat[2][3] + mat[3][3]*vec[3];
}

void mul_v3_m3v3(float r[3], float M[3][3], float a[3])
{
	r[0]= M[0][0]*a[0] + M[1][0]*a[1] + M[2][0]*a[2];
	r[1]= M[0][1]*a[0] + M[1][1]*a[1] + M[2][1]*a[2];
	r[2]= M[0][2]*a[0] + M[1][2]*a[1] + M[2][2]*a[2];
}

void mul_m3_v3(float M[3][3], float r[3])
{
	float tmp[3];

	mul_v3_m3v3(tmp, M, r);
	copy_v3_v3(r, tmp);
}

void mul_transposed_m3_v3(float mat[][3], float *vec)
{
	float x,y;

	x=vec[0]; 
	y=vec[1];
	vec[0]= x*mat[0][0] + y*mat[0][1] + mat[0][2]*vec[2];
	vec[1]= x*mat[1][0] + y*mat[1][1] + mat[1][2]*vec[2];
	vec[2]= x*mat[2][0] + y*mat[2][1] + mat[2][2]*vec[2];
}

void mul_m3_fl(float m[3][3], float f)
{
	int i, j;

	for(i=0;i<3;i++)
		for(j=0;j<3;j++)
			m[i][j] *= f;
}

void mul_m4_fl(float m[4][4], float f)
{
	int i, j;

	for(i=0;i<4;i++)
		for(j=0;j<4;j++)
			m[i][j] *= f;
}

void mul_mat3_m4_fl(float m[4][4], float f)
{
	int i, j;

	for(i=0; i<3; i++)
		for(j=0; j<3; j++)
			m[i][j] *= f;
}

void mul_m3_v3_double(float mat[][3], double *vec)
{
	double x,y;

	x=vec[0]; 
	y=vec[1];
	vec[0]= x*mat[0][0] + y*mat[1][0] + mat[2][0]*vec[2];
	vec[1]= x*mat[0][1] + y*mat[1][1] + mat[2][1]*vec[2];
	vec[2]= x*mat[0][2] + y*mat[1][2] + mat[2][2]*vec[2];
}

void add_m3_m3m3(float m1[][3], float m2[][3], float m3[][3])
{
	int i, j;

	for(i=0;i<3;i++)
		for(j=0;j<3;j++)
			m1[i][j]= m2[i][j] + m3[i][j];
}

void add_m4_m4m4(float m1[][4], float m2[][4], float m3[][4])
{
	int i, j;

	for(i=0;i<4;i++)
		for(j=0;j<4;j++)
			m1[i][j]= m2[i][j] + m3[i][j];
}

int invert_m3(float m[3][3])
{
	float tmp[3][3];
	int success;

	success= invert_m3_m3(tmp, m);
	copy_m3_m3(m, tmp);

	return success;
}

int invert_m3_m3(float m1[3][3], float m2[3][3])
{
	float det;
	int a, b, success;

	/* calc adjoint */
	adjoint_m3_m3(m1,m2);

	/* then determinant old matrix! */
	det= m2[0][0]* (m2[1][1]*m2[2][2] - m2[1][2]*m2[2][1])
	    -m2[1][0]* (m2[0][1]*m2[2][2] - m2[0][2]*m2[2][1])
	    +m2[2][0]* (m2[0][1]*m2[1][2] - m2[0][2]*m2[1][1]);

	success= (det != 0);

	if(det==0) det=1;
	det= 1/det;
	for(a=0;a<3;a++) {
		for(b=0;b<3;b++) {
			m1[a][b]*=det;
		}
	}

	return success;
}

int invert_m4(float m[4][4])
{
	float tmp[4][4];
	int success;

	success= invert_m4_m4(tmp, m);
	copy_m4_m4(m, tmp);

	return success;
}

/*
 * invertmat - 
 * 		computes the inverse of mat and puts it in inverse.  Returns 
 *	TRUE on success (i.e. can always find a pivot) and FALSE on failure.
 * 	Uses Gaussian Elimination with partial (maximal column) pivoting.
 *
 *					Mark Segal - 1992
 */

int invert_m4_m4(float inverse[4][4], float mat[4][4])
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
		max = fabs(tempmat[i][i]);
		maxj = i;
		for(j = i + 1; j < 4; j++) {
			if(fabs(tempmat[j][i]) > max) {
				max = fabs(tempmat[j][i]);
				maxj = j;
			}
		}
		/* Swap rows if necessary */
		if (maxj != i) {
			for(k = 0; k < 4; k++) {
				SWAP(float, tempmat[i][k], tempmat[maxj][k]);
				SWAP(float, inverse[i][k], inverse[maxj][k]);
			}
		}

		temp = tempmat[i][i];
		if (temp == 0)
			return 0;  /* No non-zero pivot */
		for(k = 0; k < 4; k++) {
			tempmat[i][k] = (float)(tempmat[i][k]/temp);
			inverse[i][k] = (float)(inverse[i][k]/temp);
		}
		for(j = 0; j < 4; j++) {
			if(j != i) {
				temp = tempmat[j][i];
				for(k = 0; k < 4; k++) {
					tempmat[j][k] -= (float)(tempmat[i][k]*temp);
					inverse[j][k] -= (float)(inverse[i][k]*temp);
				}
			}
		}
	}
	return 1;
}

/****************************** Linear Algebra *******************************/

void transpose_m3(float mat[][3])
{
	float t;

	t = mat[0][1] ; 
	mat[0][1] = mat[1][0] ; 
	mat[1][0] = t;
	t = mat[0][2] ; 
	mat[0][2] = mat[2][0] ; 
	mat[2][0] = t;
	t = mat[1][2] ; 
	mat[1][2] = mat[2][1] ; 
	mat[2][1] = t;
}

void transpose_m4(float mat[][4])
{
	float t;

	t = mat[0][1] ; 
	mat[0][1] = mat[1][0] ; 
	mat[1][0] = t;
	t = mat[0][2] ; 
	mat[0][2] = mat[2][0] ; 
	mat[2][0] = t;
	t = mat[0][3] ; 
	mat[0][3] = mat[3][0] ; 
	mat[3][0] = t;

	t = mat[1][2] ; 
	mat[1][2] = mat[2][1] ; 
	mat[2][1] = t;
	t = mat[1][3] ; 
	mat[1][3] = mat[3][1] ; 
	mat[3][1] = t;

	t = mat[2][3] ; 
	mat[2][3] = mat[3][2] ; 
	mat[3][2] = t;
}

void orthogonalize_m3(float mat[][3], int axis)
{
	float size[3];
	size[0] = len_v3(mat[0]);
	size[1] = len_v3(mat[1]);
	size[2] = len_v3(mat[2]);
	normalize_v3(mat[axis]);
	switch(axis)
	{
		case 0:
			if (dot_v3v3(mat[0], mat[1]) < 1) {
				cross_v3_v3v3(mat[2], mat[0], mat[1]);
				normalize_v3(mat[2]);
				cross_v3_v3v3(mat[1], mat[2], mat[0]);
			} else if (dot_v3v3(mat[0], mat[2]) < 1) {
				cross_v3_v3v3(mat[1], mat[2], mat[0]);
				normalize_v3(mat[1]);
				cross_v3_v3v3(mat[2], mat[0], mat[1]);
			} else {
				float vec[3] = {mat[0][1], mat[0][2], mat[0][0]};

				cross_v3_v3v3(mat[2], mat[0], vec);
				normalize_v3(mat[2]);
				cross_v3_v3v3(mat[1], mat[2], mat[0]);
			}
		case 1:
			if (dot_v3v3(mat[1], mat[0]) < 1) {
				cross_v3_v3v3(mat[2], mat[0], mat[1]);
				normalize_v3(mat[2]);
				cross_v3_v3v3(mat[0], mat[1], mat[2]);
			} else if (dot_v3v3(mat[0], mat[2]) < 1) {
				cross_v3_v3v3(mat[0], mat[1], mat[2]);
				normalize_v3(mat[0]);
				cross_v3_v3v3(mat[2], mat[0], mat[1]);
			} else {
				float vec[3] = {mat[1][1], mat[1][2], mat[1][0]};

				cross_v3_v3v3(mat[0], mat[1], vec);
				normalize_v3(mat[0]);
				cross_v3_v3v3(mat[2], mat[0], mat[1]);
			}
		case 2:
			if (dot_v3v3(mat[2], mat[0]) < 1) {
				cross_v3_v3v3(mat[1], mat[2], mat[0]);
				normalize_v3(mat[1]);
				cross_v3_v3v3(mat[0], mat[1], mat[2]);
			} else if (dot_v3v3(mat[2], mat[1]) < 1) {
				cross_v3_v3v3(mat[0], mat[1], mat[2]);
				normalize_v3(mat[0]);
				cross_v3_v3v3(mat[1], mat[2], mat[0]);
			} else {
				float vec[3] = {mat[2][1], mat[2][2], mat[2][0]};

				cross_v3_v3v3(mat[0], vec, mat[2]);
				normalize_v3(mat[0]);
				cross_v3_v3v3(mat[1], mat[2], mat[0]);
			}
	}
	mul_v3_fl(mat[0], size[0]);
	mul_v3_fl(mat[1], size[1]);
	mul_v3_fl(mat[2], size[2]);
}

void orthogonalize_m4(float mat[][4], int axis)
{
	float size[3];
	size[0] = len_v3(mat[0]);
	size[1] = len_v3(mat[1]);
	size[2] = len_v3(mat[2]);
	normalize_v3(mat[axis]);
	switch(axis)
	{
		case 0:
			if (dot_v3v3(mat[0], mat[1]) < 1) {
				cross_v3_v3v3(mat[2], mat[0], mat[1]);
				normalize_v3(mat[2]);
				cross_v3_v3v3(mat[1], mat[2], mat[0]);
			} else if (dot_v3v3(mat[0], mat[2]) < 1) {
				cross_v3_v3v3(mat[1], mat[2], mat[0]);
				normalize_v3(mat[1]);
				cross_v3_v3v3(mat[2], mat[0], mat[1]);
			} else {
				float vec[3] = {mat[0][1], mat[0][2], mat[0][0]};

				cross_v3_v3v3(mat[2], mat[0], vec);
				normalize_v3(mat[2]);
				cross_v3_v3v3(mat[1], mat[2], mat[0]);
			}
		case 1:
			normalize_v3(mat[0]);
			if (dot_v3v3(mat[1], mat[0]) < 1) {
				cross_v3_v3v3(mat[2], mat[0], mat[1]);
				normalize_v3(mat[2]);
				cross_v3_v3v3(mat[0], mat[1], mat[2]);
			} else if (dot_v3v3(mat[0], mat[2]) < 1) {
				cross_v3_v3v3(mat[0], mat[1], mat[2]);
				normalize_v3(mat[0]);
				cross_v3_v3v3(mat[2], mat[0], mat[1]);
			} else {
				float vec[3] = {mat[1][1], mat[1][2], mat[1][0]};

				cross_v3_v3v3(mat[0], mat[1], vec);
				normalize_v3(mat[0]);
				cross_v3_v3v3(mat[2], mat[0], mat[1]);
			}
		case 2:
			if (dot_v3v3(mat[2], mat[0]) < 1) {
				cross_v3_v3v3(mat[1], mat[2], mat[0]);
				normalize_v3(mat[1]);
				cross_v3_v3v3(mat[0], mat[1], mat[2]);
			} else if (dot_v3v3(mat[2], mat[1]) < 1) {
				cross_v3_v3v3(mat[0], mat[1], mat[2]);
				normalize_v3(mat[0]);
				cross_v3_v3v3(mat[1], mat[2], mat[0]);
			} else {
				float vec[3] = {mat[2][1], mat[2][2], mat[2][0]};

				cross_v3_v3v3(mat[0], vec, mat[2]);
				normalize_v3(mat[0]);
				cross_v3_v3v3(mat[1], mat[2], mat[0]);
			}
	}
	mul_v3_fl(mat[0], size[0]);
	mul_v3_fl(mat[1], size[1]);
	mul_v3_fl(mat[2], size[2]);
}

int is_orthogonal_m3(float mat[][3])
{
	if (fabs(dot_v3v3(mat[0], mat[1])) > 1.5 * FLT_EPSILON)
		return 0;

	if (fabs(dot_v3v3(mat[1], mat[2])) > 1.5 * FLT_EPSILON)
		return 0;

	if (fabs(dot_v3v3(mat[0], mat[2])) > 1.5 * FLT_EPSILON)
		return 0;
	
	return 1;
}

int is_orthogonal_m4(float mat[][4])
{
	if (fabs(dot_v3v3(mat[0], mat[1])) > 1.5 * FLT_EPSILON)
		return 0;

	if (fabs(dot_v3v3(mat[1], mat[2])) > 1.5 * FLT_EPSILON)
		return 0;

	if (fabs(dot_v3v3(mat[0], mat[2])) > 1.5 * FLT_EPSILON)
		return 0;
	
	return 1;
}

void normalize_m3(float mat[][3])
{	
	normalize_v3(mat[0]);
	normalize_v3(mat[1]);
	normalize_v3(mat[2]);
}

void normalize_m4(float mat[][4])
{
	float len;
	
	len= normalize_v3(mat[0]);
	if(len!=0.0) mat[0][3]/= len;
	len= normalize_v3(mat[1]);
	if(len!=0.0) mat[1][3]/= len;
	len= normalize_v3(mat[2]);
	if(len!=0.0) mat[2][3]/= len;
}

void adjoint_m3_m3(float m1[][3], float m[][3])
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

void adjoint_m4_m4(float out[][4], float in[][4])	/* out = ADJ(in) */
{
	float a1, a2, a3, a4, b1, b2, b3, b4;
	float c1, c2, c3, c4, d1, d2, d3, d4;

	a1= in[0][0]; 
	b1= in[0][1];
	c1= in[0][2]; 
	d1= in[0][3];

	a2= in[1][0]; 
	b2= in[1][1];
	c2= in[1][2]; 
	d2= in[1][3];

	a3= in[2][0]; 
	b3= in[2][1];
	c3= in[2][2]; 
	d3= in[2][3];

	a4= in[3][0]; 
	b4= in[3][1];
	c4= in[3][2]; 
	d4= in[3][3];


	out[0][0]  =   determinant_m3(b2, b3, b4, c2, c3, c4, d2, d3, d4);
	out[1][0]  = - determinant_m3(a2, a3, a4, c2, c3, c4, d2, d3, d4);
	out[2][0]  =   determinant_m3(a2, a3, a4, b2, b3, b4, d2, d3, d4);
	out[3][0]  = - determinant_m3(a2, a3, a4, b2, b3, b4, c2, c3, c4);

	out[0][1]  = - determinant_m3(b1, b3, b4, c1, c3, c4, d1, d3, d4);
	out[1][1]  =   determinant_m3(a1, a3, a4, c1, c3, c4, d1, d3, d4);
	out[2][1]  = - determinant_m3(a1, a3, a4, b1, b3, b4, d1, d3, d4);
	out[3][1]  =   determinant_m3(a1, a3, a4, b1, b3, b4, c1, c3, c4);

	out[0][2]  =   determinant_m3(b1, b2, b4, c1, c2, c4, d1, d2, d4);
	out[1][2]  = - determinant_m3(a1, a2, a4, c1, c2, c4, d1, d2, d4);
	out[2][2]  =   determinant_m3(a1, a2, a4, b1, b2, b4, d1, d2, d4);
	out[3][2]  = - determinant_m3(a1, a2, a4, b1, b2, b4, c1, c2, c4);

	out[0][3]  = - determinant_m3(b1, b2, b3, c1, c2, c3, d1, d2, d3);
	out[1][3]  =   determinant_m3(a1, a2, a3, c1, c2, c3, d1, d2, d3);
	out[2][3]  = - determinant_m3(a1, a2, a3, b1, b2, b3, d1, d2, d3);
	out[3][3]  =   determinant_m3(a1, a2, a3, b1, b2, b3, c1, c2, c3);
}

float determinant_m2(float a,float b,float c,float d)
{

	return a*d - b*c;
}

float determinant_m3(float a1, float a2, float a3,
			 float b1, float b2, float b3,
			 float c1, float c2, float c3)
{
	float ans;

	ans = a1 * determinant_m2(b2, b3, c2, c3)
	    - b1 * determinant_m2(a2, a3, c2, c3)
	    + c1 * determinant_m2(a2, a3, b2, b3);

	return ans;
}

float determinant_m4(float m[][4])
{
	float ans;
	float a1,a2,a3,a4,b1,b2,b3,b4,c1,c2,c3,c4,d1,d2,d3,d4;

	a1= m[0][0]; 
	b1= m[0][1];
	c1= m[0][2]; 
	d1= m[0][3];

	a2= m[1][0]; 
	b2= m[1][1];
	c2= m[1][2]; 
	d2= m[1][3];

	a3= m[2][0]; 
	b3= m[2][1];
	c3= m[2][2]; 
	d3= m[2][3];

	a4= m[3][0]; 
	b4= m[3][1];
	c4= m[3][2]; 
	d4= m[3][3];

	ans = a1 * determinant_m3(b2, b3, b4, c2, c3, c4, d2, d3, d4)
	    - b1 * determinant_m3(a2, a3, a4, c2, c3, c4, d2, d3, d4)
	    + c1 * determinant_m3(a2, a3, a4, b2, b3, b4, d2, d3, d4)
	    - d1 * determinant_m3(a2, a3, a4, b2, b3, b4, c2, c3, c4);

	return ans;
}

/****************************** Transformations ******************************/

void size_to_mat3(float mat[][3], float *size)
{
	mat[0][0]= size[0];
	mat[0][1]= 0.0f;
	mat[0][2]= 0.0f;
	mat[1][1]= size[1];
	mat[1][0]= 0.0f;
	mat[1][2]= 0.0f;
	mat[2][2]= size[2];
	mat[2][1]= 0.0f;
	mat[2][0]= 0.0f;
}

void size_to_mat4(float mat[][4], float *size)
{
	float tmat[3][3];
	
	size_to_mat3(tmat,size);
	unit_m4(mat);
	copy_m4_m3(mat, tmat);
}

void mat3_to_size(float *size, float mat[][3])
{
	size[0]= len_v3(mat[0]);
	size[1]= len_v3(mat[1]);
	size[2]= len_v3(mat[2]);
}

void mat4_to_size(float *size, float mat[][4])
{
	size[0]= len_v3(mat[0]);
	size[1]= len_v3(mat[1]);
	size[2]= len_v3(mat[2]);
}

/* this gets the average scale of a matrix, only use when your scaling
 * data that has no idea of scale axis, examples are bone-envelope-radius
 * and curve radius */
float mat3_to_scale(float mat[][3])
{
	/* unit length vector */
	float unit_vec[3] = {0.577350269189626f, 0.577350269189626f, 0.577350269189626f};
	mul_m3_v3(mat, unit_vec);
	return len_v3(unit_vec);
}

float mat4_to_scale(float mat[][4])
{
	float tmat[3][3];
	copy_m3_m4(tmat, mat);
	return mat3_to_scale(tmat);
}

void scale_m3_fl(float m[][3], float scale)
{
	m[0][0]= m[1][1]= m[2][2]= scale;
	m[0][1]= m[0][2]= 0.0;
	m[1][0]= m[1][2]= 0.0;
	m[2][0]= m[2][1]= 0.0;
}

void scale_m4_fl(float m[][4], float scale)
{
	m[0][0]= m[1][1]= m[2][2]= scale;
	m[3][3]= 1.0;
	m[0][1]= m[0][2]= m[0][3]= 0.0;
	m[1][0]= m[1][2]= m[1][3]= 0.0;
	m[2][0]= m[2][1]= m[2][3]= 0.0;
	m[3][0]= m[3][1]= m[3][2]= 0.0;
}

void translate_m4(float mat[][4],float Tx, float Ty, float Tz)
{
    mat[3][0] += (Tx*mat[0][0] + Ty*mat[1][0] + Tz*mat[2][0]);
    mat[3][1] += (Tx*mat[0][1] + Ty*mat[1][1] + Tz*mat[2][1]);
    mat[3][2] += (Tx*mat[0][2] + Ty*mat[1][2] + Tz*mat[2][2]);
}

void rotate_m4(float mat[][4], const char axis, const float angle)
{
	int col;
    float temp[4]= {0.0f, 0.0f, 0.0f, 0.0f};
    float cosine, sine;

    cosine = (float)cos(angle);
    sine = (float)sin(angle);
    switch(axis){
    case 'X':    
        for(col=0 ; col<4 ; col++)
            temp[col] = cosine*mat[1][col] + sine*mat[2][col];
        for(col=0 ; col<4 ; col++) {
	    mat[2][col] = - sine*mat[1][col] + cosine*mat[2][col];
            mat[1][col] = temp[col];
	}
        break;

    case 'Y':
        for(col=0 ; col<4 ; col++)
            temp[col] = cosine*mat[0][col] - sine*mat[2][col];
        for(col=0 ; col<4 ; col++) {
            mat[2][col] = sine*mat[0][col] + cosine*mat[2][col];
            mat[0][col] = temp[col];
        }
	break;

    case 'Z':
        for(col=0 ; col<4 ; col++)
            temp[col] = cosine*mat[0][col] + sine*mat[1][col];
        for(col=0 ; col<4 ; col++) {
            mat[1][col] = - sine*mat[0][col] + cosine*mat[1][col];
            mat[0][col] = temp[col];
        }
	break;
    }
}

void blend_m3_m3m3(float out[][3], float dst[][3], float src[][3], float srcweight)
{
	float squat[4], dquat[4], fquat[4];
	float ssize[3], dsize[3], fsize[4];
	float rmat[3][3], smat[3][3];
	
	mat3_to_quat(dquat,dst);
	mat3_to_size(dsize,dst);

	mat3_to_quat(squat,src);
	mat3_to_size(ssize,src);
	
	/* do blending */
	interp_qt_qtqt(fquat, dquat, squat, srcweight);
	interp_v3_v3v3(fsize, dsize, ssize, srcweight);

	/* compose new matrix */
	quat_to_mat3(rmat,fquat);
	size_to_mat3(smat,fsize);
	mul_m3_m3m3(out, rmat, smat);
}

void blend_m4_m4m4(float out[][4], float dst[][4], float src[][4], float srcweight)
{
	float squat[4], dquat[4], fquat[4];
	float ssize[3], dsize[3], fsize[4];
	float sloc[3], dloc[3], floc[3];
	
	mat4_to_quat(dquat,dst);
	mat4_to_size(dsize,dst);
	copy_v3_v3(dloc, dst[3]);

	mat4_to_quat(squat,src);
	mat4_to_size(ssize,src);
	copy_v3_v3(sloc, src[3]);
	
	/* do blending */
	interp_v3_v3v3(floc, dloc, sloc, srcweight);
	interp_qt_qtqt(fquat, dquat, squat, srcweight);
	interp_v3_v3v3(fsize, dsize, ssize, srcweight);

	/* compose new matrix */
	loc_quat_size_to_mat4(out, floc, fquat, fsize);
}


int is_negative_m3(float mat[][3])
{
	float vec[3];
	cross_v3_v3v3(vec, mat[0], mat[1]);
	return (dot_v3v3(vec, mat[2]) < 0.0f);
}

int is_negative_m4(float mat[][4])
{
	float vec[3];
	cross_v3_v3v3(vec, mat[0], mat[1]);
	return (dot_v3v3(vec, mat[2]) < 0.0f);
}

/* make a 4x4 matrix out of 3 transform components */
/* matrices are made in the order: scale * rot * loc */
// TODO: need to have a version that allows for rotation order...
void loc_eul_size_to_mat4(float mat[4][4], float loc[3], float eul[3], float size[3])
{
	float rmat[3][3], smat[3][3], tmat[3][3];
	
	/* initialise new matrix */
	unit_m4(mat);
	
	/* make rotation + scaling part */
	eul_to_mat3(rmat,eul);
	size_to_mat3(smat,size);
	mul_m3_m3m3(tmat, rmat, smat);
	
	/* copy rot/scale part to output matrix*/
	copy_m4_m3(mat, tmat);
	
	/* copy location to matrix */
	mat[3][0] = loc[0];
	mat[3][1] = loc[1];
	mat[3][2] = loc[2];
}

/* make a 4x4 matrix out of 3 transform components */
/* matrices are made in the order: scale * rot * loc */
void loc_eulO_size_to_mat4(float mat[4][4], float loc[3], float eul[3], float size[3], short rotOrder)
{
	float rmat[3][3], smat[3][3], tmat[3][3];
	
	/* initialise new matrix */
	unit_m4(mat);
	
	/* make rotation + scaling part */
	eulO_to_mat3(rmat,eul, rotOrder);
	size_to_mat3(smat,size);
	mul_m3_m3m3(tmat, rmat, smat);
	
	/* copy rot/scale part to output matrix*/
	copy_m4_m3(mat, tmat);
	
	/* copy location to matrix */
	mat[3][0] = loc[0];
	mat[3][1] = loc[1];
	mat[3][2] = loc[2];
}


/* make a 4x4 matrix out of 3 transform components */
/* matrices are made in the order: scale * rot * loc */
void loc_quat_size_to_mat4(float mat[4][4], float loc[3], float quat[4], float size[3])
{
	float rmat[3][3], smat[3][3], tmat[3][3];
	
	/* initialise new matrix */
	unit_m4(mat);
	
	/* make rotation + scaling part */
	quat_to_mat3(rmat,quat);
	size_to_mat3(smat,size);
	mul_m3_m3m3(tmat, rmat, smat);
	
	/* copy rot/scale part to output matrix*/
	copy_m4_m3(mat, tmat);
	
	/* copy location to matrix */
	mat[3][0] = loc[0];
	mat[3][1] = loc[1];
	mat[3][2] = loc[2];
}

/*********************************** Other ***********************************/

void print_m3(char *str, float m[][3])
{
	printf("%s\n", str);
	printf("%f %f %f\n",m[0][0],m[1][0],m[2][0]);
	printf("%f %f %f\n",m[0][1],m[1][1],m[2][1]);
	printf("%f %f %f\n",m[0][2],m[1][2],m[2][2]);
	printf("\n");
}

void print_m4(char *str, float m[][4])
{
	printf("%s\n", str);
	printf("%f %f %f %f\n",m[0][0],m[1][0],m[2][0],m[3][0]);
	printf("%f %f %f %f\n",m[0][1],m[1][1],m[2][1],m[3][1]);
	printf("%f %f %f %f\n",m[0][2],m[1][2],m[2][2],m[3][2]);
	printf("%f %f %f %f\n",m[0][3],m[1][3],m[2][3],m[3][3]);
	printf("\n");
}
