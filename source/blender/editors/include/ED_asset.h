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
 * \ingroup editors
 */

#pragma once

#include "DNA_ID_enums.h"

#ifdef __cplusplus
extern "C" {
#endif

struct AssetFilterSettings;
struct AssetLibraryReference;
struct Main;
struct ReportList;
struct bContext;
struct wmNotifier;

typedef struct AssetTempIDConsumer AssetTempIDConsumer;

bool ED_asset_mark_id(const struct bContext *C, struct ID *id);
bool ED_asset_clear_id(struct ID *id);

bool ED_asset_can_make_single_from_context(const struct bContext *C);

int ED_asset_library_reference_to_enum_value(const struct AssetLibraryReference *library);
struct AssetLibraryReference ED_asset_library_reference_from_enum_value(int value);

const char *ED_asset_handle_get_name(const AssetHandle *asset);
void ED_asset_handle_get_full_library_path(const struct bContext *C,
                                           const AssetLibraryReference *asset_library,
                                           const AssetHandle *asset,
                                           char r_full_lib_path[]);

AssetTempIDConsumer *ED_asset_temp_id_consumer_create(const AssetHandle *handle);
void ED_asset_temp_id_consumer_free(AssetTempIDConsumer **consumer);
struct ID *ED_asset_temp_id_consumer_ensure_local_id(AssetTempIDConsumer *consumer,
                                                     const struct bContext *C,
                                                     const AssetLibraryReference *asset_library,
                                                     ID_Type id_type,
                                                     struct Main *bmain,
                                                     struct ReportList *reports);

void ED_assetlist_storage_fetch(const struct AssetLibraryReference *library_reference,
                                const struct AssetFilterSettings *filter_settings,
                                const struct bContext *C);
void ED_assetlist_ensure_previews_job(const struct AssetLibraryReference *library_reference,
                                      struct bContext *C);
void ED_assetlist_clear(const struct AssetLibraryReference *library_reference, struct bContext *C);
bool ED_assetlist_storage_has_list_for_library(const AssetLibraryReference *library_reference);
void ED_assetlist_storage_tag_main_data_dirty(void);
void ED_assetlist_storage_id_remap(struct ID *id_old, struct ID *id_new);
void ED_assetlist_storage_exit(void);

ID *ED_assetlist_asset_local_id_get(const AssetHandle *asset_handle);
struct ImBuf *ED_assetlist_asset_image_get(const AssetHandle *asset_handle);
const char *ED_assetlist_library_path(const struct AssetLibraryReference *library_reference);

bool ED_assetlist_listen(const struct AssetLibraryReference *library_reference,
                         const struct wmNotifier *notifier);
int ED_assetlist_size(const struct AssetLibraryReference *library_reference);

void ED_operatortypes_asset(void);

#ifdef __cplusplus
}
#endif

/* TODO move to C++ asset-list header? */
#ifdef __cplusplus

#  include <string>

std::string ED_assetlist_asset_filepath_get(const bContext *C,
                                            const AssetLibraryReference &library_reference,
                                            const AssetHandle &asset_handle);

#  include "BLI_function_ref.hh"
/* Can return false to stop iterating. */
using AssetListIterFn = blender::FunctionRef<bool(FileDirEntry &)>;
void ED_assetlist_iterate(const AssetLibraryReference *library_reference, AssetListIterFn fn);
#endif
