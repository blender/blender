/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editors
 *
 * Editor level functions for Blender projects.
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct BlenderProject;

void ED_operatortypes_project(void);

/** Sets the project name to the directory name it is located in and registers a "Project Library"
 * asset library pointing to `//assets/`. */
void ED_project_set_defaults(struct BlenderProject *project);

#ifdef __cplusplus
}
#endif
