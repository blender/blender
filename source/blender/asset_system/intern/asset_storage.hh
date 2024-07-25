/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup asset_system
 *
 * The database implementation for asset representations within a library. Not part of the public
 * API, storage access happens via the #AssetLibrary API.
 */

#pragma once

#include <memory>

#include "BLI_set.hh"

struct AssetMetaData;
struct ID;

namespace blender::bke::id {
class IDRemapper;
}

namespace blender::asset_system {

class AssetIdentifier;
class AssetLibrary;
class AssetRepresentation;

class AssetStorage {
  /* Uses shared pointers so the UI can acquire weak pointers. It can then ensure pointers are not
   * dangling before accessing. */
  using StorageT = Set<std::shared_ptr<AssetRepresentation>>;

  StorageT external_assets_;
  /* Store local ID assets separately for efficient lookups.
   * TODO(Julian): A [ID *, asset] or even [ID.session_uid, asset] map would be preferable for
   * faster lookups. Not possible until each asset is only represented once in the storage. */
  StorageT local_id_assets_;

 public:
  /** See #AssetLibrary::add_external_asset(). */
  std::weak_ptr<AssetRepresentation> add_external_asset(AssetIdentifier &&identifier,
                                                        StringRef name,
                                                        int id_type,
                                                        std::unique_ptr<AssetMetaData> metadata,
                                                        const AssetLibrary &owner_asset_library);
  /** See #AssetLibrary::add_external_asset(). */
  std::weak_ptr<AssetRepresentation> add_local_id_asset(AssetIdentifier &&identifier,
                                                        ID &id,
                                                        const AssetLibrary &owner_asset_library);

  /** See #AssetLibrary::remove_asset(). */
  bool remove_asset(AssetRepresentation &asset);

  /** See #AssetLibrary::remap_ids_and_remove_nulled(). */
  void remap_ids_and_remove_invalid(const blender::bke::id::IDRemapper &mappings);
};

}  // namespace blender::asset_system
