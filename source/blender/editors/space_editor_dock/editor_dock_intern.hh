/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

struct ARegionType;

namespace blender::ed::editor_dock {

void main_region_panels_register(ARegionType *art);

void register_operatortypes();

}  // namespace blender::ed::editor_dock
