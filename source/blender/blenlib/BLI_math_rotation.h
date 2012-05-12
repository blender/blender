/*
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
 * */

#ifndef __BLI_MATH_ROTATION_H__
#define __BLI_MATH_ROTATION_H__

/** \file BLI_math_rotation.h
 *  \ingroup bli
 */

#ifdef __cplusplus
extern "C" {
#endif

#define RAD2DEG(_rad) ((_rad) * (180.0 / M_PI))
#define DEG2RAD(_deg) ((_deg) * (M_PI / 180.0))


#define RAD2DEGF(_rad) ((_rad) * (float)(180.0 / M_PI))
#define DEG2RADF(_deg) ((_deg) * (float)(M_PI / 180.0))

/******************************** Quaternions ********************************/
/* stored in (w, x, y, z) order                                              */

/* init */
void unit_axis_angle(float axis[3], float *angle);
void unit_qt(float q[4]);
void copy_qt_qt(float q[4], const float a[4]);

/* arithmetic */
void mul_qt_qtqt(float q[4], const float a[4], const float b[4]);
void mul_qt_v3(const float q[4], float r[3]);
void mul_qt_fl(float q[4], const float f);
void mul_fac_qt_fl(float q[4], const float f);

void sub_qt_qtqt(float q[4], const float a[4], const float b[4]);

void invert_qt(float q[4]);
void invert_qt_qt(float q1[4], const float q2[4]);
void conjugate_qt(float q[4]);
float dot_qtqt(const float a[4], const float b[4]);
float normalize_qt(float q[4]);
float normalize_qt_qt(float q1[4], const float q2[4]);

/* comparison */
int is_zero_qt(float q[4]);

/* interpolation */
void interp_qt_qtqt(float q[4], const float a[4], const float b[4], const float t);
void add_qt_qtqt(float q[4], const float a[4], const float b[4], const float t);

/* conversion */
void quat_to_mat3(float mat[3][3], const float q[4]);
void quat_to_mat4(float mat[4][4], const float q[4]);

void mat3_to_quat(float q[4], float mat[3][3]);
void mat4_to_quat(float q[4], float mat[4][4]);
void tri_to_quat(float q[4], const float a[3], const float b[3], const float c[3]);
void vec_to_quat(float q[4], const float vec[3], short axis, const short upflag);
/* note: v1 and v2 must be normalized */
void rotation_between_vecs_to_quat(float q[4], const float v1[3], const float v2[3]);
void rotation_between_quats_to_quat(float q[4], const float q1[4], const float q2[4]);

/* TODO: don't what this is, but it's not the same as mat3_to_quat */
void mat3_to_quat_is_ok(float q[4], float mat[3][3]);

/* other */
void print_qt(const char *str, const float q[4]);

/******************************** Axis Angle *********************************/

/* conversion */
void axis_angle_to_quat(float r[4], const float axis[3], float angle);
void axis_angle_to_mat3(float R[3][3], const float axis[3], const float angle);
void axis_angle_to_mat4(float R[4][4], const float axis[3], const float angle);

void quat_to_axis_angle(float axis[3], float *angle, const float q[4]);
void mat3_to_axis_angle(float axis[3], float *angle, float M[3][3]);
void mat4_to_axis_angle(float axis[3], float *angle, float M[4][4]);

void single_axis_angle_to_mat3(float R[3][3], const char axis, const float angle);

/****************************** Vector/Rotation ******************************/
/* old axis angle code                                                       */
/* TODO: the following calls should probably be depreceated sometime         */

/* conversion */
void vec_rot_to_quat(float quat[4], const float vec[3], const float phi);
void vec_rot_to_mat3(float mat[3][3], const float vec[3], const float phi);
void vec_rot_to_mat4(float mat[4][4], const float vec[3], const float phi);

/******************************** XYZ Eulers *********************************/

void eul_to_quat(float quat[4], const float eul[3]);
void eul_to_mat3(float mat[3][3], const float eul[3]);
void eul_to_mat4(float mat[4][4], const float eul[3]);

void quat_to_eul(float eul[3], const float quat[4]);
void mat3_to_eul(float eul[3], float mat[3][3]);
void mat4_to_eul(float eul[3], float mat[4][4]);

void compatible_eul(float eul[3], const float old[3]);
void mat3_to_compatible_eul(float eul[3], const float old[3], float mat[3][3]);

void rotate_eul(float eul[3], const char axis, const float angle);

/************************** Arbitrary Order Eulers ***************************/

/* warning: must match the eRotationModes in DNA_action_types.h
 * order matters - types are saved to file. */

typedef enum eEulerRotationOrders {
	EULER_ORDER_DEFAULT = 1, /* blender classic = XYZ */
	EULER_ORDER_XYZ = 1,
	EULER_ORDER_XZY,
	EULER_ORDER_YXZ,
	EULER_ORDER_YZX,
	EULER_ORDER_ZXY,
	EULER_ORDER_ZYX
	/* there are 6 more entries with dulpicate entries included */
} eEulerRotationOrders;

void eulO_to_quat(float quat[4], const float eul[3], const short order);
void eulO_to_mat3(float mat[3][3], const float eul[3], const short order);
void eulO_to_mat4(float mat[4][4], const float eul[3], const short order);
void eulO_to_axis_angle(float axis[3], float *angle, const float eul[3], const short order);
void eulO_to_gimbal_axis(float gmat[3][3], const float eul[3], const short order);
 
void quat_to_eulO(float eul[3], const short order, const float quat[4]);
void mat3_to_eulO(float eul[3], const short order, float mat[3][3]);
void mat4_to_eulO(float eul[3], const short order, float mat[4][4]);
void axis_angle_to_eulO(float eul[3], const short order, const float axis[3], const float angle);

void mat3_to_compatible_eulO(float eul[3], float old[3], short order, float mat[3][3]);
void mat4_to_compatible_eulO(float eul[3], float old[3], short order, float mat[4][4]);

void rotate_eulO(float eul[3], short order, char axis, float angle);

/******************************* Dual Quaternions ****************************/

typedef struct DualQuat {
	float quat[4];
	float trans[4];

	float scale[4][4];
	float scale_weight;
} DualQuat;

void copy_dq_dq(DualQuat *r, DualQuat *dq);
void normalize_dq(DualQuat *dq, float totw);
void add_weighted_dq_dq(DualQuat *r, DualQuat *dq, float weight);
void mul_v3m3_dq(float r[3], float R[3][3], DualQuat * dq);

void mat4_to_dquat(DualQuat * r, float base[4][4], float M[4][4]);
void dquat_to_mat4(float R[4][4], DualQuat * dq);

void quat_apply_track(float quat[4], short axis, short upflag);
void vec_apply_track(float vec[3], short axis);

float focallength_to_fov(float focal_length, float sensor);
float fov_to_focallength(float fov, float sensor);

float angle_wrap_rad(float angle);
float angle_wrap_deg(float angle);

#ifdef __cplusplus
}
#endif

#endif /* __BLI_MATH_ROTATION_H__ */

