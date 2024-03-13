/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup ply
 */

#pragma once

#include "IO_ply.hh"
#include "ply_data.hh"

struct bContext;
struct Main;
struct Scene;
struct ViewLayer;

namespace blender::io::ply {

class PlyReadBuffer;

/* Main import function used from within Blender. */
void importer_main(bContext *C, const PLYImportParams &import_params);

/* Used from tests, where full bContext does not exist. */
void importer_main(Main *bmain,
                   Scene *scene,
                   ViewLayer *view_layer,
                   const PLYImportParams &import_params);

const char *read_header(PlyReadBuffer &file, PlyHeader &r_header);

}  // namespace blender::io::ply
