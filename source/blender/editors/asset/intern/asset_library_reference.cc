/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edasset
 */

#include "BKE_asset_library_custom.h"
#include "BKE_blender_project.h"
#include "BKE_context.h"

#include "DNA_userdef_types.h"

#include "BLI_hash.hh"

#include "ED_asset_library.h"
#include "asset_library_reference.hh"

namespace blender::ed::asset {

AssetLibraryReferenceWrapper::AssetLibraryReferenceWrapper(const AssetLibraryReference &reference)
    : AssetLibraryReference(reference)
{
}

bool operator==(const AssetLibraryReferenceWrapper &a, const AssetLibraryReferenceWrapper &b)
{
  return (a.type == b.type) &&
         (ELEM(a.type, ASSET_LIBRARY_CUSTOM_FROM_PREFERENCES, ASSET_LIBRARY_CUSTOM_FROM_PROJECT) ?
              (a.custom_library_index == b.custom_library_index) :
              true);
}

uint64_t AssetLibraryReferenceWrapper::hash() const
{
  uint64_t hash1 = DefaultHash<decltype(type)>{}(type);
  if (!ELEM(type, ASSET_LIBRARY_CUSTOM_FROM_PREFERENCES, ASSET_LIBRARY_CUSTOM_FROM_PROJECT)) {
    return hash1;
  }

  uint64_t hash2 = DefaultHash<decltype(custom_library_index)>{}(custom_library_index);
  return hash1 ^ (hash2 * 33); /* Copied from DefaultHash for std::pair. */
}

}  // namespace blender::ed::asset

CustomAssetLibraryDefinition *ED_asset_library_find_custom_library_from_reference(
    const AssetLibraryReference *library_ref)
{
  switch (library_ref->type) {
    case ASSET_LIBRARY_CUSTOM_FROM_PREFERENCES:
      return BKE_asset_library_custom_find_from_index(&U.asset_libraries,
                                                      library_ref->custom_library_index);
    case ASSET_LIBRARY_CUSTOM_FROM_PROJECT: {
      BlenderProject *project = CTX_wm_project();
      if (project) {
        return BKE_asset_library_custom_find_from_index(
            BKE_project_custom_asset_libraries_get(project), library_ref->custom_library_index);
      }
      break;
    }
    case ASSET_LIBRARY_LOCAL:
      return nullptr;
  }

  return nullptr;
}
