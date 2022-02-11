/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2001-2002 NaN Holding BV. All rights reserved. */

/** \file
 * \ingroup DNA
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* types */

/** vector of two shorts. */
typedef struct vec2s {
  short x, y;
} vec2s;

/** vector of two floats. */
typedef struct vec2f {
  float x, y;
} vec2f;

/* not used at the moment */
/*
typedef struct vec2i {
  int x, y;
} vec2i;

typedef struct vec2d {
  double x, y;
} vec2d;

typedef struct vec3i {
  int x, y, z;
} vec3i;
*/
typedef struct vec3f {
  float x, y, z;
} vec3f;
/*
typedef struct vec3d {
  double x, y, z;
} vec3d;

typedef struct vec4i {
  int x, y, z, w;
} vec4i;

typedef struct vec4f {
  float x, y, z, w;
} vec4f;

typedef struct vec4d {
  double x, y, z, w;
} vec4d;
*/

/** integer rectangle. */
typedef struct rcti {
  int xmin, xmax;
  int ymin, ymax;
} rcti;

/** float rectangle. */
typedef struct rctf {
  float xmin, xmax;
  float ymin, ymax;
} rctf;

/** dual quaternion. */
typedef struct DualQuat {
  float quat[4];
  float trans[4];

  float scale[4][4];
  float scale_weight;
} DualQuat;

#ifdef __cplusplus
}
#endif
