/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/** \file
 * \ingroup bke
 */

#include "BKE_asset_library.hh"

#include "MEM_guardedalloc.h"

#include <memory>

/**
 * Loading an asset library at this point only means loading the catalogs. Later on this should
 * invoke reading of asset representations too.
 */
struct AssetLibrary *BKE_asset_library_load(const char *library_path)
{
  blender::bke::AssetLibrary *lib = new blender::bke::AssetLibrary();
  lib->load(library_path);
  return reinterpret_cast<struct AssetLibrary *>(lib);
}

void BKE_asset_library_free(struct AssetLibrary *asset_library)
{
  blender::bke::AssetLibrary *lib = reinterpret_cast<blender::bke::AssetLibrary *>(asset_library);
  delete lib;
}

namespace blender::bke {

void AssetLibrary::load(StringRefNull library_root_directory)
{
  auto catalog_service = std::make_unique<AssetCatalogService>(library_root_directory);
  catalog_service->load_from_disk();
  this->catalog_service = std::move(catalog_service);
}

}  // namespace blender::bke
