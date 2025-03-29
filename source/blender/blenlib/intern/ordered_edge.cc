/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 */

#include "BLI_ordered_edge.hh"

namespace blender {

std::ostream &operator<<(std::ostream &stream, const OrderedEdge &e)
{
  return stream << "OrderedEdge(" << e.v_low << ", " << e.v_high << ")";
}

}  // namespace blender
