/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup asset_system
 */

#include <stdexcept>

#include "DNA_ID.h"
#include "DNA_asset_types.h"
#include "DNA_userdef_types.h"

#include "AS_asset_identifier.hh"
#include "AS_asset_library.hh"
#include "AS_asset_representation.h"
#include "AS_asset_representation.hh"

namespace blender::asset_system {

AssetRepresentation::AssetRepresentation(AssetIdentifier &&identifier,
                                         StringRef name,
                                         const int id_type,
                                         std::unique_ptr<AssetMetaData> metadata,
                                         const AssetLibrary &owner_asset_library)
    : identifier_(identifier),
      is_local_id_(false),
      owner_asset_library_(&owner_asset_library),
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
      owner_asset_library_(&owner_asset_library),
      local_asset_id_(&id)
{
  if (!id.asset_data) {
    throw std::invalid_argument("Passed ID is not an asset");
  }
}

AssetRepresentation::AssetRepresentation(AssetRepresentation &&other)
    : identifier_(std::move(other.identifier_)), is_local_id_(other.is_local_id_)
{
  if (is_local_id_) {
    local_asset_id_ = other.local_asset_id_;
    other.local_asset_id_ = nullptr;
  }
  else {
    external_asset_ = std::move(other.external_asset_);
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

std::unique_ptr<AssetWeakReference> AssetRepresentation::make_weak_reference() const
{
  if (!owner_asset_library_) {
    return nullptr;
  }

  return AssetWeakReference::make_reference(*owner_asset_library_, identifier_);
}

StringRefNull AssetRepresentation::get_name() const
{
  if (is_local_id_) {
    return local_asset_id_->name + 2;
  }

  return external_asset_.name;
}

int AssetRepresentation::get_id_type() const
{
  if (is_local_id_) {
    return GS(local_asset_id_->name);
  }

  return external_asset_.id_type;
}

AssetMetaData &AssetRepresentation::get_metadata() const
{
  return is_local_id_ ? *local_asset_id_->asset_data : *external_asset_.metadata_;
}

std::optional<eAssetImportMethod> AssetRepresentation::get_import_method() const
{
  if (!owner_asset_library_) {
    return {};
  }
  return owner_asset_library_->import_method_;
}

bool AssetRepresentation::may_override_import_method() const
{
  if (!owner_asset_library_ || !owner_asset_library_->import_method_) {
    return true;
  }
  return owner_asset_library_->may_override_import_method_;
}

bool AssetRepresentation::get_use_relative_path() const
{
  if (!owner_asset_library_) {
    return false;
  }
  return owner_asset_library_->use_relative_path_;
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
  return *owner_asset_library_;
}

}  // namespace blender::asset_system

using namespace blender;

const StringRefNull AS_asset_representation_library_relative_identifier_get(
    const AssetRepresentation *asset_handle)
{
  const asset_system::AssetRepresentation *asset =
      reinterpret_cast<const asset_system::AssetRepresentation *>(asset_handle);
  const asset_system::AssetIdentifier &identifier = asset->get_identifier();
  return identifier.library_relative_identifier();
}

std::string AS_asset_representation_full_path_get(const AssetRepresentation *asset_handle)
{
  const asset_system::AssetRepresentation *asset =
      reinterpret_cast<const asset_system::AssetRepresentation *>(asset_handle);
  const asset_system::AssetIdentifier &identifier = asset->get_identifier();
  return identifier.full_path();
}

std::string AS_asset_representation_full_library_path_get(const AssetRepresentation *asset_handle)
{
  const asset_system::AssetRepresentation *asset =
      reinterpret_cast<const asset_system::AssetRepresentation *>(asset_handle);
  return asset->get_identifier().full_library_path();
}

std::optional<eAssetImportMethod> AS_asset_representation_import_method_get(
    const AssetRepresentation *asset_handle)
{
  const asset_system::AssetRepresentation *asset =
      reinterpret_cast<const asset_system::AssetRepresentation *>(asset_handle);
  return asset->get_import_method();
}

bool AS_asset_representation_may_override_import_method(const AssetRepresentation *asset_handle)
{
  const asset_system::AssetRepresentation *asset =
      reinterpret_cast<const asset_system::AssetRepresentation *>(asset_handle);
  return asset->may_override_import_method();
}

bool AS_asset_representation_use_relative_path_get(const AssetRepresentation *asset_handle)
{
  const asset_system::AssetRepresentation *asset =
      reinterpret_cast<const asset_system::AssetRepresentation *>(asset_handle);
  return asset->get_use_relative_path();
}

/* ---------------------------------------------------------------------- */
/** \name C-API
 * \{ */

const char *AS_asset_representation_name_get(const AssetRepresentation *asset_handle)
{
  const asset_system::AssetRepresentation *asset =
      reinterpret_cast<const asset_system::AssetRepresentation *>(asset_handle);
  return asset->get_name().c_str();
}

int AS_asset_representation_id_type_get(const AssetRepresentation *asset_handle)
{
  const asset_system::AssetRepresentation *asset =
      reinterpret_cast<const asset_system::AssetRepresentation *>(asset_handle);
  return asset->get_id_type();
}

AssetMetaData *AS_asset_representation_metadata_get(const AssetRepresentation *asset_handle)
{
  const asset_system::AssetRepresentation *asset =
      reinterpret_cast<const asset_system::AssetRepresentation *>(asset_handle);
  return &asset->get_metadata();
}

ID *AS_asset_representation_local_id_get(const AssetRepresentation *asset_handle)
{
  const asset_system::AssetRepresentation *asset =
      reinterpret_cast<const asset_system::AssetRepresentation *>(asset_handle);
  return asset->local_id();
}

bool AS_asset_representation_is_local_id(const AssetRepresentation *asset_handle)
{
  const asset_system::AssetRepresentation *asset =
      reinterpret_cast<const asset_system::AssetRepresentation *>(asset_handle);
  return asset->is_local_id();
}

AssetWeakReference *AS_asset_representation_weak_reference_create(
    const AssetRepresentation *asset_handle)
{
  const asset_system::AssetRepresentation *asset =
      reinterpret_cast<const asset_system::AssetRepresentation *>(asset_handle);
  std::unique_ptr<AssetWeakReference> weak_ref = asset->make_weak_reference();
  return MEM_new<AssetWeakReference>(__func__, std::move(*weak_ref));
}

/** \} */
