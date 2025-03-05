/* SPDX-FileCopyrightText: 2013 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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
  const SortAnyByFloat *a = static_cast<const SortAnyByFloat *>(a_);
  const SortAnyByFloat *b = static_cast<const SortAnyByFloat *>(b_);
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
  const SortAnyByFloat *a = static_cast<const SortAnyByFloat *>(a_);
  const SortAnyByFloat *b = static_cast<const SortAnyByFloat *>(b_);
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
  const SortAnyByInt *a = static_cast<const SortAnyByInt *>(a_);
  const SortAnyByInt *b = static_cast<const SortAnyByInt *>(b_);
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
  const SortAnyByInt *a = static_cast<const SortAnyByInt *>(a_);
  const SortAnyByInt *b = static_cast<const SortAnyByInt *>(b_);
  if (a->sort_value < b->sort_value) {
    return 1;
  }
  if (a->sort_value > b->sort_value) {
    return -1;
  }

  return 0;
}
