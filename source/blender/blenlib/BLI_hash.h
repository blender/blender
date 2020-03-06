/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef __BLI_HASH_H__
#define __BLI_HASH_H__

/** \file
 * \ingroup bli
 */

#ifdef __cplusplus
extern "C" {
#endif

#include "BLI_utildefines.h"

BLI_INLINE unsigned int BLI_hash_int_2d(unsigned int kx, unsigned int ky)
{
#define rot(x, k) (((x) << (k)) | ((x) >> (32 - (k))))

  unsigned int a, b, c;

  a = b = c = 0xdeadbeef + (2 << 2) + 13;
  a += kx;
  b += ky;

  c ^= b;
  c -= rot(b, 14);
  a ^= c;
  a -= rot(c, 11);
  b ^= a;
  b -= rot(a, 25);
  c ^= b;
  c -= rot(b, 16);
  a ^= c;
  a -= rot(c, 4);
  b ^= a;
  b -= rot(a, 14);
  c ^= b;
  c -= rot(b, 24);

  return c;

#undef rot
}

BLI_INLINE unsigned int BLI_hash_string(const char *str)
{
  unsigned int i = 0, c;

  while ((c = *str++)) {
    i = i * 37 + c;
  }
  return i;
}

BLI_INLINE unsigned int BLI_hash_int(unsigned int k)
{
  return BLI_hash_int_2d(k, 0);
}

BLI_INLINE float BLI_hash_int_01(unsigned int k)
{
  return (float)BLI_hash_int(k) * (1.0f / (float)0xFFFFFFFF);
}

BLI_INLINE void BLI_hash_pointer_to_color(const void *ptr, int *r, int *g, int *b)
{
  size_t val = (size_t)ptr;
  const size_t hash_a = BLI_hash_int(val & 0x0000ffff);
  const size_t hash_b = BLI_hash_int((uint)((val & 0xffff0000) >> 32));
  const size_t hash = hash_a ^ (hash_b + 0x9e3779b9 + (hash_a << 6) + (hash_a >> 2));
  *r = (hash & 0xff0000) >> 16;
  *g = (hash & 0x00ff00) >> 8;
  *b = hash & 0x0000ff;
}

#ifdef __cplusplus
}
#endif

#endif  // __BLI_HASH_H__
