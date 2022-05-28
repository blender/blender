/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#include "BLI_sort.hh"
#include "BLI_vector.hh"
#include "MOD_lineart.h"
#include "lineart_intern.h"

static bool cmp_adjacent_items(const LineartAdjacentEdge &p1, const LineartAdjacentEdge &p2)
{
  int a = p1.v1 - p2.v1;
  int b = p1.v2 - p2.v2;
  /* parallel_sort() requires cmp() to return true when the first element needs to appear before
   * the second element in the sorted array, false otherwise (strict weak ordering), see
   * https://en.cppreference.com/w/cpp/named_req/Compare. */
  return a < 0 ? true : (a == 0 ? b < 0 : false);
}

void lineart_sort_adjacent_items(LineartAdjacentEdge *ai, int length)
{
  blender::parallel_sort(ai, ai + length, cmp_adjacent_items);
}
