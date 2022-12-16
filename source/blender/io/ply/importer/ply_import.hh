/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup ply
 */

#pragma once

#include "IO_ply.h"
#include "ply_data.hh"
#include <cstdio>

namespace blender::io::ply {

void ply_import_report_error(FILE *file);

enum PlyDataTypes from_string(const std::string &input);

void splitstr(std::string str, std::vector<std::string> &words, const std::string &deli);

/* Main import function used from within Blender. */
void importer_main(bContext *C, const PLYImportParams &import_params);

/* Used from tests, where full bContext does not exist. */
void importer_main(Main *bmain,
                   Scene *scene,
                   ViewLayer *view_layer,
                   const PLYImportParams &import_params);

}  // namespace blender::io::ply
