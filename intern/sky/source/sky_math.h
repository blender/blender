/* SPDX-FileCopyrightText: 2020-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup intern_sky_modal
 */

#ifndef __SKY_MATH_H__
#define __SKY_MATH_H__

#ifdef WITH_TBB
#  include <tbb/parallel_for.h>
#else
#  include <algorithm>
#endif

/* Minimal math implementation for sky model. */

#include <cmath>

#ifndef M_PI_F
#  define M_PI_F (3.1415926535897932f) /* pi */
#endif
#ifndef M_PI_2_F
#  define M_PI_2_F (1.5707963267948966f) /* pi/2 */
#endif
#ifndef M_2PI_F
#  define M_2PI_F (6.2831853071795864f) /* 2*pi */
#endif
#ifndef M_1_PI_F
#  define M_1_PI_F (0.3183098861837067f) /* 1/pi */
#endif
#ifndef M_4PI_F
#  define M_4PI_F (12.566370614359172f) /* 4*pi */
#endif
#ifndef M_1_4PI_F
#  define M_1_4PI_F (0.0795774715459476f) /* 1/(4*pi) */
#endif

/* float2 */
struct float2 {
  float x, y;

  float2() = default;

  float2(const float *ptr) : x{ptr[0]}, y{ptr[1]} {}

  float2(const float (*ptr)[2]) : float2((const float *)ptr) {}

  explicit float2(float value) : x(value), y(value) {}

  explicit float2(int value) : x(value), y(value) {}

  float2(float x, float y) : x{x}, y{y} {}

  operator const float *() const
  {
    return &x;
  }

  operator float *()
  {
    return &x;
  }

  friend float2 operator*(const float2 &a, float b)
  {
    return {a.x * b, a.y * b};
  }

  friend float2 operator*(float b, const float2 &a)
  {
    return {a.x * b, a.y * b};
  }

  friend float2 operator/(const float2 &a, float b)
  {
    return {a.x / b, a.y / b};
  }

  friend float2 operator/(float b, const float2 &a)
  {
    return {a.x / b, a.y / b};
  }

  friend float2 operator/(const float2 &a, const float2 &b)
  {
    return {a.x / b.x, a.y / b.y};
  }

  friend float2 operator-(const float2 &a, const float2 &b)
  {
    return {a.x - b.x, a.y - b.y};
  }

  friend float2 operator-(const float2 &a)
  {
    return {-a.x, -a.y};
  }

  float length_squared() const
  {
    return x * x + y * y;
  }

  float length() const
  {
    return sqrt(length_squared());
  }

  static float distance(const float2 &a, const float2 &b)
  {
    return (a - b).length();
  }

  friend float2 operator+(const float2 &a, const float2 &b)
  {
    return {a.x + b.x, a.y + b.y};
  }

  void operator+=(const float2 &b)
  {
    this->x += b.x;
    this->y += b.y;
  }

  friend float2 operator*(const float2 &a, const float2 &b)
  {
    return {a.x * b.x, a.y * b.y};
  }
};

/* float3 */
struct float3 {
  float x, y, z;

  float3() = default;

  float3(const float *ptr) : x{ptr[0]}, y{ptr[1]}, z{ptr[2]} {}

  float3(const float (*ptr)[3]) : float3((const float *)ptr) {}

  explicit float3(float value) : x(value), y(value), z(value) {}

  explicit float3(int value) : x(value), y(value), z(value) {}

  float3(float x, float y, float z) : x{x}, y{y}, z{z} {}

  operator const float *() const
  {
    return &x;
  }

  operator float *()
  {
    return &x;
  }

  friend float3 operator*(const float3 &a, float b)
  {
    return {a.x * b, a.y * b, a.z * b};
  }

  friend float3 operator*(float b, const float3 &a)
  {
    return {a.x * b, a.y * b, a.z * b};
  }

  friend float3 operator/(const float3 &a, float b)
  {
    return {a.x / b, a.y / b, a.z / b};
  }

  friend float3 operator/(float b, const float3 &a)
  {
    return {a.x / b, a.y / b, a.z / b};
  }

  friend float3 operator-(const float3 &a, const float3 &b)
  {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
  }

  friend float3 operator-(const float3 &a)
  {
    return {-a.x, -a.y, -a.z};
  }

  float length_squared() const
  {
    return x * x + y * y + z * z;
  }

  float length() const
  {
    return sqrt(length_squared());
  }

  static float distance(const float3 &a, const float3 &b)
  {
    return (a - b).length();
  }

  friend float3 operator+(const float3 &a, const float3 &b)
  {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
  }

  void operator+=(const float3 &b)
  {
    this->x += b.x;
    this->y += b.y;
    this->z += b.z;
  }

  friend float3 operator*(const float3 &a, const float3 &b)
  {
    return {a.x * b.x, a.y * b.y, a.z * b.z};
  }
};

/* float4 */
struct float4 {
  float x, y, z, w;

  float4() = default;

  float4(const float *ptr) : x{ptr[0]}, y{ptr[1]}, z{ptr[2]}, w{ptr[3]} {}

  float4(const float (*ptr)[4]) : float4((const float *)ptr) {}

  explicit float4(float value) : x(value), y(value), z(value), w(value) {}

  explicit float4(int value) : x(value), y(value), z(value), w(value) {}

  float4(float x, float y, float z, float w) : x{x}, y{y}, z{z}, w{w} {}

  operator const float *() const
  {
    return &x;
  }

  operator float *()
  {
    return &x;
  }

  friend float4 operator*(const float4 &a, float b)
  {
    return {a.x * b, a.y * b, a.z * b, a.w * b};
  }

  friend float4 operator*(float b, const float4 &a)
  {
    return {a.x * b, a.y * b, a.z * b, a.w * b};
  }

  friend float4 operator/(const float4 &a, float b)
  {
    return {a.x / b, a.y / b, a.z / b, a.w / b};
  }

  friend float4 operator/(float b, const float4 &a)
  {
    return {a.x / b, a.y / b, a.z / b, a.w / b};
  }

  friend float4 operator*(const float4 &a, const float4 &b)
  {
    return {a.x * b.x, a.y * b.y, a.z * b.z, a.w * b.w};
  }

  friend float4 operator/(const float4 &a, const float4 &b)
  {
    return {a.x / b.x, a.y / b.y, a.z / b.z, a.w / b.w};
  }

  friend float4 operator-(const float4 &a, const float4 &b)
  {
    return {a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w};
  }

  friend float4 operator-(const float4 &a)
  {
    return {-a.x, -a.y, -a.z, -a.w};
  }

  float length_squared() const
  {
    return x * x + y * y + z * z + w * w;
  }

  float length() const
  {
    return sqrt(length_squared());
  }

  static float distance(const float4 &a, const float4 &b)
  {
    return (a - b).length();
  }

  friend float4 operator+(const float4 &a, const float4 &b)
  {
    return {a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w};
  }

  void operator+=(const float4 &b)
  {
    this->x += b.x;
    this->y += b.y;
    this->z += b.z;
    this->w += b.w;
  }

  void operator*=(const float4 &b)
  {
    this->x *= b.x;
    this->y *= b.y;
    this->z *= b.z;
    this->w *= b.w;
  }
};

inline float sqr(float a)
{
  return a * a;
}

inline float safe_sqrtf(const float f)
{
  return sqrt(fmax(f, 0.0f));
}

inline float2 make_float2(float x, float y)
{
  return float2(x, y);
}

inline float dot(const float2 &a, const float2 &b)
{
  return a.x * b.x + a.y * b.y;
}

inline float distance(const float2 &a, const float2 &b)
{
  return float2::distance(a, b);
}

inline float len_squared(float2 f)
{
  return f.length_squared();
}

inline float len(float2 f)
{
  return f.length();
}

inline float reduce_add(float2 f)
{
  return f.x + f.y;
}

inline float3 make_float3(float x, float y, float z)
{
  return float3(x, y, z);
}

inline float dot(const float3 &a, const float3 &b)
{
  return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline float distance(const float3 &a, const float3 &b)
{
  return float3::distance(a, b);
}

inline float len_squared(float3 f)
{
  return f.length_squared();
}

inline float len(float3 f)
{
  return f.length();
}

inline float reduce_add(float3 f)
{
  return f.x + f.y + f.z;
}

inline float4 make_float4(float x, float y, float z, float w)
{
  return float4(x, y, z, w);
}

inline float dot(const float4 a, const float4 b)
{
  return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
}

inline float distance(const float4 &a, const float4 &b)
{
  return float4::distance(a, b);
}

inline float len_squared(float4 f)
{
  return f.length_squared();
}

inline float len(float4 f)
{
  return f.length();
}

inline float reduce_add(float4 f)
{
  return f.x + f.y + f.z + f.w;
}

inline float4 exp(float4 a)
{
  return make_float4(expf(a.x), expf(a.y), expf(a.z), expf(a.w));
}

inline float4 max(float4 a, float b)
{
  return make_float4(fmax(a.x, b), fmax(a.y, b), fmax(a.z, b), fmax(a.w, b));
}

inline float clamp(float x, float min, float max)
{
  if (x < min) {
    return min;
  }
  if (x > max) {
    return max;
  }
  return x;
}

inline float saturate(const float a)
{
  return clamp(a, 0.0f, 1.0f);
}

template<typename T> inline T mix(T x, T y, float a)
{
  return x + a * (y - x);
}

inline float3 sun_direction(float sun_cos_theta)
{
  return make_float3(-sqrtf(1.0f - sun_cos_theta * sun_cos_theta), 0.0f, sun_cos_theta);
}

inline float ray_sphere_intersection(float3 pos, float3 dir, float radius)
{
  float b = dot(pos, dir);
  float c = dot(pos, pos) - radius * radius;
  if (c > 0.0f && b > 0.0f) {
    return -1.0f;
  }
  float d = b * b - c;
  if (d < 0) {
    return -1.0f;
  }
  if (d >= b * b) {
    return -b + sqrtf(d);
  }
  return -b - sqrtf(d);
}

/* Minimal parallel for implementation. */

template<typename Function>
inline void SKY_parallel_for(const size_t begin,
                             const size_t end,
                             const size_t grainsize,
                             const Function &function)
{
#ifdef WITH_TBB
  tbb::parallel_for(
      tbb::blocked_range<size_t>(begin, end, grainsize),
      [function](const tbb::blocked_range<size_t> &r) { function(r.begin(), r.end()); });
#else
  for (size_t i = begin; i < end; i += grainsize) {
    function(i, std::min(i + grainsize, end));
  }
  (void)grainsize;
#endif
}

#endif /* __SKY_MATH_H__ */
