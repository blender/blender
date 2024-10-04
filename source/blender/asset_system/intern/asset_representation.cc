/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup asset_system
 */

#include <stdexcept>

#include "BLI_path_utils.hh"

#include "BKE_blendfile.hh"

#include "DNA_ID.h"
#include "DNA_asset_types.h"

#include "AS_asset_library.hh"
#include "AS_asset_representation.hh"

namespace blender::asset_system {

AssetRepresentation::AssetRepresentation(StringRef relative_asset_path,
                                         StringRef name,
                                         const int id_type,
                                         std::unique_ptr<AssetMetaData> metadata,
                                         const AssetLibrary &owner_asset_library)
    : owner_asset_library_(owner_asset_library),
      relative_identifier_(relative_asset_path),
      asset_(AssetRepresentation::ExternalAsset{name, id_type, std::move(metadata)})
{
}

AssetRepresentation::AssetRepresentation(StringRef relative_asset_path,
                                         ID &id,
                                         const AssetLibrary &owner_asset_library)
    : owner_asset_library_(owner_asset_library),
      relative_identifier_(relative_asset_path),
      asset_(&id)
{
  if (!id.asset_data) {
    throw std::invalid_argument("Passed ID is not an asset");
  }
}

AssetWeakReference AssetRepresentation::make_weak_reference() const
{
  return AssetWeakReference::make_reference(owner_asset_library_, relative_identifier_);
}

StringRefNull AssetRepresentation::get_name() const
{
  if (const ID *id = this->local_id()) {
    return id->name + 2;
  }
  return std::get<ExternalAsset>(asset_).name;
}

ID_Type AssetRepresentation::get_id_type() const
{
  if (const ID *id = this->local_id()) {
    return GS(id->name);
  }
  return ID_Type(std::get<ExternalAsset>(asset_).id_type);
}

AssetMetaData &AssetRepresentation::get_metadata() const
{
  if (const ID *id = this->local_id()) {
    return *id->asset_data;
  }
  return *std::get<ExternalAsset>(asset_).metadata_;
}

StringRefNull AssetRepresentation::library_relative_identifier() const
{
  return relative_identifier_;
}

std::string AssetRepresentation::full_path() const
{
  char filepath[FILE_MAX];
  BLI_path_join(filepath,
                sizeof(filepath),
                owner_asset_library_.root_path().c_str(),
                relative_identifier_.c_str());
  return filepath;
}

std::string AssetRepresentation::full_library_path() const
{
  std::string asset_path = full_path();

  char blend_path[1090 /*FILE_MAX_LIBEXTRA*/];
  if (!BKE_blendfile_library_path_explode(asset_path.c_str(), blend_path, nullptr, nullptr)) {
    return {};
  }

  return blend_path;
}

std::optional<eAssetImportMethod> AssetRepresentation::get_import_method() const
{
  return owner_asset_library_.import_method_;
}

bool AssetRepresentation::may_override_import_method() const
{
  if (!owner_asset_library_.import_method_) {
    return true;
  }
  return owner_asset_library_.may_override_import_method_;
}

bool AssetRepresentation::get_use_relative_path() const
{
  return owner_asset_library_.use_relative_path_;
}

ID *AssetRepresentation::local_id() const
{
  return this->is_local_id() ? std::get<ID *>(asset_) : nullptr;
}

bool AssetRepresentation::is_local_id() const
{
  return std::holds_alternative<ID *>(asset_);
}

const AssetLibrary &AssetRepresentation::owner_asset_library() const
{
  return owner_asset_library_;
}

}  // namespace blender::asset_system
