/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2020 Blender Foundation */

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
#include "BKE_mesh.hh"
#include "BKE_mesh_runtime.h"
#include "BKE_modifier.h"
#include "BKE_multires.h"
#include "BKE_subdiv.h"
#include "BKE_subsurf.h"
#include "BLI_math_vector.h"

#include "DEG_depsgraph_query.h"

#include "multires_reshape.hh"

static void multires_subdivide_create_object_space_linear_grids(Mesh *mesh)
{
  using namespace blender;
  using namespace blender::bke;
  const Span<float3> positions = mesh->vert_positions();
  const blender::OffsetIndices polys = mesh->polys();
  const blender::Span<int> corner_verts = mesh->corner_verts();

  MDisps *mdisps = static_cast<MDisps *>(
      CustomData_get_layer_for_write(&mesh->ldata, CD_MDISPS, mesh->totloop));
  for (const int p : polys.index_range()) {
    const blender::IndexRange poly = polys[p];
    const float3 poly_center = mesh::poly_center_calc(positions, corner_verts.slice(poly));
    for (int l = 0; l < poly.size(); l++) {
      const int loop_index = poly[l];

      float(*disps)[3] = mdisps[loop_index].disps;
      mdisps[loop_index].totdisp = 4;
      mdisps[loop_index].level = 1;

      int prev_loop_index = l - 1 >= 0 ? loop_index - 1 : loop_index + poly.size() - 1;
      int next_loop_index = l + 1 < poly.size() ? loop_index + 1 : poly.start();

      const int vert = corner_verts[loop_index];
      const int vert_next = corner_verts[next_loop_index];
      const int vert_prev = corner_verts[prev_loop_index];

      copy_v3_v3(disps[0], poly_center);
      mid_v3_v3v3(disps[1], positions[vert], positions[vert_next]);
      mid_v3_v3v3(disps[2], positions[vert], positions[vert_prev]);
      copy_v3_v3(disps[3], positions[vert]);
    }
  }
}

void multires_subdivide_create_tangent_displacement_linear_grids(Object *object,
                                                                 MultiresModifierData *mmd)
{
  Mesh *coarse_mesh = static_cast<Mesh *>(object->data);
  multires_force_sculpt_rebuild(object);

  MultiresReshapeContext reshape_context;

  const int new_top_level = mmd->totlvl + 1;

  const bool has_mdisps = CustomData_has_layer(&coarse_mesh->ldata, CD_MDISPS);
  if (!has_mdisps) {
    CustomData_add_layer(&coarse_mesh->ldata, CD_MDISPS, CD_SET_DEFAULT, coarse_mesh->totloop);
  }

  if (new_top_level == 1) {
    /* No MDISPS. Create new grids for level 1 using the edges mid point and poly centers. */
    multires_reshape_ensure_grids(coarse_mesh, 1);
    multires_subdivide_create_object_space_linear_grids(coarse_mesh);
  }

  /* Convert the new grids to tangent displacement. */
  multires_set_tot_level(object, mmd, new_top_level);

  if (!multires_reshape_context_create_from_modifier(&reshape_context, object, mmd, new_top_level))
  {
    return;
  }

  multires_reshape_object_grids_to_tangent_displacement(&reshape_context);
  multires_reshape_context_free(&reshape_context);
}
