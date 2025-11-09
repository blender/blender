/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edmesh
 */

#include "BLI_atomic_disjoint_set.hh"
#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"
#include "BLI_math_vector.hh"
#include "BLI_task.hh"
#include "BLI_vector_set.hh"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"

#include "BKE_attribute.hh"
#include "BKE_context.hh"
#include "BKE_customdata.hh"
#include "BKE_mesh.hh"
#include "BKE_mesh_mapping.hh"
#include "BKE_object.hh"

#include "ED_mesh.hh"
#include "ED_screen.hh"
#include "ED_select_utils.hh"
#include "ED_view3d.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_query.hh"

/* own include */

void paintface_flush_flags(bContext *C,
                           Object *ob,
                           const bool flush_selection,
                           const bool flush_hidden)
{
  using namespace blender;
  Mesh *mesh = BKE_mesh_from_object(ob);
  const int *index_array = nullptr;

  BLI_assert(flush_selection || flush_hidden);

  if (mesh == nullptr) {
    return;
  }

  /* NOTE: call #BKE_mesh_flush_hidden_from_verts_ex first when changing hidden flags. */

  /* we could call this directly in all areas that change selection,
   * since this could become slow for realtime updates (circle-select for eg) */
  if (flush_selection) {
    bke::mesh_select_face_flush(*mesh);
  }

  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Object *ob_eval = DEG_get_evaluated(depsgraph, ob);

  if (ob_eval == nullptr) {
    return;
  }

  bke::AttributeAccessor attributes_me = mesh->attributes();
  Mesh *me_orig = (Mesh *)ob_eval->runtime->data_orig;
  bke::MutableAttributeAccessor attributes_orig = me_orig->attributes_for_write();
  Mesh *mesh_eval = (Mesh *)ob_eval->runtime->data_eval;
  bke::MutableAttributeAccessor attributes_eval = mesh_eval->attributes_for_write();
  bool updated = false;

  if (me_orig != nullptr && mesh_eval != nullptr && me_orig->faces_num == mesh->faces_num) {
    /* Update the evaluated copy of the mesh. */
    if (flush_hidden) {
      const VArray<bool> hide_poly_me = *attributes_me.lookup_or_default<bool>(
          ".hide_poly", bke::AttrDomain::Face, false);
      bke::SpanAttributeWriter<bool> hide_poly_orig =
          attributes_orig.lookup_or_add_for_write_only_span<bool>(".hide_poly",
                                                                  bke::AttrDomain::Face);
      hide_poly_me.materialize(hide_poly_orig.span);
      hide_poly_orig.finish();
    }
    if (flush_selection) {
      const VArray<bool> select_poly_me = *attributes_me.lookup_or_default<bool>(
          ".select_poly", bke::AttrDomain::Face, false);
      bke::SpanAttributeWriter<bool> select_poly_orig =
          attributes_orig.lookup_or_add_for_write_only_span<bool>(".select_poly",
                                                                  bke::AttrDomain::Face);
      select_poly_me.materialize(select_poly_orig.span);
      select_poly_orig.finish();
    }

    /* Mesh faces => Final derived faces */
    if ((index_array = (const int *)CustomData_get_layer(&mesh_eval->face_data, CD_ORIGINDEX))) {
      if (flush_hidden) {
        const VArray<bool> hide_poly_orig = *attributes_orig.lookup_or_default<bool>(
            ".hide_poly", bke::AttrDomain::Face, false);
        bke::SpanAttributeWriter<bool> hide_poly_eval =
            attributes_eval.lookup_or_add_for_write_only_span<bool>(".hide_poly",
                                                                    bke::AttrDomain::Face);
        for (const int i : IndexRange(mesh_eval->faces_num)) {
          const int orig_face_index = index_array[i];
          if (orig_face_index != ORIGINDEX_NONE) {
            hide_poly_eval.span[i] = hide_poly_orig[orig_face_index];
          }
        }
        hide_poly_eval.finish();
      }
      if (flush_selection) {
        const VArray<bool> select_poly_orig = *attributes_orig.lookup_or_default<bool>(
            ".select_poly", bke::AttrDomain::Face, false);
        bke::SpanAttributeWriter<bool> select_poly_eval =
            attributes_eval.lookup_or_add_for_write_only_span<bool>(".select_poly",
                                                                    bke::AttrDomain::Face);
        for (const int i : IndexRange(mesh_eval->faces_num)) {
          const int orig_face_index = index_array[i];
          if (orig_face_index != ORIGINDEX_NONE) {
            select_poly_eval.span[i] = select_poly_orig[orig_face_index];
          }
        }
        select_poly_eval.finish();
      }

      updated = true;
    }
  }

  if (updated) {
    if (flush_hidden) {
      BKE_mesh_batch_cache_dirty_tag(mesh_eval, BKE_MESH_BATCH_DIRTY_ALL);
    }
    else {
      BKE_mesh_batch_cache_dirty_tag(mesh_eval, BKE_MESH_BATCH_DIRTY_SELECT_PAINT);
    }

    DEG_id_tag_update(static_cast<ID *>(ob->data), ID_RECALC_SELECT);
  }
  else {
    DEG_id_tag_update(static_cast<ID *>(ob->data), ID_RECALC_SYNC_TO_EVAL | ID_RECALC_SELECT);
  }

  WM_event_add_notifier(C, NC_GEOM | ND_SELECT, ob->data);
}

void paintface_hide(bContext *C, Object *ob, const bool unselected)
{
  using namespace blender;
  Mesh *mesh = BKE_mesh_from_object(ob);
  if (mesh == nullptr || mesh->faces_num == 0) {
    return;
  }

  bke::MutableAttributeAccessor attributes = mesh->attributes_for_write();
  bke::SpanAttributeWriter<bool> hide_poly = attributes.lookup_or_add_for_write_span<bool>(
      ".hide_poly", bke::AttrDomain::Face);
  bke::SpanAttributeWriter<bool> select_poly = attributes.lookup_or_add_for_write_span<bool>(
      ".select_poly", bke::AttrDomain::Face);

  for (int i = 0; i < mesh->faces_num; i++) {
    if (!hide_poly.span[i]) {
      if (!select_poly.span[i] == unselected) {
        hide_poly.span[i] = true;
      }
    }

    if (hide_poly.span[i]) {
      select_poly.span[i] = false;
    }
  }

  hide_poly.finish();
  select_poly.finish();

  bke::mesh_hide_face_flush(*mesh);

  paintface_flush_flags(C, ob, true, true);
}

void paintface_reveal(bContext *C, Object *ob, const bool select)
{
  using namespace blender;
  Mesh *mesh = BKE_mesh_from_object(ob);
  if (mesh == nullptr || mesh->faces_num == 0) {
    return;
  }

  bke::MutableAttributeAccessor attributes = mesh->attributes_for_write();

  if (select) {
    const VArray<bool> hide_poly = *attributes.lookup_or_default<bool>(
        ".hide_poly", bke::AttrDomain::Face, false);
    bke::SpanAttributeWriter<bool> select_poly = attributes.lookup_or_add_for_write_span<bool>(
        ".select_poly", bke::AttrDomain::Face);
    for (const int i : hide_poly.index_range()) {
      if (hide_poly[i]) {
        select_poly.span[i] = true;
      }
    }
    select_poly.finish();
  }

  attributes.remove(".hide_poly");

  bke::mesh_hide_face_flush(*mesh);

  paintface_flush_flags(C, ob, true, true);
}

/**
 * Join all edges of each face in the #AtomicDisjointSet. This can be used to find out which faces
 * are connected to each other.
 * \param islands: Is expected to be of length `mesh->edges_num`.
 * \param skip_seams: Faces separated by a seam will be treated as not connected.
 */
static void build_poly_connections(blender::AtomicDisjointSet &islands,
                                   Mesh &mesh,
                                   const bool skip_seams = true)
{
  using namespace blender;
  const OffsetIndices faces = mesh.faces();
  const Span<int> corner_edges = mesh.corner_edges();

  const bke::AttributeAccessor attributes = mesh.attributes();
  const VArray<bool> uv_seams = *attributes.lookup_or_default<bool>(
      "uv_seam", bke::AttrDomain::Edge, false);
  const VArray<bool> hide_poly = *attributes.lookup_or_default<bool>(
      ".hide_poly", bke::AttrDomain::Face, false);

  /* Faces are connected if they share edges. By connecting all edges of a loop (as long as they
   * are not a seam) we can find connected faces. */
  threading::parallel_for(faces.index_range(), 1024, [&](const IndexRange range) {
    for (const int face_index : range) {
      if (hide_poly[face_index]) {
        continue;
      }
      const Span<int> face_edges = corner_edges.slice(faces[face_index]);

      for (const int poly_loop_index : face_edges.index_range()) {
        const int outer_edge = face_edges[poly_loop_index];
        if (skip_seams && uv_seams[outer_edge]) {
          continue;
        }

        for (const int inner_edge :
             face_edges.slice(poly_loop_index, face_edges.size() - poly_loop_index))
        {
          if (outer_edge == inner_edge) {
            continue;
          }
          if (skip_seams && uv_seams[inner_edge]) {
            continue;
          }
          islands.join(inner_edge, outer_edge);
        }
      }
    }
  });
}

/* Select faces connected to the given face_indices. Seams are treated as separation. */
static void paintface_select_linked_faces(Mesh &mesh,
                                          const blender::Span<int> face_indices,
                                          const bool select)
{
  using namespace blender;

  AtomicDisjointSet islands(mesh.edges_num);
  build_poly_connections(islands, mesh);

  const OffsetIndices faces = mesh.faces();
  const Span<int> corner_edges = mesh.corner_edges();

  bke::MutableAttributeAccessor attributes = mesh.attributes_for_write();
  const VArray<bool> uv_seams = *attributes.lookup_or_default<bool>(
      "uv_seam", bke::AttrDomain::Edge, false);
  bke::SpanAttributeWriter<bool> select_poly = attributes.lookup_or_add_for_write_span<bool>(
      ".select_poly", bke::AttrDomain::Face);

  Set<int> selected_roots;
  for (const int i : face_indices) {
    for (const int edge : corner_edges.slice(faces[i])) {
      if (uv_seams[edge]) {
        continue;
      }
      const int root = islands.find_root(edge);
      selected_roots.add(root);
    }
  }

  threading::parallel_for(select_poly.span.index_range(), 1024, [&](const IndexRange range) {
    for (const int face_index : range) {
      for (const int edge : corner_edges.slice(faces[face_index])) {
        const int root = islands.find_root(edge);
        if (selected_roots.contains(root)) {
          select_poly.span[face_index] = select;
          break;
        }
      }
    }
  });

  select_poly.finish();
}

void paintface_select_linked(bContext *C, Object *ob, const int mval[2], const bool select)
{
  using namespace blender;
  Mesh *mesh = BKE_mesh_from_object(ob);
  if (mesh == nullptr || mesh->faces_num == 0) {
    return;
  }

  bke::MutableAttributeAccessor attributes = mesh->attributes_for_write();
  bke::SpanAttributeWriter<bool> select_poly = attributes.lookup_or_add_for_write_span<bool>(
      ".select_poly", bke::AttrDomain::Face);

  Vector<int> indices;
  if (mval) {
    uint index = uint(-1);
    if (!ED_mesh_pick_face(C, ob, mval, ED_MESH_PICK_DEFAULT_FACE_DIST, &index)) {
      select_poly.finish();
      return;
    }
    /* Since paintface_select_linked_faces might not select the face under the cursor, select it
     * here. */
    select_poly.span[index] = true;
    indices.append(index);
  }

  else {
    for (const int i : select_poly.span.index_range()) {
      if (!select_poly.span[i]) {
        continue;
      }
      indices.append(i);
    }
  }

  select_poly.finish();

  paintface_select_linked_faces(*mesh, indices, select);
  paintface_flush_flags(C, ob, true, false);
}

static int get_opposing_edge_index(const blender::IndexRange face,
                                   const blender::Span<int> corner_edges,
                                   const int current_edge_index)
{
  const int index_in_poly = corner_edges.slice(face).first_index(current_edge_index);
  /* Assumes that edge index of opposing face edge is always off by 2 on quads. */
  if (index_in_poly >= 2) {
    return corner_edges[face[index_in_poly - 2]];
  }
  /* Cannot be out of bounds because of the preceding if statement: if i < 2 then i+2 < 4. */
  return corner_edges[face[index_in_poly + 2]];
}

/**
 * Follow quads around the mesh by finding opposing edges.
 * \return True if the search has looped back on itself, finding the same index twice.
 */
static bool follow_face_loop(const int face_start_index,
                             const int edge_start_index,
                             const blender::OffsetIndices<int> faces,
                             const blender::VArray<bool> &hide_poly,
                             const blender::Span<int> corner_edges,
                             const blender::GroupedSpan<int> edge_to_face_map,
                             blender::VectorSet<int> &r_loop_faces)
{
  using namespace blender;
  int current_face_index = face_start_index;
  int current_edge_index = edge_start_index;

  while (current_edge_index > 0) {
    int next_face_index = -1;

    for (const int face_index : edge_to_face_map[current_edge_index]) {
      if (face_index != current_face_index) {
        next_face_index = face_index;
        break;
      }
    }

    /* Edge might only have 1 face connected. */
    if (next_face_index == -1) {
      return false;
    }

    /* Only works on quads. */
    if (faces[next_face_index].size() != 4) {
      return false;
    }

    /* Happens if we looped around the mesh. */
    if (r_loop_faces.contains(next_face_index)) {
      return true;
    }

    /* Hidden faces stop selection. */
    if (hide_poly[next_face_index]) {
      return false;
    }

    r_loop_faces.add(next_face_index);

    const IndexRange next_face = faces[next_face_index];
    current_edge_index = get_opposing_edge_index(next_face, corner_edges, current_edge_index);
    current_face_index = next_face_index;
  }
  return false;
}

void paintface_select_loop(bContext *C, Object *ob, const int mval[2], const bool select)
{
  using namespace blender;

  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  ViewContext vc = ED_view3d_viewcontext_init(C, depsgraph);
  ED_view3d_select_id_validate(&vc);

  Object *ob_eval = DEG_get_evaluated(depsgraph, ob);
  if (!ob_eval) {
    return;
  }

  uint closest_edge_index = uint(-1);
  if (!ED_mesh_pick_edge(C, ob, mval, ED_MESH_PICK_DEFAULT_VERT_DIST, &closest_edge_index)) {
    return;
  }

  if (closest_edge_index == -1) {
    return;
  }

  ARegion *region = CTX_wm_region(C);
  RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);
  ED_view3d_init_mats_rv3d(ob_eval, rv3d);

  Mesh *mesh = BKE_mesh_from_object(ob);
  const Span<int> corner_edges = mesh->corner_edges();
  const OffsetIndices faces = mesh->faces();

  Array<int> edge_to_face_offsets;
  Array<int> edge_to_face_indices;
  const GroupedSpan<int> edge_to_face_map = bke::mesh::build_edge_to_face_map(
      faces, corner_edges, mesh->edges_num, edge_to_face_offsets, edge_to_face_indices);

  VectorSet<int> faces_to_select;

  bke::MutableAttributeAccessor attributes = mesh->attributes_for_write();
  const VArray<bool> hide_poly = *attributes.lookup_or_default<bool>(
      ".hide_poly", bke::AttrDomain::Face, false);

  const Span<int> faces_to_closest_edge = edge_to_face_map[closest_edge_index];

  /* Picked edge may not be linked to a face (loose edge). */
  if (faces_to_closest_edge.is_empty()) {
    return;
  }

  const bool traced_full_loop = follow_face_loop(faces_to_closest_edge[0],
                                                 closest_edge_index,
                                                 faces,
                                                 hide_poly,
                                                 corner_edges,
                                                 edge_to_face_map,
                                                 faces_to_select);

  if (!traced_full_loop && faces_to_closest_edge.size() > 1) {
    /* Trace the other way. */
    follow_face_loop(faces_to_closest_edge[1],
                     closest_edge_index,
                     faces,
                     hide_poly,
                     corner_edges,
                     edge_to_face_map,
                     faces_to_select);
  }

  bke::SpanAttributeWriter<bool> select_poly = attributes.lookup_or_add_for_write_span<bool>(
      ".select_poly", bke::AttrDomain::Face);

  /* Toggling behavior. When one of the faces of the picked edge is already selected,
   * it deselects the loop instead. */
  bool any_adjacent_poly_selected = false;
  for (const int i : faces_to_closest_edge) {
    any_adjacent_poly_selected |= select_poly.span[i];
  }
  const bool select_toggle = select && !any_adjacent_poly_selected;
  select_poly.span.fill_indices(faces_to_select.as_span(), select_toggle);

  select_poly.finish();
  paintface_flush_flags(C, ob, true, false);
}

static bool poly_has_selected_neighbor(blender::Span<int> face_edges,
                                       blender::Span<blender::int2> edges,
                                       blender::Span<bool> select_vert,
                                       const bool face_step)
{
  for (const int edge_index : face_edges) {
    const blender::int2 &edge = edges[edge_index];
    /* If a face is selected, all of its verts are selected too, meaning that neighboring faces
     * will have some vertices selected. */
    if (face_step) {
      if (select_vert[edge[0]] || select_vert[edge[1]]) {
        return true;
      }
    }
    else {
      if (select_vert[edge[0]] && select_vert[edge[1]]) {
        return true;
      }
    }
  }
  return false;
}

void paintface_select_more(Mesh *mesh, const bool face_step)
{
  using namespace blender;

  bke::MutableAttributeAccessor attributes = mesh->attributes_for_write();
  bke::SpanAttributeWriter<bool> select_poly = attributes.lookup_or_add_for_write_span<bool>(
      ".select_poly", bke::AttrDomain::Face);
  bke::SpanAttributeWriter<bool> select_vert = attributes.lookup_or_add_for_write_span<bool>(
      ".select_vert", bke::AttrDomain::Point);
  const VArray<bool> hide_poly = *attributes.lookup_or_default<bool>(
      ".hide_poly", bke::AttrDomain::Face, false);

  const OffsetIndices faces = mesh->faces();
  const Span<int> corner_edges = mesh->corner_edges();
  const Span<int2> edges = mesh->edges();

  threading::parallel_for(select_poly.span.index_range(), 1024, [&](const IndexRange range) {
    for (const int i : range) {
      if (select_poly.span[i] || hide_poly[i]) {
        continue;
      }
      const IndexRange face = faces[i];
      if (poly_has_selected_neighbor(corner_edges.slice(face), edges, select_vert.span, face_step))
      {
        select_poly.span[i] = true;
      }
    }
  });

  select_poly.finish();
  select_vert.finish();
}

static bool poly_has_unselected_neighbor(blender::Span<int> face_edges,
                                         blender::Span<blender::int2> edges,
                                         blender::BitSpan verts_of_unselected_faces,
                                         const bool face_step)
{
  for (const int edge_index : face_edges) {
    const blender::int2 &edge = edges[edge_index];
    if (face_step) {
      if (verts_of_unselected_faces[edge[0]] || verts_of_unselected_faces[edge[1]]) {
        return true;
      }
    }
    else {
      if (verts_of_unselected_faces[edge[0]] && verts_of_unselected_faces[edge[1]]) {
        return true;
      }
    }
  }
  return false;
}

void paintface_select_less(Mesh *mesh, const bool face_step)
{
  using namespace blender;

  bke::MutableAttributeAccessor attributes = mesh->attributes_for_write();
  bke::SpanAttributeWriter<bool> select_poly = attributes.lookup_or_add_for_write_span<bool>(
      ".select_poly", bke::AttrDomain::Face);
  const VArray<bool> hide_poly = *attributes.lookup_or_default<bool>(
      ".hide_poly", bke::AttrDomain::Face, false);

  const OffsetIndices faces = mesh->faces();
  const Span<int> corner_verts = mesh->corner_verts();
  const Span<int> corner_edges = mesh->corner_edges();
  const Span<int2> edges = mesh->edges();

  BitVector<> verts_of_unselected_faces(mesh->verts_num);

  /* Find all vertices of unselected faces to help find neighboring faces after. */
  for (const int i : faces.index_range()) {
    if (select_poly.span[i]) {
      continue;
    }
    const IndexRange face = faces[i];
    for (const int vert : corner_verts.slice(face)) {
      verts_of_unselected_faces[vert].set(true);
    }
  }

  threading::parallel_for(faces.index_range(), 1024, [&](const IndexRange range) {
    for (const int i : range) {
      if (!select_poly.span[i] || hide_poly[i]) {
        continue;
      }
      const IndexRange face = faces[i];
      if (poly_has_unselected_neighbor(
              corner_edges.slice(face), edges, verts_of_unselected_faces, face_step))
      {
        select_poly.span[i] = false;
      }
    }
  });

  select_poly.finish();
}

bool paintface_deselect_all_visible(bContext *C, Object *ob, int action, bool flush_flags)
{
  using namespace blender;
  Mesh *mesh = BKE_mesh_from_object(ob);
  if (mesh == nullptr) {
    return false;
  }

  bke::MutableAttributeAccessor attributes = mesh->attributes_for_write();
  const VArray<bool> hide_poly = *attributes.lookup_or_default<bool>(
      ".hide_poly", bke::AttrDomain::Face, false);
  bke::SpanAttributeWriter<bool> select_poly = attributes.lookup_or_add_for_write_span<bool>(
      ".select_poly", bke::AttrDomain::Face);

  if (action == SEL_TOGGLE) {
    action = SEL_SELECT;

    for (int i = 0; i < mesh->faces_num; i++) {
      if (!hide_poly[i] && select_poly.span[i]) {
        action = SEL_DESELECT;
        break;
      }
    }
  }

  bool changed = false;

  for (int i = 0; i < mesh->faces_num; i++) {
    if (hide_poly[i]) {
      continue;
    }
    const bool old_selection = select_poly.span[i];
    switch (action) {
      case SEL_SELECT:
        select_poly.span[i] = true;
        break;
      case SEL_DESELECT:
        select_poly.span[i] = false;
        break;
      case SEL_INVERT:
        select_poly.span[i] = !select_poly.span[i];
        changed = true;
        break;
    }
    if (old_selection != select_poly.span[i]) {
      changed = true;
    }
  }

  select_poly.finish();

  if (changed) {
    if (flush_flags) {
      paintface_flush_flags(C, ob, true, false);
    }
  }
  return changed;
}

bool paintface_minmax(Object *ob, float r_min[3], float r_max[3])
{
  using namespace blender;
  bool ok = false;
  float vec[3], bmat[3][3];

  const Mesh *mesh = BKE_mesh_from_object(ob);
  if (!mesh) {
    return ok;
  }

  copy_m3_m4(bmat, ob->object_to_world().ptr());

  const Span<float3> positions = mesh->vert_positions();
  const OffsetIndices faces = mesh->faces();
  const Span<int> corner_verts = mesh->corner_verts();
  bke::AttributeAccessor attributes = mesh->attributes();
  const VArray<bool> hide_poly = *attributes.lookup_or_default<bool>(
      ".hide_poly", bke::AttrDomain::Face, false);
  const VArray<bool> select_poly = *attributes.lookup_or_default<bool>(
      ".select_poly", bke::AttrDomain::Face, false);

  for (int i = 0; i < mesh->faces_num; i++) {
    if (hide_poly[i] || !select_poly[i]) {
      continue;
    }

    for (const int vert : corner_verts.slice(faces[i])) {
      mul_v3_m3v3(vec, bmat, positions[vert]);
      add_v3_v3v3(vec, vec, ob->object_to_world().location());
      minmax_v3v3_v3(r_min, r_max, vec);
    }

    ok = true;
  }

  return ok;
}

bool paintface_mouse_select(bContext *C,
                            const int mval[2],
                            const SelectPick_Params &params,
                            Object *ob)
{
  using namespace blender;
  uint index;
  bool changed = false;
  bool found = false;

  /* Get the face under the cursor */
  Mesh *mesh = BKE_mesh_from_object(ob);

  bke::MutableAttributeAccessor attributes = mesh->attributes_for_write();
  const VArray<bool> hide_poly = *attributes.lookup_or_default<bool>(
      ".hide_poly", bke::AttrDomain::Face, false);
  bke::AttributeWriter<bool> select_poly = attributes.lookup_or_add_for_write<bool>(
      ".select_poly", bke::AttrDomain::Face);

  if (ED_mesh_pick_face(C, ob, mval, ED_MESH_PICK_DEFAULT_FACE_DIST, &index)) {
    if (index < mesh->faces_num) {
      if (!hide_poly[index]) {
        found = true;
      }
    }
  }

  if (params.sel_op == SEL_OP_SET) {
    if ((found && params.select_passthrough) && select_poly.varray[index]) {
      found = false;
    }
    else if (found || params.deselect_all) {
      /* Deselect everything. */
      changed |= paintface_deselect_all_visible(C, ob, SEL_DESELECT, false);
    }
  }

  if (found) {
    mesh->act_face = int(index);

    switch (params.sel_op) {
      case SEL_OP_SET:
      case SEL_OP_ADD:
        select_poly.varray.set(index, true);
        break;
      case SEL_OP_SUB:
        select_poly.varray.set(index, false);
        break;
      case SEL_OP_XOR:
        select_poly.varray.set(index, !select_poly.varray[index]);
        break;
      case SEL_OP_AND:
        BLI_assert_unreachable(); /* Doesn't make sense for picking. */
        break;
    }

    /* image window redraw */

    paintface_flush_flags(C, ob, true, false);
    ED_region_tag_redraw(CTX_wm_region(C)); /* XXX: should redraw all 3D views. */
    changed = true;
  }
  select_poly.finish();
  return changed || found;
}

void paintvert_flush_flags(Object *ob)
{
  using namespace blender;
  Mesh *mesh = BKE_mesh_from_object(ob);
  Mesh *mesh_eval = BKE_object_get_evaluated_mesh(ob);
  if (mesh == nullptr) {
    return;
  }

  /* we could call this directly in all areas that change selection,
   * since this could become slow for realtime updates (circle-select for eg) */
  bke::mesh_select_vert_flush(*mesh);

  if (mesh_eval == nullptr) {
    return;
  }

  const bke::AttributeAccessor attributes_orig = mesh->attributes();
  bke::MutableAttributeAccessor attributes_eval = mesh_eval->attributes_for_write();

  const int *orig_indices = (const int *)CustomData_get_layer(&mesh_eval->vert_data, CD_ORIGINDEX);

  const VArray<bool> hide_vert_orig = *attributes_orig.lookup_or_default<bool>(
      ".hide_vert", bke::AttrDomain::Point, false);
  bke::SpanAttributeWriter<bool> hide_vert_eval =
      attributes_eval.lookup_or_add_for_write_only_span<bool>(".hide_vert",
                                                              bke::AttrDomain::Point);
  if (orig_indices) {
    for (const int i : hide_vert_eval.span.index_range()) {
      if (orig_indices[i] != ORIGINDEX_NONE) {
        hide_vert_eval.span[i] = hide_vert_orig[orig_indices[i]];
      }
    }
  }
  else {
    hide_vert_orig.materialize(hide_vert_eval.span);
  }
  hide_vert_eval.finish();

  const VArray<bool> select_vert_orig = *attributes_orig.lookup_or_default<bool>(
      ".select_vert", bke::AttrDomain::Point, false);
  bke::SpanAttributeWriter<bool> select_vert_eval =
      attributes_eval.lookup_or_add_for_write_only_span<bool>(".select_vert",
                                                              bke::AttrDomain::Point);
  if (orig_indices) {
    for (const int i : select_vert_eval.span.index_range()) {
      if (orig_indices[i] != ORIGINDEX_NONE) {
        select_vert_eval.span[i] = select_vert_orig[orig_indices[i]];
      }
    }
  }
  else {
    select_vert_orig.materialize(select_vert_eval.span);
  }
  select_vert_eval.finish();

  BKE_mesh_batch_cache_dirty_tag(mesh, BKE_MESH_BATCH_DIRTY_ALL);
}

static void paintvert_select_linked_vertices(bContext *C,
                                             Object *ob,
                                             const blender::Span<int> vertex_indices,
                                             const bool select)
{
  using namespace blender;

  Mesh *mesh = BKE_mesh_from_object(ob);
  if (mesh == nullptr || mesh->faces_num == 0) {
    return;
  }

  /* AtomicDisjointSet is used to store connection information in vertex indices. */
  AtomicDisjointSet islands(mesh->verts_num);
  const Span<int2> edges = mesh->edges();

  /* By calling join() on the vertices of all edges, the AtomicDisjointSet contains information on
   * which parts of the mesh are connected. */
  threading::parallel_for(edges.index_range(), 1024, [&](const IndexRange range) {
    for (const int2 &edge : edges.slice(range)) {
      islands.join(edge[0], edge[1]);
    }
  });

  bke::MutableAttributeAccessor attributes = mesh->attributes_for_write();
  bke::SpanAttributeWriter<bool> select_vert = attributes.lookup_or_add_for_write_span<bool>(
      ".select_vert", bke::AttrDomain::Point);

  Set<int> selected_roots;

  for (const int i : vertex_indices) {
    const int root = islands.find_root(i);
    selected_roots.add(root);
  }

  threading::parallel_for(select_vert.span.index_range(), 1024, [&](const IndexRange range) {
    for (const int i : range) {
      const int root = islands.find_root(i);
      if (selected_roots.contains(root)) {
        select_vert.span[i] = select;
      }
    }
  });

  select_vert.finish();

  paintvert_flush_flags(ob);
  paintvert_tag_select_update(C, ob);
}

void paintvert_select_linked_pick(bContext *C,
                                  Object *ob,
                                  const int region_coordinates[2],
                                  const bool select)
{
  uint index = uint(-1);
  if (!ED_mesh_pick_vert(C, ob, region_coordinates, ED_MESH_PICK_DEFAULT_VERT_DIST, true, &index))
  {
    return;
  }

  paintvert_select_linked_vertices(C, ob, {int(index)}, select);
}

void paintvert_select_linked(bContext *C, Object *ob)
{
  using namespace blender;
  Mesh *mesh = BKE_mesh_from_object(ob);
  if (mesh == nullptr || mesh->faces_num == 0) {
    return;
  }

  blender::bke::MutableAttributeAccessor attributes = mesh->attributes_for_write();
  blender::bke::SpanAttributeWriter<bool> select_vert =
      attributes.lookup_or_add_for_write_span<bool>(".select_vert", bke::AttrDomain::Point);

  blender::Vector<int> indices;
  for (const int i : select_vert.span.index_range()) {
    if (!select_vert.span[i]) {
      continue;
    }
    indices.append(i);
  }
  select_vert.finish();
  paintvert_select_linked_vertices(C, ob, indices, true);
}

void paintvert_select_more(Mesh *mesh, const bool face_step)
{
  using namespace blender;

  bke::MutableAttributeAccessor attributes = mesh->attributes_for_write();
  bke::SpanAttributeWriter<bool> select_vert = attributes.lookup_or_add_for_write_span<bool>(
      ".select_vert", bke::AttrDomain::Point);
  const VArray<bool> hide_edge = *attributes.lookup_or_default<bool>(
      ".hide_edge", bke::AttrDomain::Edge, false);
  const VArray<bool> hide_poly = *attributes.lookup_or_default<bool>(
      ".hide_poly", bke::AttrDomain::Face, false);

  const OffsetIndices faces = mesh->faces();
  const Span<int> corner_edges = mesh->corner_edges();
  const Span<int> corner_verts = mesh->corner_verts();
  const Span<int2> edges = mesh->edges();

  Array<int> edge_to_face_offsets;
  Array<int> edge_to_face_indices;
  GroupedSpan<int> edge_to_face_map;
  if (face_step) {
    edge_to_face_map = bke::mesh::build_edge_to_face_map(
        faces, corner_edges, mesh->edges_num, edge_to_face_offsets, edge_to_face_indices);
  }

  /* Need a copy of the selected verts that we can read from and is not modified. */
  BitVector<> select_vert_original(mesh->verts_num, false);
  for (int i = 0; i < mesh->verts_num; i++) {
    select_vert_original[i].set(select_vert.span[i]);
  }

  /* If we iterated over faces we wouldn't extend the selection through edges that have no face
   * attached to them. */
  for (const int i : edges.index_range()) {
    const int2 &edge = edges[i];
    if ((!select_vert_original[edge[0]] && !select_vert_original[edge[1]]) || hide_edge[i]) {
      continue;
    }
    select_vert.span[edge[0]] = true;
    select_vert.span[edge[1]] = true;
    if (!face_step) {
      continue;
    }
    const Span<int> neighbor_polys = edge_to_face_map[i];
    for (const int face_i : neighbor_polys) {
      if (hide_poly[face_i]) {
        continue;
      }
      const IndexRange face = faces[face_i];
      for (const int vert : corner_verts.slice(face)) {
        select_vert.span[vert] = true;
      }
    }
  }

  select_vert.finish();
}

void paintvert_select_less(Mesh *mesh, const bool face_step)
{
  using namespace blender;

  bke::MutableAttributeAccessor attributes = mesh->attributes_for_write();
  bke::SpanAttributeWriter<bool> select_vert = attributes.lookup_or_add_for_write_span<bool>(
      ".select_vert", bke::AttrDomain::Point);
  const VArray<bool> hide_edge = *attributes.lookup_or_default<bool>(
      ".hide_edge", bke::AttrDomain::Edge, false);
  const VArray<bool> hide_poly = *attributes.lookup_or_default<bool>(
      ".hide_poly", bke::AttrDomain::Face, false);

  const OffsetIndices faces = mesh->faces();
  const Span<int> corner_edges = mesh->corner_edges();
  const Span<int> corner_verts = mesh->corner_verts();
  const Span<int2> edges = mesh->edges();

  GroupedSpan<int> edge_to_face_map;
  Array<int> edge_to_face_offsets;
  Array<int> edge_to_face_indices;
  if (face_step) {
    edge_to_face_map = bke::mesh::build_edge_to_face_map(
        faces, corner_edges, edges.size(), edge_to_face_offsets, edge_to_face_indices);
  }

  /* Need a copy of the selected verts that we can read from and is not modified. */
  BitVector<> select_vert_original(mesh->verts_num);
  for (int i = 0; i < mesh->verts_num; i++) {
    select_vert_original[i].set(select_vert.span[i]);
  }

  for (const int i : edges.index_range()) {
    const int2 &edge = edges[i];
    if ((select_vert_original[edge[0]] && select_vert_original[edge[1]]) && !hide_edge[i]) {
      continue;
    }
    select_vert.span[edge[0]] = false;
    select_vert.span[edge[1]] = false;

    if (!face_step) {
      continue;
    }
    for (const int face_i : edge_to_face_map[i]) {
      if (hide_poly[face_i]) {
        continue;
      }
      const IndexRange face = faces[face_i];
      for (const int vert : corner_verts.slice(face)) {
        select_vert.span[vert] = false;
      }
    }
  }
  select_vert.finish();
}

void paintvert_tag_select_update(bContext *C, Object *ob)
{
  DEG_id_tag_update(static_cast<ID *>(ob->data), ID_RECALC_SYNC_TO_EVAL | ID_RECALC_SELECT);
  WM_event_add_notifier(C, NC_GEOM | ND_SELECT, ob->data);
}

bool paintvert_deselect_all_visible(Object *ob, int action, bool flush_flags)
{
  using namespace blender;
  Mesh *mesh = BKE_mesh_from_object(ob);
  if (mesh == nullptr) {
    return false;
  }

  bke::MutableAttributeAccessor attributes = mesh->attributes_for_write();
  const VArray<bool> hide_vert = *attributes.lookup_or_default<bool>(
      ".hide_vert", bke::AttrDomain::Point, false);
  bke::SpanAttributeWriter<bool> select_vert = attributes.lookup_or_add_for_write_span<bool>(
      ".select_vert", bke::AttrDomain::Point);

  if (action == SEL_TOGGLE) {
    action = SEL_SELECT;

    for (int i = 0; i < mesh->verts_num; i++) {
      if (!hide_vert[i] && select_vert.span[i]) {
        action = SEL_DESELECT;
        break;
      }
    }
  }

  bool changed = false;
  for (int i = 0; i < mesh->verts_num; i++) {
    if (hide_vert[i]) {
      continue;
    }
    const bool old_selection = select_vert.span[i];
    switch (action) {
      case SEL_SELECT:
        select_vert.span[i] = true;
        break;
      case SEL_DESELECT:
        select_vert.span[i] = false;
        break;
      case SEL_INVERT:
        select_vert.span[i] = !select_vert.span[i];
        break;
    }
    if (old_selection != select_vert.span[i]) {
      changed = true;
    }
  }

  select_vert.finish();

  if (changed) {
    /* handle mselect */
    if (action == SEL_SELECT) {
      /* pass */
    }
    else if (ELEM(action, SEL_DESELECT, SEL_INVERT)) {
      BKE_mesh_mselect_clear(mesh);
    }
    else {
      BKE_mesh_mselect_validate(mesh);
    }

    if (flush_flags) {
      paintvert_flush_flags(ob);
    }
  }
  return changed;
}

void paintvert_select_ungrouped(Object *ob, bool extend, bool flush_flags)
{
  using namespace blender;
  Mesh *mesh = BKE_mesh_from_object(ob);
  if (mesh == nullptr) {
    return;
  }
  const Span<MDeformVert> dverts = mesh->deform_verts();
  if (dverts.is_empty()) {
    return;
  }

  if (!extend) {
    paintvert_deselect_all_visible(ob, SEL_DESELECT, false);
  }

  bke::MutableAttributeAccessor attributes = mesh->attributes_for_write();
  const VArray<bool> hide_vert = *attributes.lookup_or_default<bool>(
      ".hide_vert", bke::AttrDomain::Point, false);
  bke::SpanAttributeWriter<bool> select_vert = attributes.lookup_or_add_for_write_span<bool>(
      ".select_vert", bke::AttrDomain::Point);

  for (const int i : select_vert.span.index_range()) {
    if (!hide_vert[i]) {
      if (dverts[i].dw == nullptr) {
        /* if null weight then not grouped */
        select_vert.span[i] = true;
      }
    }
  }

  select_vert.finish();

  if (flush_flags) {
    paintvert_flush_flags(ob);
  }
}

void paintvert_hide(bContext *C, Object *ob, const bool unselected)
{
  using namespace blender;
  Mesh *mesh = BKE_mesh_from_object(ob);
  if (mesh == nullptr || mesh->verts_num == 0) {
    return;
  }

  bke::MutableAttributeAccessor attributes = mesh->attributes_for_write();
  bke::SpanAttributeWriter<bool> hide_vert = attributes.lookup_or_add_for_write_span<bool>(
      ".hide_vert", bke::AttrDomain::Point);
  bke::SpanAttributeWriter<bool> select_vert = attributes.lookup_or_add_for_write_span<bool>(
      ".select_vert", bke::AttrDomain::Point);

  for (const int i : hide_vert.span.index_range()) {
    if (!hide_vert.span[i]) {
      if (!select_vert.span[i] == unselected) {
        hide_vert.span[i] = true;
      }
    }

    if (hide_vert.span[i]) {
      select_vert.span[i] = false;
    }
  }
  hide_vert.finish();
  select_vert.finish();

  bke::mesh_hide_vert_flush(*mesh);

  paintvert_flush_flags(ob);
  paintvert_tag_select_update(C, ob);
}

void paintvert_reveal(bContext *C, Object *ob, const bool select)
{
  using namespace blender;
  Mesh *mesh = BKE_mesh_from_object(ob);
  if (mesh == nullptr || mesh->verts_num == 0) {
    return;
  }

  bke::MutableAttributeAccessor attributes = mesh->attributes_for_write();
  const VArray<bool> hide_vert = *attributes.lookup_or_default<bool>(
      ".hide_vert", bke::AttrDomain::Point, false);
  bke::SpanAttributeWriter<bool> select_vert = attributes.lookup_or_add_for_write_span<bool>(
      ".select_vert", bke::AttrDomain::Point);

  for (const int i : select_vert.span.index_range()) {
    if (hide_vert[i]) {
      select_vert.span[i] = select;
    }
  }

  select_vert.finish();

  /* Remove the hide attribute to reveal all vertices. */
  attributes.remove(".hide_vert");

  bke::mesh_hide_vert_flush(*mesh);

  paintvert_flush_flags(ob);
  paintvert_tag_select_update(C, ob);
}
