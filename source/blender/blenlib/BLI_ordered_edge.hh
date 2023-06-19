/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_assert.h"
#include "BLI_math_vector_types.hh"

namespace blender {

/**
 * A version of `int2` used as a key for hash-maps, agnostic of the arbitrary order of the two
 * vertices in a mesh edge.
 */
struct OrderedEdge {
  int v_low;
  int v_high;

  OrderedEdge(const int v1, const int v2)
  {
    if (v1 < v2) {
      v_low = v1;
      v_high = v2;
    }
    else {
      v_low = v2;
      v_high = v1;
    }
  }
  OrderedEdge(const int2 edge) : OrderedEdge(edge[0], edge[1]) {}
  OrderedEdge(const uint v1, const uint v2) : OrderedEdge(int(v1), int(v2)) {}

  uint64_t hash() const
  {
    return (this->v_low << 8) ^ this->v_high;
  }

  friend bool operator==(const OrderedEdge &e1, const OrderedEdge &e2)
  {
    BLI_assert(e1.v_low < e1.v_high);
    BLI_assert(e2.v_low < e2.v_high);
    return e1.v_low == e2.v_low && e1.v_high == e2.v_high;
  }
};

}  // namespace blender
