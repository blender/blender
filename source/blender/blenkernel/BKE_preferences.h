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
struct bUserExtensionRepo;
struct bUserAssetLibrary;

/* -------------------------------------------------------------------- */
/** \name Assert Libraries
 * \{ */

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

struct bUserAssetLibrary *BKE_preferences_asset_library_find_index(const struct UserDef *userdef,
                                                                   int index)
    ATTR_NONNULL() ATTR_WARN_UNUSED_RESULT;
struct bUserAssetLibrary *BKE_preferences_asset_library_find_by_name(const struct UserDef *userdef,
                                                                     const char *name)
    ATTR_NONNULL() ATTR_WARN_UNUSED_RESULT;

/**
 * Return the bUserAssetLibrary that contains the given file/directory path. The given path can be
 * the library's top-level directory, or any path inside that directory.
 *
 * When more than one asset libraries match, the first matching one is returned (no smartness when
 * there nested asset libraries).
 *
 * Return NULL when no such asset library is found.
 */
struct bUserAssetLibrary *BKE_preferences_asset_library_containing_path(
    const struct UserDef *userdef, const char *path) ATTR_NONNULL() ATTR_WARN_UNUSED_RESULT;

int BKE_preferences_asset_library_get_index(const struct UserDef *userdef,
                                            const struct bUserAssetLibrary *library)
    ATTR_NONNULL() ATTR_WARN_UNUSED_RESULT;

void BKE_preferences_asset_library_default_add(struct UserDef *userdef) ATTR_NONNULL();

/** \} */

/* -------------------------------------------------------------------- */
/** \name Extension Repositories
 * \{ */

bUserExtensionRepo *BKE_preferences_extension_repo_add(UserDef *userdef,
                                                       const char *name,
                                                       const char *dirpath);
void BKE_preferences_extension_repo_remove(UserDef *userdef, bUserExtensionRepo *repo);

void BKE_preferences_extension_repo_name_set(UserDef *userdef,
                                             bUserExtensionRepo *repo,
                                             const char *name);
void BKE_preferences_extension_repo_module_set(UserDef *userdef,
                                               bUserExtensionRepo *repo,
                                               const char *module);

void BKE_preferences_extension_repo_path_set(bUserExtensionRepo *repo, const char *path);
bUserExtensionRepo *BKE_preferences_extension_repo_find_index(const UserDef *userdef, int index);
bUserExtensionRepo *BKE_preferences_extension_repo_find_by_module(const UserDef *userdef,
                                                                  const char *module);
int BKE_preferences_extension_repo_get_index(const UserDef *userdef,
                                             const bUserExtensionRepo *repo);

/** \} */

#ifdef __cplusplus
}
#endif
