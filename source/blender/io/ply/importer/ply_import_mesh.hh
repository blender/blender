/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup ply
 */

#pragma once

#include "IO_ply.h"
#include "ply_data.hh"

namespace blender::io::ply {

/**
 * Converts the #PlyData data-structure to a mesh.
 * \return A new mesh that can be used inside blender.
 */
Mesh *convert_ply_to_mesh(PlyData &data, const PLYImportParams &params);

}  // namespace blender::io::ply
