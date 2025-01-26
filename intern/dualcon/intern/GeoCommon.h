/* SPDX-FileCopyrightText: 2002-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef __GEOCOMMON_H__
#define __GEOCOMMON_H__

#define UCHAR unsigned char
#define USHORT unsigned short

#define USE_MINIMIZER

/**
 * Structure definitions for points and triangles.
 *
 * @author Tao Ju
 */

// 3d point with integer coordinates
struct Point3i {
  int x, y, z;
};

struct BoundingBox {
  Point3i begin;
  Point3i end;
};

// triangle that points to three vertices
struct Triangle {
  float vt[3][3];
};

// 3d point with float coordinates
struct Point3f {
  float x, y, z;
};

struct BoundingBoxf {
  Point3f begin;
  Point3f end;
};

#endif /* __GEOCOMMON_H__ */
