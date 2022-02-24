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
 * Do a mesh boolean operation directly on meshes (without going back and forth to BMesh).
 * \param meshes: An array of Mesh pointers.
 * \param obmats: An array of pointers to the obmat matrices that transform local
 * coordinates to global ones. It is allowed for the pointers to be null, meaning the
 * transformation is the identity.
 * \param material_remaps: An array of pointers to arrays of maps from material slot numbers in the
 * corresponding mesh to the material slot in the first mesh. It is OK for material_remaps or any
 * of its constituent arrays to be empty.
 */
Mesh *direct_mesh_boolean(blender::Span<const Mesh *> meshes,
                          blender::Span<const float4x4 *> obmats,
                          const float4x4 &target_transform,
                          blender::Span<blender::Array<short>> material_remaps,
                          bool use_self,
                          bool hole_tolerant,
                          int boolean_mode);

}  // namespace blender::meshintersect
