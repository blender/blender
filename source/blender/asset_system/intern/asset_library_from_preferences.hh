/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup asset_system
 */

#pragma once

#include "asset_library_on_disk.hh"

namespace blender::asset_system {

class PreferencesOnDiskAssetLibrary : public OnDiskAssetLibrary {
 public:
  PreferencesOnDiskAssetLibrary(StringRef name = "", StringRef root_path = "");
};

}  // namespace blender::asset_system
