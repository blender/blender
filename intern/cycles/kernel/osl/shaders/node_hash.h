/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "stdcycles.h"
#include "vector2.h"
#include "vector4.h"

#define vector3 point

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

#undef vector3
