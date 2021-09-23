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
 * \ingroup bke
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/** Forward declaration, defined in intern/asset_library.hh */
typedef struct AssetLibrary AssetLibrary;

/** TODO(@sybren): properly have a think/discussion about the API for this. */
struct AssetLibrary *BKE_asset_library_load(const char *library_path);
void BKE_asset_library_free(struct AssetLibrary *asset_library);

#ifdef __cplusplus
}
#endif
