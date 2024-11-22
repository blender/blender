/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup asset_system
 *
 * An asset library that is purely stored in-memory. Used for the "Current File" asset library
 * while the file has not been saved on disk yet.
 */

#pragma once

#include "AS_asset_library.hh"

namespace blender::asset_system {

class RuntimeAssetLibrary : public AssetLibrary {
 public:
  RuntimeAssetLibrary();
};

}  // namespace blender::asset_system
