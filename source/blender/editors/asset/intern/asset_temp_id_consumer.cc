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
 * \ingroup edasset
 *
 * API for temporary loading of asset IDs.
 * Uses the `BLO_library_temp_xxx()` API internally.
 */

#include <new>

#include "DNA_space_types.h"

#include "BKE_report.h"

#include "BLI_utility_mixins.hh"

#include "BLO_readfile.h"

#include "MEM_guardedalloc.h"

#include "ED_asset_handle.h"
#include "ED_asset_temp_id_consumer.h"

using namespace blender;

class AssetTemporaryIDConsumer : NonCopyable, NonMovable {
  const AssetHandle &handle_;
  TempLibraryContext *temp_lib_context_ = nullptr;

 public:
  AssetTemporaryIDConsumer(const AssetHandle &handle) : handle_(handle)
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
    return ED_asset_handle_get_local_id(&handle_);
  }

  ID *import_id(const bContext *C,
                const AssetLibraryReference &asset_library_ref,
                ID_Type id_type,
                Main &bmain,
                ReportList &reports)
  {
    const char *asset_name = ED_asset_handle_get_name(&handle_);
    char blend_file_path[FILE_MAX_LIBEXTRA];
    ED_asset_handle_get_full_library_path(C, &asset_library_ref, &handle_, blend_file_path);

    temp_lib_context_ = BLO_library_temp_load_id(
        &bmain, blend_file_path, id_type, asset_name, &reports);

    if (temp_lib_context_ == nullptr || temp_lib_context_->temp_id == nullptr) {
      BKE_reportf(&reports, RPT_ERROR, "Unable to load %s from %s", asset_name, blend_file_path);
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
  BLI_assert(handle->file_data->asset_data != nullptr);
  return reinterpret_cast<AssetTempIDConsumer *>(
      OBJECT_GUARDED_NEW(AssetTemporaryIDConsumer, *handle));
}

void ED_asset_temp_id_consumer_free(AssetTempIDConsumer **consumer)
{
  OBJECT_GUARDED_SAFE_DELETE(*consumer, AssetTemporaryIDConsumer);
}

ID *ED_asset_temp_id_consumer_ensure_local_id(AssetTempIDConsumer *consumer_,
                                              const bContext *C,
                                              const AssetLibraryReference *asset_library_ref,
                                              ID_Type id_type,
                                              Main *bmain,
                                              ReportList *reports)
{
  if (!(consumer_ && asset_library_ref && bmain && reports)) {
    return nullptr;
  }
  AssetTemporaryIDConsumer *consumer = reinterpret_cast<AssetTemporaryIDConsumer *>(consumer_);

  if (ID *local_id = consumer->get_local_id()) {
    return local_id;
  }
  return consumer->import_id(C, *asset_library_ref, id_type, *bmain, *reports);
}
