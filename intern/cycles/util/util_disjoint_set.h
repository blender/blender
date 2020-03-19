/*
 * Copyright 2011-2013 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __UTIL_DISJOINT_SET_H__
#define __UTIL_DISJOINT_SET_H__

#include "util_array.h"
#include <utility>

CCL_NAMESPACE_BEGIN

class DisjointSet {
 private:
  array<size_t> parents;
  array<size_t> ranks;

 public:
  DisjointSet(size_t size) : parents(size), ranks(size)
  {
    for (size_t i = 0; i < size; i++) {
      parents[i] = i;
      ranks[i] = 0;
    }
  }

  size_t find(size_t x)
  {
    size_t root = x;
    while (parents[root] != root) {
      root = parents[root];
    }
    while (parents[x] != root) {
      size_t parent = parents[x];
      parents[x] = root;
      x = parent;
    }
    return root;
  }

  void join(size_t x, size_t y)
  {
    size_t x_root = find(x);
    size_t y_root = find(y);

    if (x_root == y_root) {
      return;
    }

    if (ranks[x_root] < ranks[y_root]) {
      std::swap(x_root, y_root);
    }
    parents[y_root] = x_root;

    if (ranks[x_root] == ranks[y_root]) {
      ranks[x_root]++;
    }
  }
};

CCL_NAMESPACE_END

#endif /* __UTIL_DISJOINT_SET_H__ */
