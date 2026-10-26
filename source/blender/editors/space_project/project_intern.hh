/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spuserpref
 */

#pragma once

#include "DNA_userdef_types.h"
#include "WM_types.hh"

namespace blender {

struct ARegionType;
struct bContext;
struct Panel;

/* internal exports only */

int project_ui_asset_libraries_count();
std::optional<int> project_ui_asset_libraries_index_from_user_library(
    const bUserAssetLibrary &user_library);

void project_asset_panel_register(ARegionType &region_type);

void PROJECT_OT_new(wmOperatorType *ot);
void PROJECT_OT_open(wmOperatorType *ot);
void PROJECT_OT_close(wmOperatorType *ot);

void PROJECT_OT_config_save(wmOperatorType *ot);

void PROJECT_OT_asset_library_add(wmOperatorType *ot);
void PROJECT_OT_asset_library_remove(wmOperatorType *ot);

void PROJECT_OT_variable_add(wmOperatorType *ot);
void PROJECT_OT_variable_remove(wmOperatorType *ot);
void PROJECT_OT_variable_move(wmOperatorType *ot);

void PROJECT_OT_splash_image_create(wmOperatorType *ot);
void PROJECT_OT_splash_image_clear(wmOperatorType *ot);

}  // namespace blender
