/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 */

#pragma once

/* types */

/** vector of two shorts. */
struct vec2s {
  short x, y;
};

/** vector of two floats. */
struct vec2f {
  float x, y;
};

struct vec2i {
  int x, y;
};

/* not used at the moment */
/*
struct vec2d {
  double x, y;
};
*/
struct vec3i {
  int x, y, z;
};

struct vec3f {
  float x, y, z;
};
/*
struct vec3d {
  double x, y, z;
};

struct vec4i {
  int x, y, z, w;
};
*/
struct vec4f {
  float x, y, z, w;
};

/**
 * This type generally shouldn't be used. It only exists for cases where a DNA type that
 * corresponds to `blender:float4x4` is required. Note that `float4x4` is 16 byte aligned, but we
 * can't enforce that this struct yet.
 */
struct mat4x4f {
  float value[4][4];
};
/*
struct vec4d {
  double x, y, z, w;
};
*/

/** integer rectangle. */
struct rcti {
  int xmin, xmax;
  int ymin, ymax;

#ifdef __cplusplus
  inline bool operator==(const rcti &other) const
  {
    return xmin == other.xmin && xmax == other.xmax && ymin == other.ymin && ymax == other.ymax;
  }
  inline bool operator!=(const rcti &other) const
  {
    return !(*this == other);
  }
#endif
};

/** float rectangle. */
struct rctf {
  float xmin, xmax;
  float ymin, ymax;
};

/** dual quaternion. */
struct DualQuat {
  float quat[4] = {};
  float trans[4] = {};

  float scale[4][4] = {};
  float scale_weight = 0;
};
