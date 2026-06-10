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
#include "AS_remote_library.hh"

namespace blender::asset_system {

AssetRepresentation::AssetRepresentation(StringRef relative_asset_path,
                                         StringRef name,
                                         const int id_type,
                                         std::unique_ptr<AssetMetaData> metadata,
                                         AssetLibrary &owner_asset_library)
    : owner_asset_library_(owner_asset_library),
      relative_identifier_(relative_asset_path),
      asset_(AssetRepresentation::ExternalAsset{name, id_type, std::move(metadata)})
{
}

AssetRepresentation::AssetRepresentation(StringRef relative_asset_path,
                                         StringRef name,
                                         const int id_type,
                                         std::unique_ptr<AssetMetaData> metadata,
                                         AssetLibrary &owner_asset_library,
                                         OnlineAssetInfo online_info)
    : owner_asset_library_(owner_asset_library),
      relative_identifier_(relative_asset_path),
      asset_(AssetRepresentation::ExternalAsset{
          name,
          id_type,
          std::move(metadata),
          nullptr,
          RemoteAssetFileStatus::UNSET,
          std::make_unique<OnlineAssetInfo>(std::move(online_info))})
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

void AssetRepresentation::ensure_previewable(const bContext &C, ReportList *reports)
{
  if (ID *id = this->local_id()) {
    PreviewImage *preview = BKE_previewimg_id_get(id);
    BKE_icon_preview_ensure(id, preview);
    return;
  }

  ExternalAsset &extern_asset = std::get<ExternalAsset>(asset_);

  if (extern_asset.preview_ && extern_asset.preview_->runtime->icon_id) {
    return;
  }

  /* Only use the remote thumbnail when there is no asset file on disk. Otherwise use the on-disk
   * file. */
  if (this->is_online_only()) {
    if (!extern_asset.online_info_->preview_url) {
      return;
    }

    const std::string preview_path = remote_library_asset_preview_path(*this);
    /* Doesn't do the actual reading, just allocates and attaches the derived load info. */
    extern_asset.preview_ = BKE_previewimg_online_thumbnail_read(
        this->full_path().c_str(), preview_path.c_str(), false);
    remote_library_request_preview_download(C, *this, preview_path, reports);
  }
  else {
    /* Use the full path as preview name, it's the only unique identifier we have. */
    const std::string full_path = this->full_path();

    /* Doesn't do the actual reading, just allocates and attaches the derived load info. */
    extern_asset.preview_ = BKE_previewimg_cached_thumbnail_read(
        full_path.c_str(), full_path.c_str(), THB_SOURCE_BLEND, false);
  }

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

Span<OnlineAssetFile> AssetRepresentation::online_asset_files() const
{
  const ExternalAsset *extern_asset = std::get_if<ExternalAsset>(&asset_);
  if (!extern_asset || !extern_asset->online_info_) {
    return {};
  }
  return extern_asset->online_info_->files;
}

std::optional<int64_t> AssetRepresentation::online_asset_files_combined_size_in_bytes() const
{
  const ExternalAsset *extern_asset = std::get_if<ExternalAsset>(&asset_);
  if (!extern_asset || !extern_asset->online_info_) {
    return {};
  }
  int64_t size = 0;
  for (const OnlineAssetFile &file : online_asset_files()) {
    size += file.size_in_bytes;
  }
  return size;
}

std::optional<StringRefNull> AssetRepresentation::online_asset_preview_url() const
{
  if (!this->is_online_only()) {
    return {};
  }
  std::optional<URLWithHash> &url_with_hash =
      std::get<ExternalAsset>(asset_).online_info_->preview_url;
  if (!url_with_hash) {
    return {};
  }
  return url_with_hash->url;
}

std::optional<StringRefNull> AssetRepresentation::online_asset_preview_hash() const
{
  if (!this->is_online_only()) {
    return {};
  }
  std::optional<URLWithHash> &url_with_hash =
      std::get<ExternalAsset>(asset_).online_info_->preview_url;
  if (!url_with_hash) {
    return {};
  }
  return url_with_hash->hash;
}

void AssetRepresentation::online_asset_mark_downloaded()
{
  ExternalAsset *extern_asset = std::get_if<ExternalAsset>(&asset_);
  if (!extern_asset) {
    return;
  }
  /* Since it was just downloaded, let's assume the file matches the listed hash. If not, the
   * next refresh will show the correct status.
   * TODO: ensure that the file status is actually checked, instead of just making assumptions. */
  extern_asset->remote_file_status_ = RemoteAssetFileStatus::MATCH;
}

std::optional<eAssetImportMethod> AssetRepresentation::get_import_method() const
{
  const AssetMetaData &metadata = this->get_metadata();
  if (metadata.flag & ASSETDATA_USE_OWN_IMPORT_METHOD) {
    return metadata.preferred_import_method;
  }
  return owner_asset_library_.import_method();
}

bool AssetRepresentation::may_override_import_method() const
{
  if (!owner_asset_library_.import_method()) {
    return true;
  }
  return owner_asset_library_.may_override_import_method_;
}

bool AssetRepresentation::get_use_relative_path() const
{
  return owner_asset_library_.use_relative_paths();
}

ID *AssetRepresentation::local_id() const
{
  return this->is_local_id() ? std::get<ID *>(asset_) : nullptr;
}

bool AssetRepresentation::is_local_id() const
{
  return std::holds_alternative<ID *>(asset_);
}

bool AssetRepresentation::is_online_only() const
{
  const ExternalAsset *extern_asset = std::get_if<ExternalAsset>(&asset_);
  if (!extern_asset || !extern_asset->online_info_) {
    return false;
  }
  /* An asset is considered 'online' if there is no file on disk for it.
   *
   * About also allowing UNSET: This function is (indirectly) called from all kinds of
   * places, like `get_node_tools_type_data()` in `node_group_operators.cc` to figure out which
   * node tools are available. Since that happens on startup, the actual on-disk file status may
   * not have been checked yet. Until that time, just assume that having `online_info_` means "it
   * is online". */
  return ELEM(extern_asset->remote_file_status_,
              RemoteAssetFileStatus::NOT_ON_DISK,
              RemoteAssetFileStatus::UNSET);
}

bool AssetRepresentation::is_potentially_editable_asset_blend() const
{
  if (this->owner_asset_library().is_read_only()) {
    return false;
  }

  std::string lib_path = this->full_library_path();
  return StringRef(lib_path).endswith(BLENDER_ASSET_FILE_SUFFIX);
}

RemoteAssetFileStatus AssetRepresentation::remote_file_status() const
{
  const ExternalAsset *extern_asset = std::get_if<ExternalAsset>(&asset_);
  if (!extern_asset) {
    return RemoteAssetFileStatus::UNSET;
  }
  return extern_asset->remote_file_status_;
}

void AssetRepresentation::online_info_set(OnlineAssetInfo info)
{
  ExternalAsset *extern_asset = std::get_if<ExternalAsset>(&asset_);
  if (!extern_asset) {
    return;
  }
  extern_asset->online_info_ = std::make_unique<OnlineAssetInfo>(std::move(info));
}

void AssetRepresentation::remote_file_status_set(const RemoteAssetFileStatus status)
{
  ExternalAsset *extern_asset = std::get_if<ExternalAsset>(&asset_);
  if (!extern_asset) {
    return;
  }
  extern_asset->remote_file_status_ = status;
}

bool AssetRepresentation::needs_download() const
{
  return this->is_online_only() || this->remote_file_status() == RemoteAssetFileStatus::NO_MATCH;
}

AssetLibrary &AssetRepresentation::owner_asset_library() const
{
  return owner_asset_library_;
}

}  // namespace blender::asset_system
