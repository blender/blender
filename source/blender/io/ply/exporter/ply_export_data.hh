/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup ply
 */

#pragma once

namespace blender::io::ply {

class FileBuffer;
struct PlyData;

void write_vertices(FileBuffer &buffer, const PlyData &ply_data);

void write_faces(FileBuffer &buffer, const PlyData &ply_data);

void write_edges(FileBuffer &buffer, const PlyData &ply_data);

}  // namespace blender::io::ply
