/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup ply
 */

#pragma once

struct PLYExportParams;

namespace blender::io::ply {

class FileBuffer;
struct PlyData;

void write_header(FileBuffer &buffer,
                  const PlyData &ply_data,
                  const PLYExportParams &export_params);

}  // namespace blender::io::ply
