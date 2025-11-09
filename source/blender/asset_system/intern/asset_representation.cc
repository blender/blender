/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup asset_system
 */

#include <stdexcept>

#include "BLI_path_utils.hh"

#include "BKE_blendfile.hh"
#include "BKE_icons.hh"
#include "BKE_idtype.hh"
#include "BKE_lib_id.hh"
#include "BKE_preview_image.hh"

#include "DNA_ID.h"
#include "DNA_asset_types.h"

#include "IMB_thumbs.hh"

#include "AS_asset_library.hh"
#include "AS_asset_representation.hh"

namespace blender::asset_system {

AssetRepresentation::AssetRepresentation(StringRef relative_asset_path,
                                         StringRef name,
                                         const int id_type,
                                         std::unique_ptr<AssetMetaData> metadata,
                                         AssetLibrary &owner_asset_library)
    : owner_asset_library_(owner_asset_library),
      relative_identifier_(relative_asset_path),
      asset_(AssetRepresentation::ExternalAsset{name, id_type, std::move(metadata), nullptr})
{
}

AssetRepresentation::AssetRepresentation(ID &id, AssetLibrary &owner_asset_library)
    : owner_asset_library_(owner_asset_library), asset_(&id)
{
  if (!id.asset_data) {
    throw std::invalid_argument("Passed ID is not an asset");
  }
}

AssetRepresentation::~AssetRepresentation()
{
  if (const ExternalAsset *extern_asset = std::get_if<ExternalAsset>(&asset_);
      extern_asset && extern_asset->preview_)
  {
    BKE_previewimg_cached_release(this->full_path().c_str());
  }
}

AssetWeakReference AssetRepresentation::make_weak_reference() const
{
  return AssetWeakReference::make_reference(owner_asset_library_, library_relative_identifier());
}

void AssetRepresentation::ensure_previewable()
{
  if (ID *id = this->local_id()) {
    PreviewImage *preview = BKE_previewimg_id_get(id);
    BKE_icon_preview_ensure(id, preview);
    return;
  }

  ExternalAsset &extern_asset = std::get<ExternalAsset>(asset_);

  /* Use the full path as preview name, it's the only unique identifier we have. */
  const std::string full_path = this->full_path();
  /* Doesn't do the actual reading, just allocates and attaches the derived load info. */
  extern_asset.preview_ = BKE_previewimg_cached_thumbnail_read(
      full_path.c_str(), full_path.c_str(), THB_SOURCE_BLEND, false);

  BKE_icon_preview_ensure(nullptr, extern_asset.preview_);
}

PreviewImage *AssetRepresentation::get_preview() const
{
  if (const ID *id = this->local_id()) {
    return BKE_previewimg_id_get(id);
  }

  return std::get<ExternalAsset>(asset_).preview_;
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
  if (const ID *id = this->local_id()) {
    StringRef idname = BKE_id_name(*id);
    /* Lazy-create/-update with the latest ID name. */
    if (!StringRef{relative_identifier_}.endswith(idname)) {
      relative_identifier_ = StringRef{BKE_idtype_idcode_to_name(GS(id->name))} + SEP_STR + idname;
    }
  }

  return relative_identifier_;
}

std::string AssetRepresentation::full_path() const
{
  char filepath[FILE_MAX];
  BLI_path_join(filepath,
                sizeof(filepath),
                owner_asset_library_.root_path().c_str(),
                library_relative_identifier().c_str());
  return filepath;
}

std::string AssetRepresentation::full_library_path() const
{
  std::string asset_path = full_path();

  char blend_path[/*FILE_MAX_LIBEXTRA*/ 1282];
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

AssetLibrary &AssetRepresentation::owner_asset_library() const
{
  return owner_asset_library_;
}

}  // namespace blender::asset_system
