/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editors
 *
 * Editor level functions for Blender projects.
 */

#pragma once

struct Main;
struct ReportList;
namespace blender::bke {
class BlenderProject;
}

void ED_operatortypes_project(void);

/** Sets the project name to the directory name it is located in and registers a "Project Library"
 * asset library pointing to `//assets/`. */
void ED_project_set_defaults(blender::bke::BlenderProject &project);
/**
 * High level project creation (create project + load if needed + write default settings if
 * needed).
 *
 * Initializes a new project in \a project_root_dir by creating the `.blender_project/` directory
 * there. The new project will only be loaded if \a bmain represents a file within the project
 * directory.
 *
 * \return True on success.
 */
bool ED_project_new(const struct Main *bmain,
                    const char *project_root_dir,
                    struct ReportList *reports);
