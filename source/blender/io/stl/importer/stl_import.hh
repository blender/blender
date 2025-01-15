/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup stl
 */

#pragma once

#include <cstdio>

#include "IO_stl.hh"

struct bContext;
struct Main;
struct Mesh;
struct Scene;
struct ViewLayer;

namespace blender::io::stl {

void stl_import_report_error(FILE *file);

/* Used from Geo nodes import for Mesh* access */
Mesh *read_stl_file(const STLImportParams &import_params);

/* Main import function used from within Blender. */
void importer_main(const bContext *C, const STLImportParams &import_params);

/* Used from tests, where full bContext does not exist. */
void importer_main(Main *bmain,
                   Scene *scene,
                   ViewLayer *view_layer,
                   const STLImportParams &import_params);
}  // namespace blender::io::stl
