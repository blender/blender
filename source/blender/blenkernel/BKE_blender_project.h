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

BlenderProject *BKE_project_active_get(void) ATTR_WARN_UNUSED_RESULT;
/**
 * \note: When unsetting an active project, the previously active one will be destroyed, so
 *        pointers may dangle.
 */
void BKE_project_active_unset(void);
/**
 * Attempt to load and activate a project based on the given path. If the path doesn't lead into a
 * project, the active project is unset. Note that the project will be unset on any failure when
 * loading the project.
 *
 * \note: When setting an active project, the previously active one will be destroyed, so pointers
 *        may dangle.
 */
BlenderProject *BKE_project_active_load_from_path(const char *path) ATTR_NONNULL();

const char *BKE_project_root_path_get(const BlenderProject *project) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL();
const char *BKE_project_name_get(const BlenderProject *project) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL();

#ifdef __cplusplus
}
#endif
