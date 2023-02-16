/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup asset_system
 *
 * \brief Main runtime representation of an asset.
 *
 * Abstraction to reference an asset, with necessary data for display & interaction.
 * https://wiki.blender.org/wiki/Source/Architecture/Asset_System/Back_End#Asset_Representation
 */

#pragma once

#include <memory>
#include <optional>
#include <string>

#include "BLI_string_ref.hh"

#include "DNA_asset_types.h"

#include "AS_asset_identifier.hh"

struct AssetMetaData;
struct ID;

namespace blender::asset_system {

class AssetLibrary;

class AssetRepresentation {
  AssetIdentifier identifier_;
  /** Indicate if this is a local or external asset, and as such, which of the union members below
   * should be used. */
  const bool is_local_id_ = false;
  /** Asset library that owns this asset representation. */
  const AssetLibrary *owner_asset_library_;

  struct ExternalAsset {
    std::string name;
    std::unique_ptr<AssetMetaData> metadata_ = nullptr;
  };
  union {
    ExternalAsset external_asset_;
    ID *local_asset_id_ = nullptr; /* Non-owning. */
  };

  friend class AssetStorage;

 public:
  /** Constructs an asset representation for an external ID. The asset will not be editable. */
  AssetRepresentation(AssetIdentifier &&identifier,
                      StringRef name,
                      std::unique_ptr<AssetMetaData> metadata,
                      const AssetLibrary &owner_asset_library);
  /** Constructs an asset representation for an ID stored in the current file. This makes the asset
   * local and fully editable. */
  AssetRepresentation(AssetIdentifier &&identifier,
                      ID &id,
                      const AssetLibrary &owner_asset_library);
  AssetRepresentation(AssetRepresentation &&other);
  /* Non-copyable type. */
  AssetRepresentation(const AssetRepresentation &other) = delete;
  ~AssetRepresentation();

  /* Non-move-assignable type. Move construction is fine, but treat the "identity" (e.g. local vs
   * external asset) of an asset representation as immutable. */
  AssetRepresentation &operator=(AssetRepresentation &&other) = delete;
  /* Non-copyable type. */
  AssetRepresentation &operator=(const AssetRepresentation &other) = delete;

  const AssetIdentifier &get_identifier() const;

  StringRefNull get_name() const;
  AssetMetaData &get_metadata() const;
  /** Get the import method to use for this asset. A different one may be used if
   * #may_override_import_method() returns true, otherwise, the returned value must be used. If
   * there is no import method predefined for this asset no value is returned.
   */
  std::optional<eAssetImportMethod> get_import_method() const;
  /** Returns if this asset may be imported with an import method other than the one returned by
   * #get_import_method(). Also returns true if there is no predefined import method
   * (when #get_import_method() returns no value). */
  bool may_override_import_method() const;
  /** Returns if this asset is stored inside this current file, and as such fully editable. */
  bool is_local_id() const;
  const AssetLibrary &owner_asset_library() const;
};

}  // namespace blender::asset_system

/* C-Handle */
struct AssetRepresentation;

const std::string AS_asset_representation_full_path_get(const ::AssetRepresentation *asset);
std::optional<eAssetImportMethod> AS_asset_representation_import_method_get(
    const ::AssetRepresentation *asset_handle);
bool AS_asset_representation_may_override_import_method(const ::AssetRepresentation *asset_handle);
