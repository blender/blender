/*
 * Copyright 2011-2013 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __UTIL_TRANSFORM_H__
#define __UTIL_TRANSFORM_H__

#ifndef __KERNEL_GPU__
#include <string.h>
#endif

#include "util_math.h"
#include "util_types.h"

CCL_NAMESPACE_BEGIN

/* Data Types */

typedef struct Transform {
	float4 x, y, z, w; /* rows */

#ifndef __KERNEL_GPU__
	float4 operator[](int i) const { return *(&x + i); }
	float4& operator[](int i) { return *(&x + i); }
#endif
} Transform;

/* transform decomposed in rotation/translation/scale. we use the same data
 * structure as Transform, and tightly pack decomposition into it. first the
 * rotation (4), then translation (3), then 3x3 scale matrix (9).
 *
 * For the DecompMotionTransform we drop scale from pre/post. */

typedef struct ccl_may_alias MotionTransform {
	Transform pre;
	Transform mid;
	Transform post;
} MotionTransform;

typedef struct DecompMotionTransform {
	Transform mid;
	float4 pre_x, pre_y;
	float4 post_x, post_y;
} DecompMotionTransform;

/* Functions */

ccl_device_inline float3 transform_perspective(const Transform *t, const float3 a)
{
	float4 b = make_float4(a.x, a.y, a.z, 1.0f);
	float3 c = make_float3(dot(t->x, b), dot(t->y, b), dot(t->z, b));
	float w = dot(t->w, b);

	return (w != 0.0f)? c/w: make_float3(0.0f, 0.0f, 0.0f);
}

ccl_device_inline float3 transform_point(const Transform *t, const float3 a)
{
	float3 c = make_float3(
		a.x*t->x.x + a.y*t->x.y + a.z*t->x.z + t->x.w,
		a.x*t->y.x + a.y*t->y.y + a.z*t->y.z + t->y.w,
		a.x*t->z.x + a.y*t->z.y + a.z*t->z.z + t->z.w);

	return c;
}

ccl_device_inline float3 transform_direction(const Transform *t, const float3 a)
{
	float3 c = make_float3(
		a.x*t->x.x + a.y*t->x.y + a.z*t->x.z,
		a.x*t->y.x + a.y*t->y.y + a.z*t->y.z,
		a.x*t->z.x + a.y*t->z.y + a.z*t->z.z);

	return c;
}

ccl_device_inline float3 transform_direction_transposed(const Transform *t, const float3 a)
{
	float3 x = make_float3(t->x.x, t->y.x, t->z.x);
	float3 y = make_float3(t->x.y, t->y.y, t->z.y);
	float3 z = make_float3(t->x.z, t->y.z, t->z.z);

	return make_float3(dot(x, a), dot(y, a), dot(z, a));
}

ccl_device_inline Transform transform_transpose(const Transform a)
{
	Transform t;

	t.x.x = a.x.x; t.x.y = a.y.x; t.x.z = a.z.x; t.x.w = a.w.x;
	t.y.x = a.x.y; t.y.y = a.y.y; t.y.z = a.z.y; t.y.w = a.w.y;
	t.z.x = a.x.z; t.z.y = a.y.z; t.z.z = a.z.z; t.z.w = a.w.z;
	t.w.x = a.x.w; t.w.y = a.y.w; t.w.z = a.z.w; t.w.w = a.w.w;

	return t;
}

ccl_device_inline Transform make_transform(float a, float b, float c, float d,
                                           float e, float f, float g, float h,
                                           float i, float j, float k, float l,
                                           float m, float n, float o, float p)
{
	Transform t;

	t.x.x = a; t.x.y = b; t.x.z = c; t.x.w = d;
	t.y.x = e; t.y.y = f; t.y.z = g; t.y.w = h;
	t.z.x = i; t.z.y = j; t.z.z = k; t.z.w = l;
	t.w.x = m; t.w.y = n; t.w.z = o; t.w.w = p;

	return t;
}

#ifndef __KERNEL_GPU__

ccl_device_inline Transform operator*(const Transform a, const Transform b)
{
	Transform c = transform_transpose(b);
	Transform t;

	t.x = make_float4(dot(a.x, c.x), dot(a.x, c.y), dot(a.x, c.z), dot(a.x, c.w));
	t.y = make_float4(dot(a.y, c.x), dot(a.y, c.y), dot(a.y, c.z), dot(a.y, c.w));
	t.z = make_float4(dot(a.z, c.x), dot(a.z, c.y), dot(a.z, c.z), dot(a.z, c.w));
	t.w = make_float4(dot(a.w, c.x), dot(a.w, c.y), dot(a.w, c.z), dot(a.w, c.w));

	return t;
}

ccl_device_inline void print_transform(const char *label, const Transform& t)
{
	print_float4(label, t.x);
	print_float4(label, t.y);
	print_float4(label, t.z);
	print_float4(label, t.w);
	printf("\n");
}

ccl_device_inline Transform transform_translate(float3 t)
{
	return make_transform(
		1, 0, 0, t.x,
		0, 1, 0, t.y,
		0, 0, 1, t.z,
		0, 0, 0, 1);
}

ccl_device_inline Transform transform_translate(float x, float y, float z)
{
	return transform_translate(make_float3(x, y, z));
}

ccl_device_inline Transform transform_scale(float3 s)
{
	return make_transform(
		s.x, 0, 0, 0,
		0, s.y, 0, 0,
		0, 0, s.z, 0,
		0, 0, 0, 1);
}

ccl_device_inline Transform transform_scale(float x, float y, float z)
{
	return transform_scale(make_float3(x, y, z));
}

ccl_device_inline Transform transform_perspective(float fov, float n, float f)
{
	Transform persp = make_transform(
		1, 0, 0, 0,
		0, 1, 0, 0,
		0, 0, f / (f - n), -f*n / (f - n),
		0, 0, 1, 0);

	float inv_angle = 1.0f/tanf(0.5f*fov);

	Transform scale = transform_scale(inv_angle, inv_angle, 1);

	return scale * persp;
}

ccl_device_inline Transform transform_rotate(float angle, float3 axis)
{
	float s = sinf(angle);
	float c = cosf(angle);
	float t = 1.0f - c;

	axis = normalize(axis);

	return make_transform(
		axis.x*axis.x*t + c,
		axis.x*axis.y*t - s*axis.z,
		axis.x*axis.z*t + s*axis.y,
		0.0f,

		axis.y*axis.x*t + s*axis.z,
		axis.y*axis.y*t + c,
		axis.y*axis.z*t - s*axis.x,
		0.0f,

		axis.z*axis.x*t - s*axis.y,
		axis.z*axis.y*t + s*axis.x,
		axis.z*axis.z*t + c,
		0.0f,

		0.0f, 0.0f, 0.0f, 1.0f);
}

ccl_device_inline Transform transform_euler(float3 euler)
{
	return
		transform_rotate(euler.x, make_float3(1.0f, 0.0f, 0.0f)) *
		transform_rotate(euler.y, make_float3(0.0f, 1.0f, 0.0f)) *
		transform_rotate(euler.z, make_float3(0.0f, 0.0f, 1.0f));
}

ccl_device_inline Transform transform_orthographic(float znear, float zfar)
{
	return transform_scale(1.0f, 1.0f, 1.0f / (zfar-znear)) *
		transform_translate(0.0f, 0.0f, -znear);
}

ccl_device_inline Transform transform_identity()
{
	return transform_scale(1.0f, 1.0f, 1.0f);
}

ccl_device_inline bool operator==(const Transform& A, const Transform& B)
{
	return memcmp(&A, &B, sizeof(Transform)) == 0;
}

ccl_device_inline bool operator!=(const Transform& A, const Transform& B)
{
	return !(A == B);
}

ccl_device_inline float3 transform_get_column(const Transform *t, int column)
{
	return make_float3(t->x[column], t->y[column], t->z[column]);
}

ccl_device_inline void transform_set_column(Transform *t, int column, float3 value)
{
	t->x[column] = value.x;
	t->y[column] = value.y;
	t->z[column] = value.z;
}

Transform transform_inverse(const Transform& a);

ccl_device_inline bool transform_uniform_scale(const Transform& tfm, float& scale)
{
	/* the epsilon here is quite arbitrary, but this function is only used for
	 * surface area and bump, where we except it to not be so sensitive */
	Transform ttfm = transform_transpose(tfm);
	float eps = 1e-6f;
	
	float sx = len_squared(float4_to_float3(tfm.x));
	float sy = len_squared(float4_to_float3(tfm.y));
	float sz = len_squared(float4_to_float3(tfm.z));
	float stx = len_squared(float4_to_float3(ttfm.x));
	float sty = len_squared(float4_to_float3(ttfm.y));
	float stz = len_squared(float4_to_float3(ttfm.z));

	if(fabsf(sx - sy) < eps && fabsf(sx - sz) < eps &&
	   fabsf(sx - stx) < eps && fabsf(sx - sty) < eps &&
	   fabsf(sx - stz) < eps)
	{
		scale = sx;
		return true;
	}

	return false;
}

ccl_device_inline bool transform_negative_scale(const Transform& tfm)
{
	float3 c0 = transform_get_column(&tfm, 0);
	float3 c1 = transform_get_column(&tfm, 1);
	float3 c2 = transform_get_column(&tfm, 2);

	return (dot(cross(c0, c1), c2) < 0.0f);
}

ccl_device_inline Transform transform_clear_scale(const Transform& tfm)
{
	Transform ntfm = tfm;

	transform_set_column(&ntfm, 0, normalize(transform_get_column(&ntfm, 0)));
	transform_set_column(&ntfm, 1, normalize(transform_get_column(&ntfm, 1)));
	transform_set_column(&ntfm, 2, normalize(transform_get_column(&ntfm, 2)));

	return ntfm;
}

#endif

/* Motion Transform */

ccl_device_inline float4 quat_interpolate(float4 q1, float4 q2, float t)
{
	/* use simpe nlerp instead of slerp. it's faster and almost the same */
	return normalize((1.0f - t)*q1 + t*q2);

#if 0
	/* note: this does not ensure rotation around shortest angle, q1 and q2
	 * are assumed to be matched already in transform_motion_decompose */
	float costheta = dot(q1, q2);

	/* possible optimization: it might be possible to precompute theta/qperp */

	if(costheta > 0.9995f) {
		/* linear interpolation in degenerate case */
		return normalize((1.0f - t)*q1 + t*q2);
	}
	else  {
		/* slerp */
		float theta = acosf(clamp(costheta, -1.0f, 1.0f));
		float4 qperp = normalize(q2 - q1 * costheta);
		float thetap = theta * t;
		return q1 * cosf(thetap) + qperp * sinf(thetap);
	}
#endif
}

ccl_device_inline Transform transform_quick_inverse(Transform M)
{
	/* possible optimization: can we avoid doing this altogether and construct
	 * the inverse matrix directly from negated translation, transposed rotation,
	 * scale can be inverted but what about shearing? */
	Transform R;
	float det = M.x.x*(M.z.z*M.y.y - M.z.y*M.y.z) - M.y.x*(M.z.z*M.x.y - M.z.y*M.x.z) + M.z.x*(M.y.z*M.x.y - M.y.y*M.x.z);

	det = (det != 0.0f)? 1.0f/det: 0.0f;

	float3 Rx = det*make_float3(M.z.z*M.y.y - M.z.y*M.y.z, M.z.y*M.x.z - M.z.z*M.x.y, M.y.z*M.x.y - M.y.y*M.x.z);
	float3 Ry = det*make_float3(M.z.x*M.y.z - M.z.z*M.y.x, M.z.z*M.x.x - M.z.x*M.x.z, M.y.x*M.x.z - M.y.z*M.x.x);
	float3 Rz = det*make_float3(M.z.y*M.y.x - M.z.x*M.y.y, M.z.x*M.x.y - M.z.y*M.x.x, M.y.y*M.x.x - M.y.x*M.x.y);
	float3 T = -make_float3(M.x.w, M.y.w, M.z.w);

	R.x = make_float4(Rx.x, Rx.y, Rx.z, dot(Rx, T));
	R.y = make_float4(Ry.x, Ry.y, Ry.z, dot(Ry, T));
	R.z = make_float4(Rz.x, Rz.y, Rz.z, dot(Rz, T));
	R.w = make_float4(0.0f, 0.0f, 0.0f, 1.0f);

	return R;
}

ccl_device_inline void transform_compose(Transform *tfm, const Transform *decomp)
{
	/* rotation */
	float q0, q1, q2, q3, qda, qdb, qdc, qaa, qab, qac, qbb, qbc, qcc;

	q0 = M_SQRT2_F * decomp->x.w;
	q1 = M_SQRT2_F * decomp->x.x;
	q2 = M_SQRT2_F * decomp->x.y;
	q3 = M_SQRT2_F * decomp->x.z;

	qda = q0*q1;
	qdb = q0*q2;
	qdc = q0*q3;
	qaa = q1*q1;
	qab = q1*q2;
	qac = q1*q3;
	qbb = q2*q2;
	qbc = q2*q3;
	qcc = q3*q3;

	float3 rotation_x = make_float3(1.0f-qbb-qcc, -qdc+qab, qdb+qac);
	float3 rotation_y = make_float3(qdc+qab, 1.0f-qaa-qcc, -qda+qbc);
	float3 rotation_z = make_float3(-qdb+qac, qda+qbc, 1.0f-qaa-qbb);

	/* scale */
	float3 scale_x = make_float3(decomp->y.w, decomp->z.z, decomp->w.y);
	float3 scale_y = make_float3(decomp->z.x, decomp->z.w, decomp->w.z);
	float3 scale_z = make_float3(decomp->z.y, decomp->w.x, decomp->w.w);

	/* compose with translation */
	tfm->x = make_float4(dot(rotation_x, scale_x), dot(rotation_x, scale_y), dot(rotation_x, scale_z), decomp->y.x);
	tfm->y = make_float4(dot(rotation_y, scale_x), dot(rotation_y, scale_y), dot(rotation_y, scale_z), decomp->y.y);
	tfm->z = make_float4(dot(rotation_z, scale_x), dot(rotation_z, scale_y), dot(rotation_z, scale_z), decomp->y.z);
	tfm->w = make_float4(0.0f, 0.0f, 0.0f, 1.0f);
}

/* Disabled for now, need arc-length parametrization for constant speed motion.
 * #define CURVED_MOTION_INTERPOLATE */

ccl_device void transform_motion_interpolate(Transform *tfm, const DecompMotionTransform *motion, float t)
{
	/* possible optimization: is it worth it adding a check to skip scaling?
	 * it's probably quite uncommon to have scaling objects. or can we skip
	 * just shearing perhaps? */
	Transform decomp;

#ifdef CURVED_MOTION_INTERPOLATE
	/* 3 point bezier curve interpolation for position */
	float3 Ppre = float4_to_float3(motion->pre_y);
	float3 Pmid = float4_to_float3(motion->mid.y);
	float3 Ppost = float4_to_float3(motion->post_y);

	float3 Pcontrol = 2.0f*Pmid - 0.5f*(Ppre + Ppost);
	float3 P = Ppre*t*t + Pcontrol*2.0f*t*(1.0f - t) + Ppost*(1.0f - t)*(1.0f - t);

	decomp.y.x = P.x;
	decomp.y.y = P.y;
	decomp.y.z = P.z;
#endif

	/* linear interpolation for rotation and scale */
	if(t < 0.5f) {
		t *= 2.0f;

		decomp.x = quat_interpolate(motion->pre_x, motion->mid.x, t);
#ifdef CURVED_MOTION_INTERPOLATE
		decomp.y.w = (1.0f - t)*motion->pre_y.w + t*motion->mid.y.w;
#else
		decomp.y = (1.0f - t)*motion->pre_y + t*motion->mid.y;
#endif
	}
	else {
		t = (t - 0.5f)*2.0f;

		decomp.x = quat_interpolate(motion->mid.x, motion->post_x, t);
#ifdef CURVED_MOTION_INTERPOLATE
		decomp.y.w = (1.0f - t)*motion->mid.y.w + t*motion->post_y.w;
#else
		decomp.y = (1.0f - t)*motion->mid.y + t*motion->post_y;
#endif
	}

	decomp.z = motion->mid.z;
	decomp.w = motion->mid.w;

	/* compose rotation, translation, scale into matrix */
	transform_compose(tfm, &decomp);
}

#ifndef __KERNEL_GPU__

ccl_device_inline bool operator==(const MotionTransform& A, const MotionTransform& B)
{
	return (A.pre == B.pre && A.post == B.post);
}

float4 transform_to_quat(const Transform& tfm);
void transform_motion_decompose(DecompMotionTransform *decomp, const MotionTransform *motion, const Transform *mid);

#endif

CCL_NAMESPACE_END

#endif /* __UTIL_TRANSFORM_H__ */

