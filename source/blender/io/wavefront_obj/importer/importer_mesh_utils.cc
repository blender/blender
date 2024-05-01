/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup obj
 */

#include "BKE_mesh.hh"
#include "BKE_object.hh"

#include "BLI_delaunay_2d.hh"
#include "BLI_math_geom.h"
#include "BLI_math_matrix.h"
#include "BLI_math_rotation.h"
#include "BLI_math_vector.h"

#include "DNA_object_types.h"

#include "IO_wavefront_obj.hh"

#include "importer_mesh_utils.hh"

#include <numeric>

namespace blender::io::obj {

Vector<Vector<int>> fixup_invalid_face(Span<float3> vert_positions, Span<int> face_verts)
{
  using namespace blender::meshintersect;
  if (face_verts.size() < 3) {
    return {};
  }

  const float3 normal = bke::mesh::face_normal_calc(vert_positions, face_verts);
  float axis_mat[3][3];
  axis_dominant_v3_to_m3(axis_mat, normal);

  /* Project vertices to 2D. */
  Array<double2> input_verts(face_verts.size());
  for (const int i : face_verts.index_range()) {
    int idx = face_verts[i];
    BLI_assert(idx >= 0 && idx < vert_positions.size());
    float2 coord2d;
    mul_v2_m3v3(coord2d, axis_mat, vert_positions[idx]);
    input_verts[i] = double2(coord2d.x, coord2d.y);
  }

  Array<Vector<int>> input_faces(1);
  input_faces.first().resize(input_verts.size());

  std::iota(input_faces.first().begin(), input_faces.first().end(), 0);

  /* Prepare data for CDT. */
  CDT_input<double> input;
  input.vert = std::move(input_verts);
  input.face = std::move(input_faces);
  input.epsilon = 1.0e-6f;
  input.need_ids = true;
  CDT_result<double> res = delaunay_2d_calc(input, CDT_CONSTRAINTS_VALID_BMESH_WITH_HOLES);

  /* Emit new face information from CDT result. */
  Vector<Vector<int>> faces;
  faces.reserve(res.face.size());
  for (const auto &res_face : res.face) {
    Vector<int> res_face_verts;
    res_face_verts.reserve(res_face.size());
    for (int64_t i = 0; i < res_face.size(); ++i) {
      int idx = res_face[i];
      BLI_assert(idx >= 0 && idx < res.vert_orig.size());
      if (res.vert_orig[idx].is_empty()) {
        /* If we have a whole new vertex in the tessellated result, we won't quite know what to do
         * with it (how to create normal/UV for it, for example). Such vertices are often due to
         * self-intersecting faces. Just skip them from the output face. */
      }
      else {
        /* Vertex corresponds to one or more of the input vertices, use it. */
        idx = res.vert_orig[idx][0];
        BLI_assert(idx >= 0 && idx < face_verts.size());
        res_face_verts.append(idx);
      }
    }
    faces.append(res_face_verts);
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
    BLI_assert(object->type == OB_MESH);
    const Mesh *mesh = static_cast<const Mesh *>(object->data);
    const Bounds<float3> bounds = *mesh->bounds_min_max();
    const float max_diff = math::reduce_max(bounds.max - bounds.min);

    float scale = 1.0f;
    while (import_params.clamp_size < max_diff * scale) {
      scale = scale / 10;
    }
    copy_v3_fl(object->scale, scale);
  }
}

std::string get_geometry_name(const std::string &full_name, char separator)
{
  if (separator == 0) {
    return full_name;
  }
  size_t pos = full_name.find_last_of(separator);
  if (pos == std::string::npos) {
    return full_name;
  }
  return full_name.substr(pos + 1);
}

}  // namespace blender::io::obj
