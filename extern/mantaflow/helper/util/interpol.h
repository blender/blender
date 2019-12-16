/******************************************************************************
 *
 * MantaFlow fluid solver framework
 * Copyright 2011 Tobias Pfaff, Nils Thuerey
 *
 * This program is free software, distributed under the terms of the
 * Apache License, Version 2.0
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Helper functions for interpolation
 *
 ******************************************************************************/

#ifndef _INTERPOL_H
#define _INTERPOL_H

#include "vectorbase.h"

// Grid values are stored at i+0.5, j+0.5, k+0.5
// MAC grid values are stored at i,j+0.5,k+0.5 (for x) ...

namespace Manta {

inline Vec3 fdTangent(const Vec3 &p0, const Vec3 &p1, const Vec3 &p2)
{
  return 0.5 * (getNormalized(p2 - p1) + getNormalized(p1 - p0));
}

inline Vec3 crTangent(const Vec3 &p0, const Vec3 &p1, const Vec3 &p2)
{
  return 0.5 * (p2 - p0);
}

inline Vec3 hermiteSpline(const Vec3 &p0, const Vec3 &p1, const Vec3 &m0, const Vec3 &m1, Real t)
{
  const Real t2 = t * t, t3 = t2 * t;
  return (2.0 * t3 - 3.0 * t2 + 1.0) * p0 + (t3 - 2.0 * t2 + t) * m0 +
         (-2.0 * t3 + 3.0 * t2) * p1 + (t3 - t2) * m1;
}

static inline void checkIndexInterpol(const Vec3i &size, IndexInt idx)
{
  if (idx < 0 || idx > (IndexInt)size.x * size.y * size.z) {
    std::ostringstream s;
    s << "Grid interpol dim " << size << " : index " << idx << " out of bound ";
    errMsg(s.str());
  }
}

// ----------------------------------------------------------------------
// Grid interpolators
// ----------------------------------------------------------------------

#define BUILD_INDEX \
  Real px = pos.x - 0.5f, py = pos.y - 0.5f, pz = pos.z - 0.5f; \
  int xi = (int)px; \
  int yi = (int)py; \
  int zi = (int)pz; \
  Real s1 = px - (Real)xi, s0 = 1. - s1; \
  Real t1 = py - (Real)yi, t0 = 1. - t1; \
  Real f1 = pz - (Real)zi, f0 = 1. - f1; \
  /* clamp to border */ \
  if (px < 0.) { \
    xi = 0; \
    s0 = 1.0; \
    s1 = 0.0; \
  } \
  if (py < 0.) { \
    yi = 0; \
    t0 = 1.0; \
    t1 = 0.0; \
  } \
  if (pz < 0.) { \
    zi = 0; \
    f0 = 1.0; \
    f1 = 0.0; \
  } \
  if (xi >= size.x - 1) { \
    xi = size.x - 2; \
    s0 = 0.0; \
    s1 = 1.0; \
  } \
  if (yi >= size.y - 1) { \
    yi = size.y - 2; \
    t0 = 0.0; \
    t1 = 1.0; \
  } \
  if (size.z > 1) { \
    if (zi >= size.z - 1) { \
      zi = size.z - 2; \
      f0 = 0.0; \
      f1 = 1.0; \
    } \
  } \
  const int X = 1; \
  const int Y = size.x;

template<class T> inline T interpol(const T *data, const Vec3i &size, const int Z, const Vec3 &pos)
{
  BUILD_INDEX
  IndexInt idx = (IndexInt)xi + (IndexInt)Y * yi + (IndexInt)Z * zi;
  DEBUG_ONLY(checkIndexInterpol(size, idx));
  DEBUG_ONLY(checkIndexInterpol(size, idx + X + Y + Z));

  return ((data[idx] * t0 + data[idx + Y] * t1) * s0 +
          (data[idx + X] * t0 + data[idx + X + Y] * t1) * s1) *
             f0 +
         ((data[idx + Z] * t0 + data[idx + Y + Z] * t1) * s0 +
          (data[idx + X + Z] * t0 + data[idx + X + Y + Z] * t1) * s1) *
             f1;
}

template<int c>
inline Real interpolComponent(const Vec3 *data, const Vec3i &size, const int Z, const Vec3 &pos)
{
  BUILD_INDEX
  IndexInt idx = (IndexInt)xi + (IndexInt)Y * yi + (IndexInt)Z * zi;
  DEBUG_ONLY(checkIndexInterpol(size, idx));
  DEBUG_ONLY(checkIndexInterpol(size, idx + X + Y + Z));

  return ((data[idx][c] * t0 + data[idx + Y][c] * t1) * s0 +
          (data[idx + X][c] * t0 + data[idx + X + Y][c] * t1) * s1) *
             f0 +
         ((data[idx + Z][c] * t0 + data[idx + Y + Z][c] * t1) * s0 +
          (data[idx + X + Z][c] * t0 + data[idx + X + Y + Z][c] * t1) * s1) *
             f1;
}

template<class T>
inline void setInterpol(
    T *data, const Vec3i &size, const int Z, const Vec3 &pos, const T &v, Real *sumBuffer)
{
  BUILD_INDEX
  IndexInt idx = (IndexInt)xi + (IndexInt)Y * yi + (IndexInt)Z * zi;
  DEBUG_ONLY(checkIndexInterpol(size, idx));
  DEBUG_ONLY(checkIndexInterpol(size, idx + X + Y + Z));

  T *ref = &data[idx];
  Real *sum = &sumBuffer[idx];
  Real s0f0 = s0 * f0, s1f0 = s1 * f0, s0f1 = s0 * f1, s1f1 = s1 * f1;
  Real w0 = t0 * s0f0, wx = t0 * s1f0, wy = t1 * s0f0, wxy = t1 * s1f0;
  Real wz = t0 * s0f1, wxz = t0 * s1f1, wyz = t1 * s0f1, wxyz = t1 * s1f1;

  sum[Z] += wz;
  sum[X + Z] += wxz;
  sum[Y + Z] += wyz;
  sum[X + Y + Z] += wxyz;
  ref[Z] += wz * v;
  ref[X + Z] += wxz * v;
  ref[Y + Z] += wyz * v;
  ref[X + Y + Z] += wxyz * v;
  sum[0] += w0;
  sum[X] += wx;
  sum[Y] += wy;
  sum[X + Y] += wxy;
  ref[0] += w0 * v;
  ref[X] += wx * v;
  ref[Y] += wy * v;
  ref[X + Y] += wxy * v;
}

#define BUILD_INDEX_SHIFT \
  BUILD_INDEX \
  /* shifted coords */ \
  int s_xi = (int)pos.x, s_yi = (int)pos.y, s_zi = (int)pos.z; \
  Real s_s1 = pos.x - (Real)s_xi, s_s0 = 1. - s_s1; \
  Real s_t1 = pos.y - (Real)s_yi, s_t0 = 1. - s_t1; \
  Real s_f1 = pos.z - (Real)s_zi, s_f0 = 1. - s_f1; \
  /* clamp to border */ \
  if (pos.x < 0) { \
    s_xi = 0; \
    s_s0 = 1.0; \
    s_s1 = 0.0; \
  } \
  if (pos.y < 0) { \
    s_yi = 0; \
    s_t0 = 1.0; \
    s_t1 = 0.0; \
  } \
  if (pos.z < 0) { \
    s_zi = 0; \
    s_f0 = 1.0; \
    s_f1 = 0.0; \
  } \
  if (s_xi >= size.x - 1) { \
    s_xi = size.x - 2; \
    s_s0 = 0.0; \
    s_s1 = 1.0; \
  } \
  if (s_yi >= size.y - 1) { \
    s_yi = size.y - 2; \
    s_t0 = 0.0; \
    s_t1 = 1.0; \
  } \
  if (size.z > 1) { \
    if (s_zi >= size.z - 1) { \
      s_zi = size.z - 2; \
      s_f0 = 0.0; \
      s_f1 = 1.0; \
    } \
  }

inline Vec3 interpolMAC(const Vec3 *data, const Vec3i &size, const int Z, const Vec3 &pos)
{
  BUILD_INDEX_SHIFT;
  DEBUG_ONLY(checkIndexInterpol(size, (zi * (IndexInt)size.y + yi) * (IndexInt)size.x + xi));
  DEBUG_ONLY(checkIndexInterpol(
      size, (s_zi * (IndexInt)size.y + s_yi) * (IndexInt)size.x + s_xi + X + Y + Z));

  // process individual components
  Vec3 ret(0.);
  {  // X
    const Vec3 *ref = &data[((zi * size.y + yi) * size.x + s_xi)];
    ret.x = f0 * ((ref[0].x * t0 + ref[Y].x * t1) * s_s0 +
                  (ref[X].x * t0 + ref[X + Y].x * t1) * s_s1) +
            f1 * ((ref[Z].x * t0 + ref[Z + Y].x * t1) * s_s0 +
                  (ref[X + Z].x * t0 + ref[X + Y + Z].x * t1) * s_s1);
  }
  {  // Y
    const Vec3 *ref = &data[((zi * size.y + s_yi) * size.x + xi)];
    ret.y = f0 * ((ref[0].y * s_t0 + ref[Y].y * s_t1) * s0 +
                  (ref[X].y * s_t0 + ref[X + Y].y * s_t1) * s1) +
            f1 * ((ref[Z].y * s_t0 + ref[Z + Y].y * s_t1) * s0 +
                  (ref[X + Z].y * s_t0 + ref[X + Y + Z].y * s_t1) * s1);
  }
  {  // Z
    const Vec3 *ref = &data[((s_zi * size.y + yi) * size.x + xi)];
    ret.z = s_f0 *
                ((ref[0].z * t0 + ref[Y].z * t1) * s0 + (ref[X].z * t0 + ref[X + Y].z * t1) * s1) +
            s_f1 * ((ref[Z].z * t0 + ref[Z + Y].z * t1) * s0 +
                    (ref[X + Z].z * t0 + ref[X + Y + Z].z * t1) * s1);
  }
  return ret;
}

inline void setInterpolMAC(
    Vec3 *data, const Vec3i &size, const int Z, const Vec3 &pos, const Vec3 &val, Vec3 *sumBuffer)
{
  BUILD_INDEX_SHIFT;
  DEBUG_ONLY(checkIndexInterpol(size, (zi * (IndexInt)size.y + yi) * (IndexInt)size.x + xi));
  DEBUG_ONLY(checkIndexInterpol(
      size, (s_zi * (IndexInt)size.y + s_yi) * (IndexInt)size.x + s_xi + X + Y + Z));

  // process individual components
  {  // X
    const IndexInt idx = (IndexInt)(zi * size.y + yi) * size.x + s_xi;
    Vec3 *ref = &data[idx], *sum = &sumBuffer[idx];
    Real s0f0 = s_s0 * f0, s1f0 = s_s1 * f0, s0f1 = s_s0 * f1, s1f1 = s_s1 * f1;
    Real w0 = t0 * s0f0, wx = t0 * s1f0, wy = t1 * s0f0, wxy = t1 * s1f0;
    Real wz = t0 * s0f1, wxz = t0 * s1f1, wyz = t1 * s0f1, wxyz = t1 * s1f1;

    sum[Z].x += wz;
    sum[X + Z].x += wxz;
    sum[Y + Z].x += wyz;
    sum[X + Y + Z].x += wxyz;
    ref[Z].x += wz * val.x;
    ref[X + Z].x += wxz * val.x;
    ref[Y + Z].x += wyz * val.x;
    ref[X + Y + Z].x += wxyz * val.x;
    sum[0].x += w0;
    sum[X].x += wx;
    sum[Y].x += wy;
    sum[X + Y].x += wxy;
    ref[0].x += w0 * val.x;
    ref[X].x += wx * val.x;
    ref[Y].x += wy * val.x;
    ref[X + Y].x += wxy * val.x;
  }
  {  // Y
    const IndexInt idx = (IndexInt)(zi * size.y + s_yi) * size.x + xi;
    Vec3 *ref = &data[idx], *sum = &sumBuffer[idx];
    Real s0f0 = s0 * f0, s1f0 = s1 * f0, s0f1 = s0 * f1, s1f1 = s1 * f1;
    Real w0 = s_t0 * s0f0, wx = s_t0 * s1f0, wy = s_t1 * s0f0, wxy = s_t1 * s1f0;
    Real wz = s_t0 * s0f1, wxz = s_t0 * s1f1, wyz = s_t1 * s0f1, wxyz = s_t1 * s1f1;

    sum[Z].y += wz;
    sum[X + Z].y += wxz;
    sum[Y + Z].y += wyz;
    sum[X + Y + Z].y += wxyz;
    ref[Z].y += wz * val.y;
    ref[X + Z].y += wxz * val.y;
    ref[Y + Z].y += wyz * val.y;
    ref[X + Y + Z].y += wxyz * val.y;
    sum[0].y += w0;
    sum[X].y += wx;
    sum[Y].y += wy;
    sum[X + Y].y += wxy;
    ref[0].y += w0 * val.y;
    ref[X].y += wx * val.y;
    ref[Y].y += wy * val.y;
    ref[X + Y].y += wxy * val.y;
  }
  {  // Z
    const IndexInt idx = (IndexInt)(s_zi * size.y + yi) * size.x + xi;
    Vec3 *ref = &data[idx], *sum = &sumBuffer[idx];
    Real s0f0 = s0 * s_f0, s1f0 = s1 * s_f0, s0f1 = s0 * s_f1, s1f1 = s1 * s_f1;
    Real w0 = t0 * s0f0, wx = t0 * s1f0, wy = t1 * s0f0, wxy = t1 * s1f0;
    Real wz = t0 * s0f1, wxz = t0 * s1f1, wyz = t1 * s0f1, wxyz = t1 * s1f1;

    sum[0].z += w0;
    sum[X].z += wx;
    sum[Y].z += wy;
    sum[X + Y].z += wxy;
    sum[Z].z += wz;
    sum[X + Z].z += wxz;
    sum[Y + Z].z += wyz;
    sum[X + Y + Z].z += wxyz;
    ref[0].z += w0 * val.z;
    ref[X].z += wx * val.z;
    ref[Y].z += wy * val.z;
    ref[X + Y].z += wxy * val.z;
    ref[Z].z += wz * val.z;
    ref[X + Z].z += wxz * val.z;
    ref[Y + Z].z += wyz * val.z;
    ref[X + Y + Z].z += wxyz * val.z;
  }
}

#undef BUILD_INDEX
#undef BUILD_INDEX_SHIFT

}  // namespace Manta

#endif
