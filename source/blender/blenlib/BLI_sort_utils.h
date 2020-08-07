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
 *
 * The Original Code is Copyright (C) 2013 Blender Foundation.
 * All rights reserved.
 */

#pragma once

/** \file
 * \ingroup bli
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \note keep \a sort_value first,
 * so cmp functions can be reused.
 */
struct SortPtrByFloat {
  float sort_value;
  void *data;
};

struct SortIntByFloat {
  float sort_value;
  int data;
};

struct SortPtrByInt {
  int sort_value;
  void *data;
};

struct SortIntByInt {
  int sort_value;
  int data;
};

int BLI_sortutil_cmp_float(const void *a_, const void *b_);
int BLI_sortutil_cmp_float_reverse(const void *a_, const void *b_);

int BLI_sortutil_cmp_int(const void *a_, const void *b_);
int BLI_sortutil_cmp_int_reverse(const void *a_, const void *b_);

int BLI_sortutil_cmp_ptr(const void *a_, const void *b_);
int BLI_sortutil_cmp_ptr_reverse(const void *a_, const void *b_);

#ifdef __cplusplus
}
#endif
