/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup obj
 */

#include "BKE_mesh.hh"
#include "BKE_object.h"

#include "BLI_delaunay_2d.h"
#include "BLI_math_geom.h"
#include "BLI_math_matrix.h"
#include "BLI_math_rotation.h"
#include "BLI_math_vector.h"
#include "BLI_set.hh"

#include "DNA_object_types.h"

#include "IO_wavefront_obj.hh"

#include "importer_mesh_utils.hh"

namespace blender::io::obj {

Vector<Vector<int>> fixup_invalid_polygon(Span<float3> vertex_coords,
                                          Span<int> face_vertex_indices)
{
  using namespace blender::meshintersect;
  if (face_vertex_indices.size() < 3) {
    return {};
  }

  /* Calculate face normal, to project verts to 2D. */
  float normal[3] = {0, 0, 0};
  float3 co_prev = vertex_coords[face_vertex_indices.last()];
  for (int idx : face_vertex_indices) {
    BLI_assert(idx >= 0 && idx < vertex_coords.size());
    float3 co_curr = vertex_coords[idx];
    add_newell_cross_v3_v3v3(normal, co_prev, co_curr);
    co_prev = co_curr;
  }
  if (UNLIKELY(normalize_v3(normal) == 0.0f)) {
    normal[2] = 1.0f;
  }
  float axis_mat[3][3];
  axis_dominant_v3_to_m3(axis_mat, normal);

  /* Prepare data for CDT. */
  CDT_input<double> input;
  input.vert.reinitialize(face_vertex_indices.size());
  input.face.reinitialize(1);
  input.face[0].resize(face_vertex_indices.size());
  for (int64_t i = 0; i < face_vertex_indices.size(); ++i) {
    input.face[0][i] = i;
  }
  input.epsilon = 1.0e-6f;
  input.need_ids = true;
  /* Project vertices to 2D. */
  for (size_t i = 0; i < face_vertex_indices.size(); ++i) {
    int idx = face_vertex_indices[i];
    BLI_assert(idx >= 0 && idx < vertex_coords.size());
    float3 coord = vertex_coords[idx];
    float2 coord2d;
    mul_v2_m3v3(coord2d, axis_mat, coord);
    input.vert[i] = double2(coord2d.x, coord2d.y);
  }

  CDT_result<double> res = delaunay_2d_calc(input, CDT_CONSTRAINTS_VALID_BMESH_WITH_HOLES);

  /* Emit new face information from CDT result. */
  Vector<Vector<int>> faces;
  faces.reserve(res.face.size());
  for (const auto &f : res.face) {
    Vector<int> face_verts;
    face_verts.reserve(f.size());
    for (int64_t i = 0; i < f.size(); ++i) {
      int idx = f[i];
      BLI_assert(idx >= 0 && idx < res.vert_orig.size());
      if (res.vert_orig[idx].is_empty()) {
        /* If we have a whole new vertex in the tessellated result,
         * we won't quite know what to do with it (how to create normal/UV
         * for it, for example). Such vertices are often due to
         * self-intersecting polygons. Just skip them from the output
         * face. */
      }
      else {
        /* Vertex corresponds to one or more of the input vertices, use it. */
        idx = res.vert_orig[idx][0];
        BLI_assert(idx >= 0 && idx < face_vertex_indices.size());
        face_verts.append(idx);
      }
    }
    faces.append(face_verts);
  }
  return faces;
}

void transform_object(Object *object, const OBJImportParams &import_params)
{
  float axes_transform[3][3];
  unit_m3(axes_transform);
  float obmat[4][4];
  unit_m4(obmat);
  /* +Y-forward and +Z-up are the default Blender axis settings. */
  mat3_from_axis_conversion(
      IO_AXIS_Y, IO_AXIS_Z, import_params.forward_axis, import_params.up_axis, axes_transform);
  copy_m4_m3(obmat, axes_transform);

  float scale_vec[3] = {
      import_params.global_scale, import_params.global_scale, import_params.global_scale};
  rescale_m4(obmat, scale_vec);
  BKE_object_apply_mat4(object, obmat, true, false);

  if (import_params.clamp_size != 0.0f) {
    float3 max_coord(-INT_MAX);
    float3 min_coord(INT_MAX);
    BoundBox *bb = BKE_mesh_boundbox_get(object);
    for (const float(&vertex)[3] : bb->vec) {
      for (int axis = 0; axis < 3; axis++) {
        max_coord[axis] = max_ff(max_coord[axis], vertex[axis]);
        min_coord[axis] = min_ff(min_coord[axis], vertex[axis]);
      }
    }
    const float max_diff = max_fff(
        max_coord[0] - min_coord[0], max_coord[1] - min_coord[1], max_coord[2] - min_coord[2]);
    float scale = 1.0f;
    while (import_params.clamp_size < max_diff * scale) {
      scale = scale / 10;
    }
    copy_v3_fl(object->scale, scale);
  }
}

}  // namespace blender::io::obj
