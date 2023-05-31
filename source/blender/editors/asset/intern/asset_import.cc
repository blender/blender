/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edasset
 */

#include "AS_asset_representation.h"
#include "AS_asset_representation.hh"

#include "BLO_readfile.h"

#include "WM_api.h"

#include "ED_asset_import.h"

using namespace blender;

ID *ED_asset_get_local_id_from_asset_or_append_and_reuse(Main *bmain,
                                                         const AssetRepresentation *asset_c_ptr,
                                                         ID_Type idtype)
{
  const asset_system::AssetRepresentation &asset =
      *reinterpret_cast<const asset_system::AssetRepresentation *>(asset_c_ptr);

  if (ID *local_id = asset.local_id()) {
    return local_id;
  }

  std::string blend_path = asset.get_identifier().full_library_path();
  if (blend_path.empty()) {
    return nullptr;
  }

  return WM_file_append_datablock(bmain,
                                  nullptr,
                                  nullptr,
                                  nullptr,
                                  blend_path.c_str(),
                                  idtype,
                                  asset.get_name().c_str(),
                                  BLO_LIBLINK_APPEND_RECURSIVE |
                                      BLO_LIBLINK_APPEND_ASSET_DATA_CLEAR |
                                      BLO_LIBLINK_APPEND_LOCAL_ID_REUSE);
}
