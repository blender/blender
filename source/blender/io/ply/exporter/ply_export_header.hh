/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup ply
 */

#pragma once

#include "ply_data.hh"
#include "ply_file_buffer.hh"

namespace blender::io::ply {

void write_header(FileBuffer &buffer,
                  const PlyData &ply_data,
                  const PLYExportParams &export_params);

}  // namespace blender::io::ply
