/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

struct bContext;
struct PointerRNA;
struct uiLayout;

namespace blender::nodes {

void draw_geometry_nodes_modifier_ui(const bContext &C,
                                     PointerRNA *modifier_ptr,
                                     uiLayout &layout);

}
