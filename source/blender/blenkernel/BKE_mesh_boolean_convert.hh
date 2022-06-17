/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2019 Blender Foundation. All rights reserved. */

#pragma once

/** \file
 * \ingroup bke
 */

#include "BLI_array.hh"
#include "BLI_float4x4.hh"
#include "BLI_mesh_boolean.hh"
#include "BLI_span.hh"

struct Mesh;

namespace blender::meshintersect {

/**
 * Do a mesh boolean operation directly on meshes (without going back and forth from BMesh).
 * \param transforms: An array of pointers to transform matrices used for each mesh's positions.
 * It is allowed for the pointers to be null, meaning the transformation is the identity.
 * \param material_remaps: An array of maps from material slot numbers in the corresponding mesh
 * to the material slot in the first mesh. It is OK for material_remaps or any of its constituent
 * arrays to be empty.
 * \param r_intersecting_edges: Array to store indices of edges on the resulting mesh in. These
 * 'new' edges are the result of the intersections.
 */
Mesh *direct_mesh_boolean(Span<const Mesh *> meshes,
                          Span<const float4x4 *> transforms,
                          const float4x4 &target_transform,
                          Span<Array<short>> material_remaps,
                          bool use_self,
                          bool hole_tolerant,
                          int boolean_mode,
                          Vector<int> *r_intersecting_edges);

}  // namespace blender::meshintersect
