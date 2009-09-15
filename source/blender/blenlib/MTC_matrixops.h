/*
 * matrixops.h
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

#ifndef MATRIXOPS_H
#define MATRIXOPS_H 

#ifdef __cplusplus
extern "C" { 
#endif

/* ------------------------------------------------------------------------- */
/* need rewriting: */
/**
 * copy the left upp3 3 by 3 of m2 to m1
 */
void MTC_Mat3CpyMat4(float m1[][3], float m2[][4]);

/* ------------------------------------------------------------------------- */
/* operations based on 4 by 4 matrices */

/**
 * Copy m1 to m2
 */
void MTC_Mat4CpyMat4(float m1[][4], float m2[][4]);

/**
 * Multiply all matrices after the first, leave the result in the
 * first argument
 */
void MTC_Mat4MulSerie(float answ[][4],
					  float m1[][4], float m2[][4], float m3[][4],
					  float m4[][4], float m5[][4], float m6[][4],
					  float m7[][4], float m8[][4]);

/**
 * m1 = m2 matprod m3
 */
void MTC_Mat4MulMat4(float m1[][4], float m2[][4], float m3[][4]);

/**
 * Do vec^t prod mat, result in vec. Ignore vec[3] (vec is a
 * float[3])
 */
void MTC_Mat4MulVecfl(float mat[][4], float *vec);

/**
 * Invert mat, result in inverse. Always returns 1
 */
int MTC_Mat4Invert(float inverse[][4], float mat[][4]);

/**
 * Make the set of mat orthonormal (mat should already be orthogonal)?
 * (doesn't appear to normalize properly?)
 */
void MTC_Mat4Ortho(float mat[][4]);

/**
 * vec = mat prod vec, result in vec, ignore fourth component entirely
 * (4th component is _not_ accessed!!! vec is 3d)
 */
void MTC_Mat4Mul3Vecfl(float mat[][4], float *vec);

/**
 * vec = mat prod vec, result in vec
 */
void MTC_Mat4MulVec4fl(float mat[][4], float *vec);

/**
 * Set <m> to the 4-D unity matrix
 */
void MTC_Mat4One(float m[][4]);

/**
 * Swap matrices m1 and m2
 */
void MTC_Mat4SwapMat4(float m1[][4], float m2[][4]);

/**
 * Copy m2 to the top-left 3x3 of m1, don't touch the remaining elements.
 */
void MTC_Mat4CpyMat3nc(float m1[][4], float m2[][3]);

/**
 * m1 = m2 * m3, but only the top-left 3x3
 */
void MTC_Mat4MulMat33(float m1[][3], float m2[][4], float m3[][3]); 

/* ------------------------------------------------------------------------- */
/* Operations based on 3 by 3 matrices */
/**
 * Do vec^t prod mat, result in vec.(vex is 3d)
 */
void MTC_Mat3MulVecfl(float mat[][3], float *vec);

/**
 * Copy m1 to m2
 */
void MTC_Mat3CpyMat3(float m1[][3], float m2[][3]);

/**
 * m1 = m2 prod m3
 */
void MTC_Mat3MulMat3(float m1[][3], float m3[][3], float m2[][3]);

/**
 * vec = vec prod mat
 */
void MTC_Mat3MulVecd(float mat[][3], double *vec);

/**
 * Guess: invert matrix
 * result goes to m1
 */
void MTC_Mat3Inv(float m1[][3], float m2[][3]);

/**
 * Sort of a determinant matrix? Doesn't seem very adjoint to me...
 * result goes to m1
 */
void MTC_Mat3Adj(float m1[][3], float m[][3]);

/**
 * Set <m> to the 3D unity matrix
 */
void MTC_Mat3One(float m[][3]);

/* ------------------------------------------------------------------------- */

#ifdef __cplusplus
}
#endif

#endif /* MATRIXOPS_H */

