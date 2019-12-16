/******************************************************************************
 *
 * MantaFlow fluid solver framework
 * Copyright 2014 Tobias Pfaff, Nils Thuerey
 *
 * This program is free software, distributed under the terms of the
 * Apache License, Version 2.0
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Helper functions for higher order interpolation
 *
 ******************************************************************************/

#ifndef _INTERPOLHIGH_H
#define _INTERPOLHIGH_H

#include "vectorbase.h"

namespace Manta {

template<class T> inline T cubicInterp(const Real interp, const T *points)
{
  T d0 = (points[2] - points[0]) * 0.5;
  T d1 = (points[3] - points[1]) * 0.5;
  T deltak = (points[2] - points[1]);

  // disabled: if (deltak * d0 < 0.0) d0 = 0;
  // disabled: if (deltak * d1 < 0.0) d1 = 0;

  T a0 = points[1];
  T a1 = d0;
  T a2 = 3.0 * deltak - 2.0 * d0 - d1;
  T a3 = -2.0 * deltak + d0 + d1;

  Real squared = interp * interp;
  Real cubed = squared * interp;
  return a3 * cubed + a2 * squared + a1 * interp + a0;
}

template<class T> inline T interpolCubic2D(const T *data, const Vec3i &size, const Vec3 &pos)
{
  const Real px = pos.x - 0.5f, py = pos.y - 0.5f;

  const int x1 = (int)px;
  const int x2 = x1 + 1;
  const int x3 = x1 + 2;
  const int x0 = x1 - 1;

  const int y1 = (int)py;
  const int y2 = y1 + 1;
  const int y3 = y1 + 2;
  const int y0 = y1 - 1;

  if (x0 < 0 || y0 < 0 || x3 >= size[0] || y3 >= size[1]) {
    return interpol(data, size, 0, pos);
  }

  const Real xInterp = px - x1;
  const Real yInterp = py - y1;

  const int y0x = y0 * size[0];
  const int y1x = y1 * size[0];
  const int y2x = y2 * size[0];
  const int y3x = y3 * size[0];

  const T p0[] = {data[x0 + y0x], data[x1 + y0x], data[x2 + y0x], data[x3 + y0x]};
  const T p1[] = {data[x0 + y1x], data[x1 + y1x], data[x2 + y1x], data[x3 + y1x]};
  const T p2[] = {data[x0 + y2x], data[x1 + y2x], data[x2 + y2x], data[x3 + y2x]};
  const T p3[] = {data[x0 + y3x], data[x1 + y3x], data[x2 + y3x], data[x3 + y3x]};

  const T finalPoints[] = {cubicInterp(xInterp, p0),
                           cubicInterp(xInterp, p1),
                           cubicInterp(xInterp, p2),
                           cubicInterp(xInterp, p3)};

  return cubicInterp(yInterp, finalPoints);
}

template<class T>
inline T interpolCubic(const T *data, const Vec3i &size, const int Z, const Vec3 &pos)
{
  if (Z == 0)
    return interpolCubic2D(data, size, pos);

  const Real px = pos.x - 0.5f, py = pos.y - 0.5f, pz = pos.z - 0.5f;

  const int x1 = (int)px;
  const int x2 = x1 + 1;
  const int x3 = x1 + 2;
  const int x0 = x1 - 1;

  const int y1 = (int)py;
  const int y2 = y1 + 1;
  const int y3 = y1 + 2;
  const int y0 = y1 - 1;

  const int z1 = (int)pz;
  const int z2 = z1 + 1;
  const int z3 = z1 + 2;
  const int z0 = z1 - 1;

  if (x0 < 0 || y0 < 0 || z0 < 0 || x3 >= size[0] || y3 >= size[1] || z3 >= size[2]) {
    return interpol(data, size, Z, pos);
  }

  const Real xInterp = px - x1;
  const Real yInterp = py - y1;
  const Real zInterp = pz - z1;

  const int slabsize = size[0] * size[1];
  const int z0Slab = z0 * slabsize;
  const int z1Slab = z1 * slabsize;
  const int z2Slab = z2 * slabsize;
  const int z3Slab = z3 * slabsize;

  const int y0x = y0 * size[0];
  const int y1x = y1 * size[0];
  const int y2x = y2 * size[0];
  const int y3x = y3 * size[0];

  const int y0z0 = y0x + z0Slab;
  const int y1z0 = y1x + z0Slab;
  const int y2z0 = y2x + z0Slab;
  const int y3z0 = y3x + z0Slab;

  const int y0z1 = y0x + z1Slab;
  const int y1z1 = y1x + z1Slab;
  const int y2z1 = y2x + z1Slab;
  const int y3z1 = y3x + z1Slab;

  const int y0z2 = y0x + z2Slab;
  const int y1z2 = y1x + z2Slab;
  const int y2z2 = y2x + z2Slab;
  const int y3z2 = y3x + z2Slab;

  const int y0z3 = y0x + z3Slab;
  const int y1z3 = y1x + z3Slab;
  const int y2z3 = y2x + z3Slab;
  const int y3z3 = y3x + z3Slab;

  // get the z0 slice
  const T p0[] = {data[x0 + y0z0], data[x1 + y0z0], data[x2 + y0z0], data[x3 + y0z0]};
  const T p1[] = {data[x0 + y1z0], data[x1 + y1z0], data[x2 + y1z0], data[x3 + y1z0]};
  const T p2[] = {data[x0 + y2z0], data[x1 + y2z0], data[x2 + y2z0], data[x3 + y2z0]};
  const T p3[] = {data[x0 + y3z0], data[x1 + y3z0], data[x2 + y3z0], data[x3 + y3z0]};

  // get the z1 slice
  const T p4[] = {data[x0 + y0z1], data[x1 + y0z1], data[x2 + y0z1], data[x3 + y0z1]};
  const T p5[] = {data[x0 + y1z1], data[x1 + y1z1], data[x2 + y1z1], data[x3 + y1z1]};
  const T p6[] = {data[x0 + y2z1], data[x1 + y2z1], data[x2 + y2z1], data[x3 + y2z1]};
  const T p7[] = {data[x0 + y3z1], data[x1 + y3z1], data[x2 + y3z1], data[x3 + y3z1]};

  // get the z2 slice
  const T p8[] = {data[x0 + y0z2], data[x1 + y0z2], data[x2 + y0z2], data[x3 + y0z2]};
  const T p9[] = {data[x0 + y1z2], data[x1 + y1z2], data[x2 + y1z2], data[x3 + y1z2]};
  const T p10[] = {data[x0 + y2z2], data[x1 + y2z2], data[x2 + y2z2], data[x3 + y2z2]};
  const T p11[] = {data[x0 + y3z2], data[x1 + y3z2], data[x2 + y3z2], data[x3 + y3z2]};

  // get the z3 slice
  const T p12[] = {data[x0 + y0z3], data[x1 + y0z3], data[x2 + y0z3], data[x3 + y0z3]};
  const T p13[] = {data[x0 + y1z3], data[x1 + y1z3], data[x2 + y1z3], data[x3 + y1z3]};
  const T p14[] = {data[x0 + y2z3], data[x1 + y2z3], data[x2 + y2z3], data[x3 + y2z3]};
  const T p15[] = {data[x0 + y3z3], data[x1 + y3z3], data[x2 + y3z3], data[x3 + y3z3]};

  // interpolate
  const T z0Points[] = {cubicInterp(xInterp, p0),
                        cubicInterp(xInterp, p1),
                        cubicInterp(xInterp, p2),
                        cubicInterp(xInterp, p3)};
  const T z1Points[] = {cubicInterp(xInterp, p4),
                        cubicInterp(xInterp, p5),
                        cubicInterp(xInterp, p6),
                        cubicInterp(xInterp, p7)};
  const T z2Points[] = {cubicInterp(xInterp, p8),
                        cubicInterp(xInterp, p9),
                        cubicInterp(xInterp, p10),
                        cubicInterp(xInterp, p11)};
  const T z3Points[] = {cubicInterp(xInterp, p12),
                        cubicInterp(xInterp, p13),
                        cubicInterp(xInterp, p14),
                        cubicInterp(xInterp, p15)};

  const T finalPoints[] = {cubicInterp(yInterp, z0Points),
                           cubicInterp(yInterp, z1Points),
                           cubicInterp(yInterp, z2Points),
                           cubicInterp(yInterp, z3Points)};

  return cubicInterp(zInterp, finalPoints);
}

inline Vec3 interpolCubicMAC(const Vec3 *data, const Vec3i &size, const int Z, const Vec3 &pos)
{
  // warning - not yet optimized...
  Real vx = interpolCubic<Vec3>(data, size, Z, pos + Vec3(0.5, 0, 0))[0];
  Real vy = interpolCubic<Vec3>(data, size, Z, pos + Vec3(0, 0.5, 0))[1];
  Real vz = 0.f;
  if (Z != 0)
    vz = interpolCubic<Vec3>(data, size, Z, pos + Vec3(0, 0, 0.5))[2];
  return Vec3(vx, vy, vz);
}

}  // namespace Manta

#endif
