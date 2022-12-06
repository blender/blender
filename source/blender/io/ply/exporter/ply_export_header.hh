/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup ply
 */

#pragma once

#include "IO_ply.h"
#include "intern/ply_data.hh"
#include "ply_file_buffer.hh"

namespace blender::io::ply {

void write_header(std::unique_ptr<FileBuffer> &buffer,
                     std::unique_ptr<PlyData> &plyData,
                     const PLYExportParams &export_params);

}  // namespace blender::io::ply
