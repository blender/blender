/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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

/** Name of the asset library added by default. Needs translation with `DATA_()` still. */
#define BKE_PREFS_ASSET_LIBRARY_DEFAULT_NAME N_("User Library")

struct bUserAssetLibrary *BKE_preferences_asset_library_add(struct UserDef *userdef,
                                                            const char *name,
                                                            const char *dirpath) ATTR_NONNULL(1);
/**
 * Unlink and free a library preference member.
 * \note Free's \a library itself.
 */
void BKE_preferences_asset_library_remove(struct UserDef *userdef,
                                          struct bUserAssetLibrary *library) ATTR_NONNULL();

void BKE_preferences_asset_library_name_set(struct UserDef *userdef,
                                            struct bUserAssetLibrary *library,
                                            const char *name) ATTR_NONNULL();

/**
 * Set the library path, ensuring it is pointing to a directory.
 * Single blend files can only act as "Current File" library; libraries on disk
 * should always be directories. If the path does not exist, that's fine; it can
 * created as directory if necessary later.
 */
void BKE_preferences_asset_library_path_set(struct bUserAssetLibrary *library, const char *path)
    ATTR_NONNULL();

struct bUserAssetLibrary *BKE_preferences_asset_library_find_from_index(
    const struct UserDef *userdef, int index) ATTR_NONNULL() ATTR_WARN_UNUSED_RESULT;
struct bUserAssetLibrary *BKE_preferences_asset_library_find_from_name(
    const struct UserDef *userdef, const char *name) ATTR_NONNULL() ATTR_WARN_UNUSED_RESULT;

/**
 * Return the bUserAssetLibrary that contains the given file/directory path. The given path can be
 * the library's top-level directory, or any path inside that directory.
 *
 * When more than one asset libraries match, the first matching one is returned (no smartness when
 * there nested asset libraries).
 *
 * Return NULL when no such asset library is found. */
struct bUserAssetLibrary *BKE_preferences_asset_library_containing_path(
    const struct UserDef *userdef, const char *path) ATTR_NONNULL() ATTR_WARN_UNUSED_RESULT;

int BKE_preferences_asset_library_get_index(const struct UserDef *userdef,
                                            const struct bUserAssetLibrary *library)
    ATTR_NONNULL() ATTR_WARN_UNUSED_RESULT;

void BKE_preferences_asset_library_default_add(struct UserDef *userdef) ATTR_NONNULL();

#ifdef __cplusplus
}
#endif
