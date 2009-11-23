#undef TEST_ACTIVE
//#define ACTIVE 1
/**
 * blenlib/BLI_math.h    mar 2001 Nzc
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

#ifdef WIN32
#define _USE_MATH_DEFINES
#endif

#include <math.h>

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
#ifndef M_1_PI
#define M_1_PI		0.318309886183790671538
#endif

#ifndef M_E
#define M_E             2.7182818284590452354
#endif
#ifndef M_LOG2E
#define M_LOG2E         1.4426950408889634074
#endif
#ifndef M_LOG10E
#define M_LOG10E        0.43429448190325182765
#endif
#ifndef M_LN2
#define M_LN2           0.69314718055994530942
#endif
#ifndef M_LN10
#define M_LN10          2.30258509299404568402
#endif

#ifndef sqrtf
#define sqrtf(a) ((float)sqrt(a))
#endif
#ifndef powf
#define powf(a, b) ((float)pow(a, b))
#endif
#ifndef cosf
#define cosf(a) ((float)cos(a))
#endif
#ifndef sinf
#define sinf(a) ((float)sin(a))
#endif
#ifndef acosf
#define acosf(a) ((float)acos(a))
#endif
#ifndef asinf
#define asinf(a) ((float)asin(a))
#endif
#ifndef atan2f
#define atan2f(a, b) ((float)atan2(a, b))
#endif
#ifndef tanf
#define tanf(a) ((float)tan(a))
#endif
#ifndef atanf
#define atanf(a) ((float)atan(a))
#endif
#ifndef floorf
#define floorf(a) ((float)floor(a))
#endif
#ifndef ceilf
#define ceilf(a) ((float)ceil(a))
#endif
#ifndef fabsf
#define fabsf(a) ((float)fabs(a))
#endif
#ifndef logf
#define logf(a) ((float)log(a))
#endif
#ifndef expf
#define expf(a) ((float)exp(a))
#endif
#ifndef fmodf
#define fmodf(a, b) ((float)fmod(a, b))
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


void cent_tri_v3(float *cent,  float *v1, float *v2, float *v3);
void cent_quad_v3(float *cent, float *v1, float *v2, float *v3, float *v4);

void cross_v3_v3v3(float *c, float *a, float *b);
void project_v3_v3v3(float *c, float *v1, float *v2);

float dot_v3v3(float *v1, float *v2);
float dot_v2v2(float *v1, float *v2);

float normalize_v3(float *n);
float normalize_v2(float *n);
double normalize_dv3(double n[3]);

float Sqrt3f(float f);
double sqrt3d(double d);

float saacos(float fac);
float saasin(float fac);
float sasqrt(float fac);
float saacosf(float fac);
float saasinf(float fac);
float sasqrtf(float fac);

int compare_v3v3(float *v1, float *v2, float limit);
int compare_v4v4(float *v1, float *v2, float limit);
float interpf(float target, float origin, float fac);

float normal_tri_v3( float *n,float *v1, float *v2, float *v3);
float normal_quad_v3( float *n,float *v1, float *v2, float *v3, float *v4);

void CalcNormLong(int *v1, int *v2, int *v3, float *n);
/* CalcNormShort: is ook uitprodukt - (translates as 'is also out/cross product') */
void CalcNormShort(short *v1, short *v2, short *v3, float *n);
float power_of_2(float val);

/**
 * @section Euler conversion routines (With Custom Order)
 */

/* Defines for rotation orders 
 * WARNING: must match the eRotationModes in DNA_action_types.h
 *		   order matters - types are saved to file!
 */
typedef enum eEulerRotationOrders {
	EULER_ORDER_DEFAULT = 1,	/* Blender 'default' (classic) is basically XYZ */
	EULER_ORDER_XYZ = 1,		/* Blender 'default' (classic) - must be as 1 to sync with PoseChannel rotmode */
	EULER_ORDER_XZY,
	EULER_ORDER_YXZ,
	EULER_ORDER_YZX,
	EULER_ORDER_ZXY,
	EULER_ORDER_ZYX,
	/* NOTE: there are about 6 more entries when including duplicated entries too */
} eEulerRotationOrders;

void eulO_to_quat( float quat[4],float eul[3], short order);
void quat_to_eulO( float eul[3], short order,float quat[4]);

void eulO_to_mat3( float Mat[3][3],float eul[3], short order);
void eulO_to_mat4( float Mat[4][4],float eul[3], short order);
 
void mat3_to_eulO( float eul[3], short order,float Mat[3][3]);
void mat4_to_eulO( float eul[3], short order,float Mat[4][4]);

void mat3_to_compatible_eulO( float eul[3], float oldrot[3], short order,float mat[3][3]);

void rotate_eulO(float beul[3], short order, char axis, float ang);
 
/**
 * @section Euler conversion routines (Blender XYZ)
 */

void eul_to_mat3( float mat[][3],float *eul);
void eul_to_mat4( float mat[][4],float *eul);

void mat3_to_eul( float *eul,float tmat[][3]);
void mat4_to_eul(float *eul,float tmat[][4]);

void eul_to_quat( float *quat,float *eul);

void mat3_to_compatible_eul( float *eul, float *oldrot,float mat[][3]);
void eulO_to_gimbal_axis(float gmat[][3], float *eul, short order);


void compatible_eul(float *eul, float *oldrot);
void rotate_eul(float *beul, char axis, float ang);


/**
 * @section Quaternion arithmetic routines
 */

int  is_zero_qt(float *q);
void quat_to_eul( float *eul,float *quat);
void unit_qt(float *);
void mul_qt_qtqt(float *, float *, float *);
void mul_qt_v3(float *q, float *v);
void mul_qt_fl(float *q, float f);
void mul_fac_qt_fl(float *q, float fac);

void normalize_qt(float *);
void axis_angle_to_quat( float *quat,float *vec, float phi);

void sub_qt_qtqt(float *q, float *q1, float *q2);
void conjugate_qt(float *q);
void invert_qt(float *q);
float dot_qtqt(float *q1, float *q2);
void copy_qt_qt(float *q1, float *q2);

void print_qt(char *str, float q[4]);

void interp_qt_qtqt(float *result, float *quat1, float *quat2, float t);
void add_qt_qtqt(float *result, float *quat1, float *quat2, float t);

void quat_to_mat3( float m[][3],float *q);
void quat_to_mat4( float m[][4],float *q);

/**
 * @section matrix multiplication and copying routines
 */

void mul_m3_fl(float *m, float f);
void mul_m4_fl(float *m, float f);
void mul_mat3_m4_fl(float *m, float f);

void transpose_m3(float mat[][3]);
void transpose_m4(float mat[][4]);

int invert_m4_m4(float inverse[][4], float mat[][4]);
void invert_m4_m4(float inverse[][4], float mat[][4]);
void invert_m4_m4(float *m1, float *m2);
void invert_m4_m4(float out[][4], float in[][4]);
void invert_m3_m3(float m1[][3], float m2[][3]);

void copy_m3_m4(float m1[][3],float m2[][4]);
void copy_m4_m3(float m1[][4], float m2[][3]); 

void blend_m3_m3m3(float out[][3], float dst[][3], float src[][3], float srcweight);
void blend_m4_m4m4(float out[][4], float dst[][4], float src[][4], float srcweight);

float determinant_m2(float a,float b,float c, float d);

float determinant_m3(
	float a1, float a2, float a3,
	float b1, float b2, float b3,
	float c1, float c2, float c3 
);

float determinant_m4(float m[][4]);

void adjoint_m3_m3(float m1[][3], float m[][3]);
void adjoint_m4_m4(float out[][4], float in[][4]);

void mul_m4_m4m4(float m1[][4], float m2[][4], float m3[][4]);
void subMat4MulMat4(float *m1, float *m2, float *m3);
#ifndef TEST_ACTIVE
void mul_m3_m3m3(float m1[][3], float m3[][3], float m2[][3]);
#else
void mul_m3_m3m3(float *m1, float *m3, float *m2);
#endif
void mul_m4_m3m4(float (*m1)[4], float (*m3)[3], float (*m2)[4]);
void copy_m4_m4(float m1[][4], float m2[][4]);
void swap_m4m4(float m1[][4], float m2[][4]);
void copy_m3_m3(float m1[][3], float m2[][3]);

void mul_serie_m3(float answ[][3],
	float m1[][3], float m2[][3], float m3[][3],
	float m4[][3], float m5[][3], float m6[][3],
	float m7[][3], float m8[][3]
);

void mul_serie_m4(float answ[][4], float m1[][4],
	float m2[][4], float m3[][4], float m4[][4],
	float m5[][4], float m6[][4], float m7[][4],
	float m8[][4]
);
	
void zero_m4(float *m);
void zero_m3(float *m);
	
void unit_m3(float m[][3]);
void unit_m4(float m[][4]);

/* NOTE: These only normalise the matrix, they don't make it orthogonal */
void normalize_m3(float mat[][3]);
void normalize_m4(float mat[][4]);

int is_orthogonal_m3(float mat[][3]);
void orthogonalize_m3(float mat[][3], int axis); /* axis is the one to keep in place (assumes it is non-null) */
int is_orthogonal_m4(float mat[][4]);
void orthogonalize_m4(float mat[][4], int axis); /* axis is the one to keep in place (assumes it is non-null) */

void mul_v3_m4v3(float *in, float mat[][4], float *vec);
void mul_m4_m4m3(float (*m1)[4], float (*m3)[4], float (*m2)[3]);
void mul_m3_m3m4(float m1[][3], float m2[][3], float m3[][4]);

void Mat4MulVec(float mat[][4],int *vec);
void mul_m4_v3(float mat[][4], float *vec);
void mul_mat3_m4_v3(float mat[][4], float *vec);
void mul_project_m4_v4(float mat[][4],float *vec);
void mul_m4_v4(float mat[][4], float *vec);
void Mat3MulVec(float mat[][3],int *vec);
void mul_m3_v3(float mat[][3], float *vec);
void mul_m3_v3_double(float mat[][3], double *vec);
void mul_transposed_m3_v3(float mat[][3], float *vec);

void add_m3_m3m3(float m1[][3], float m2[][3], float m3[][3]);
void add_m4_m4m4(float m1[][4], float m2[][4], float m3[][4]);

void VecUpMat3old(float *vec, float mat[][3], short axis);
void VecUpMat3(float *vec, float mat[][3], short axis);

void copy_v3_v3(float *v1, float *v2);
int VecLen(int *v1, int *v2);
float len_v3v3(float v1[3], float v2[3]);
float len_v3(float *v);
void mul_v3_fl(float *v1, float f);
void negate_v3(float *v1);

int compare_len_v3v3(float *v1, float *v2, float limit);
int compare_v3v3(float *v1, float *v2, float limit);
int equals_v3v3(float *v1, float *v2);
int is_zero_v3(float *v);

void print_v3(char *str,float v[3]);
void print_v4(char *str, float v[4]);

void add_v3_v3v3(float *v, float *v1, float *v2);
void sub_v3_v3v3(float *v, float *v1, float *v2);
void mul_v3_v3v3(float *v, float *v1, float *v2);
void interp_v3_v3v3(float *target, const float *a, const float *b, const float t);
void interp_v3_v3v3v3(float p[3], const float v1[3], const float v2[3], const float v3[3], const float w[3]);
void mid_v3_v3v3(float *v, float *v1, float *v2);

void ortho_basis_v3v3_v3( float *v1, float *v2,float *v);

float len_v2v2(float *v1, float *v2);
float len_v2(float *v);
void mul_v2_fl(float *v1, float f);
void add_v2_v2v2(float *v, float *v1, float *v2);
void sub_v2_v2v2(float *v, float *v1, float *v2);
void copy_v2_v2(float *v1, float *v2);
void interp_v2_v2v2(float *target, const float *a, const float *b, const float t);
void interp_v2_v2v2v2(float p[2], const float v1[2], const float v2[2], const float v3[2], const float w[3]);

void axis_angle_to_quat(float q[4], float axis[3], float angle);
void quat_to_axis_angle( float axis[3], float *angle,float q[4]);
void axis_angle_to_eulO( float eul[3], short order,float axis[3], float angle);
void eulO_to_axis_angle( float axis[3], float *angle,float eul[3], short order);
void axis_angle_to_mat3( float mat[3][3],float axis[3], float angle);
void axis_angle_to_mat4( float mat[4][4],float axis[3], float angle);
void mat3_to_axis_angle( float axis[3], float *angle,float mat[3][3]);
void mat4_to_axis_angle( float axis[3], float *angle,float mat[4][4]);

void mat3_to_vec_rot( float axis[3], float *angle,float mat[3][3]);
void mat4_to_vec_rot( float axis[3], float *angle,float mat[4][4]);
void vec_rot_to_mat3( float mat[][3],float *vec, float phi);
void vec_rot_to_mat4( float mat[][4],float *vec, float phi);

void rotation_between_vecs_to_quat(float *q, float v1[3], float v2[3]);
void vec_to_quat( float *q,float *vec, short axis, short upflag);
void mat3_to_quat_is_ok( float *q,float wmat[][3]);

void reflect_v3_v3v3(float *out, float *v1, float *v2);
void bisect_v3_v3v3v3(float *v, float *v1, float *v2, float *v3);
float angle_v2v2(float *v1, float *v2);
float angle_v3v3v3(float *v1, float *v2, float *v3);
float angle_normalized_v3v3(float *v1, float *v2);

float angle_v2v2v2(float *v1, float *v2, float *v3);
float angle_normalized_v2v2(float *v1, float *v2);
	
void normal_short_to_float_v3(float *out, short *in);
void normal_float_to_short_v3(short *out, float *in);

float dist_to_line_v2(float *v1, float *v2, float *v3);
float dist_to_line_segment_v2(float *v1, float *v2, float *v3);
float dist_to_line_segment_v3(float *v1, float *v2, float *v3);
void closest_to_line_segment_v3(float *closest, float v1[3], float v2[3], float v3[3]);
float area_tri_v2(float *v1, float *v2, float *v3);
float area_quad_v3(float *v1, float *v2, float *v3, float *v4);
float area_tri_v3(float *v1, float *v2, float *v3);
float area_poly_v3(int nr, float *verts, float *normal);

/* intersect Line-Line
	return:
	-1: colliniar
	 0: no intersection of segments
	 1: exact intersection of segments
	 2: cross-intersection of segments
*/
extern short isect_line_line_v2(float *v1, float *v2, float *v3, float *v4);
extern short isect_line_line_v2_short(short *v1, short *v2, short *v3, short *v4);

/*point in tri,  0 no intersection, 1 intersect */
int isect_point_tri_v2(float pt[2], float v1[2], float v2[2], float v3[2]);
/* point in quad,  0 no intersection, 1 intersect */
int isect_point_quad_v2(float pt[2], float v1[2], float v2[2], float v3[2], float v4[2]);

/* interpolation weights of point in a triangle or quad, v4 may be NULL */
void interp_weights_face_v3( float *w,float *v1, float *v2, float *v3, float *v4, float *co);
/* interpolation weights of point in a polygon with >= 3 vertices */
void interp_weights_poly_v3( float *w,float v[][3], int n, float *co);

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

#define BLI_CS_SMPTE	0
#define BLI_CS_REC709	1
#define BLI_CS_CIE		2

#define RAD2DEG(_rad) ((_rad)*(180.0/M_PI))
#define DEG2RAD(_deg) ((_deg)*(M_PI/180.0))

void hsv_to_rgb(float h, float s, float v, float *r, float *g, float *b);
void hex_to_rgb(char *hexcol, float *r, float *g, float *b);
void rgb_to_yuv(float r, float g, float b, float *ly, float *lu, float *lv);
void yuv_to_rgb(float y, float u, float v, float *lr, float *lg, float *lb);
void ycc_to_rgb(float y, float cb, float cr, float *lr, float *lg, float *lb);
void rgb_to_ycc(float r, float g, float b, float *ly, float *lcb, float *lcr);
void rgb_to_hsv(float r, float g, float b, float *lh, float *ls, float *lv);
void xyz_to_rgb(float x, float y, float z, float *r, float *g, float *b, int colorspace);
int constrain_rgb(float *r, float *g, float *b);
unsigned int hsv_to_cpack(float h, float s, float v);
unsigned int rgb_to_cpack(float r, float g, float b);
void cpack_to_rgb(unsigned int col, float *r, float *g, float *b);
void minmax_rgb(short c[]);



void star_m3_v3(float mat[][3],float *vec);

short EenheidsMat(float mat[][3]);

void orthographic_m4( float matrix[][4],float left, float right, float bottom, float top, float nearClip, float farClip);
void polarview_m4( float Vm[][4],float dist, float azimuth, float incidence, float twist);
void translate_m4( float mat[][4],float Tx, float Ty, float Tz);
void i_multmatrix(float icand[][4], float Vm[][4]);
void rotate_m4( float mat[][4], char axis,float angle);



void minmax_v3_v3v3(float *min, float *max, float *vec);
void size_to_mat3( float mat[][3],float *size);
void size_to_mat4( float mat[][4],float *size);

float mat3_to_scale(float mat[][3]);
float mat4_to_scale(float mat[][4]);

void print_m3(char *str, float m[][3]);
void print_m4(char *str, float m[][4]);

/* uit Sig.Proc.85 pag 253 */
void mat3_to_quat( float *q,float wmat[][3]);
void mat4_to_quat( float *q,float m[][4]);

void mat3_to_size( float *size,float mat[][3]);
void mat4_to_size( float *size,float mat[][4]);

void tri_to_quat( float *quat,float *v1, float *v2, float *v3);

void loc_eul_size_to_mat4(float mat[4][4], float loc[3], float eul[3], float size[3]);
void loc_eulO_size_to_mat4(float mat[4][4], float loc[3], float eul[3], float size[3], short rotOrder);
void loc_quat_size_to_mat4(float mat[4][4], float loc[3], float quat[4], float size[3]);

void map_to_tube( float *u, float *v,float x, float y, float z);
void map_to_sphere( float *u, float *v,float x, float y, float z);

int isect_line_line_v3(float v1[3], float v2[3], float v3[3], float v4[3], float i1[3], float i2[3]);
int isect_line_line_strict_v3(float v1[3], float v2[3], float v3[3], float v4[3], float vi[3], float *lambda);
int isect_line_tri_v3(float p1[3], float p2[3], float v0[3], float v1[3], float v2[3], float *lambda, float *uv);
int isect_ray_tri_v3(float p1[3], float d[3], float v0[3], float v1[3], float v2[3], float *lambda, float *uv);
int isect_ray_tri_threshold_v3(float p1[3], float d[3], float v0[3], float v1[3], float v2[3], float *lambda, float *uv, float threshold);
int isect_sweeping_sphere_tri_v3(float p1[3], float p2[3], float radius, float v0[3], float v1[3], float v2[3], float *lambda, float *ipoint);
int isect_axial_line_tri_v3(int axis, float co1[3], float co2[3], float v0[3], float v1[3], float v2[3], float *lambda);
int isect_aabb_aabb_v3(float min1[3], float max1[3], float min2[3], float max2[3]);
void interp_cubic_v3( float *x, float *v,float *x1, float *v1, float *x2, float *v2, float t);
void isect_point_quad_uv_v2(float v0[2], float v1[2], float v2[2], float v3[2], float pt[2], float *uv);
void isect_point_face_uv_v2(int isquad, float v0[2], float v1[2], float v2[2], float v3[2], float pt[2], float *uv);
int isect_point_tri_v2(float v1[2], float v2[2], float v3[2], float pt[2]);
int isect_point_tri_v2_int(int x1, int y1, int x2, int y2, int a, int b);
int isect_point_tri_prism_v3(float p[3], float v1[3], float v2[3], float v3[3]);

float closest_to_line_v3( float cp[3],float p[3], float l1[3], float l2[3]);

float shell_angle_to_dist(const float angle);

typedef struct DualQuat {
	float quat[4];
	float trans[4];

	float scale[4][4];
	float scale_weight;
} DualQuat;

void mat4_to_dquat( DualQuat *dq,float basemat[][4], float mat[][4]);
void dquat_to_mat4( float mat[][4],DualQuat *dq);
void add_weighted_dq_dq(DualQuat *dqsum, DualQuat *dq, float weight);
void normalize_dq(DualQuat *dq, float totweight);
void mul_v3m3_dq( float *co, float mat[][3],DualQuat *dq);
void copy_dq_dq(DualQuat *dq1, DualQuat *dq2);
			  
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

