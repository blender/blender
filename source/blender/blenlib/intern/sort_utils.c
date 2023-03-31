/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2013 Blender Foundation */

/** \file
 * \ingroup bli
 *
 * Utility functions for sorting common types.
 */

#include "BLI_sort_utils.h" /* own include */

struct SortAnyByFloat {
  float sort_value;
};

struct SortAnyByInt {
  int sort_value;
};

struct SortAnyByPtr {
  const void *sort_value;
};

int BLI_sortutil_cmp_float(const void *a_, const void *b_)
{
  const struct SortAnyByFloat *a = a_;
  const struct SortAnyByFloat *b = b_;
  if (a->sort_value > b->sort_value) {
    return 1;
  }
  if (a->sort_value < b->sort_value) {
    return -1;
  }

  return 0;
}

int BLI_sortutil_cmp_float_reverse(const void *a_, const void *b_)
{
  const struct SortAnyByFloat *a = a_;
  const struct SortAnyByFloat *b = b_;
  if (a->sort_value < b->sort_value) {
    return 1;
  }
  if (a->sort_value > b->sort_value) {
    return -1;
  }

  return 0;
}

int BLI_sortutil_cmp_int(const void *a_, const void *b_)
{
  const struct SortAnyByInt *a = a_;
  const struct SortAnyByInt *b = b_;
  if (a->sort_value > b->sort_value) {
    return 1;
  }
  if (a->sort_value < b->sort_value) {
    return -1;
  }

  return 0;
}

int BLI_sortutil_cmp_int_reverse(const void *a_, const void *b_)
{
  const struct SortAnyByInt *a = a_;
  const struct SortAnyByInt *b = b_;
  if (a->sort_value < b->sort_value) {
    return 1;
  }
  if (a->sort_value > b->sort_value) {
    return -1;
  }

  return 0;
}

int BLI_sortutil_cmp_ptr(const void *a_, const void *b_)
{
  const struct SortAnyByPtr *a = a_;
  const struct SortAnyByPtr *b = b_;
  if (a->sort_value > b->sort_value) {
    return 1;
  }
  if (a->sort_value < b->sort_value) {
    return -1;
  }

  return 0;
}

int BLI_sortutil_cmp_ptr_reverse(const void *a_, const void *b_)
{
  const struct SortAnyByPtr *a = a_;
  const struct SortAnyByPtr *b = b_;
  if (a->sort_value < b->sort_value) {
    return 1;
  }
  if (a->sort_value > b->sort_value) {
    return -1;
  }

  return 0;
}
