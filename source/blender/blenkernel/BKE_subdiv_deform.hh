/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#pragma once

#include "BLI_math_vector_types.hh"
#include "BLI_span.hh"

struct Mesh;

namespace blender::bke::subdiv {

struct Subdiv;

/* Special version of subdivision surface which calculates final positions for coarse vertices.
 * Effectively is pushing the coarse positions to the limit surface.
 *
 * One of the usage examples is calculation of crazy space of subdivision modifier, allowing to
 * paint on a deformed mesh with sub-surf on it.
 *
 * vertex_cos are supposed to hold coordinates of the coarse mesh. */
void deform_coarse_vertices(Subdiv *subdiv,
                            const Mesh *coarse_mesh,
                            MutableSpan<float3> vert_positions);

}  // namespace blender::bke::subdiv
