/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bmesh
 *
 * Flattens vertices on a best-fitting plane.
 */

#include "BLI_array.hh"
#include "BLI_math_vector.hh"
#include "BLI_math_vector_c.hh"
#include "BLI_vector.hh"
#include "BLI_vector_set.hh"

#include "bmesh.hh"
#include "intern/bmesh_operators_private.hh" /* own include */

namespace blender {

static float3 compute_centroid(Span<BMVert *> verts)
{
  float3 center(0.0f);
  for (BMVert *v : verts) {
    center += float3(v->co);
  }
  center /= float(verts.size());
  return center;
}

static float3 compute_average_face_normal(Span<BMFace *> faces)
{
  float3 normal(0.0f);
  for (BMFace *f : faces) {
    normal += float3(f->no) * BM_face_calc_area(f);
  }
  return (normalize_v3(normal) != 0.0f) ? normal : float3(0.0f, 0.0f, 1.0f);
}

static Vector<BMVert *> collect_verts_from_faces(Span<BMFace *> faces)
{
  VectorSet<BMVert *> verts;
  for (BMFace *f : faces) {
    BMIter viter;
    BMVert *v;
    BM_ITER_ELEM (v, &viter, f, BM_VERTS_OF_FACE) {
      verts.add(v);
    }
  }
  return verts.extract_vector();
}

void bmo_flatten_exec(BMesh *bm, BMOperator *op)
{
  const float factor = BMO_slot_float_get(op->slots_in, "factor");
  const FlattenMethod method = static_cast<FlattenMethod>(
      BMO_slot_int_get(op->slots_in, "method"));
  const bool lock_x = BMO_slot_bool_get(op->slots_in, "lock_x");
  const bool lock_y = BMO_slot_bool_get(op->slots_in, "lock_y");
  const bool lock_z = BMO_slot_bool_get(op->slots_in, "lock_z");

  float3 view_direction(0.0f, 0.0f, 1.0f);
  if (method == FLATTEN_VIEW) {
    BMO_slot_vec_get(op->slots_in, "view_normal", view_direction);
  }

  auto flatten_verts_fn = [&](MutableSpan<BMVert *> verts, Span<BMFace *> faces) {
    float3 center;
    float3 normal(0.0f);

    FlattenMethod actual_method = method;
    /* The normal method is computed using the average face normal so for vertices
     * not attached to any faces, the best fit normal is used in place. */
    if (actual_method == FLATTEN_NORMAL && faces.is_empty()) {
      actual_method = FLATTEN_BEST_FIT;
    }

    switch (actual_method) {
      case FLATTEN_BEST_FIT:
        BM_verts_calc_normal_from_cloud_ex(
            verts.data(), int(verts.size()), normal, center, nullptr);
        break;
      case FLATTEN_NORMAL:
        normal = compute_average_face_normal(faces);
        center = compute_centroid(verts);
        break;
      case FLATTEN_VIEW:
        normal = view_direction;
        center = compute_centroid(verts);
        break;
    }
    BLI_assert(!math::is_zero(normal));
    for (BMVert *v : verts) {
      float3 co(v->co);
      float3 projected = co - math::dot(co - center, normal) * normal;

      if (lock_x) {
        projected.x = co.x;
      }
      if (lock_y) {
        projected.y = co.y;
      }
      if (lock_z) {
        projected.z = co.z;
      }

      float3 co_final = math::interpolate(co, projected, factor);
      copy_v3_v3(v->co, co_final);
    }
  };

  BM_mesh_elem_hflag_disable_all(bm, BM_FACE | BM_EDGE, BM_ELEM_TAG, false);
  BMO_slot_buffer_hflag_enable(bm, op->slots_in, "geom", BM_FACE | BM_EDGE, BM_ELEM_TAG, false);

  Array<int> face_groups_array(bm->totface);
  int (*face_group_index)[2];
  const int face_group_num = BM_mesh_calc_face_groups(bm,
                                                      face_groups_array.data(),
                                                      &face_group_index,
                                                      nullptr,
                                                      nullptr,
                                                      nullptr,
                                                      BM_ELEM_TAG,
                                                      BM_EDGE);

  BM_mesh_elem_table_ensure(bm, BM_FACE);

  for (const int g : IndexRange(face_group_num)) {
    const int start = face_group_index[g][0];
    const int length = face_group_index[g][1];

    Array<BMFace *> faces(length);
    for (const int i : IndexRange(start, length)) {
      BMFace *f = BM_face_at_index(bm, face_groups_array[i]);
      faces[i - start] = f;
      /* For every edge in the face, the BM_ELEM_TAG is disabled so the only edges that will still
       * have them enabled are those without faces which would be detected in the second pass.  */
      BMIter eiter;
      BMEdge *e;
      BM_ITER_ELEM (e, &eiter, f, BM_EDGES_OF_FACE) {
        BM_elem_flag_disable(e, BM_ELEM_TAG);
      }
    }

    Vector<BMVert *> verts = collect_verts_from_faces(faces);
    flatten_verts_fn(verts, faces);
  }
  MEM_delete(face_group_index);

  Array<int> edge_groups_array(bm->totedge);
  int (*edge_group_index)[2];
  const int edge_group_num = BM_mesh_calc_edge_groups(
      bm, edge_groups_array.data(), &edge_group_index, nullptr, nullptr, BM_ELEM_TAG);

  BM_mesh_elem_table_ensure(bm, BM_EDGE);

  for (const int g : IndexRange(edge_group_num)) {
    const int start = edge_group_index[g][0];
    const int length = edge_group_index[g][1];

    VectorSet<BMVert *> verts_set;
    for (const int i : IndexRange(start, length)) {
      BMEdge *e = BM_edge_at_index(bm, edge_groups_array[i]);
      verts_set.add(e->v1);
      verts_set.add(e->v2);
    }
    Vector<BMVert *> verts = verts_set.extract_vector();
    flatten_verts_fn(verts, {});
  }
  MEM_delete(edge_group_index);
}

}  // namespace blender
