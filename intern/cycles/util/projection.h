/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "util/transform.h"

CCL_NAMESPACE_BEGIN

ccl_device float2 direction_to_spherical(const float3 dir)
{
  const float theta = safe_acosf(dir.z);
  const float phi = atan2f(dir.y, dir.x);

  return make_float2(theta, phi);
}

ccl_device float3 spherical_to_direction(const float theta, const float phi)
{
  return make_float3(sinf(theta) * cosf(phi), sinf(theta) * sinf(phi), cosf(theta));
}

ccl_device float3 spherical_cos_to_direction(const float cos_theta, const float phi)
{
  const float sin_theta = sin_from_cos(cos_theta);
  return make_float3(sin_theta * cosf(phi), sin_theta * sinf(phi), cos_theta);
}

ccl_device_inline float2 polar_to_cartesian(const float r, const float phi)
{
  return make_float2(r * cosf(phi), r * sinf(phi));
}

/* Transform p from a local coordinate system (spanned by X and Y) into global coordinates. */
template<class T> ccl_device_inline T to_global(const float2 p, const T X, const T Y)
{
  return p.x * X + p.y * Y;
}

/* Transform p from a local coordinate system (spanned by X, Y and Z) into global coordinates. */
template<class T> ccl_device_inline T to_global(const float3 p, const T X, const T Y, const T Z)
{
  return p.x * X + p.y * Y + p.z * Z;
}

/* Transform p from global coordinates into a local coordinate system (spanned by X and Y). */
template<class T> ccl_device_inline float2 to_local(const T p, const T X, const T Y)
{
  return make_float2(dot(p, X), dot(p, Y));
}

/* Transform p from global coordinates into a local coordinate system (spanned by X, Y and Z). */
template<class T> ccl_device_inline float3 to_local(const T p, const T X, const T Y, const T Z)
{
  return make_float3(dot(p, X), dot(p, Y), dot(p, Z));
}

ccl_device_inline float3 disk_to_hemisphere(const float2 p)
{
  return make_float3(p.x, p.y, safe_sqrtf(1.0f - len_squared(p)));
}

/* 4x4 projection matrix, perspective or orthographic. */

struct ProjectionTransform {
  float4 x, y, z, w; /* rows */

#ifndef __KERNEL_GPU__
  ProjectionTransform() = default;

  explicit ProjectionTransform(const Transform &tfm)
      : x(tfm.x), y(tfm.y), z(tfm.z), w(make_float4(0.0f, 0.0f, 0.0f, 1.0f))
  {
  }
#endif
};

struct PerspectiveMotionTransform {
  ProjectionTransform pre;
  ProjectionTransform post;
};

CCL_NAMESPACE_END

#if !defined(__KERNEL_METAL__)
#  include "util/projection_inverse.h"
#endif

CCL_NAMESPACE_BEGIN

/* Functions */

ccl_device_inline float3 transform_perspective(const ccl_private ProjectionTransform *t,
                                               const float3 a)
{
  const float4 b = make_float4(a.x, a.y, a.z, 1.0f);
  const float3 c = make_float3(dot(t->x, b), dot(t->y, b), dot(t->z, b));
  const float w = dot(t->w, b);

  return (w != 0.0f) ? c / w : zero_float3();
}

ccl_device_inline float3 transform_perspective_direction(const ccl_private ProjectionTransform *t,
                                                         const float3 a)
{
  const float3 c = make_float3(a.x * t->x.x + a.y * t->x.y + a.z * t->x.z,
                               a.x * t->y.x + a.y * t->y.y + a.z * t->y.z,
                               a.x * t->z.x + a.y * t->z.y + a.z * t->z.z);

  return c;
}

ccl_device_inline ProjectionTransform make_projection(const float a,
                                                      const float b,
                                                      const float c,
                                                      const float d,
                                                      const float e,
                                                      const float f,
                                                      const float g,
                                                      const float h,
                                                      const float i,
                                                      const float j,
                                                      const float k,
                                                      const float l,
                                                      const float m,
                                                      const float n,
                                                      const float o,
                                                      const float p)
{
  ProjectionTransform t;

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
  t.w.x = m;
  t.w.y = n;
  t.w.z = o;
  t.w.w = p;

  return t;
}

ccl_device_inline ProjectionTransform projection_identity()
{
  return make_projection(1.0f,
                         0.0f,
                         0.0f,
                         0.0f,
                         0.0f,
                         1.0f,
                         0.0f,
                         0.0f,
                         0.0f,
                         0.0f,
                         1.0f,
                         0.0f,
                         0.0f,
                         0.0f,
                         0.0f,
                         1.0f);
}

#ifndef __KERNEL_GPU__
ccl_device_inline Transform projection_to_transform(const ProjectionTransform &a)
{
  Transform tfm = {a.x, a.y, a.z};
  return tfm;
}
#endif

ccl_device_inline ProjectionTransform projection_transpose(const ProjectionTransform a)
{
  ProjectionTransform t;

  t.x.x = a.x.x;
  t.x.y = a.y.x;
  t.x.z = a.z.x;
  t.x.w = a.w.x;
  t.y.x = a.x.y;
  t.y.y = a.y.y;
  t.y.z = a.z.y;
  t.y.w = a.w.y;
  t.z.x = a.x.z;
  t.z.y = a.y.z;
  t.z.z = a.z.z;
  t.z.w = a.w.z;
  t.w.x = a.x.w;
  t.w.y = a.y.w;
  t.w.z = a.z.w;
  t.w.w = a.w.w;

  return t;
}

#if !defined(__KERNEL_METAL__)
ccl_device_inline ProjectionTransform projection_inverse(const ProjectionTransform tfm)
{
  ProjectionTransform tfmR = projection_identity();
  float M[4][4];
  float R[4][4];

  memcpy(R, (const float *)&tfmR, sizeof(R));
  memcpy(M, (const float *)&tfm, sizeof(M));

  if (UNLIKELY(!projection_inverse_impl(R, M))) {
    return projection_identity();
  }

  memcpy((void *)&tfmR, R, sizeof(R));

  return tfmR;
}

ccl_device_inline ProjectionTransform operator*(const ProjectionTransform a,
                                                const ProjectionTransform b)
{
  const ProjectionTransform c = projection_transpose(b);
  ProjectionTransform t;

  t.x = make_float4(dot(a.x, c.x), dot(a.x, c.y), dot(a.x, c.z), dot(a.x, c.w));
  t.y = make_float4(dot(a.y, c.x), dot(a.y, c.y), dot(a.y, c.z), dot(a.y, c.w));
  t.z = make_float4(dot(a.z, c.x), dot(a.z, c.y), dot(a.z, c.z), dot(a.z, c.w));
  t.w = make_float4(dot(a.w, c.x), dot(a.w, c.y), dot(a.w, c.z), dot(a.w, c.w));

  return t;
}
#endif

#ifndef __KERNEL_GPU__

ccl_device_inline ProjectionTransform operator*(const ProjectionTransform &a, const Transform &b)
{
  return a * ProjectionTransform(b);
}

ccl_device_inline ProjectionTransform operator*(const Transform &a, const ProjectionTransform &b)
{
  return ProjectionTransform(a) * b;
}

ccl_device_inline void print_projection(const char *label, const ProjectionTransform &t)
{
  print_float4(label, t.x);
  print_float4(label, t.y);
  print_float4(label, t.z);
  print_float4(label, t.w);
  printf("\n");
}

ccl_device_inline ProjectionTransform projection_perspective(const float fov,
                                                             const float n,
                                                             float f)
{
  const ProjectionTransform persp = make_projection(
      1, 0, 0, 0, 0, 1, 0, 0, 0, 0, f / (f - n), -f * n / (f - n), 0, 0, 1, 0);

  const float inv_angle = 1.0f / tanf(0.5f * fov);

  const Transform scale = transform_scale(inv_angle, inv_angle, 1);

  return scale * persp;
}

ccl_device_inline ProjectionTransform projection_orthographic(const float znear, const float zfar)
{
  const Transform t = transform_scale(1.0f, 1.0f, 1.0f / (zfar - znear));

  return ProjectionTransform(t);
}

#endif /* __KERNEL_GPU__ */

CCL_NAMESPACE_END
