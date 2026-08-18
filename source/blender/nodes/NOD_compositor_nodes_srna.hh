/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "NOD_nodes_srna.hh"

namespace blender {

struct StructRNA;
struct bNodeTree;

namespace nodes {

enum class CompositorNodesInputType {
  Fallback = 0,
  Value = 1,
};

extern const EnumPropertyItem compositor_nodes_input_type_items_fallback[];
extern const EnumPropertyItem compositor_nodes_input_type_items_value[];

std::shared_ptr<GeneratedTreeSrnaData> create_compositor_nodes_rna_for_strip_modifier(
    const bNodeTree &tree);

std::shared_ptr<GeneratedTreeSrnaData> create_compositor_nodes_rna_for_effect(
    const bNodeTree &tree);

}  // namespace nodes
}  // namespace blender
