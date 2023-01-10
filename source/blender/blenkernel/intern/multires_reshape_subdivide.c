/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2020 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup bke
 */

#include "MEM_guardedalloc.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_scene_types.h"

#include "BKE_customdata.h"
#include "BKE_lib_id.h"
#include "BKE_mesh.h"
#include "BKE_mesh_runtime.h"
#include "BKE_modifier.h"
#include "BKE_multires.h"
#include "BKE_subdiv.h"
#include "BKE_subsurf.h"
#include "BLI_math_vector.h"

#include "DEG_depsgraph_query.h"

#include "multires_reshape.h"

static void multires_subdivide_create_object_space_linear_grids(Mesh *mesh)
{
  const float(*positions)[3] = BKE_mesh_vert_positions(mesh);
  const MPoly *polys = BKE_mesh_polys(mesh);
  const MLoop *loops = BKE_mesh_loops(mesh);

  MDisps *mdisps = CustomData_get_layer(&mesh->ldata, CD_MDISPS);
  const int totpoly = mesh->totpoly;
  for (int p = 0; p < totpoly; p++) {
    const MPoly *poly = &polys[p];
    float poly_center[3];
    BKE_mesh_calc_poly_center(poly, &loops[poly->loopstart], positions, poly_center);
    for (int l = 0; l < poly->totloop; l++) {
      const int loop_index = poly->loopstart + l;

      float(*disps)[3] = mdisps[loop_index].disps;
      mdisps[loop_index].totdisp = 4;
      mdisps[loop_index].level = 1;

      int prev_loop_index = l - 1 >= 0 ? loop_index - 1 : loop_index + poly->totloop - 1;
      int next_loop_index = l + 1 < poly->totloop ? loop_index + 1 : poly->loopstart;

      const MLoop *loop = &loops[loop_index];
      const MLoop *loop_next = &loops[next_loop_index];
      const MLoop *loop_prev = &loops[prev_loop_index];

      copy_v3_v3(disps[0], poly_center);
      mid_v3_v3v3(disps[1], positions[loop->v], positions[loop_next->v]);
      mid_v3_v3v3(disps[2], positions[loop->v], positions[loop_prev->v]);
      copy_v3_v3(disps[3], positions[loop->v]);
    }
  }
}

void multires_subdivide_create_tangent_displacement_linear_grids(Object *object,
                                                                 MultiresModifierData *mmd)
{
  Mesh *coarse_mesh = object->data;
  multires_force_sculpt_rebuild(object);

  MultiresReshapeContext reshape_context;

  const int new_top_level = mmd->totlvl + 1;

  const bool has_mdisps = CustomData_has_layer(&coarse_mesh->ldata, CD_MDISPS);
  if (!has_mdisps) {
    CustomData_add_layer(
        &coarse_mesh->ldata, CD_MDISPS, CD_SET_DEFAULT, NULL, coarse_mesh->totloop);
  }

  if (new_top_level == 1) {
    /* No MDISPS. Create new grids for level 1 using the edges mid point and poly centers. */
    multires_reshape_ensure_grids(coarse_mesh, 1);
    multires_subdivide_create_object_space_linear_grids(coarse_mesh);
  }

  /* Convert the new grids to tangent displacement. */
  multires_set_tot_level(object, mmd, new_top_level);

  if (!multires_reshape_context_create_from_modifier(
          &reshape_context, object, mmd, new_top_level)) {
    return;
  }

  multires_reshape_object_grids_to_tangent_displacement(&reshape_context);
  multires_reshape_context_free(&reshape_context);
}
