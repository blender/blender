/* SPDX-FileCopyrightText: 2025 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "vector2.h"
#include "vector4.h"

struct int2 {
  int x;
  int y;
};

int2 __operator__add__(int2 a, int2 b)
{
  return int2(a.x + b.x, a.y + b.y);
}

int2 __operator__add__(int2 a, int b)
{
  return int2(a.x + b, a.y + b);
}

int2 __operator__mul__(int2 a, int2 b)
{
  return int2(a.x * b.x, a.y * b.y);
}

int2 __operator__mul__(int2 a, int b)
{
  return int2(a.x * b, a.y * b);
}

int2 __operator__shr__(int2 a, int b)
{
  return int2(a.x >> b, a.y >> b);
}

int2 __operator__xor__(int2 a, int2 b)
{
  return int2(a.x ^ b.x, a.y ^ b.y);
}

int2 __operator__bitand__(int2 a, int b)
{
  return int2(a.x & b, a.y & b);
}

int2 vec2_to_int2(vector2 k)
{
  return int2((int)k.x, (int)k.y);
}

vector2 int2_to_vec2(int2 k)
{
  return vector2((float)k.x, (float)k.y);
}

struct int3 {
  int x;
  int y;
  int z;
};

int3 __operator__add__(int3 a, int3 b)
{
  return int3(a.x + b.x, a.y + b.y, a.z + b.z);
}

int3 __operator__add__(int3 a, int b)
{
  return int3(a.x + b, a.y + b, a.z + b);
}

int3 __operator__mul__(int3 a, int3 b)
{
  return int3(a.x * b.x, a.y * b.y, a.z * b.z);
}

int3 __operator__mul__(int3 a, int b)
{
  return int3(a.x * b, a.y * b, a.z * b);
}

int3 __operator__shr__(int3 a, int b)
{
  return int3(a.x >> b, a.y >> b, a.z >> b);
}

int3 __operator__xor__(int3 a, int3 b)
{
  return int3(a.x ^ b.x, a.y ^ b.y, a.z ^ b.z);
}

int3 __operator__bitand__(int3 a, int b)
{
  return int3(a.x & b, a.y & b, a.z & b);
}

int3 vec3_to_int3(point k)
{
  return int3((int)k.x, (int)k.y, (int)k.z);
}

point int3_to_vec3(int3 k)
{
  return point((float)k.x, (float)k.y, (float)k.z);
}

struct int4 {
  int x;
  int y;
  int z;
  int w;
};

int4 __operator__add__(int4 a, int4 b)
{
  return int4(a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w);
}

int4 __operator__add__(int4 a, int b)
{
  return int4(a.x + b, a.y + b, a.z + b, a.w + b);
}

int4 __operator__mul__(int4 a, int4 b)
{
  return int4(a.x * b.x, a.y * b.y, a.z * b.z, a.w * b.w);
}

int4 __operator__mul__(int4 a, int b)
{
  return int4(a.x * b, a.y * b, a.z * b, a.w * b);
}

int4 __operator__shr__(int4 a, int b)
{
  return int4(a.x >> b, a.y >> b, a.z >> b, a.w >> b);
}

int4 __operator__xor__(int4 a, int4 b)
{
  return int4(a.x ^ b.x, a.y ^ b.y, a.z ^ b.z, a.w ^ b.w);
}

int4 __operator__bitand__(int4 a, int b)
{
  return int4(a.x & b, a.y & b, a.z & b, a.w & b);
}

int4 vec4_to_int4(vector4 k)
{
  return int4((int)k.x, (int)k.y, (int)k.z, (int)k.w);
}

vector4 int4_to_vec4(int4 k)
{
  return vector4((float)k.x, (float)k.y, (float)k.z, (float)k.w);
}
