/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2018 Blender Foundation */

/** \file
 * \ingroup bke
 */

#pragma once

#include "BLI_math_vector_types.hh"
#include "BLI_sys_types.h"

struct Mesh;
struct MeshElemMap;
struct Subdiv;

struct SubdivToMeshSettings {
  /* Resolution at which regular ptex (created for quad polygon) are being
   * evaluated. This defines how many vertices final mesh will have: every
   * regular ptex has resolution^2 vertices. Special (irregular, or ptex
   * created for a corner of non-quad polygon) will have resolution of
   * `resolution - 1`.
   */
  int resolution;
  /* When true, only edges emitted from coarse ones will be displayed. */
  bool use_optimal_display;
};

/* Create real hi-res mesh from subdivision, all geometry is "real". */
Mesh *BKE_subdiv_to_mesh(Subdiv *subdiv,
                         const SubdivToMeshSettings *settings,
                         const Mesh *coarse_mesh);

/* Interpolate a position along the `coarse_edge` at the relative `u` coordinate. If `is_simple` is
 * false, this will perform a B-Spline interpolation using the edge neighbors, otherwise a linear
 * interpolation will be done base on the edge vertices. */
void BKE_subdiv_mesh_interpolate_position_on_edge(const float (*coarse_positions)[3],
                                                  const blender::int2 *coarse_edges,
                                                  const MeshElemMap *vert_to_edge_map,
                                                  int coarse_edge_index,
                                                  bool is_simple,
                                                  float u,
                                                  float pos_r[3]);
