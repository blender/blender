/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup asset_system
 */

#include "BLI_listbase.hh"

#include "DNA_userdef_types.h"

#include "common.hh"

namespace blender::asset_system {

UserAssetLibraryWrapper::UserAssetLibraryWrapper(const bUserAssetLibrary &user_asset_library)
    : user_asset_library_(&user_asset_library)
{
}

const bUserAssetLibrary *UserAssetLibraryWrapper::user_asset_library() const
{
  if (user_asset_library_ == nullptr) {
    return nullptr;
  }
  if (BLI_findindex(&U.asset_libraries, user_asset_library_) == -1) {
    user_asset_library_ = nullptr;
    return nullptr;
  }
  return user_asset_library_;
}

}  // namespace blender::asset_system
