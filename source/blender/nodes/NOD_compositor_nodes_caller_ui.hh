/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

namespace blender {

struct bContext;
struct PointerRNA;

namespace ui {
struct Layout;
}  // namespace ui

namespace nodes {

void draw_compositor_nodes_modifier_ui(const bContext &C,
                                       PointerRNA *modifier_ptr,
                                       ui::Layout &layout);

void draw_compositor_nodes_effect_ui(const bContext &C, PointerRNA *strip_ptr, ui::Layout &layout);

}  // namespace nodes
}  // namespace blender
