/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

namespace blender {

struct bContext;
struct PointerRNA;
struct wmOperator;
struct bNodeTree;

namespace ui {
struct Layout;
}  // namespace ui

namespace nodes {

namespace eval_log {
class NodeTreeLog;
}

void draw_geometry_nodes_modifier_ui(const bContext &C,
                                     PointerRNA *modifier_ptr,
                                     ui::Layout &layout);

void draw_geometry_nodes_operator_redo_ui(const bContext &C,
                                          wmOperator &op,
                                          bNodeTree &tree,
                                          eval_log::NodeTreeLog *tree_log);

}  // namespace nodes
}  // namespace blender
