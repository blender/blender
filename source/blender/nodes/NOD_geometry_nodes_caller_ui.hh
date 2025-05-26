/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

struct bContext;
struct PointerRNA;
struct uiLayout;
struct wmOperator;
struct bNodeTree;

namespace blender::nodes {

namespace geo_eval_log {
class GeoTreeLog;
}

void draw_geometry_nodes_modifier_ui(const bContext &C,
                                     PointerRNA *modifier_ptr,
                                     uiLayout &layout);

void draw_geometry_nodes_operator_redo_ui(const bContext &C,
                                          wmOperator &op,
                                          bNodeTree &tree,
                                          geo_eval_log::GeoTreeLog *tree_log);

}  // namespace blender::nodes
