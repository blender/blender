/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edasset
 */

#include "AS_asset_representation.hh"

#include "DNA_space_types.h"

#include "BLO_readfile.hh"

#include "WM_api.hh"

#include "ED_asset_import.hh"

namespace blender::ed::asset {

ID *asset_local_id_ensure_imported(
    Main &bmain,
    const asset_system::AssetRepresentation &asset,
    const int flags, /* #eFileSel_Params_Flag + #eBLOLibLinkFlags */
    const std::optional<eAssetImportMethod> import_method,
    const std::optional<ImportInstantiateContext> instantiate_context,
    ReportList *reports)
{
  if (ID *local_id = asset.local_id()) {
    return local_id;
  }

  std::string blend_path = asset.full_library_path();
  if (blend_path.empty()) {
    return nullptr;
  }

  const eAssetImportMethod method = [&]() {
    const bool no_packing = U.experimental.no_data_block_packing;
    if (import_method) {
      return (no_packing && *import_method == ASSET_IMPORT_PACK) ? ASSET_IMPORT_APPEND_REUSE :
                                                                   *import_method;
    }
    if (std::optional asset_method = asset.get_import_method()) {
      return (no_packing && *asset_method == ASSET_IMPORT_PACK) ? ASSET_IMPORT_APPEND_REUSE :
                                                                  *asset_method;
    }
    return ASSET_IMPORT_APPEND_REUSE;
  }();

  Scene *scene = instantiate_context ? instantiate_context->scene : nullptr;
  ViewLayer *view_layer = instantiate_context ? instantiate_context->view_layer : nullptr;
  View3D *view3d = instantiate_context ? instantiate_context->view3d : nullptr;

  switch (method) {
    case ASSET_IMPORT_LINK:
      return WM_file_link_datablock(&bmain,
                                    scene,
                                    view_layer,
                                    view3d,
                                    blend_path.c_str(),
                                    asset.get_id_type(),
                                    asset.get_name().c_str(),
                                    flags | (asset.get_use_relative_path() ? FILE_RELPATH : 0),
                                    reports);
    case ASSET_IMPORT_PACK:
      return WM_file_link_datablock(&bmain,
                                    scene,
                                    view_layer,
                                    view3d,
                                    blend_path.c_str(),
                                    asset.get_id_type(),
                                    asset.get_name().c_str(),
                                    flags | BLO_LIBLINK_PACK |
                                        (asset.get_use_relative_path() ? FILE_RELPATH : 0),
                                    reports);
    case ASSET_IMPORT_APPEND:
      return WM_file_append_datablock(&bmain,
                                      scene,
                                      view_layer,
                                      view3d,
                                      blend_path.c_str(),
                                      asset.get_id_type(),
                                      asset.get_name().c_str(),
                                      flags | BLO_LIBLINK_APPEND_RECURSIVE |
                                          BLO_LIBLINK_APPEND_ASSET_DATA_CLEAR |
                                          (asset.get_use_relative_path() ? FILE_RELPATH : 0),
                                      reports);
    case ASSET_IMPORT_APPEND_REUSE:
      return WM_file_append_datablock(&bmain,
                                      scene,
                                      view_layer,
                                      view3d,
                                      blend_path.c_str(),
                                      asset.get_id_type(),
                                      asset.get_name().c_str(),
                                      flags | BLO_LIBLINK_APPEND_RECURSIVE |
                                          BLO_LIBLINK_APPEND_ASSET_DATA_CLEAR |
                                          BLO_LIBLINK_APPEND_LOCAL_ID_REUSE |
                                          (asset.get_use_relative_path() ? FILE_RELPATH : 0),
                                      reports);
  }
  BLI_assert_unreachable();
  return nullptr;
}

}  // namespace blender::ed::asset
