/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#pragma once

/**
 * Editing of datablocks from asset libraries, separate from the current open
 * blend file.
 *
 * Each asset blend file is loaded into a separate main database, including the
 * asset datablocks and their dependencies. These datablocks are all tagged with
 * LIB_TAG_ASSET_EDIT_MAIN. These can not be linked with other datablocks in the
 * current blend file.
 *
 * For editable assets in user asset libraries, each asset is stored in its own
 * blend file. This way the blend file can be easily saved, reloaded and deleted.
 *
 * This mechanism is currently only used for brush assets.
 */

#include <optional>
#include <string>

#include "BLI_string_ref.hh"

#include "AS_asset_catalog.hh"

#include "DNA_ID_enums.h"

struct bUserAssetLibrary;
struct AssetMetaData;
struct AssetWeakReference;
struct ID;
struct Main;
struct ReportList;

namespace blender::bke {

/** Get datablock from weak reference, loading the blend file as needed. */
ID *asset_edit_id_from_weak_reference(Main &global_main,
                                      ID_Type id_type,
                                      const AssetWeakReference &weak_ref);

/** Get main database that a given asset datablock corresponds to. */
Main *asset_edit_main(const ID &id);

/** Asset editing operations. */

bool asset_edit_id_is_editable(const ID &id);

std::optional<std::string> asset_edit_id_save_as(Main &global_main,
                                                 const ID &id,
                                                 StringRef name,
                                                 const bUserAssetLibrary &user_library,
                                                 ReportList &reports);

bool asset_edit_id_save(Main &global_main, const ID &id, ReportList &reports);
bool asset_edit_id_revert(Main &global_main, const ID &id, ReportList &reports);
bool asset_edit_id_delete(Main &global_main, const ID &id, ReportList &reports);

/** Clean up on exit. */
void asset_edit_main_free_all();

}  // namespace blender::bke
