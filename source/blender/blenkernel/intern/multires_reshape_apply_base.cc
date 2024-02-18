/* SPDX-FileCopyrightText: 2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "multires_reshape.hh"

#include "MEM_guardedalloc.h"

#include "DNA_mesh_types.h"

#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"

#include "BKE_mesh.hh"
#include "BKE_multires.hh"
#include "BKE_subdiv_eval.hh"

void multires_reshape_apply_base_update_mesh_coords(MultiresReshapeContext *reshape_context)
{
  Mesh *base_mesh = reshape_context->base_mesh;
  blender::MutableSpan<blender::float3> base_positions = base_mesh->vert_positions_for_write();
  /* Update the context in case the vertices were duplicated. */
  reshape_context->base_positions = base_positions;

  const blender::Span<int> corner_verts = reshape_context->base_corner_verts;
  for (const int loop_index : corner_verts.index_range()) {

    GridCoord grid_coord;
    grid_coord.grid_index = loop_index;
    grid_coord.u = 1.0f;
    grid_coord.v = 1.0f;

    float P[3];
    float tangent_matrix[3][3];
    multires_reshape_evaluate_limit_at_grid(reshape_context, &grid_coord, P, tangent_matrix);

    ReshapeConstGridElement grid_element = multires_reshape_orig_grid_element_for_grid_coord(
        reshape_context, &grid_coord);
    float D[3];
    mul_v3_m3v3(D, tangent_matrix, grid_element.displacement);

    add_v3_v3v3(base_positions[corner_verts[loop_index]], P, D);
  }
}

/* Assumes no is normalized; return value's sign is negative if v is on the other side of the
 * plane. */
static float v3_dist_from_plane(const float v[3], const float center[3], const float no[3])
{
  float s[3];
  sub_v3_v3v3(s, v, center);
  return dot_v3v3(s, no);
}

void multires_reshape_apply_base_refit_base_mesh(MultiresReshapeContext *reshape_context)
{
  Mesh *base_mesh = reshape_context->base_mesh;
  blender::MutableSpan<blender::float3> base_positions = base_mesh->vert_positions_for_write();
  /* Update the context in case the vertices were duplicated. */
  reshape_context->base_positions = base_positions;
  const blender::GroupedSpan<int> vert_to_face_map = base_mesh->vert_to_face_map();

  float(*origco)[3] = static_cast<float(*)[3]>(
      MEM_calloc_arrayN(base_mesh->verts_num, sizeof(float[3]), __func__));
  for (int i = 0; i < base_mesh->verts_num; i++) {
    copy_v3_v3(origco[i], base_positions[i]);
  }

  for (int i = 0; i < base_mesh->verts_num; i++) {
    float avg_no[3] = {0, 0, 0}, center[3] = {0, 0, 0}, push[3];

    /* Don't adjust vertices not used by at least one face. */
    if (!vert_to_face_map[i].size()) {
      continue;
    }

    /* Find center. */
    int tot = 0;
    for (const int face : vert_to_face_map[i]) {
      /* This double counts, not sure if that's bad or good. */
      for (const int corner : reshape_context->base_faces[face]) {
        const int vndx = reshape_context->base_corner_verts[corner];
        if (vndx != i) {
          add_v3_v3(center, origco[vndx]);
          tot++;
        }
      }
    }
    mul_v3_fl(center, 1.0f / tot);

    /* Find normal. */
    for (int j = 0; j < vert_to_face_map[i].size(); j++) {
      const blender::IndexRange face = reshape_context->base_faces[vert_to_face_map[i][j]];

      /* Set up face, loops, and coords in order to call #bke::mesh::face_normal_calc(). */
      blender::Array<int> face_verts(face.size());
      blender::Array<blender::float3> fake_co(face.size());

      for (int k = 0; k < face.size(); k++) {
        const int vndx = reshape_context->base_corner_verts[face[k]];

        face_verts[k] = k;

        if (vndx == i) {
          copy_v3_v3(fake_co[k], center);
        }
        else {
          copy_v3_v3(fake_co[k], origco[vndx]);
        }
      }

      const blender::float3 no = blender::bke::mesh::face_normal_calc(fake_co, face_verts);
      add_v3_v3(avg_no, no);
    }
    normalize_v3(avg_no);

    /* Push vertex away from the plane. */
    const float dist = v3_dist_from_plane(base_positions[i], center, avg_no);
    copy_v3_v3(push, avg_no);
    mul_v3_fl(push, dist);
    add_v3_v3(base_positions[i], push);
  }

  MEM_freeN(origco);

  /* Vertices were moved around, need to update normals after all the vertices are updated
   * Probably this is possible to do in the loop above, but this is rather tricky because
   * we don't know all needed vertices' coordinates there yet. */
  base_mesh->tag_positions_changed();
}

void multires_reshape_apply_base_refine_from_base(MultiresReshapeContext *reshape_context)
{
  BKE_subdiv_eval_refine_from_mesh(reshape_context->subdiv, reshape_context->base_mesh, nullptr);
}

void multires_reshape_apply_base_refine_from_deform(MultiresReshapeContext *reshape_context)
{
  Depsgraph *depsgraph = reshape_context->depsgraph;
  Object *object = reshape_context->object;
  MultiresModifierData *mmd = reshape_context->mmd;
  BLI_assert(depsgraph != nullptr);
  BLI_assert(object != nullptr);
  BLI_assert(mmd != nullptr);

  blender::Array<blender::float3> deformed_verts =
      BKE_multires_create_deformed_base_mesh_vert_coords(depsgraph, object, mmd);

  BKE_subdiv_eval_refine_from_mesh(reshape_context->subdiv,
                                   reshape_context->base_mesh,
                                   reinterpret_cast<float(*)[3]>(deformed_verts.data()));
}
