/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edasset
 *
 * Supplement for `ED_asset_catalog.hh`. Part of the same API but usable in C.
 */

#pragma once

#include "BLI_utildefines.h"

#ifdef __cplusplus
extern "C" {
#endif

struct AssetLibrary;
struct Main;

void ED_asset_catalogs_save_from_main_path(struct AssetLibrary *library, const struct Main *bmain);

void ED_asset_catalogs_set_save_catalogs_when_file_is_saved(bool should_save);
bool ED_asset_catalogs_get_save_catalogs_when_file_is_saved(void);

#ifdef __cplusplus
}
#endif
