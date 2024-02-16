/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup asset_system
 *
 * \brief Main runtime representation of an asset.
 *
 * Abstraction to reference an asset, with necessary data for display & interaction.
 * https://developer.blender.org/docs/features/asset_system/backend/#asset-representation
 */

#pragma once

#include <memory>
#include <optional>
#include <string>

#include "BLI_string_ref.hh"

#include "DNA_ID_enums.h"
#include "DNA_asset_types.h"

#include "AS_asset_identifier.hh"

struct AssetMetaData;
struct ID;

namespace blender::asset_system {

class AssetLibrary;

class AssetRepresentation {
  AssetIdentifier identifier_;
  /**
   * Indicate if this is a local or external asset, and as such, which of the union members below
   * should be used.
   */
  const bool is_local_id_ = false;
  /** Asset library that owns this asset representation. */
  const AssetLibrary &owner_asset_library_;

  struct ExternalAsset {
    std::string name;
    int id_type = 0;
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
                      int id_type,
                      std::unique_ptr<AssetMetaData> metadata,
                      const AssetLibrary &owner_asset_library);
  /**
   * Constructs an asset representation for an ID stored in the current file. This makes the asset
   * local and fully editable.
   */
  AssetRepresentation(AssetIdentifier &&identifier,
                      ID &id,
                      const AssetLibrary &owner_asset_library);
  ~AssetRepresentation();

  AssetRepresentation(const AssetRepresentation &) = delete;
  AssetRepresentation(AssetRepresentation &&) = delete;
  AssetRepresentation &operator=(AssetRepresentation &&) = delete;
  AssetRepresentation &operator=(const AssetRepresentation &) = delete;

  const AssetIdentifier &get_identifier() const;

  /**
   * Create a weak reference for this asset that can be written to files, but can break under a
   * number of conditions.
   * A weak reference can only be created if an asset representation is owned by an asset library.
   *
   * Must be freed using #BKE_asset_weak_reference_free().
   */
  AssetWeakReference *make_weak_reference() const;

  StringRefNull get_name() const;
  ID_Type get_id_type() const;
  AssetMetaData &get_metadata() const;
  /**
   * Get the import method to use for this asset. A different one may be used if
   * #may_override_import_method() returns true, otherwise, the returned value must be used. If
   * there is no import method predefined for this asset no value is returned.
   */
  std::optional<eAssetImportMethod> get_import_method() const;
  /**
   * Returns if this asset may be imported with an import method other than the one returned by
   * #get_import_method(). Also returns true if there is no predefined import method
   * (when #get_import_method() returns no value).
   */
  bool may_override_import_method() const;
  bool get_use_relative_path() const;
  /**
   * If this asset is stored inside this current file (#is_local_id() is true), this returns the
   * ID's pointer, otherwise null.
   */
  ID *local_id() const;
  /** Returns if this asset is stored inside this current file, and as such fully editable. */
  bool is_local_id() const;
  const AssetLibrary &owner_asset_library() const;
};

}  // namespace blender::asset_system
