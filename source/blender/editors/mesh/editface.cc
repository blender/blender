/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edmesh
 */

#include "MEM_guardedalloc.h"

#include "BLI_atomic_disjoint_set.hh"
#include "BLI_bitmap.h"
#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_task.hh"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"

#include "BKE_attribute.hh"
#include "BKE_context.h"
#include "BKE_customdata.h"
#include "BKE_global.h"
#include "BKE_mesh.hh"
#include "BKE_mesh_mapping.h"
#include "BKE_object.h"

#include "ED_mesh.h"
#include "ED_screen.h"
#include "ED_select_utils.h"
#include "ED_view3d.h"

#include "WM_api.h"
#include "WM_types.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

/* own include */

void paintface_flush_flags(bContext *C,
                           Object *ob,
                           const bool flush_selection,
                           const bool flush_hidden)
{
  using namespace blender;
  Mesh *me = BKE_mesh_from_object(ob);
  const int *index_array = nullptr;

  BLI_assert(flush_selection || flush_hidden);

  if (me == nullptr) {
    return;
  }

  /* NOTE: call #BKE_mesh_flush_hidden_from_verts_ex first when changing hidden flags. */

  /* we could call this directly in all areas that change selection,
   * since this could become slow for realtime updates (circle-select for eg) */
  if (flush_selection) {
    BKE_mesh_flush_select_from_polys(me);
  }

  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Object *ob_eval = DEG_get_evaluated_object(depsgraph, ob);

  if (ob_eval == nullptr) {
    return;
  }

  bke::AttributeAccessor attributes_me = me->attributes();
  Mesh *me_orig = (Mesh *)ob_eval->runtime.data_orig;
  bke::MutableAttributeAccessor attributes_orig = me_orig->attributes_for_write();
  Mesh *me_eval = (Mesh *)ob_eval->runtime.data_eval;
  bke::MutableAttributeAccessor attributes_eval = me_eval->attributes_for_write();
  bool updated = false;

  if (me_orig != nullptr && me_eval != nullptr && me_orig->totpoly == me->totpoly) {
    /* Update the COW copy of the mesh. */
    if (flush_hidden) {
      const VArray<bool> hide_poly_me = *attributes_me.lookup_or_default<bool>(
          ".hide_poly", ATTR_DOMAIN_FACE, false);
      bke::SpanAttributeWriter<bool> hide_poly_orig =
          attributes_orig.lookup_or_add_for_write_only_span<bool>(".hide_poly", ATTR_DOMAIN_FACE);
      hide_poly_me.materialize(hide_poly_orig.span);
      hide_poly_orig.finish();
    }
    if (flush_selection) {
      const VArray<bool> select_poly_me = *attributes_me.lookup_or_default<bool>(
          ".select_poly", ATTR_DOMAIN_FACE, false);
      bke::SpanAttributeWriter<bool> select_poly_orig =
          attributes_orig.lookup_or_add_for_write_only_span<bool>(".select_poly",
                                                                  ATTR_DOMAIN_FACE);
      select_poly_me.materialize(select_poly_orig.span);
      select_poly_orig.finish();
    }

    /* Mesh polys => Final derived polys */
    if ((index_array = (const int *)CustomData_get_layer(&me_eval->pdata, CD_ORIGINDEX))) {
      if (flush_hidden) {
        const VArray<bool> hide_poly_orig = *attributes_orig.lookup_or_default<bool>(
            ".hide_poly", ATTR_DOMAIN_FACE, false);
        bke::SpanAttributeWriter<bool> hide_poly_eval =
            attributes_eval.lookup_or_add_for_write_only_span<bool>(".hide_poly",
                                                                    ATTR_DOMAIN_FACE);
        for (const int i : IndexRange(me_eval->totpoly)) {
          const int orig_poly_index = index_array[i];
          if (orig_poly_index != ORIGINDEX_NONE) {
            hide_poly_eval.span[i] = hide_poly_orig[orig_poly_index];
          }
        }
        hide_poly_eval.finish();
      }
      if (flush_selection) {
        const VArray<bool> select_poly_orig = *attributes_orig.lookup_or_default<bool>(
            ".select_poly", ATTR_DOMAIN_FACE, false);
        bke::SpanAttributeWriter<bool> select_poly_eval =
            attributes_eval.lookup_or_add_for_write_only_span<bool>(".select_poly",
                                                                    ATTR_DOMAIN_FACE);
        for (const int i : IndexRange(me_eval->totpoly)) {
          const int orig_poly_index = index_array[i];
          if (orig_poly_index != ORIGINDEX_NONE) {
            select_poly_eval.span[i] = select_poly_orig[orig_poly_index];
          }
        }
        select_poly_eval.finish();
      }

      updated = true;
    }
  }

  if (updated) {
    if (flush_hidden) {
      BKE_mesh_batch_cache_dirty_tag(me_eval, BKE_MESH_BATCH_DIRTY_ALL);
    }
    else {
      BKE_mesh_batch_cache_dirty_tag(me_eval, BKE_MESH_BATCH_DIRTY_SELECT_PAINT);
    }

    DEG_id_tag_update(static_cast<ID *>(ob->data), ID_RECALC_SELECT);
  }
  else {
    DEG_id_tag_update(static_cast<ID *>(ob->data), ID_RECALC_COPY_ON_WRITE | ID_RECALC_SELECT);
  }

  WM_event_add_notifier(C, NC_GEOM | ND_SELECT, ob->data);
}

void paintface_hide(bContext *C, Object *ob, const bool unselected)
{
  using namespace blender;
  Mesh *me = BKE_mesh_from_object(ob);
  if (me == nullptr || me->totpoly == 0) {
    return;
  }

  bke::MutableAttributeAccessor attributes = me->attributes_for_write();
  bke::SpanAttributeWriter<bool> hide_poly = attributes.lookup_or_add_for_write_span<bool>(
      ".hide_poly", ATTR_DOMAIN_FACE);
  bke::SpanAttributeWriter<bool> select_poly = attributes.lookup_or_add_for_write_span<bool>(
      ".select_poly", ATTR_DOMAIN_FACE);

  for (int i = 0; i < me->totpoly; i++) {
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

  BKE_mesh_flush_hidden_from_polys(me);

  paintface_flush_flags(C, ob, true, true);
}

void paintface_reveal(bContext *C, Object *ob, const bool select)
{
  using namespace blender;
  Mesh *me = BKE_mesh_from_object(ob);
  if (me == nullptr || me->totpoly == 0) {
    return;
  }

  bke::MutableAttributeAccessor attributes = me->attributes_for_write();

  if (select) {
    const VArray<bool> hide_poly = *attributes.lookup_or_default<bool>(
        ".hide_poly", ATTR_DOMAIN_FACE, false);
    bke::SpanAttributeWriter<bool> select_poly = attributes.lookup_or_add_for_write_span<bool>(
        ".select_poly", ATTR_DOMAIN_FACE);
    for (const int i : hide_poly.index_range()) {
      if (hide_poly[i]) {
        select_poly.span[i] = true;
      }
    }
    select_poly.finish();
  }

  attributes.remove(".hide_poly");

  BKE_mesh_flush_hidden_from_polys(me);

  paintface_flush_flags(C, ob, true, true);
}

/**
 * Join all edges of each poly in the #AtomicDisjointSet. This can be used to find out which polys
 * are connected to each other.
 * \param islands: Is expected to be of length `mesh->totedge`.
 * \param skip_seams: Polys separated by a seam will be treated as not connected.
 */
static void build_poly_connections(blender::AtomicDisjointSet &islands,
                                   Mesh &mesh,
                                   const bool skip_seams = true)
{
  using namespace blender;
  const OffsetIndices polys = mesh.polys();
  const Span<int> corner_edges = mesh.corner_edges();

  const bke::AttributeAccessor attributes = mesh.attributes();
  const VArray<bool> uv_seams = *attributes.lookup_or_default<bool>(
      ".uv_seam", ATTR_DOMAIN_EDGE, false);
  const VArray<bool> hide_poly = *attributes.lookup_or_default<bool>(
      ".hide_poly", ATTR_DOMAIN_FACE, false);

  /* Polys are connected if they share edges. By connecting all edges of a loop (as long as they
   * are not a seam) we can find connected faces. */
  threading::parallel_for(polys.index_range(), 1024, [&](const IndexRange range) {
    for (const int poly_index : range) {
      if (hide_poly[poly_index]) {
        continue;
      }
      const Span<int> poly_edges = corner_edges.slice(polys[poly_index]);

      for (const int poly_loop_index : poly_edges.index_range()) {
        const int outer_edge = poly_edges[poly_loop_index];
        if (skip_seams && uv_seams[outer_edge]) {
          continue;
        }

        for (const int inner_edge :
             poly_edges.slice(poly_loop_index, poly_edges.size() - poly_loop_index)) {
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

  AtomicDisjointSet islands(mesh.totedge);
  build_poly_connections(islands, mesh);

  const OffsetIndices polys = mesh.polys();
  const Span<int> corner_edges = mesh.corner_edges();

  bke::MutableAttributeAccessor attributes = mesh.attributes_for_write();
  const VArray<bool> uv_seams = *attributes.lookup_or_default<bool>(
      ".uv_seam", ATTR_DOMAIN_EDGE, false);
  bke::SpanAttributeWriter<bool> select_poly = attributes.lookup_or_add_for_write_span<bool>(
      ".select_poly", ATTR_DOMAIN_FACE);

  Set<int> selected_roots;
  for (const int i : face_indices) {
    for (const int edge : corner_edges.slice(polys[i])) {
      if (uv_seams[edge]) {
        continue;
      }
      const int root = islands.find_root(edge);
      selected_roots.add(root);
    }
  }

  threading::parallel_for(select_poly.span.index_range(), 1024, [&](const IndexRange range) {
    for (const int poly_index : range) {
      for (const int edge : corner_edges.slice(polys[poly_index])) {
        const int root = islands.find_root(edge);
        if (selected_roots.contains(root)) {
          select_poly.span[poly_index] = select;
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
  Mesh *me = BKE_mesh_from_object(ob);
  if (me == nullptr || me->totpoly == 0) {
    return;
  }

  bke::MutableAttributeAccessor attributes = me->attributes_for_write();
  bke::SpanAttributeWriter<bool> select_poly = attributes.lookup_or_add_for_write_span<bool>(
      ".select_poly", ATTR_DOMAIN_FACE);

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

  paintface_select_linked_faces(*me, indices, select);
  paintface_flush_flags(C, ob, true, false);
}

static bool poly_has_selected_neighbor(blender::Span<int> poly_edges,
                                       blender::Span<blender::int2> edges,
                                       blender::Span<bool> select_vert,
                                       const bool face_step)
{
  for (const int edge_index : poly_edges) {
    const blender::int2 &edge = edges[edge_index];
    /* If a poly is selected, all of its verts are selected too, meaning that neighboring faces
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
      ".select_poly", ATTR_DOMAIN_FACE);
  bke::SpanAttributeWriter<bool> select_vert = attributes.lookup_or_add_for_write_span<bool>(
      ".select_vert", ATTR_DOMAIN_POINT);
  const VArray<bool> hide_poly = *attributes.lookup_or_default<bool>(
      ".hide_poly", ATTR_DOMAIN_FACE, false);

  const OffsetIndices polys = mesh->polys();
  const Span<int> corner_edges = mesh->corner_edges();
  const Span<int2> edges = mesh->edges();

  threading::parallel_for(select_poly.span.index_range(), 1024, [&](const IndexRange range) {
    for (const int i : range) {
      if (select_poly.span[i] || hide_poly[i]) {
        continue;
      }
      const IndexRange poly = polys[i];
      if (poly_has_selected_neighbor(corner_edges.slice(poly), edges, select_vert.span, face_step))
      {
        select_poly.span[i] = true;
      }
    }
  });

  select_poly.finish();
  select_vert.finish();
}

static bool poly_has_unselected_neighbor(blender::Span<int> poly_edges,
                                         blender::Span<blender::int2> edges,
                                         blender::BitSpan verts_of_unselected_faces,
                                         const bool face_step)
{
  for (const int edge_index : poly_edges) {
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
      ".select_poly", ATTR_DOMAIN_FACE);
  const VArray<bool> hide_poly = *attributes.lookup_or_default<bool>(
      ".hide_poly", ATTR_DOMAIN_FACE, false);

  const OffsetIndices polys = mesh->polys();
  const Span<int> corner_verts = mesh->corner_verts();
  const Span<int> corner_edges = mesh->corner_edges();
  const Span<int2> edges = mesh->edges();

  BitVector<> verts_of_unselected_faces(mesh->totvert);

  /* Find all vertices of unselected faces to help find neighboring faces after. */
  for (const int i : polys.index_range()) {
    if (select_poly.span[i]) {
      continue;
    }
    const IndexRange poly = polys[i];
    for (const int vert : corner_verts.slice(poly)) {
      verts_of_unselected_faces[vert].set(true);
    }
  }

  threading::parallel_for(polys.index_range(), 1024, [&](const IndexRange range) {
    for (const int i : range) {
      if (!select_poly.span[i] || hide_poly[i]) {
        continue;
      }
      const IndexRange poly = polys[i];
      if (poly_has_unselected_neighbor(
              corner_edges.slice(poly), edges, verts_of_unselected_faces, face_step))
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
  Mesh *me = BKE_mesh_from_object(ob);
  if (me == nullptr) {
    return false;
  }

  bke::MutableAttributeAccessor attributes = me->attributes_for_write();
  const VArray<bool> hide_poly = *attributes.lookup_or_default<bool>(
      ".hide_poly", ATTR_DOMAIN_FACE, false);
  bke::SpanAttributeWriter<bool> select_poly = attributes.lookup_or_add_for_write_span<bool>(
      ".select_poly", ATTR_DOMAIN_FACE);

  if (action == SEL_TOGGLE) {
    action = SEL_SELECT;

    for (int i = 0; i < me->totpoly; i++) {
      if (!hide_poly[i] && select_poly.span[i]) {
        action = SEL_DESELECT;
        break;
      }
    }
  }

  bool changed = false;

  for (int i = 0; i < me->totpoly; i++) {
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

  const Mesh *me = BKE_mesh_from_object(ob);
  if (!me || !CustomData_has_layer(&me->ldata, CD_PROP_FLOAT2)) {
    return ok;
  }

  copy_m3_m4(bmat, ob->object_to_world);

  const Span<float3> positions = me->vert_positions();
  const OffsetIndices polys = me->polys();
  const Span<int> corner_verts = me->corner_verts();
  bke::AttributeAccessor attributes = me->attributes();
  const VArray<bool> hide_poly = *attributes.lookup_or_default<bool>(
      ".hide_poly", ATTR_DOMAIN_FACE, false);
  const VArray<bool> select_poly = *attributes.lookup_or_default<bool>(
      ".select_poly", ATTR_DOMAIN_FACE, false);

  for (int i = 0; i < me->totpoly; i++) {
    if (hide_poly[i] || !select_poly[i]) {
      continue;
    }

    for (const int vert : corner_verts.slice(polys[i])) {
      mul_v3_m3v3(vec, bmat, positions[vert]);
      add_v3_v3v3(vec, vec, ob->object_to_world[3]);
      minmax_v3v3_v3(r_min, r_max, vec);
    }

    ok = true;
  }

  return ok;
}

bool paintface_mouse_select(bContext *C,
                            const int mval[2],
                            const SelectPick_Params *params,
                            Object *ob)
{
  using namespace blender;
  uint index;
  bool changed = false;
  bool found = false;

  /* Get the face under the cursor */
  Mesh *me = BKE_mesh_from_object(ob);

  bke::MutableAttributeAccessor attributes = me->attributes_for_write();
  const VArray<bool> hide_poly = *attributes.lookup_or_default<bool>(
      ".hide_poly", ATTR_DOMAIN_FACE, false);
  bke::AttributeWriter<bool> select_poly = attributes.lookup_or_add_for_write<bool>(
      ".select_poly", ATTR_DOMAIN_FACE);

  if (ED_mesh_pick_face(C, ob, mval, ED_MESH_PICK_DEFAULT_FACE_DIST, &index)) {
    if (index < me->totpoly) {
      if (!hide_poly[index]) {
        found = true;
      }
    }
  }

  if (params->sel_op == SEL_OP_SET) {
    if ((found && params->select_passthrough) && select_poly.varray[index]) {
      found = false;
    }
    else if (found || params->deselect_all) {
      /* Deselect everything. */
      changed |= paintface_deselect_all_visible(C, ob, SEL_DESELECT, false);
    }
  }

  if (found) {
    me->act_face = int(index);

    switch (params->sel_op) {
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
  using namespace blender;
  Mesh *me = BKE_mesh_from_object(ob);
  Mesh *me_eval = BKE_object_get_evaluated_mesh(ob);
  if (me == nullptr) {
    return;
  }

  /* we could call this directly in all areas that change selection,
   * since this could become slow for realtime updates (circle-select for eg) */
  BKE_mesh_flush_select_from_verts(me);

  if (me_eval == nullptr) {
    return;
  }

  const bke::AttributeAccessor attributes_orig = me->attributes();
  bke::MutableAttributeAccessor attributes_eval = me_eval->attributes_for_write();

  const int *orig_indices = (const int *)CustomData_get_layer(&me_eval->vdata, CD_ORIGINDEX);

  const VArray<bool> hide_vert_orig = *attributes_orig.lookup_or_default<bool>(
      ".hide_vert", ATTR_DOMAIN_POINT, false);
  bke::SpanAttributeWriter<bool> hide_vert_eval =
      attributes_eval.lookup_or_add_for_write_only_span<bool>(".hide_vert", ATTR_DOMAIN_POINT);
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
      ".select_vert", ATTR_DOMAIN_POINT, false);
  bke::SpanAttributeWriter<bool> select_vert_eval =
      attributes_eval.lookup_or_add_for_write_only_span<bool>(".select_vert", ATTR_DOMAIN_POINT);
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

  BKE_mesh_batch_cache_dirty_tag(me, BKE_MESH_BATCH_DIRTY_ALL);
}

static void paintvert_select_linked_vertices(bContext *C,
                                             Object *ob,
                                             const blender::Span<int> vertex_indices,
                                             const bool select)
{
  using namespace blender;

  Mesh *mesh = BKE_mesh_from_object(ob);
  if (mesh == nullptr || mesh->totpoly == 0) {
    return;
  }

  /* AtomicDisjointSet is used to store connection information in vertex indices. */
  AtomicDisjointSet islands(mesh->totvert);
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
      ".select_vert", ATTR_DOMAIN_POINT);

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
  Mesh *mesh = BKE_mesh_from_object(ob);
  if (mesh == nullptr || mesh->totpoly == 0) {
    return;
  }

  blender::bke::MutableAttributeAccessor attributes = mesh->attributes_for_write();
  blender::bke::SpanAttributeWriter<bool> select_vert =
      attributes.lookup_or_add_for_write_span<bool>(".select_vert", ATTR_DOMAIN_POINT);

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
      ".select_vert", ATTR_DOMAIN_POINT);
  const VArray<bool> hide_edge = *attributes.lookup_or_default<bool>(
      ".hide_edge", ATTR_DOMAIN_EDGE, false);
  const VArray<bool> hide_poly = *attributes.lookup_or_default<bool>(
      ".hide_poly", ATTR_DOMAIN_FACE, false);

  const OffsetIndices polys = mesh->polys();
  const Span<int> corner_edges = mesh->corner_edges();
  const Span<int> corner_verts = mesh->corner_verts();
  const Span<int2> edges = mesh->edges();

  Array<int> edge_to_face_offsets;
  Array<int> edge_to_face_indices;
  GroupedSpan<int> edge_to_face_map;
  if (face_step) {
    edge_to_face_map = bke::mesh::build_edge_to_poly_map(
        polys, corner_edges, mesh->totedge, edge_to_face_offsets, edge_to_face_indices);
  }

  /* Need a copy of the selected verts that we can read from and is not modified. */
  BitVector<> select_vert_original(mesh->totvert, false);
  for (int i = 0; i < mesh->totvert; i++) {
    select_vert_original[i].set(select_vert.span[i]);
  }

  /* If we iterated over polys we wouldn't extend the selection through edges that have no face
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
    for (const int poly_i : neighbor_polys) {
      if (hide_poly[poly_i]) {
        continue;
      }
      const IndexRange poly = polys[poly_i];
      for (const int vert : corner_verts.slice(poly)) {
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
      ".select_vert", ATTR_DOMAIN_POINT);
  const VArray<bool> hide_edge = *attributes.lookup_or_default<bool>(
      ".hide_edge", ATTR_DOMAIN_EDGE, false);
  const VArray<bool> hide_poly = *attributes.lookup_or_default<bool>(
      ".hide_poly", ATTR_DOMAIN_FACE, false);

  const OffsetIndices polys = mesh->polys();
  const Span<int> corner_edges = mesh->corner_edges();
  const Span<int> corner_verts = mesh->corner_verts();
  const Span<int2> edges = mesh->edges();

  GroupedSpan<int> edge_to_poly_map;
  Array<int> edge_to_poly_offsets;
  Array<int> edge_to_poly_indices;
  if (face_step) {
    edge_to_poly_map = bke::mesh::build_edge_to_poly_map(
        polys, corner_edges, edges.size(), edge_to_poly_offsets, edge_to_poly_indices);
  }

  /* Need a copy of the selected verts that we can read from and is not modified. */
  BitVector<> select_vert_original(mesh->totvert);
  for (int i = 0; i < mesh->totvert; i++) {
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
    for (const int poly_i : edge_to_poly_map[i]) {
      if (hide_poly[poly_i]) {
        continue;
      }
      const IndexRange poly = polys[poly_i];
      for (const int vert : corner_verts.slice(poly)) {
        select_vert.span[vert] = false;
      }
    }
  }
  select_vert.finish();
}

void paintvert_tag_select_update(bContext *C, Object *ob)
{
  DEG_id_tag_update(static_cast<ID *>(ob->data), ID_RECALC_COPY_ON_WRITE | ID_RECALC_SELECT);
  WM_event_add_notifier(C, NC_GEOM | ND_SELECT, ob->data);
}

bool paintvert_deselect_all_visible(Object *ob, int action, bool flush_flags)
{
  using namespace blender;
  Mesh *me = BKE_mesh_from_object(ob);
  if (me == nullptr) {
    return false;
  }

  bke::MutableAttributeAccessor attributes = me->attributes_for_write();
  const VArray<bool> hide_vert = *attributes.lookup_or_default<bool>(
      ".hide_vert", ATTR_DOMAIN_POINT, false);
  bke::SpanAttributeWriter<bool> select_vert = attributes.lookup_or_add_for_write_span<bool>(
      ".select_vert", ATTR_DOMAIN_POINT);

  if (action == SEL_TOGGLE) {
    action = SEL_SELECT;

    for (int i = 0; i < me->totvert; i++) {
      if (!hide_vert[i] && select_vert.span[i]) {
        action = SEL_DESELECT;
        break;
      }
    }
  }

  bool changed = false;
  for (int i = 0; i < me->totvert; i++) {
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
      BKE_mesh_mselect_clear(me);
    }
    else {
      BKE_mesh_mselect_validate(me);
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
  Mesh *me = BKE_mesh_from_object(ob);
  if (me == nullptr) {
    return;
  }
  const Span<MDeformVert> dverts = me->deform_verts();
  if (dverts.is_empty()) {
    return;
  }

  if (!extend) {
    paintvert_deselect_all_visible(ob, SEL_DESELECT, false);
  }

  bke::MutableAttributeAccessor attributes = me->attributes_for_write();
  const VArray<bool> hide_vert = *attributes.lookup_or_default<bool>(
      ".hide_vert", ATTR_DOMAIN_POINT, false);
  bke::SpanAttributeWriter<bool> select_vert = attributes.lookup_or_add_for_write_span<bool>(
      ".select_vert", ATTR_DOMAIN_POINT);

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
  Mesh *me = BKE_mesh_from_object(ob);
  if (me == nullptr || me->totvert == 0) {
    return;
  }

  bke::MutableAttributeAccessor attributes = me->attributes_for_write();
  bke::SpanAttributeWriter<bool> hide_vert = attributes.lookup_or_add_for_write_span<bool>(
      ".hide_vert", ATTR_DOMAIN_POINT);
  bke::SpanAttributeWriter<bool> select_vert = attributes.lookup_or_add_for_write_span<bool>(
      ".select_vert", ATTR_DOMAIN_POINT);

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

  BKE_mesh_flush_hidden_from_verts(me);

  paintvert_flush_flags(ob);
  paintvert_tag_select_update(C, ob);
}

void paintvert_reveal(bContext *C, Object *ob, const bool select)
{
  using namespace blender;
  Mesh *me = BKE_mesh_from_object(ob);
  if (me == nullptr || me->totvert == 0) {
    return;
  }

  bke::MutableAttributeAccessor attributes = me->attributes_for_write();
  const VArray<bool> hide_vert = *attributes.lookup_or_default<bool>(
      ".hide_vert", ATTR_DOMAIN_POINT, false);
  bke::SpanAttributeWriter<bool> select_vert = attributes.lookup_or_add_for_write_span<bool>(
      ".select_vert", ATTR_DOMAIN_POINT);

  for (const int i : select_vert.span.index_range()) {
    if (hide_vert[i]) {
      select_vert.span[i] = select;
    }
  }

  select_vert.finish();

  /* Remove the hide attribute to reveal all vertices. */
  attributes.remove(".hide_vert");

  BKE_mesh_flush_hidden_from_verts(me);

  paintvert_flush_flags(ob);
  paintvert_tag_select_update(C, ob);
}
