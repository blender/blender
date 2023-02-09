/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup ply
 */

#pragma once

#include "ply_data.hh"
#include "ply_file_buffer.hh"

namespace blender::io::ply {

void write_vertices(FileBuffer &buffer, const PlyData &ply_data);

void write_faces(FileBuffer &buffer, const PlyData &ply_data);

void write_edges(FileBuffer &buffer, const PlyData &ply_data);

}  // namespace blender::io::ply
