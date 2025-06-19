/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup asset_system
 */

#include "BLI_listbase.h"

#include "DNA_userdef_types.h"

#include "remote_library.hh"

namespace blender::asset_system {

RemoteAssetLibrary::RemoteAssetLibrary(const StringRef remote_url, StringRef cache_root_path)
    : AssetLibrary(ASSET_LIBRARY_CUSTOM, "", cache_root_path)
{
  import_method_ = ASSET_IMPORT_APPEND_REUSE;
  may_override_import_method_ = false;
  remote_url_ = remote_url;
}

std::optional<AssetLibraryReference> RemoteAssetLibrary::library_reference() const
{
  int i;
  LISTBASE_FOREACH_INDEX (const bUserAssetLibrary *, asset_library, &U.asset_libraries, i) {
    if ((asset_library->flag & ASSET_LIBRARY_USE_REMOTE_URL) == 0) {
      continue;
    }

    if (asset_library->remote_url == this->remote_url_) {
      AssetLibraryReference library_ref{};
      library_ref.type = ASSET_LIBRARY_CUSTOM;
      library_ref.custom_library_index = i;
      return library_ref;
    }
  }

  return {};
}

void RemoteAssetLibrary::refresh_catalogs()
{
  this->catalog_service().reload_catalogs();
}

}  // namespace blender::asset_system
