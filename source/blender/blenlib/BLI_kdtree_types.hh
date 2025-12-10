/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 * \brief A KD-tree for nearest neighbor search.
 */

#include <cstdint>

namespace blender {

template<int DimsNum> struct KDTreeNode_head {
  uint32_t left, right;
  float co[DimsNum];
  int index;
};

template<int DimsNum> struct KDTreeNode {
  uint32_t left, right;
  float co[DimsNum];
  int index;
  uint d; /* range is only (0..DimsNum - 1) */
};

template<int DimsNum> struct KDTree {
  KDTreeNode<DimsNum> *nodes;
  uint32_t nodes_len;
  uint32_t root;
  int max_node_index;
#ifndef NDEBUG
  bool is_balanced;            /* ensure we call balance first */
  uint32_t nodes_len_capacity; /* max size of the tree */
#endif
};

template<int DimsNum> struct KDTreeNearest {
  int index;
  float dist;
  float co[DimsNum];
};

using KDTree_1d = KDTree<1>;
using KDTree_2d = KDTree<2>;
using KDTree_3d = KDTree<3>;
using KDTree_4d = KDTree<4>;

using KDTreeNearest_1d = KDTreeNearest<1>;
using KDTreeNearest_2d = KDTreeNearest<2>;
using KDTreeNearest_3d = KDTreeNearest<3>;
using KDTreeNearest_4d = KDTreeNearest<4>;

}  // namespace blender
