/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup asset_system
 */

#include <stdexcept>

#include "DNA_ID.h"
#include "DNA_asset_types.h"

#include "AS_asset_identifier.hh"
#include "AS_asset_representation.h"
#include "AS_asset_representation.hh"

namespace blender::asset_system {

AssetRepresentation::AssetRepresentation(AssetIdentifier &&identifier,
                                         StringRef name,
                                         std::unique_ptr<AssetMetaData> metadata)
    : identifier_(identifier), is_local_id_(false), external_asset_()
{
  external_asset_.name = name;
  external_asset_.metadata_ = std::move(metadata);
}

AssetRepresentation::AssetRepresentation(AssetIdentifier &&identifier, ID &id)
    : identifier_(identifier), is_local_id_(true), local_asset_id_(&id)
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

StringRefNull AssetRepresentation::get_name() const
{
  if (is_local_id_) {
    return local_asset_id_->name + 2;
  }

  return external_asset_.name;
}

AssetMetaData &AssetRepresentation::get_metadata() const
{
  return is_local_id_ ? *local_asset_id_->asset_data : *external_asset_.metadata_;
}

bool AssetRepresentation::is_local_id() const
{
  return is_local_id_;
}

}  // namespace blender::asset_system

using namespace blender;

const std::string AS_asset_representation_full_path_get(const AssetRepresentation *asset_handle)
{
  const asset_system::AssetRepresentation *asset =
      reinterpret_cast<const asset_system::AssetRepresentation *>(asset_handle);
  const asset_system::AssetIdentifier &identifier = asset->get_identifier();
  return identifier.full_path();
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

AssetMetaData *AS_asset_representation_metadata_get(const AssetRepresentation *asset_handle)
{
  const asset_system::AssetRepresentation *asset =
      reinterpret_cast<const asset_system::AssetRepresentation *>(asset_handle);
  return &asset->get_metadata();
}

bool AS_asset_representation_is_local_id(const AssetRepresentation *asset_handle)
{
  const asset_system::AssetRepresentation *asset =
      reinterpret_cast<const asset_system::AssetRepresentation *>(asset_handle);
  return asset->is_local_id();
}

/** \} */
