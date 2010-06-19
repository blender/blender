/**
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
 
 * The Original Code is: some of this file.
 *
 * ***** END GPL LICENSE BLOCK *****
 * */

#ifndef BLI_MATH_MATRIX
#define BLI_MATH_MATRIX

#ifdef __cplusplus
extern "C" {
#endif

/********************************* Init **************************************/

#define MAT4_UNITY {{ 1.0, 0.0, 0.0, 0.0},\
					{ 0.0, 1.0, 0.0, 0.0},\
					{ 0.0, 0.0, 1.0, 0.0},\
					{ 0.0, 0.0, 0.0, 1.0}}

#define MAT3_UNITY {{ 1.0, 0.0, 0.0},\
					{ 0.0, 1.0, 0.0},\
					{ 0.0, 0.0, 1.0}}

void zero_m3(float R[3][3]);
void zero_m4(float R[4][4]);

void unit_m3(float R[3][3]);
void unit_m4(float R[4][4]);

void copy_m3_m3(float R[3][3], float A[3][3]);
void copy_m4_m4(float R[4][4], float A[4][4]);
void copy_m3_m4(float R[3][3], float A[4][4]);
void copy_m4_m3(float R[4][4], float A[3][3]);

void swap_m3m3(float A[3][3], float B[3][3]);
void swap_m4m4(float A[4][4], float B[4][4]);

/******************************** Arithmetic *********************************/

void add_m3_m3m3(float R[3][3], float A[3][3], float B[3][3]);
void add_m4_m4m4(float R[4][4], float A[4][4], float B[4][4]);

void mul_m3_m3m3(float R[3][3], float A[3][3], float B[3][3]);
void mul_m4_m4m4(float R[4][4], float A[4][4], float B[4][4]);
void mul_m4_m3m4(float R[4][4], float A[3][3], float B[4][4]);
void mul_m4_m4m3(float R[4][4], float A[4][4], float B[3][3]);
void mul_m3_m3m4(float R[3][3], float A[3][3], float B[4][4]);

void mul_serie_m3(float R[3][3],
	float M1[3][3], float M2[3][3], float M3[3][3], float M4[3][3],
	float M5[3][3], float M6[3][3], float M7[3][3], float M8[3][3]);
void mul_serie_m4(float R[4][4],
	float M1[4][4], float M2[4][4], float M3[4][4], float M4[4][4],
	float M5[4][4], float M6[4][4], float M7[4][4], float M8[4][4]);

void mul_m4_v3(float M[4][4], float r[3]);
void mul_v3_m4v3(float r[3], float M[4][4], float v[3]);
void mul_mat3_m4_v3(float M[4][4], float r[3]);
void mul_m4_v4(float M[4][4], float r[4]);
void mul_v4_m4v4(float r[4], float M[4][4], float v[4]);
void mul_project_m4_v4(float M[4][4], float r[3]);

void mul_m3_v3(float M[3][3], float r[3]);
void mul_v3_m3v3(float r[3], float M[3][3], float a[3]);
void mul_transposed_m3_v3(float M[3][3], float r[3]);
void mul_m3_v3_double(float M[3][3], double r[3]);

void mul_m3_fl(float R[3][3], float f);
void mul_m4_fl(float R[4][4], float f);
void mul_mat3_m4_fl(float R[4][4], float f);

int invert_m3(float R[3][3]);
int invert_m3_m3(float R[3][3], float A[3][3]);
int invert_m4(float R[4][4]);
int invert_m4_m4(float R[4][4], float A[4][4]);

/****************************** Linear Algebra *******************************/

void transpose_m3(float R[3][3]);
void transpose_m4(float R[4][4]);

void normalize_m3(float R[3][3]);
void normalize_m4(float R[4][4]);

void orthogonalize_m3(float R[3][3], int axis);
void orthogonalize_m4(float R[4][4], int axis);

int is_orthogonal_m3(float mat[3][3]);
int is_orthogonal_m4(float mat[4][4]);

void adjoint_m3_m3(float R[3][3], float A[3][3]);
void adjoint_m4_m4(float R[4][4], float A[4][4]);

float determinant_m2(
	float a, float b,
	float c, float d);
float determinant_m3(
	float a, float b, float c,
	float d, float e, float f,
	float g, float h, float i);
float determinant_m4(float A[4][4]);

/****************************** Transformations ******************************/

void scale_m3_fl(float R[3][3], float scale);
void scale_m4_fl(float R[4][4], float scale);

float mat3_to_scale(float M[3][3]);
float mat4_to_scale(float M[4][4]);

void size_to_mat3(float R[3][3], float size[3]);
void size_to_mat4(float R[4][4], float size[3]);

void mat3_to_size(float r[3], float M[3][3]);
void mat4_to_size(float r[3], float M[4][4]);

void translate_m4(float mat[4][4], float tx, float ty, float tz);
void rotate_m4(float mat[4][4], char axis, float angle);

void loc_eul_size_to_mat4(float R[4][4],
	float loc[3], float eul[3], float size[3]);
void loc_eulO_size_to_mat4(float R[4][4],
	float loc[3], float eul[3], float size[3], short order);
void loc_quat_size_to_mat4(float R[4][4],
	float loc[3], float quat[4], float size[3]);

void blend_m3_m3m3(float R[3][3], float A[3][3], float B[3][3], float t);
void blend_m4_m4m4(float R[4][4], float A[4][4], float B[4][4], float t);

int is_negative_m3(float mat[3][3]);
int is_negative_m4(float mat[4][4]);

/*********************************** Other ***********************************/

void print_m3(char *str, float M[3][3]);
void print_m4(char *str, float M[3][4]);

#ifdef __cplusplus
}
#endif

#endif /* BLI_MATH_MATRIX */

