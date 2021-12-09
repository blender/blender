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
 * API to abstract away details for temporary loading of an ID from an asset. If the ID is stored
 * in the current file (or more precisely, in the #Main given when requesting an ID) no loading is
 * performed and the ID is returned. Otherwise it's imported for temporary access using the
 * `BLO_library_temp` API.
 */

#pragma once

#include "DNA_ID_enums.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct AssetTempIDConsumer AssetTempIDConsumer;

struct AssetHandle;
struct AssetLibraryReference;
struct Main;
struct ReportList;
struct bContext;

AssetTempIDConsumer *ED_asset_temp_id_consumer_create(const struct AssetHandle *handle);
void ED_asset_temp_id_consumer_free(AssetTempIDConsumer **consumer);
struct ID *ED_asset_temp_id_consumer_ensure_local_id(
    AssetTempIDConsumer *consumer,
    const struct bContext *C,
    const struct AssetLibraryReference *asset_library_ref,
    ID_Type id_type,
    struct Main *bmain,
    struct ReportList *reports);

#ifdef __cplusplus
}
#endif
