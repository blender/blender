/* SPDX-FileCopyrightText: 2019 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#pragma once

#include "BLI_sys_types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct Mesh;
struct Subdiv;

/* Special version of subdivision surface which calculates final positions for coarse vertices.
 * Effectively is pushing the coarse positions to the limit surface.
 *
 * One of the usage examples is calculation of crazy space of subdivision modifier, allowing to
 * paint on a deformed mesh with sub-surf on it.
 *
 * vertex_cos are supposed to hold coordinates of the coarse mesh. */
void BKE_subdiv_deform_coarse_vertices(struct Subdiv *subdiv,
                                       const struct Mesh *coarse_mesh,
                                       float (*vertex_cos)[3],
                                       int num_verts);

#ifdef __cplusplus
}
#endif
