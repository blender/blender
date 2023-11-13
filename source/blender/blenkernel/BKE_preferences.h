/* SPDX-FileCopyrightText: 2023 Blender Authors
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

/** Name of the asset library added by default. Needs translation with `DATA_()` still. */
#define BKE_PREFS_ASSET_LIBRARY_DEFAULT_NAME N_("User Library")

void BKE_preferences_custom_asset_library_default_add(struct UserDef *userdef) ATTR_NONNULL();

/** \} */

/* -------------------------------------------------------------------- */
/** \name Extension Repositories
 * \{ */

bUserExtensionRepo *BKE_preferences_extension_repo_add(UserDef *userdef,
                                                       const char *name,
                                                       const char *module,
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
