/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup asset_system
 */

#include "AS_asset_representation.hh"

#include "DNA_ID.h"
#include "DNA_asset_types.h"

#include "BKE_lib_remap.h"

#include "asset_storage.hh"

namespace blender::asset_system {

AssetRepresentation &AssetStorage::add_local_id_asset(ID &id)
{
  return *local_id_assets_.lookup_key_or_add(std::make_unique<AssetRepresentation>(id));
}

AssetRepresentation &AssetStorage::add_external_asset(StringRef name,
                                                      std::unique_ptr<AssetMetaData> metadata)
{
  return *external_assets_.lookup_key_or_add(
      std::make_unique<AssetRepresentation>(name, std::move(metadata)));
}

bool AssetStorage::remove_asset(AssetRepresentation &asset)
{
  auto remove_if_contained_fn = [&asset](StorageT &storage) {
    /* Create a "fake" unique_ptr to figure out the hash for the pointed to asset representation.
     * The standard requires that this is the same for all unique_ptr's wrapping the same address.
     */
    std::unique_ptr<AssetRepresentation> fake_asset_ptr{&asset};

    const std::unique_ptr<AssetRepresentation> *real_asset_ptr = storage.lookup_key_ptr_as(
        fake_asset_ptr);
    /* Make sure the contained storage is not destructed. */
    fake_asset_ptr.release();

    if (!real_asset_ptr) {
      return false;
    }

    storage.remove_contained(*real_asset_ptr);
    return true;
  };

  if (remove_if_contained_fn(local_id_assets_)) {
    return true;
  }
  return remove_if_contained_fn(external_assets_);
}

void AssetStorage::remap_ids_and_remove_invalid(const IDRemapper &mappings)
{
  Set<AssetRepresentation *> removed_assets{};

  for (auto &asset_ptr : local_id_assets_) {
    AssetRepresentation &asset = *asset_ptr;
    BLI_assert(asset.is_local_id());

    const IDRemapperApplyResult result = BKE_id_remapper_apply(
        &mappings, &asset.local_asset_id_, ID_REMAP_APPLY_DEFAULT);

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
