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
typedef struct {
  int x, y, z;
} Point3i;

typedef struct {
  Point3i begin;
  Point3i end;
} BoundingBox;

// triangle that points to three vertices
typedef struct {
  float vt[3][3];
} Triangle;

// 3d point with float coordinates
typedef struct {
  float x, y, z;
} Point3f;

typedef struct {
  Point3f begin;
  Point3f end;
} BoundingBoxf;

#endif /* __GEOCOMMON_H__ */
