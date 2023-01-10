/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2018 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup bke
 */

#pragma once

#include "BLI_sys_types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct Mesh;
struct MeshElemMap;
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
void BKE_subdiv_mesh_interpolate_position_on_edge(const float (*coarse_positions)[3],
                                                  const struct MEdge *coarse_edges,
                                                  const struct MeshElemMap *vert_to_edge_map,
                                                  int coarse_edge_index,
                                                  bool is_simple,
                                                  float u,
                                                  float pos_r[3]);
#ifdef __cplusplus
}
#endif
