/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup ply
 */

#pragma once

#include "IO_ply.hh"
#include "ply_data.hh"

namespace blender::io::ply {

class PlyReadBuffer;

void splitstr(std::string str, Vector<std::string> &words, const StringRef &deli);

/* Main import function used from within Blender. */
void importer_main(bContext *C, const PLYImportParams &import_params, wmOperator *op);

/* Used from tests, where full bContext does not exist. */
void importer_main(Main *bmain,
                   Scene *scene,
                   ViewLayer *view_layer,
                   const PLYImportParams &import_params,
                   wmOperator *op);

const char *read_header(PlyReadBuffer &file, PlyHeader &r_header);

}  // namespace blender::io::ply
