/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#pragma once

#include "DNA_space_types.h"

#include "BLI_compiler_attrs.h"

#ifdef __cplusplus
extern "C" {
#endif

/* C-handle for #bke::BlenderProject. */
typedef struct BlenderProject BlenderProject;

/** See #bke::ProjectSettings::create_settings_directory(). */
bool BKE_project_create_settings_directory(const char *project_root_path) ATTR_NONNULL();
/** See #bke::ProjectSettings::delete_settings_directory(). */
bool BKE_project_delete_settings_directory(const BlenderProject *project) ATTR_NONNULL();

BlenderProject *BKE_project_active_get(void) ATTR_WARN_UNUSED_RESULT;
/**
 * \note: When unsetting an active project, the previously active one will be destroyed, so
 *        pointers may dangle.
 */
void BKE_project_active_unset(void);
/**
 * Check if \a path references a project root directory. Will return false for paths pointing into
 * the project root directory.
 */
bool BKE_project_is_path_project_root(const char *path) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
/**
 * Check if \a path points to or into a project root path (i.e. if one of the ancestors of the
 * referenced file/directory is a project root directory).
 */
bool BKE_project_contains_path(const char *path) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
/**
 * Attempt to load and activate a project based on the given path. If the path doesn't lead
 * into a project, the active project is unset. Note that the project will be unset on any
 * failure when loading the project.
 *
 * \note: When setting an active project, the previously active one will be destroyed, so
 * pointers may dangle.
 */
BlenderProject *BKE_project_active_load_from_path(const char *path) ATTR_NONNULL();

bool BKE_project_settings_save(const BlenderProject *project) ATTR_NONNULL();

const char *BKE_project_root_path_get(const BlenderProject *project) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL();
/**
 * \param name The new name to set, expected to be 0 terminated.
 */
void BKE_project_name_set(const BlenderProject *project_handle, const char *name) ATTR_NONNULL();
const char *BKE_project_name_get(const BlenderProject *project) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL();
ListBase *BKE_project_custom_asset_libraries_get(const BlenderProject *project)
    ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
void BKE_project_tag_has_unsaved_changes(const BlenderProject *project) ATTR_NONNULL();
bool BKE_project_has_unsaved_changes(const BlenderProject *project) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL();

#ifdef __cplusplus
}
#endif
