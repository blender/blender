/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup ply
 */

#pragma once

#include "BLI_fileops.hh"

#include "DNA_mesh_types.h"

#include "IO_ply.h"
#include "ply_data.hh"

namespace blender::io::ply {

/**
 * The function that gets called from the importer.
 * \param file: The PLY file that was opened.
 * \param header: The information in the PLY header.
 */
std::unique_ptr<PlyData> import_ply_ascii(fstream &file, PlyHeader *header);

/**
 * Loads the information from the PLY file in ASCII format to the #PlyData data-structure.
 * \param file: The PLY file that was opened.
 * \param header: The information in the PLY header.
 * \return The #PlyData data-structure that can be used for conversion to a Mesh.
 */
PlyData load_ply_ascii(fstream &file, const PlyHeader *header);

int3 get_vertex_index(const PlyHeader *header);
int3 get_color_index(const PlyHeader *header);
int3 get_normal_index(const PlyHeader *header);
int2 get_uv_index(const PlyHeader *header);
int get_index(const PlyHeader *header, std::string property, PlyDataTypes datatype);
Vector<std::string> explode(const StringRef str, const char &ch);
}  // namespace blender::io::ply
