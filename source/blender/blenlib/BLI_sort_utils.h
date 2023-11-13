/* SPDX-FileCopyrightText: 2013 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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
