/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 * \brief A KD-tree for nearest neighbor search.
 */

#include "BLI_sys_types.h"

#include <cstdint>

namespace blender {

template<typename CoordT> struct KDTreeCoordTraits {
  static constexpr int DimsNum = CoordT::type_length;
  using ValueType = typename CoordT::base_type;
  static ValueType get(const CoordT &co, const int axis)
  {
    return co[axis];
  }
};

template<> struct KDTreeCoordTraits<float> {
  static constexpr int DimsNum = 1;
  using ValueType = float;
  static float get(const float &co, const int /*axis*/)
  {
    return co;
  }
};

template<typename CoordT> struct KDTreeNode_head {
  uint32_t left, right;
  CoordT co;
  int index;
};

template<typename CoordT> struct KDTreeNode {
  constexpr static int DimsNum = KDTreeCoordTraits<CoordT>::DimsNum;
  using ValueType = typename KDTreeCoordTraits<CoordT>::ValueType;

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

}  // namespace blender
