/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup asset_system
 */

#pragma once

#include <memory>
#include <string>

#include "BLI_string_ref.hh"

struct AssetMetaData;
struct ID;

namespace blender::asset_system {

/**
 * \brief Abstraction to reference an asset, with necessary data for display & interaction.
 *
 * https://wiki.blender.org/wiki/Source/Architecture/Asset_System/Back_End#Asset_Representation
 */
class AssetRepresentation {
  friend struct AssetLibrary;
  friend class AssetStorage;

  struct ExternalAsset {
    std::string name;
    std::unique_ptr<AssetMetaData> metadata_ = nullptr;
  };

  /** Indicate if this is a local or external asset, and as such, which of the union members below
   * should be used. */
  const bool is_local_id_ = false;

  union {
    ExternalAsset external_asset_;
    ID *local_asset_id_ = nullptr; /* Non-owning. */
  };

 public:
  /** Constructs an asset representation for an external ID. The asset will not be editable. */
  explicit AssetRepresentation(StringRef name, std::unique_ptr<AssetMetaData> metadata);
  /** Constructs an asset representation for an ID stored in the current file. This makes the asset
   * local and fully editable. */
  explicit AssetRepresentation(ID &id);
  AssetRepresentation(AssetRepresentation &&other);
  /* Non-copyable type. */
  AssetRepresentation(const AssetRepresentation &other) = delete;
  ~AssetRepresentation();

  /* Non-move-assignable type. Move construction is fine, but treat the "identity" (e.g. local vs
   * external asset) of an asset representation as immutable. */
  AssetRepresentation &operator=(AssetRepresentation &&other) = delete;
  /* Non-copyable type. */
  AssetRepresentation &operator=(const AssetRepresentation &other) = delete;

  StringRefNull get_name() const;
  AssetMetaData &get_metadata() const;
  /** Returns if this asset is stored inside this current file, and as such fully editable. */
  bool is_local_id() const;
};

}  // namespace blender::asset_system
