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

#include "util/util_math.h"
#include "util/util_types.h"

CCL_NAMESPACE_BEGIN

/* Affine transformation, stored as 4x3 matrix. */

typedef struct Transform {
	float4 x, y, z;

#ifndef __KERNEL_GPU__
	float4 operator[](int i) const { return *(&x + i); }
	float4& operator[](int i) { return *(&x + i); }
#endif
} Transform;

/* Transform decomposed in rotation/translation/scale. we use the same data
 * structure as Transform, and tightly pack decomposition into it. first the
 * rotation (4), then translation (3), then 3x3 scale matrix (9). */

typedef struct DecomposedTransform {
	float4 x, y, z, w;
} DecomposedTransform;

/* Functions */

ccl_device_inline float3 transform_point(const Transform *t, const float3 a)
{
	/* TODO(sergey): Disabled for now, causes crashes in certain cases. */
#if defined(__KERNEL_SSE__) && defined(__KERNEL_SSE2__)
	ssef x, y, z, w, aa;
	aa = a.m128;

	x = _mm_loadu_ps(&t->x.x);
	y = _mm_loadu_ps(&t->y.x);
	z = _mm_loadu_ps(&t->z.x);
	w = _mm_set_ps(1.0f, 0.0f, 0.0f, 0.0f);

	_MM_TRANSPOSE4_PS(x, y, z, w);

	ssef tmp = shuffle<0>(aa) * x;
	tmp = madd(shuffle<1>(aa), y, tmp);
	tmp = madd(shuffle<2>(aa), z, tmp);
	tmp += w;

	return float3(tmp.m128);
#else
	float3 c = make_float3(
		a.x*t->x.x + a.y*t->x.y + a.z*t->x.z + t->x.w,
		a.x*t->y.x + a.y*t->y.y + a.z*t->y.z + t->y.w,
		a.x*t->z.x + a.y*t->z.y + a.z*t->z.z + t->z.w);

	return c;
#endif
}

ccl_device_inline float3 transform_direction(const Transform *t, const float3 a)
{
#if defined(__KERNEL_SSE__) && defined(__KERNEL_SSE2__)
	ssef x, y, z, w, aa;
	aa = a.m128;
	x = _mm_loadu_ps(&t->x.x);
	y = _mm_loadu_ps(&t->y.x);
	z = _mm_loadu_ps(&t->z.x);
	w = _mm_setzero_ps();

	_MM_TRANSPOSE4_PS(x, y, z, w);

	ssef tmp = shuffle<0>(aa) * x;
	tmp = madd(shuffle<1>(aa), y, tmp);
	tmp = madd(shuffle<2>(aa), z, tmp);

	return float3(tmp.m128);
#else
	float3 c = make_float3(
		a.x*t->x.x + a.y*t->x.y + a.z*t->x.z,
		a.x*t->y.x + a.y*t->y.y + a.z*t->y.z,
		a.x*t->z.x + a.y*t->z.y + a.z*t->z.z);

	return c;
#endif
}

ccl_device_inline float3 transform_direction_transposed(const Transform *t, const float3 a)
{
	float3 x = make_float3(t->x.x, t->y.x, t->z.x);
	float3 y = make_float3(t->x.y, t->y.y, t->z.y);
	float3 z = make_float3(t->x.z, t->y.z, t->z.z);

	return make_float3(dot(x, a), dot(y, a), dot(z, a));
}

ccl_device_inline Transform make_transform(float a, float b, float c, float d,
                                           float e, float f, float g, float h,
                                           float i, float j, float k, float l)
{
	Transform t;

	t.x.x = a; t.x.y = b; t.x.z = c; t.x.w = d;
	t.y.x = e; t.y.y = f; t.y.z = g; t.y.w = h;
	t.z.x = i; t.z.y = j; t.z.z = k; t.z.w = l;

	return t;
}

/* Constructs a coordinate frame from a normalized normal. */
ccl_device_inline Transform make_transform_frame(float3 N)
{
	const float3 dx0 = cross(make_float3(1.0f, 0.0f, 0.0f), N);
	const float3 dx1 = cross(make_float3(0.0f, 1.0f, 0.0f), N);
	const float3 dx = normalize((dot(dx0,dx0) > dot(dx1,dx1))?  dx0: dx1);
	const float3 dy = normalize(cross(N, dx));
	return make_transform(dx.x, dx.y, dx.z, 0.0f,
	                      dy.x, dy.y, dy.z, 0.0f,
	                      N.x , N.y,  N.z,  0.0f);
}

#ifndef __KERNEL_GPU__

ccl_device_inline Transform operator*(const Transform a, const Transform b)
{
	float4 c_x = make_float4(b.x.x, b.y.x, b.z.x, 0.0f);
	float4 c_y = make_float4(b.x.y, b.y.y, b.z.y, 0.0f);
	float4 c_z = make_float4(b.x.z, b.y.z, b.z.z, 0.0f);
	float4 c_w = make_float4(b.x.w, b.y.w, b.z.w, 1.0f);

	Transform t;
	t.x = make_float4(dot(a.x, c_x), dot(a.x, c_y), dot(a.x, c_z), dot(a.x, c_w));
	t.y = make_float4(dot(a.y, c_x), dot(a.y, c_y), dot(a.y, c_z), dot(a.y, c_w));
	t.z = make_float4(dot(a.z, c_x), dot(a.z, c_y), dot(a.z, c_z), dot(a.z, c_w));

	return t;
}

ccl_device_inline void print_transform(const char *label, const Transform& t)
{
	print_float4(label, t.x);
	print_float4(label, t.y);
	print_float4(label, t.z);
	printf("\n");
}

ccl_device_inline Transform transform_translate(float3 t)
{
	return make_transform(
		1, 0, 0, t.x,
		0, 1, 0, t.y,
		0, 0, 1, t.z);
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
		0, 0, s.z, 0);
}

ccl_device_inline Transform transform_scale(float x, float y, float z)
{
	return transform_scale(make_float3(x, y, z));
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
		0.0f);
}

/* Euler is assumed to be in XYZ order. */
ccl_device_inline Transform transform_euler(float3 euler)
{
	return
		transform_rotate(euler.z, make_float3(0.0f, 0.0f, 1.0f)) *
		transform_rotate(euler.y, make_float3(0.0f, 1.0f, 0.0f)) *
		transform_rotate(euler.x, make_float3(1.0f, 0.0f, 0.0f));
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
Transform transform_transposed_inverse(const Transform& a);

ccl_device_inline bool transform_uniform_scale(const Transform& tfm, float& scale)
{
	/* the epsilon here is quite arbitrary, but this function is only used for
	 * surface area and bump, where we expect it to not be so sensitive */
	float eps = 1e-6f;

	float sx = len_squared(float4_to_float3(tfm.x));
	float sy = len_squared(float4_to_float3(tfm.y));
	float sz = len_squared(float4_to_float3(tfm.z));
	float stx = len_squared(transform_get_column(&tfm, 0));
	float sty = len_squared(transform_get_column(&tfm, 1));
	float stz = len_squared(transform_get_column(&tfm, 2));

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

ccl_device_inline Transform transform_empty()
{
	return make_transform(
		0, 0, 0, 0,
		0, 0, 0, 0,
		0, 0, 0, 0);
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
	if(det == 0.0f) {
		M.x.x += 1e-8f;
		M.y.y += 1e-8f;
		M.z.z += 1e-8f;
		det = M.x.x*(M.z.z*M.y.y - M.z.y*M.y.z) - M.y.x*(M.z.z*M.x.y - M.z.y*M.x.z) + M.z.x*(M.y.z*M.x.y - M.y.y*M.x.z);
	}
	det = (det != 0.0f)? 1.0f/det: 0.0f;

	float3 Rx = det*make_float3(M.z.z*M.y.y - M.z.y*M.y.z, M.z.y*M.x.z - M.z.z*M.x.y, M.y.z*M.x.y - M.y.y*M.x.z);
	float3 Ry = det*make_float3(M.z.x*M.y.z - M.z.z*M.y.x, M.z.z*M.x.x - M.z.x*M.x.z, M.y.x*M.x.z - M.y.z*M.x.x);
	float3 Rz = det*make_float3(M.z.y*M.y.x - M.z.x*M.y.y, M.z.x*M.x.y - M.z.y*M.x.x, M.y.y*M.x.x - M.y.x*M.x.y);
	float3 T = -make_float3(M.x.w, M.y.w, M.z.w);

	R.x = make_float4(Rx.x, Rx.y, Rx.z, dot(Rx, T));
	R.y = make_float4(Ry.x, Ry.y, Ry.z, dot(Ry, T));
	R.z = make_float4(Rz.x, Rz.y, Rz.z, dot(Rz, T));

	return R;
}

ccl_device_inline void transform_compose(Transform *tfm, const DecomposedTransform *decomp)
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
}

/* Interpolate from array of decomposed transforms. */
ccl_device void transform_motion_array_interpolate(Transform *tfm,
                                                   const ccl_global DecomposedTransform *motion,
                                                   uint numsteps,
                                                   float time)
{
	/* Figure out which steps we need to interpolate. */
	int maxstep = numsteps-1;
	int step = min((int)(time*maxstep), maxstep-1);
	float t = time*maxstep - step;

	const ccl_global DecomposedTransform *a = motion + step;
	const ccl_global DecomposedTransform *b = motion + step + 1;

	/* Interpolate rotation, translation and scale. */
	DecomposedTransform decomp;
	decomp.x = quat_interpolate(a->x, b->x, t);
	decomp.y = (1.0f - t)*a->y + t*b->y;
	decomp.z = (1.0f - t)*a->z + t*b->z;
	decomp.w = (1.0f - t)*a->w + t*b->w;

	/* Compose rotation, translation, scale into matrix. */
	transform_compose(tfm, &decomp);
}

#ifndef __KERNEL_GPU__

class BoundBox2D;

ccl_device_inline bool operator==(const DecomposedTransform& A, const DecomposedTransform& B)
{
	return memcmp(&A, &B, sizeof(DecomposedTransform)) == 0;
}

float4 transform_to_quat(const Transform& tfm);
void transform_motion_decompose(DecomposedTransform *decomp, const Transform *motion, size_t size);
Transform transform_from_viewplane(BoundBox2D& viewplane);

#endif

/* TODO(sergey): This is only for until we've got OpenCL 2.0
 * on all devices we consider supported. It'll be replaced with
 * generic address space.
 */

#ifdef __KERNEL_OPENCL__

#define OPENCL_TRANSFORM_ADDRSPACE_GLUE(a, b) a ## b
#define OPENCL_TRANSFORM_ADDRSPACE_DECLARE(function) \
ccl_device_inline float3 OPENCL_TRANSFORM_ADDRSPACE_GLUE(function, _addrspace)( \
    ccl_addr_space const Transform *t, const float3 a) \
{ \
  Transform private_tfm = *t; \
  return function(&private_tfm, a); \
}

OPENCL_TRANSFORM_ADDRSPACE_DECLARE(transform_point)
OPENCL_TRANSFORM_ADDRSPACE_DECLARE(transform_direction)
OPENCL_TRANSFORM_ADDRSPACE_DECLARE(transform_direction_transposed)

#  undef OPENCL_TRANSFORM_ADDRSPACE_DECLARE
#  undef OPENCL_TRANSFORM_ADDRSPACE_GLUE
#  define transform_point_auto transform_point_addrspace
#  define transform_direction_auto transform_direction_addrspace
#  define transform_direction_transposed_auto transform_direction_transposed_addrspace
#else
#  define transform_point_auto transform_point
#  define transform_direction_auto transform_direction
#  define transform_direction_transposed_auto transform_direction_transposed
#endif

CCL_NAMESPACE_END

#endif /* __UTIL_TRANSFORM_H__ */
