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
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct AssetFilterSettings;
struct AssetHandle;
struct AssetLibraryReference;
struct bContext;
struct ID;
struct wmNotifier;

void ED_assetlist_storage_fetch(const struct AssetLibraryReference *library_reference,
                                const struct bContext *C);
void ED_assetlist_ensure_previews_job(const struct AssetLibraryReference *library_reference,
                                      struct bContext *C);
void ED_assetlist_clear(const struct AssetLibraryReference *library_reference, struct bContext *C);
bool ED_assetlist_storage_has_list_for_library(const AssetLibraryReference *library_reference);
void ED_assetlist_storage_tag_main_data_dirty(void);
void ED_assetlist_storage_id_remap(struct ID *id_old, struct ID *id_new);
void ED_assetlist_storage_exit(void);

struct ImBuf *ED_assetlist_asset_image_get(const AssetHandle *asset_handle);
const char *ED_assetlist_library_path(const struct AssetLibraryReference *library_reference);

bool ED_assetlist_listen(const struct AssetLibraryReference *library_reference,
                         const struct wmNotifier *notifier);
int ED_assetlist_size(const struct AssetLibraryReference *library_reference);

#ifdef __cplusplus
}
#endif
