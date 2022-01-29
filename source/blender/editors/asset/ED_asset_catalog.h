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
