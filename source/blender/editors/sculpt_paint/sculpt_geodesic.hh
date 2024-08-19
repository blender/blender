/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edsculpt
 */

#pragma once

#include "BLI_array.hh"
#include "BLI_set.hh"

struct Depsgraph;
struct Object;
struct PBVHVertRef;

namespace blender::ed::sculpt_paint::geodesic {

/**
 * Returns an array indexed by vertex index containing the geodesic distance to the closest vertex
 * in the initial vertex set. The caller is responsible for freeing the array.
 * Geodesic distances will only work when used with blender::bke::pbvh::Type::Mesh, for other
 * types of blender::bke::pbvh::Tree it will fallback to euclidean distances to one of the initial
 * vertices in the set.
 */
Array<float> distances_create(const Depsgraph &depsgraph,
                              Object &ob,
                              const Set<int> &initial_verts,
                              float limit_radius);
Array<float> distances_create_from_vert_and_symm(const Depsgraph &depsgraph,
                                                 Object &ob,
                                                 PBVHVertRef vertex,
                                                 float limit_radius);

}  // namespace blender::ed::sculpt_paint::geodesic
