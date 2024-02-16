/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup asset_system
 */

#include <stdexcept>

#include "DNA_ID.h"
#include "DNA_asset_types.h"

#include "AS_asset_identifier.hh"
#include "AS_asset_library.hh"
#include "AS_asset_representation.hh"

namespace blender::asset_system {

AssetRepresentation::AssetRepresentation(AssetIdentifier &&identifier,
                                         StringRef name,
                                         const int id_type,
                                         std::unique_ptr<AssetMetaData> metadata,
                                         const AssetLibrary &owner_asset_library)
    : identifier_(identifier),
      is_local_id_(false),
      owner_asset_library_(owner_asset_library),
      external_asset_()
{
  external_asset_.name = name;
  external_asset_.id_type = id_type;
  external_asset_.metadata_ = std::move(metadata);
}

AssetRepresentation::AssetRepresentation(AssetIdentifier &&identifier,
                                         ID &id,
                                         const AssetLibrary &owner_asset_library)
    : identifier_(identifier),
      is_local_id_(true),
      owner_asset_library_(owner_asset_library),
      local_asset_id_(&id)
{
  if (!id.asset_data) {
    throw std::invalid_argument("Passed ID is not an asset");
  }
}

AssetRepresentation::~AssetRepresentation()
{
  if (!is_local_id_) {
    external_asset_.~ExternalAsset();
  }
}

const AssetIdentifier &AssetRepresentation::get_identifier() const
{
  return identifier_;
}

AssetWeakReference *AssetRepresentation::make_weak_reference() const
{
  return AssetWeakReference::make_reference(owner_asset_library_, identifier_);
}

StringRefNull AssetRepresentation::get_name() const
{
  if (is_local_id_) {
    return local_asset_id_->name + 2;
  }

  return external_asset_.name;
}

ID_Type AssetRepresentation::get_id_type() const
{
  if (is_local_id_) {
    return GS(local_asset_id_->name);
  }

  return ID_Type(external_asset_.id_type);
}

AssetMetaData &AssetRepresentation::get_metadata() const
{
  return is_local_id_ ? *local_asset_id_->asset_data : *external_asset_.metadata_;
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
  return is_local_id_ ? local_asset_id_ : nullptr;
}

bool AssetRepresentation::is_local_id() const
{
  return is_local_id_;
}

const AssetLibrary &AssetRepresentation::owner_asset_library() const
{
  return owner_asset_library_;
}

}  // namespace blender::asset_system
