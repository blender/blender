/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#ifndef __UTIL_TRANSFORM_H__
#define __UTIL_TRANSFORM_H__

#ifndef __KERNEL_GPU__
#  include <string.h>
#endif

#include "util/math.h"
#include "util/types.h"

#ifndef __KERNEL_GPU__
#  include "util/system.h"
#endif

CCL_NAMESPACE_BEGIN

/* Affine transformation, stored as 4x3 matrix. */

typedef struct Transform {
  float4 x, y, z;

#ifndef __KERNEL_GPU__
  float4 operator[](int i) const
  {
    return *(&x + i);
  }
  float4 &operator[](int i)
  {
    return *(&x + i);
  }
#endif
} Transform;

/* Transform decomposed in rotation/translation/scale. we use the same data
 * structure as Transform, and tightly pack decomposition into it. first the
 * rotation (4), then translation (3), then 3x3 scale matrix (9). */

typedef struct DecomposedTransform {
  float4 x, y, z, w;
} DecomposedTransform;

CCL_NAMESPACE_END

#include "util/transform_inverse.h"

CCL_NAMESPACE_BEGIN

/* Functions */

#ifdef __KERNEL_METAL__
/* transform_point specialized for ccl_global */
ccl_device_inline float3 transform_point(ccl_global const Transform *t, const float3 a)
{
  ccl_global const float3x3 &b(*(ccl_global const float3x3 *)t);
  return (a * b).xyz + make_float3(t->x.w, t->y.w, t->z.w);
}
#endif

ccl_device_inline float3 transform_point(ccl_private const Transform *t, const float3 a)
{
  /* TODO(sergey): Disabled for now, causes crashes in certain cases. */
#if defined(__KERNEL_SSE__) && defined(__KERNEL_SSE2__)
  const float4 aa(a.m128);

  float4 x(_mm_loadu_ps(&t->x.x));
  float4 y(_mm_loadu_ps(&t->y.x));
  float4 z(_mm_loadu_ps(&t->z.x));
  float4 w(_mm_set_ps(1.0f, 0.0f, 0.0f, 0.0f));

  _MM_TRANSPOSE4_PS(x.m128, y.m128, z.m128, w.m128);

  float4 tmp = w;
  tmp = madd(shuffle<2>(aa), z, tmp);
  tmp = madd(shuffle<1>(aa), y, tmp);
  tmp = madd(shuffle<0>(aa), x, tmp);

  return float3(tmp.m128);
#elif defined(__KERNEL_METAL__)
  ccl_private const float3x3 &b(*(ccl_private const float3x3 *)t);
  return (a * b).xyz + make_float3(t->x.w, t->y.w, t->z.w);
#else
  float3 c = make_float3(a.x * t->x.x + a.y * t->x.y + a.z * t->x.z + t->x.w,
                         a.x * t->y.x + a.y * t->y.y + a.z * t->y.z + t->y.w,
                         a.x * t->z.x + a.y * t->z.y + a.z * t->z.z + t->z.w);

  return c;
#endif
}

ccl_device_inline float3 transform_direction(ccl_private const Transform *t, const float3 a)
{
#if defined(__KERNEL_SSE__) && defined(__KERNEL_SSE2__)
  const float4 aa(a.m128);

  float4 x(_mm_loadu_ps(&t->x.x));
  float4 y(_mm_loadu_ps(&t->y.x));
  float4 z(_mm_loadu_ps(&t->z.x));
  float4 w(_mm_setzero_ps());

  _MM_TRANSPOSE4_PS(x.m128, y.m128, z.m128, w.m128);

  float4 tmp = shuffle<2>(aa) * z;
  tmp = madd(shuffle<1>(aa), y, tmp);
  tmp = madd(shuffle<0>(aa), x, tmp);

  return float3(tmp.m128);
#elif defined(__KERNEL_METAL__)
  ccl_private const float3x3 &b(*(ccl_private const float3x3 *)t);
  return (a * b).xyz;
#else
  float3 c = make_float3(a.x * t->x.x + a.y * t->x.y + a.z * t->x.z,
                         a.x * t->y.x + a.y * t->y.y + a.z * t->y.z,
                         a.x * t->z.x + a.y * t->z.y + a.z * t->z.z);

  return c;
#endif
}

ccl_device_inline float3 transform_direction_transposed(ccl_private const Transform *t,
                                                        const float3 a)
{
  float3 x = make_float3(t->x.x, t->y.x, t->z.x);
  float3 y = make_float3(t->x.y, t->y.y, t->z.y);
  float3 z = make_float3(t->x.z, t->y.z, t->z.z);

  return make_float3(dot(x, a), dot(y, a), dot(z, a));
}

ccl_device_inline Transform make_transform(float a,
                                           float b,
                                           float c,
                                           float d,
                                           float e,
                                           float f,
                                           float g,
                                           float h,
                                           float i,
                                           float j,
                                           float k,
                                           float l)
{
  Transform t;

  t.x.x = a;
  t.x.y = b;
  t.x.z = c;
  t.x.w = d;
  t.y.x = e;
  t.y.y = f;
  t.y.z = g;
  t.y.w = h;
  t.z.x = i;
  t.z.y = j;
  t.z.z = k;
  t.z.w = l;

  return t;
}

ccl_device_inline Transform euler_to_transform(const float3 euler)
{
  float cx = cosf(euler.x);
  float cy = cosf(euler.y);
  float cz = cosf(euler.z);
  float sx = sinf(euler.x);
  float sy = sinf(euler.y);
  float sz = sinf(euler.z);

  Transform t;
  t.x.x = cy * cz;
  t.y.x = cy * sz;
  t.z.x = -sy;

  t.x.y = sy * sx * cz - cx * sz;
  t.y.y = sy * sx * sz + cx * cz;
  t.z.y = cy * sx;

  t.x.z = sy * cx * cz + sx * sz;
  t.y.z = sy * cx * sz - sx * cz;
  t.z.z = cy * cx;

  t.x.w = t.y.w = t.z.w = 0.0f;
  return t;
}

/* Constructs a coordinate frame from a normalized normal. */
ccl_device_inline Transform make_transform_frame(float3 N)
{
  const float3 dx0 = cross(make_float3(1.0f, 0.0f, 0.0f), N);
  const float3 dx1 = cross(make_float3(0.0f, 1.0f, 0.0f), N);
  const float3 dx = normalize((dot(dx0, dx0) > dot(dx1, dx1)) ? dx0 : dx1);
  const float3 dy = normalize(cross(N, dx));
  return make_transform(dx.x, dx.y, dx.z, 0.0f, dy.x, dy.y, dy.z, 0.0f, N.x, N.y, N.z, 0.0f);
}

#if !defined(__KERNEL_METAL__)
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
#endif

#ifndef __KERNEL_GPU__

ccl_device_inline Transform transform_zero()
{
  Transform zero = {zero_float4(), zero_float4(), zero_float4()};
  return zero;
}

ccl_device_inline void print_transform(const char *label, const Transform &t)
{
  print_float4(label, t.x);
  print_float4(label, t.y);
  print_float4(label, t.z);
  printf("\n");
}

ccl_device_inline Transform transform_translate(float3 t)
{
  return make_transform(1, 0, 0, t.x, 0, 1, 0, t.y, 0, 0, 1, t.z);
}

ccl_device_inline Transform transform_translate(float x, float y, float z)
{
  return transform_translate(make_float3(x, y, z));
}

ccl_device_inline Transform transform_scale(float3 s)
{
  return make_transform(s.x, 0, 0, 0, 0, s.y, 0, 0, 0, 0, s.z, 0);
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

  return make_transform(axis.x * axis.x * t + c,
                        axis.x * axis.y * t - s * axis.z,
                        axis.x * axis.z * t + s * axis.y,
                        0.0f,

                        axis.y * axis.x * t + s * axis.z,
                        axis.y * axis.y * t + c,
                        axis.y * axis.z * t - s * axis.x,
                        0.0f,

                        axis.z * axis.x * t - s * axis.y,
                        axis.z * axis.y * t + s * axis.x,
                        axis.z * axis.z * t + c,
                        0.0f);
}

/* Euler is assumed to be in XYZ order. */
ccl_device_inline Transform transform_euler(float3 euler)
{
  return transform_rotate(euler.z, make_float3(0.0f, 0.0f, 1.0f)) *
         transform_rotate(euler.y, make_float3(0.0f, 1.0f, 0.0f)) *
         transform_rotate(euler.x, make_float3(1.0f, 0.0f, 0.0f));
}

ccl_device_inline Transform transform_identity()
{
  return transform_scale(1.0f, 1.0f, 1.0f);
}

ccl_device_inline bool operator==(const Transform &A, const Transform &B)
{
  return memcmp(&A, &B, sizeof(Transform)) == 0;
}

ccl_device_inline bool operator!=(const Transform &A, const Transform &B)
{
  return !(A == B);
}

ccl_device_inline bool transform_equal_threshold(const Transform &A,
                                                 const Transform &B,
                                                 const float threshold)
{
  for (int x = 0; x < 3; x++) {
    for (int y = 0; y < 4; y++) {
      if (fabsf(A[x][y] - B[x][y]) > threshold) {
        return false;
      }
    }
  }

  return true;
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

Transform transform_transposed_inverse(const Transform &a);

ccl_device_inline bool transform_uniform_scale(const Transform &tfm, float &scale)
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

  if (fabsf(sx - sy) < eps && fabsf(sx - sz) < eps && fabsf(sx - stx) < eps &&
      fabsf(sx - sty) < eps && fabsf(sx - stz) < eps)
  {
    scale = sx;
    return true;
  }

  return false;
}

ccl_device_inline bool transform_negative_scale(const Transform &tfm)
{
  float3 c0 = transform_get_column(&tfm, 0);
  float3 c1 = transform_get_column(&tfm, 1);
  float3 c2 = transform_get_column(&tfm, 2);

  return (dot(cross(c0, c1), c2) < 0.0f);
}

ccl_device_inline Transform transform_clear_scale(const Transform &tfm)
{
  Transform ntfm = tfm;

  transform_set_column(&ntfm, 0, normalize(transform_get_column(&ntfm, 0)));
  transform_set_column(&ntfm, 1, normalize(transform_get_column(&ntfm, 1)));
  transform_set_column(&ntfm, 2, normalize(transform_get_column(&ntfm, 2)));

  return ntfm;
}

ccl_device_inline Transform transform_empty()
{
  return make_transform(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
}

#endif

/* Motion Transform */

ccl_device_inline float4 quat_interpolate(float4 q1, float4 q2, float t)
{
  /* Optix and MetalRT are using linear interpolation to interpolate motion transformations. */
#if defined(__KERNEL_GPU_RAYTRACING__)
  return normalize((1.0f - t) * q1 + t * q2);
#else  /* defined(__KERNEL_GPU_RAYTRACING__) */
  /* NOTE: this does not ensure rotation around shortest angle, q1 and q2
   * are assumed to be matched already in transform_motion_decompose */
  float costheta = dot(q1, q2);

  /* possible optimization: it might be possible to precompute theta/qperp */

  if (costheta > 0.9995f) {
    /* linear interpolation in degenerate case */
    return normalize((1.0f - t) * q1 + t * q2);
  }
  else {
    /* slerp */
    float theta = acosf(clamp(costheta, -1.0f, 1.0f));
    float4 qperp = normalize(q2 - q1 * costheta);
    float thetap = theta * t;
    return q1 * cosf(thetap) + qperp * sinf(thetap);
  }
#endif /* defined(__KERNEL_GPU_RAYTRACING__) */
}

#ifndef __KERNEL_GPU__
void transform_inverse_cpu_sse41(const Transform &tfm, Transform &itfm);
void transform_inverse_cpu_avx2(const Transform &tfm, Transform &itfm);
#endif

ccl_device_inline Transform transform_inverse(const Transform tfm)
{
  /* Optimized transform implementations. */
#ifndef __KERNEL_GPU__
  if (system_cpu_support_avx2()) {
    Transform itfm;
    transform_inverse_cpu_avx2(tfm, itfm);
    return itfm;
  }
  else if (system_cpu_support_sse41()) {
    Transform itfm;
    transform_inverse_cpu_sse41(tfm, itfm);
    return itfm;
  }
#endif

  return transform_inverse_impl(tfm);
}

ccl_device_inline void transform_compose(ccl_private Transform *tfm,
                                         ccl_private const DecomposedTransform *decomp)
{
  /* rotation */
  float q0, q1, q2, q3, qda, qdb, qdc, qaa, qab, qac, qbb, qbc, qcc;

  q0 = M_SQRT2_F * decomp->x.w;
  q1 = M_SQRT2_F * decomp->x.x;
  q2 = M_SQRT2_F * decomp->x.y;
  q3 = M_SQRT2_F * decomp->x.z;

  qda = q0 * q1;
  qdb = q0 * q2;
  qdc = q0 * q3;
  qaa = q1 * q1;
  qab = q1 * q2;
  qac = q1 * q3;
  qbb = q2 * q2;
  qbc = q2 * q3;
  qcc = q3 * q3;

  float3 rotation_x = make_float3(1.0f - qbb - qcc, -qdc + qab, qdb + qac);
  float3 rotation_y = make_float3(qdc + qab, 1.0f - qaa - qcc, -qda + qbc);
  float3 rotation_z = make_float3(-qdb + qac, qda + qbc, 1.0f - qaa - qbb);

  /* scale */
  float3 scale_x = make_float3(decomp->y.w, decomp->z.z, decomp->w.y);
  float3 scale_y = make_float3(decomp->z.x, decomp->z.w, decomp->w.z);
  float3 scale_z = make_float3(decomp->z.y, decomp->w.x, decomp->w.w);

  /* compose with translation */
  tfm->x = make_float4(
      dot(rotation_x, scale_x), dot(rotation_x, scale_y), dot(rotation_x, scale_z), decomp->y.x);
  tfm->y = make_float4(
      dot(rotation_y, scale_x), dot(rotation_y, scale_y), dot(rotation_y, scale_z), decomp->y.y);
  tfm->z = make_float4(
      dot(rotation_z, scale_x), dot(rotation_z, scale_y), dot(rotation_z, scale_z), decomp->y.z);
}

/* Interpolate from array of decomposed transforms. */
ccl_device void transform_motion_array_interpolate(ccl_private Transform *tfm,
                                                   ccl_global const DecomposedTransform *motion,
                                                   uint numsteps,
                                                   float time)
{
  /* Figure out which steps we need to interpolate. */
  int maxstep = numsteps - 1;
  int step = min((int)(time * maxstep), maxstep - 1);
  float t = time * maxstep - step;

  ccl_global const DecomposedTransform *a = motion + step;
  ccl_global const DecomposedTransform *b = motion + step + 1;

  /* Interpolate rotation, translation and scale. */
  DecomposedTransform decomp;
  decomp.x = quat_interpolate(a->x, b->x, t);
  decomp.y = (1.0f - t) * a->y + t * b->y;
  decomp.z = (1.0f - t) * a->z + t * b->z;
  decomp.w = (1.0f - t) * a->w + t * b->w;

  /* Compose rotation, translation, scale into matrix. */
  transform_compose(tfm, &decomp);
}

ccl_device_inline bool transform_isfinite_safe(ccl_private Transform *tfm)
{
  return isfinite_safe(tfm->x) && isfinite_safe(tfm->y) && isfinite_safe(tfm->z);
}

ccl_device_inline bool transform_decomposed_isfinite_safe(ccl_private DecomposedTransform *decomp)
{
  return isfinite_safe(decomp->x) && isfinite_safe(decomp->y) && isfinite_safe(decomp->z) &&
         isfinite_safe(decomp->w);
}

#ifndef __KERNEL_GPU__

class BoundBox2D;

ccl_device_inline bool operator==(const DecomposedTransform &A, const DecomposedTransform &B)
{
  return memcmp(&A, &B, sizeof(DecomposedTransform)) == 0;
}

float4 transform_to_quat(const Transform &tfm);
void transform_motion_decompose(DecomposedTransform *decomp, const Transform *motion, size_t size);
Transform transform_from_viewplane(BoundBox2D &viewplane);

#endif

/* TODO: This can be removed when we know if no devices will require explicit
 * address space qualifiers for this case. */

#define transform_point_auto transform_point
#define transform_direction_auto transform_direction
#define transform_direction_transposed_auto transform_direction_transposed

CCL_NAMESPACE_END

#endif /* __UTIL_TRANSFORM_H__ */
