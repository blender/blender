/* SPDX-FileCopyrightText: 2011-2025 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "int_vector_types.h"
#include "stdcycles.h"
#include "vector2.h"
#include "vector4.h"

#define vector3 point

/* Hashing `uint` or `uint[234]` into a float in the range [0, 1].
 * Based on PCG 2D/3D/4D hash, but with signed integers. */

vector2 hash_int2_to_vector2(int2 k)
{
  int2 v = k * 1664525 + 1013904223;
  v.x += v.y * 1664525;
  v.y += v.x * 1664525;
  v = v ^ (v >> 16);
  v.x += v.y * 1664525;
  v.y += v.x * 1664525;
  vector2 f = int2_to_vec2(v & 0x7FFFFFFF);
  return f * (1.0 / (float)0x7FFFFFFF);
}

vector3 hash_int3_to_vector3(int3 k)
{
  int3 v = k * 1664525 + 1013904223;
  v.x += v.y * v.z;
  v.y += v.z * v.x;
  v.z += v.x * v.y;
  v = v ^ (v >> 16);
  v.x += v.y * v.z;
  v.y += v.z * v.x;
  v.z += v.x * v.y;
  vector3 f = int3_to_vec3(v & 0x7FFFFFFF);
  return f * (1.0 / (float)0x7FFFFFFF);
}

vector4 hash_int4_to_vector4(int4 k)
{
  int4 v = k * 1664525 + 1013904223;
  v.x += v.y * v.w;
  v.y += v.z * v.x;
  v.z += v.x * v.y;
  v.w += v.y * v.z;
  v = v ^ (v >> 16);
  v.x += v.y * v.w;
  v.y += v.z * v.x;
  v.z += v.x * v.y;
  v.w += v.y * v.z;
  vector4 f = int4_to_vec4(v & 0x7FFFFFFF);
  return f * (1.0 / (float)0x7FFFFFFF);
}

color hash_int2_to_color(int2 k)
{
  return hash_int3_to_vector3(int3(k.x, k.y, 0));
}

color hash_int4_to_color(int4 k)
{
  vector4 v = hash_int4_to_vector4(k);
  return color(v.x, v.y, v.z);
}

/* **** Hash a float or vector[234] into a float [0, 1] **** */

float hash_float_to_float(float k)
{
  return hashnoise(k);
}

float hash_vector2_to_float(vector2 k)
{
  return hashnoise(k.x, k.y);
}

float hash_vector3_to_float(vector3 k)
{
  return hashnoise(k);
}

float hash_vector4_to_float(vector4 k)
{
  return hashnoise(vector3(k.x, k.y, k.z), k.w);
}

/* **** Hash a vector[234] into a vector[234] [0, 1] **** */

vector2 hash_vector2_to_vector2(vector2 k)
{
  return vector2(hash_vector2_to_float(k), hash_vector3_to_float(vector3(k.x, k.y, 1.0)));
}

vector3 hash_vector3_to_vector3(vector3 k)
{
  return vector3(hash_vector3_to_float(k),
                 hash_vector4_to_float(vector4(k[0], k[1], k[2], 1.0)),
                 hash_vector4_to_float(vector4(k[0], k[1], k[2], 2.0)));
}

vector4 hash_vector4_to_vector4(vector4 k)
{
  return vector4(hash_vector4_to_float(k),
                 hash_vector4_to_float(vector4(k.w, k.x, k.y, k.z)),
                 hash_vector4_to_float(vector4(k.z, k.w, k.x, k.y)),
                 hash_vector4_to_float(vector4(k.y, k.z, k.w, k.x)));
}

/* **** Hash a float or a vec[234] into a color [0, 1] **** */

color hash_float_to_color(float k)
{
  return color(hash_float_to_float(k),
               hash_vector2_to_float(vector2(k, 1.0)),
               hash_vector2_to_float(vector2(k, 2.0)));
}

color hash_vector2_to_color(vector2 k)
{
  return color(hash_vector2_to_float(k),
               hash_vector3_to_float(vector3(k.x, k.y, 1.0)),
               hash_vector3_to_float(vector3(k.x, k.y, 2.0)));
}

color hash_vector3_to_color(vector3 k)
{
  return color(hash_vector3_to_float(k),
               hash_vector4_to_float(vector4(k[0], k[1], k[2], 1.0)),
               hash_vector4_to_float(vector4(k[0], k[1], k[2], 2.0)));
}

color hash_vector4_to_color(vector4 k)
{
  return color(hash_vector4_to_float(k),
               hash_vector4_to_float(vector4(k.z, k.x, k.w, k.y)),
               hash_vector4_to_float(vector4(k.w, k.z, k.y, k.x)));
}

/* **** Hash a float or a vec[234] into a vector3 [0, 1] **** */

vector3 hash_float_to_vector3(float k)
{
  return vector3(hash_float_to_float(k),
                 hash_vector2_to_float(vector2(k, 1.0)),
                 hash_vector2_to_float(vector2(k, 2.0)));
}

vector3 hash_vector2_to_vector3(vector2 k)
{
  return vector3(hash_vector2_to_float(k),
                 hash_vector3_to_float(vector3(k.x, k.y, 1.0)),
                 hash_vector3_to_float(vector3(k.x, k.y, 2.0)));
}

vector3 hash_vector4_to_vector3(vector4 k)
{
  return vector3(hash_vector4_to_float(k),
                 hash_vector4_to_float(vector4(k.z, k.x, k.w, k.y)),
                 hash_vector4_to_float(vector4(k.w, k.z, k.y, k.x)));
}

/* Hashing float or vector[234] into vector2 of components in range [0, 1]. */

vector2 hash_float_to_vector2(float k)
{
  return vector2(hash_float_to_float(k), hash_vector2_to_float(vector2(k, 1.0)));
}

vector2 hash_vector3_to_vector2(vector3 k)
{
  return vector2(hash_vector3_to_float(vector3(k.x, k.y, k.z)),
                 hash_vector3_to_float(vector3(k.z, k.x, k.y)));
}

vector2 hash_vector4_to_vector2(vector4 k)
{
  return vector2(hash_vector4_to_float(vector4(k.x, k.y, k.z, k.w)),
                 hash_vector4_to_float(vector4(k.z, k.x, k.w, k.y)));
}

#undef vector3
