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
 * The Original Code is Copyright (C) 2020 Blender Foundation.
 * All rights reserved.
 */

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
  MDisps *mdisps = CustomData_get_layer(&mesh->ldata, CD_MDISPS);
  const int totpoly = mesh->totpoly;
  for (int p = 0; p < totpoly; p++) {
    MPoly *poly = &mesh->mpoly[p];
    float poly_center[3];
    BKE_mesh_calc_poly_center(poly, &mesh->mloop[poly->loopstart], mesh->mvert, poly_center);
    for (int l = 0; l < poly->totloop; l++) {
      const int loop_index = poly->loopstart + l;

      float(*disps)[3] = mdisps[loop_index].disps;
      mdisps[loop_index].totdisp = 4;
      mdisps[loop_index].level = 1;

      int prev_loop_index = l - 1 >= 0 ? loop_index - 1 : loop_index + poly->totloop - 1;
      int next_loop_index = l + 1 < poly->totloop ? loop_index + 1 : poly->loopstart;

      MLoop *loop = &mesh->mloop[loop_index];
      MLoop *loop_next = &mesh->mloop[next_loop_index];
      MLoop *loop_prev = &mesh->mloop[prev_loop_index];

      copy_v3_v3(disps[0], poly_center);
      mid_v3_v3v3(disps[1], mesh->mvert[loop->v].co, mesh->mvert[loop_next->v].co);
      mid_v3_v3v3(disps[2], mesh->mvert[loop->v].co, mesh->mvert[loop_prev->v].co);
      copy_v3_v3(disps[3], mesh->mvert[loop->v].co);
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
    CustomData_add_layer(&coarse_mesh->ldata, CD_MDISPS, CD_CALLOC, NULL, coarse_mesh->totloop);
  }

  if (new_top_level == 1) {
    /* No MDISPS. Create new grids for level 1 using the edges mid point and poly centers. */
    multires_reshape_ensure_grids(coarse_mesh, 1);
    multires_subdivide_create_object_space_linear_grids(coarse_mesh);
  }

  /* Convert the new grids to tangent displacement. */
  multires_set_tot_level(object, mmd, new_top_level);

  if (!multires_reshape_context_create_from_subdivide(
          &reshape_context, object, mmd, new_top_level)) {
    return;
  }

  multires_reshape_object_grids_to_tangent_displacement(&reshape_context);
  multires_reshape_context_free(&reshape_context);
}
