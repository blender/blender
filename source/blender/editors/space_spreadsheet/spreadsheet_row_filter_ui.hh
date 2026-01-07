/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

namespace blender {

struct ARegionType;

namespace ed::spreadsheet {

void register_row_filter_panels(ARegionType &region_type);

}

}  // namespace blender
