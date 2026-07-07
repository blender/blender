/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 * \brief A KD-tree for nearest neighbor search.
 */

#include <cstdint>

#include "BLI_math_vector_types.hh"

namespace blender {

template<typename CoordT> struct KDTreeNode_head {
  uint32_t left, right;
  CoordT co;
  int index;
};

template<typename CoordT> struct KDTreeNode {
  constexpr static int DimsNum = CoordT::type_length;
  using ValueType = typename CoordT::base_type;

  uint32_t left, right;
  CoordT co;
  int index;
  uint d; /* range is only (0..DimsNum - 1) */
};

template<typename CoordT> struct KDTree {
  using NodeT = KDTreeNode<CoordT>;
  constexpr static int DimsNum = NodeT::DimsNum;
  using ValueType = typename NodeT::ValueType;

  NodeT *nodes;
  uint32_t nodes_len;
  uint32_t root;
  int max_node_index;
#ifndef NDEBUG
  bool is_balanced;            /* ensure we call balance first */
  uint32_t nodes_len_capacity; /* max size of the tree */
#endif
};

template<typename CoordT> struct KDTreeNearest {
  using TreeT = KDTree<CoordT>;
  using ValueType = typename TreeT::ValueType;

  int index;
  ValueType dist;
  CoordT co;
};

using KDTree_1d = KDTree<float1>;
using KDTree_2d = KDTree<float2>;
using KDTree_3d = KDTree<float3>;
using KDTree_4d = KDTree<float4>;

using KDTreeNearest_1d = KDTreeNearest<float1>;
using KDTreeNearest_2d = KDTreeNearest<float2>;
using KDTreeNearest_3d = KDTreeNearest<float3>;
using KDTreeNearest_4d = KDTreeNearest<float4>;

}  // namespace blender
