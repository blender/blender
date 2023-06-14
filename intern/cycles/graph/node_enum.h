/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "util/map.h"
#include "util/param.h"

CCL_NAMESPACE_BEGIN

/* Enum
 *
 * Utility class for enum values. */

struct NodeEnum {
  bool empty() const
  {
    return left.empty();
  }
  void insert(const char *x, int y)
  {
    ustring ustr_x(x);

    left[ustr_x] = y;
    right[y] = ustr_x;
  }

  bool exists(ustring x) const
  {
    return left.find(x) != left.end();
  }
  bool exists(int y) const
  {
    return right.find(y) != right.end();
  }

  int operator[](const char *x) const
  {
    return left.find(ustring(x))->second;
  }
  int operator[](ustring x) const
  {
    return left.find(x)->second;
  }
  ustring operator[](int y) const
  {
    return right.find(y)->second;
  }

  unordered_map<ustring, int, ustringHash>::const_iterator begin() const
  {
    return left.begin();
  }
  unordered_map<ustring, int, ustringHash>::const_iterator end() const
  {
    return left.end();
  }

 private:
  unordered_map<ustring, int, ustringHash> left;
  unordered_map<int, ustring> right;
};

CCL_NAMESPACE_END
