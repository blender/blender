/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup asset_system
 *
 * \brief Information to uniquely identify and locate an asset.
 *
 * https://developer.blender.org/docs/features/asset_system/backend/#asset-identifier
 */

#pragma once

#include <memory>
#include <string>

#include "BLI_string_ref.hh"

struct AssetWeakReference;

namespace blender::asset_system {

class AssetIdentifier {
  std::shared_ptr<std::string> library_root_path_;
  std::string relative_asset_path_;

 public:
  AssetIdentifier(std::shared_ptr<std::string> library_root_path, std::string relative_asset_path);
  AssetIdentifier(AssetIdentifier &&) = default;
  AssetIdentifier(const AssetIdentifier &) = default;

  StringRefNull library_relative_identifier() const;

  std::string full_path() const;
  std::string full_library_path() const;
};

}  // namespace blender::asset_system
