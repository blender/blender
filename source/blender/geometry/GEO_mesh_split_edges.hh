/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_index_mask.hh"

#include "BKE_attribute_filter.hh"

namespace blender {

struct Mesh;

namespace geometry {

void split_edges(Mesh &mesh,
                 const IndexMask &selected_edges,
                 const bke::AttributeFilter &attribute_filter = {});

}  // namespace geometry
}  // namespace blender
