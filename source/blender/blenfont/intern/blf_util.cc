/* SPDX-FileCopyrightText: 2009 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup blf
 *
 * Internal utility API for BLF.
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "BLI_utildefines.h"

#include "blf_internal.hh"

uint blf_next_p2(uint x)
{
  x -= 1;
  x |= (x >> 16);
  x |= (x >> 8);
  x |= (x >> 4);
  x |= (x >> 2);
  x |= (x >> 1);
  x += 1;
  return x;
}

uint blf_hash(uint val)
{
  uint key;

  key = val;
  key += ~(key << 16);
  key ^= (key >> 5);
  key += (key << 3);
  key ^= (key >> 13);
  key += ~(key << 9);
  key ^= (key >> 17);
  return key % 257;
}
