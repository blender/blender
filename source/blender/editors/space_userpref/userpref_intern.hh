/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spuserpref
 */

#pragma once

#include <array>
#include <optional>
#include <string>

#include "DNA_userdef_types.h"

namespace blender {

struct ARegionType;
struct bContext;
struct wmOperatorType;
struct Panel;

/* internal exports only */

void userpref_panels_register(ARegionType &region_type);

void userpref_asset_libraries_panel_draw(const bContext *C, Panel *panel);

/* `userpref_asset_libraries_list.cc` */

/* -------------------------------------------------------------------- */
/** \name Asset UI List
 *
 * The list in the UI includes items like the "All" or "Essentials" libraries. These functions help
 * converting the index from that list to the #U.asset_libraries index (which doesn't contain these
 * libraries).
 *
 * \{ */

int userpref_ui_asset_libraries_count();
std::optional<int> userpref_ui_asset_libraries_index_from_user_library(
    const bUserAssetLibrary &user_library);

/** \} */

/* `userpref_ops.cc` */

void PREFERENCES_OT_start_filter(wmOperatorType *ot);
void PREFERENCES_OT_clear_filter(wmOperatorType *ot);

struct SpaceUserPref_Runtime {
  /** For filtering properties displayed in the space. */
  std::string search_string;
  /**
   * Results (in the same order as the tabs) for whether each tab has properties
   * that match the search filter. Only valid when #search_string is set.
   */
  std::array<bool, USER_SECTION_DEVELOPER_TOOLS * 2> tab_search_results = {};
};

}  // namespace blender
