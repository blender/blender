/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_offset_indices.hh"
#include "BLI_virtual_array.hh"

#include "BKE_attribute_filter.hh"

namespace blender {

struct GreasePencil;

namespace geometry {

/**
 * Creates a new grease pencil geometry that has groups of layers merged into one layer per group.
 *
 * \param layers_to_merge: A list of source layer indices for each new layers. Each new layer must
 *   have at least one source layer.
 */
GreasePencil *merge_layers(const GreasePencil &src_grease_pencil,
                           GroupedSpan<int> layers_to_merge,
                           const bke::AttributeFilter &attribute_filter);

GreasePencil *merge_layers_by_name(const GreasePencil &src_grease_pencil,
                                   const VArray<bool> &selection,
                                   const bke::AttributeFilter &attribute_filter);

}  // namespace geometry
}  // namespace blender
