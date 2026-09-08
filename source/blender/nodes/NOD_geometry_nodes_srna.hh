/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup nodes
 */

#pragma once

#include "NOD_nodes_srna.hh"

namespace blender {

struct StructRNA;
struct bNodeTree;

namespace nodes {

/**
 * Shared across all socket types, though some entries don't make sense for some types.
 */
enum class GeometryNodesInputType {
  Fallback = 0,
  Value = 1,
  Attribute = 2,
  Layer = 3,
};

extern const EnumPropertyItem geometry_nodes_input_type_items_fallback[];
extern const EnumPropertyItem geometry_nodes_input_type_items_value[];
extern const EnumPropertyItem geometry_nodes_input_type_items_value_or_attribute[];
extern const EnumPropertyItem geometry_nodes_input_type_items_value_or_attribute_or_layer[];

std::shared_ptr<GeneratedTreeSrnaData> create_geometry_nodes_rna_for_modifier(
    const bNodeTree &tree);

}  // namespace nodes
}  // namespace blender
