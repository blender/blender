/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup imbdds
 */

#pragma once

#ifndef MIN
#  define MIN(a, b) ((a) <= (b) ? (a) : (b))
#endif
#ifndef MAX
#  define MAX(a, b) ((a) >= (b) ? (a) : (b))
#endif
#ifndef CLAMP
#  define CLAMP(x, a, b) MIN(MAX((x), (a)), (b))
#endif

template<typename T> inline void swap(T &a, T &b)
{
  T tmp = a;
  a = b;
  b = tmp;
}

typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned int uint;
typedef unsigned int uint32;
typedef unsigned long long uint64;

// copied from nvtt src/nvimage/nvimage.h
inline uint computePitch(uint w, uint bitsize, uint alignment)
{
  return ((w * bitsize + 8 * alignment - 1) / (8 * alignment)) * alignment;
}
