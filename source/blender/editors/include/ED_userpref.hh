/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editors
 */

#pragma once

#include <optional>

#include "BLI_uuid.hh"
#include "BLI_vector.hh"

namespace blender {

/* Structs */
struct SpaceUserPref;

void ED_operatortypes_userpref();

Vector<int> ED_userpref_tabs_list(SpaceUserPref *prefs);
bool ED_userpref_tab_has_search_result(SpaceUserPref *sprefs, int index);
void ED_userpref_search_string_set(SpaceUserPref *sprefs, const char *value);
int ED_userpref_search_string_length(SpaceUserPref *sprefs);
const char *ED_userpref_search_string_get(SpaceUserPref *sprefs);

enum class bUserAssetLibraryAddType {
  Remote = 0,
  Local = 1,
};

struct bUserAssetLibrary *ED_userpref_asset_library_new(const struct bContext *C,
                                                        const char *name,
                                                        const char *dirpath,
                                                        bUserAssetLibraryAddType library_type,
                                                        bool is_project_defined,
                                                        std::optional<UUID> uuid,
                                                        std::optional<char *> auth_token);
void ED_userpref_asset_library_remove(bContext *C, struct bUserAssetLibrary *asset_library);

}  // namespace blender
