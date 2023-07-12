/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edasset
 *
 * API for temporary loading of asset IDs.
 * Uses the `BLO_library_temp_xxx()` API internally.
 */

#include <new>
#include <string>

#include "AS_asset_representation.hh"

#include "DNA_space_types.h"

#include "ED_asset.h"

#include "BKE_report.h"

#include "BLI_utility_mixins.hh"

#include "BLO_readfile.h"

#include "MEM_guardedalloc.h"

#include "ED_asset_temp_id_consumer.h"

using namespace blender;

class AssetTemporaryIDConsumer : NonCopyable, NonMovable {
  const blender::asset_system::AssetRepresentation *asset_;
  TempLibraryContext *temp_lib_context_ = nullptr;

 public:
  AssetTemporaryIDConsumer(const blender::asset_system::AssetRepresentation *asset) : asset_(asset)
  {
  }
  ~AssetTemporaryIDConsumer()
  {
    if (temp_lib_context_) {
      BLO_library_temp_free(temp_lib_context_);
    }
  }

  ID *get_local_id()
  {
    return asset_->local_id();
  }

  ID *import_id(ID_Type id_type, Main &bmain, ReportList &reports)
  {
    const char *asset_name = asset_->get_name().c_str();
    std::string blend_file_path = asset_->get_identifier().full_library_path();

    temp_lib_context_ = BLO_library_temp_load_id(
        &bmain, blend_file_path.c_str(), id_type, asset_name, &reports);

    if (temp_lib_context_ == nullptr || temp_lib_context_->temp_id == nullptr) {
      BKE_reportf(
          &reports, RPT_ERROR, "Unable to load %s from %s", asset_name, blend_file_path.c_str());
      return nullptr;
    }

    BLI_assert(GS(temp_lib_context_->temp_id->name) == id_type);
    return temp_lib_context_->temp_id;
  }
};

AssetTempIDConsumer *ED_asset_temp_id_consumer_create(const AssetHandle *handle)
{
  if (!handle) {
    return nullptr;
  }
  BLI_assert(handle->file_data->asset != nullptr);
  return reinterpret_cast<AssetTempIDConsumer *>(
      MEM_new<AssetTemporaryIDConsumer>(__func__, ED_asset_handle_get_representation(handle)));
}

void ED_asset_temp_id_consumer_free(AssetTempIDConsumer **consumer)
{
  MEM_delete(reinterpret_cast<AssetTemporaryIDConsumer *>(*consumer));
  *consumer = nullptr;
}

ID *ED_asset_temp_id_consumer_ensure_local_id(AssetTempIDConsumer *consumer_,
                                              ID_Type id_type,
                                              Main *bmain,
                                              ReportList *reports)
{
  if (!(consumer_ && bmain && reports)) {
    return nullptr;
  }
  AssetTemporaryIDConsumer *consumer = reinterpret_cast<AssetTemporaryIDConsumer *>(consumer_);

  if (ID *local_id = consumer->get_local_id()) {
    return local_id;
  }
  return consumer->import_id(id_type, *bmain, *reports);
}
