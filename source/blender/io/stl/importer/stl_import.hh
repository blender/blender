/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup stl
 */

#pragma once

#include "IO_stl.hh"

namespace blender::io::stl {

void stl_import_report_error(FILE *file);

/* Main import function used from within Blender. */
void importer_main(bContext *C, const STLImportParams &import_params);

/* Used from tests, where full bContext does not exist. */
void importer_main(Main *bmain,
                   Scene *scene,
                   ViewLayer *view_layer,
                   const STLImportParams &import_params);

}  // namespace blender::io::stl
