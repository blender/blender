/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_index_mask.hh"

struct Mesh;

namespace blender::geometry {

/**
 * Reverse the order of the vertices in the selected faces. This effectively changes the
 * direction of the face normal.
 */
void flip_faces(Mesh &mesh, const IndexMask &selection);

}  // namespace blender::geometry
