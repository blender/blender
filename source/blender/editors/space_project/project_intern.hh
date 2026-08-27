/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spuserpref
 */

#pragma once

#include "DNA_userdef_types.h"

namespace blender {

struct ARegionType;
struct bContext;
struct Panel;

/* internal exports only */

int project_ui_asset_libraries_count();
std::optional<int> project_ui_asset_libraries_index_from_user_library(
    const bUserAssetLibrary &user_library);

void project_asset_panel_register(ARegionType &region_type);

}  // namespace blender
