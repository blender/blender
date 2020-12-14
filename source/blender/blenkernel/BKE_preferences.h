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

#pragma once

/** \file
 * \ingroup bke
 */

#ifdef __cplusplus
extern "C" {
#endif

#include "BLI_compiler_attrs.h"

struct UserDef;
struct bUserAssetLibrary;

void BKE_preferences_asset_library_free(struct bUserAssetLibrary *library) ATTR_NONNULL();

struct bUserAssetLibrary *BKE_preferences_asset_library_add(struct UserDef *userdef,
                                                            const char *name,
                                                            const char *path) ATTR_NONNULL(1);
void BKE_preferences_asset_library_name_set(struct UserDef *userdef,
                                            struct bUserAssetLibrary *library,
                                            const char *name) ATTR_NONNULL();

void BKE_preferences_asset_library_remove(struct UserDef *userdef,
                                          struct bUserAssetLibrary *library) ATTR_NONNULL();

struct bUserAssetLibrary *BKE_preferences_asset_library_find_from_index(
    const struct UserDef *userdef, int index) ATTR_NONNULL() ATTR_WARN_UNUSED_RESULT;
struct bUserAssetLibrary *BKE_preferences_asset_library_find_from_name(
    const struct UserDef *userdef, const char *name) ATTR_NONNULL() ATTR_WARN_UNUSED_RESULT;
int BKE_preferences_asset_library_get_index(const struct UserDef *userdef,
                                            const struct bUserAssetLibrary *library)
    ATTR_NONNULL() ATTR_WARN_UNUSED_RESULT;

void BKE_preferences_asset_library_default_add(struct UserDef *userdef) ATTR_NONNULL();

#ifdef __cplusplus
}
#endif
