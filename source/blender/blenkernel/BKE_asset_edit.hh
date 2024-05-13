/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#pragma once

/**
 * Editing of datablocks from asset libraries.
 *
 * Asset blend files are linked into the global main database, with the asset
 * datablock itself and its dependencies. These datablocks remain linked but
 * are marked as editable.
 *
 * User edited asset datablocks are written to individual blend files per
 * asset. These blend files include any datablock dependencies and packaged
 * image files.
 *
 * This way the blend file can be easily saved, reloaded and deleted.
 *
 * This mechanism is currently only used for brush assets.
 */

#include <optional>
#include <string>

#include "BLI_string_ref.hh"

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

/** Get asset weak reference from ID. */
std::optional<AssetWeakReference> asset_edit_weak_reference_from_id(ID &id);

/** Asset editing operations. */

bool asset_edit_id_is_editable(const ID &id);
bool asset_edit_id_is_writable(const ID &id);

std::optional<std::string> asset_edit_id_save_as(Main &global_main,
                                                 const ID &id,
                                                 StringRef name,
                                                 const bUserAssetLibrary &user_library,
                                                 AssetWeakReference &weak_ref,
                                                 ReportList &reports);

bool asset_edit_id_save(Main &global_main, const ID &id, ReportList &reports);
bool asset_edit_id_revert(Main &global_main, ID &id, ReportList &reports);
bool asset_edit_id_delete(Main &global_main, ID &id, ReportList &reports);

}  // namespace blender::bke
