// Begin License:
// Copyright (C) 2006-2014 Tobias Sargeant (tobias.sargeant@gmail.com).
// All rights reserved.
//
// This file is part of the Carve CSG Library (http://carve-csg.com/)
//
// This file may be used under the terms of either the GNU General
// Public License version 2 or 3 (at your option) as published by the
// Free Software Foundation and appearing in the files LICENSE.GPL2
// and LICENSE.GPL3 included in the packaging of this file.
//
// This file is provided "AS IS" with NO WARRANTY OF ANY KIND,
// INCLUDING THE WARRANTIES OF DESIGN, MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE.
// End:


#pragma once

#include "intersect_common.hpp"

template<typename T>
static int is_same(const std::vector<T> &a,
                   const std::vector<T> &b) {
  if (a.size() != b.size()) return false;

  const size_t S = a.size();
  size_t i, j, p;

  for (p = 0; p < S; ++p) {
    if (a[0] == b[p]) break;
  }
  if (p == S) return 0;

  for (i = 1, j = p + 1; j < S; ++i, ++j) if (a[i] != b[j]) goto not_fwd;
  for (       j = 0;     i < S; ++i, ++j) if (a[i] != b[j]) goto not_fwd;
  return +1;

not_fwd:
  for (i = 1, j = p - 1; j != (size_t)-1; ++i, --j) if (a[i] != b[j]) goto not_rev;
  for (       j = S - 1;           i < S; ++i, --j) if (a[i] != b[j]) goto not_rev;
  return -1;

not_rev:
  return 0;
}
