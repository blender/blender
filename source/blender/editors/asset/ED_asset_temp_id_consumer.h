/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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
struct Main;
struct ReportList;

AssetTempIDConsumer *ED_asset_temp_id_consumer_create(const struct AssetHandle *handle);
void ED_asset_temp_id_consumer_free(AssetTempIDConsumer **consumer);
struct ID *ED_asset_temp_id_consumer_ensure_local_id(AssetTempIDConsumer *consumer,
                                                     ID_Type id_type,
                                                     struct Main *bmain,
                                                     struct ReportList *reports);

#ifdef __cplusplus
}
#endif
