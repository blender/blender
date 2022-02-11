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

#ifdef __cplusplus
}
#endif
