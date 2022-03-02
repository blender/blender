/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2018 by Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup bke
 */

#pragma once

#include "BLI_sys_types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct Mesh;
struct MEdge;
struct Subdiv;

typedef struct SubdivToMeshSettings {
  /* Resolution at which regular ptex (created for quad polygon) are being
   * evaluated. This defines how many vertices final mesh will have: every
   * regular ptex has resolution^2 vertices. Special (irregular, or ptex
   * created for a corner of non-quad polygon) will have resolution of
   * `resolution - 1`.
   */
  int resolution;
  /* When true, only edges emitted from coarse ones will be displayed. */
  bool use_optimal_display;
} SubdivToMeshSettings;

/* Create real hi-res mesh from subdivision, all geometry is "real". */
struct Mesh *BKE_subdiv_to_mesh(struct Subdiv *subdiv,
                                const SubdivToMeshSettings *settings,
                                const struct Mesh *coarse_mesh);

/* Interpolate a position along the `coarse_edge` at the relative `u` coordinate. If `is_simple` is
 * false, this will perform a B-Spline interpolation using the edge neighbors, otherwise a linear
 * interpolation will be done base on the edge vertices. */
void BKE_subdiv_mesh_interpolate_position_on_edge(const struct Mesh *coarse_mesh,
                                                  const struct MEdge *coarse_edge,
                                                  bool is_simple,
                                                  float u,
                                                  float pos_r[3]);
#ifdef __cplusplus
}
#endif
