/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup ply
 */

#pragma once

#include "DNA_mesh_types.h"
#include "ply_data.hh"

namespace blender::io::ply {

/**
 * Converts the PlyData datastructure to a mesh.
 * @param data The PLY data.
 * @return The mesh that can be used inside blender.
 */
Mesh *convert_ply_to_mesh(PlyData &data, Mesh *mesh);

}  // namespace blender::io::ply
