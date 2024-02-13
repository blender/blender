/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup asset_system
 */

#include "AS_asset_representation.hh"

#include "DNA_ID.h"
#include "DNA_asset_types.h"

#include "BKE_lib_remap.hh"

#include "asset_storage.hh"

namespace blender::asset_system {

AssetRepresentation &AssetStorage::add_local_id_asset(AssetIdentifier &&identifier,
                                                      ID &id,
                                                      const AssetLibrary &owner_asset_library)
{
  return *local_id_assets_.lookup_key_or_add(
      std::make_unique<AssetRepresentation>(std::move(identifier), id, owner_asset_library));
}

AssetRepresentation &AssetStorage::add_external_asset(AssetIdentifier &&identifier,
                                                      StringRef name,
                                                      const int id_type,
                                                      std::unique_ptr<AssetMetaData> metadata,
                                                      const AssetLibrary &owner_asset_library)
{
  return *external_assets_.lookup_key_or_add(std::make_unique<AssetRepresentation>(
      std::move(identifier), name, id_type, std::move(metadata), owner_asset_library));
}

bool AssetStorage::remove_asset(AssetRepresentation &asset)
{
  if (local_id_assets_.remove_as(&asset)) {
    return true;
  }
  return external_assets_.remove_as(&asset);
}

void AssetStorage::remap_ids_and_remove_invalid(const blender::bke::id::IDRemapper &mappings)
{
  Set<AssetRepresentation *> removed_assets{};

  for (auto &asset_ptr : local_id_assets_) {
    AssetRepresentation &asset = *asset_ptr;
    BLI_assert(asset.is_local_id());

    const IDRemapperApplyResult result = mappings.apply(&asset.local_asset_id_,
                                                        ID_REMAP_APPLY_DEFAULT);

    /* Entirely remove assets whose ID is unset. We don't want assets with a null ID pointer. */
    if (result == ID_REMAP_RESULT_SOURCE_UNASSIGNED) {
      removed_assets.add(&asset);
    }
  }

  for (AssetRepresentation *asset : removed_assets) {
    remove_asset(*asset);
  }
}

}  // namespace blender::asset_system
