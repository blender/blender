/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_vector.hh"

#include "BKE_attribute_filter.hh"

struct GreasePencil;

namespace blender::geometry {

/**
 * Creates a new grease pencil geometry that has groups of layers merged into one layer per group.
 *
 * \param layers_to_merge: A list of source layer indices for each new layers. Each new layer must
 *   have at least one source layer.
 */
GreasePencil *merge_layers(const GreasePencil &src_grease_pencil,
                           Span<Vector<int>> layers_to_merge,
                           const bke::AttributeFilter &attribute_filter);

}  // namespace blender::geometry
