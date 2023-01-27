/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edmesh
 */

#include "MEM_guardedalloc.h"

#include "BLI_bitmap.h"
#include "BLI_blenlib.h"
#include "BLI_math.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"

#include "BKE_attribute.hh"
#include "BKE_context.h"
#include "BKE_customdata.h"
#include "BKE_global.h"
#include "BKE_mesh.h"
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
      const VArray<bool> hide_poly_me = attributes_me.lookup_or_default<bool>(
          ".hide_poly", ATTR_DOMAIN_FACE, false);
      bke::SpanAttributeWriter<bool> hide_poly_orig =
          attributes_orig.lookup_or_add_for_write_only_span<bool>(".hide_poly", ATTR_DOMAIN_FACE);
      hide_poly_me.materialize(hide_poly_orig.span);
      hide_poly_orig.finish();
    }
    if (flush_selection) {
      const VArray<bool> select_poly_me = attributes_me.lookup_or_default<bool>(
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
        const VArray<bool> hide_poly_orig = attributes_orig.lookup_or_default<bool>(
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
        const VArray<bool> select_poly_orig = attributes_orig.lookup_or_default<bool>(
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
    const VArray<bool> hide_poly = attributes.lookup_or_default<bool>(
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

/* Set object-mode face selection seams based on edge data, uses hash table to find seam edges. */

static void select_linked_tfaces_with_seams(Mesh *me, const uint index, const bool select)
{
  using namespace blender;
  bool do_it = true;
  bool mark = false;

  BLI_bitmap *edge_tag = BLI_BITMAP_NEW(me->totedge, __func__);
  BLI_bitmap *poly_tag = BLI_BITMAP_NEW(me->totpoly, __func__);

  const Span<MEdge> edges = me->edges();
  const Span<MPoly> polys = me->polys();
  const Span<MLoop> loops = me->loops();
  bke::MutableAttributeAccessor attributes = me->attributes_for_write();
  const VArray<bool> hide_poly = attributes.lookup_or_default<bool>(
      ".hide_poly", ATTR_DOMAIN_FACE, false);
  bke::SpanAttributeWriter<bool> select_poly = attributes.lookup_or_add_for_write_span<bool>(
      ".select_poly", ATTR_DOMAIN_FACE);

  if (index != uint(-1)) {
    /* only put face under cursor in array */
    const MPoly &poly = polys[index];
    BKE_mesh_poly_edgebitmap_insert(edge_tag, &poly, &loops[poly.loopstart]);
    BLI_BITMAP_ENABLE(poly_tag, index);
  }
  else {
    /* fill array by selection */
    for (int i = 0; i < me->totpoly; i++) {
      if (hide_poly[i]) {
        /* pass */
      }
      else if (select_poly.span[i]) {
        const MPoly &poly = polys[i];
        BKE_mesh_poly_edgebitmap_insert(edge_tag, &poly, &loops[poly.loopstart]);
        BLI_BITMAP_ENABLE(poly_tag, i);
      }
    }
  }

  while (do_it) {
    do_it = false;

    /* expand selection */
    for (int i = 0; i < me->totpoly; i++) {
      if (hide_poly[i]) {
        continue;
      }

      if (!BLI_BITMAP_TEST(poly_tag, i)) {
        mark = false;

        const MPoly &poly = polys[i];
        const MLoop *ml = &loops[poly.loopstart];
        for (int b = 0; b < poly.totloop; b++, ml++) {
          if ((edges[ml->e].flag & ME_SEAM) == 0) {
            if (BLI_BITMAP_TEST(edge_tag, ml->e)) {
              mark = true;
              break;
            }
          }
        }

        if (mark) {
          BLI_BITMAP_ENABLE(poly_tag, i);
          BKE_mesh_poly_edgebitmap_insert(edge_tag, &poly, &loops[poly.loopstart]);
          do_it = true;
        }
      }
    }
  }

  MEM_freeN(edge_tag);

  for (int i = 0; i < me->totpoly; i++) {
    if (BLI_BITMAP_TEST(poly_tag, i)) {
      select_poly.span[i] = select;
    }
  }

  MEM_freeN(poly_tag);
}

void paintface_select_linked(bContext *C, Object *ob, const int mval[2], const bool select)
{
  uint index = uint(-1);

  Mesh *me = BKE_mesh_from_object(ob);
  if (me == nullptr || me->totpoly == 0) {
    return;
  }

  if (mval) {
    if (!ED_mesh_pick_face(C, ob, mval, ED_MESH_PICK_DEFAULT_FACE_DIST, &index)) {
      return;
    }
  }

  select_linked_tfaces_with_seams(me, index, select);

  paintface_flush_flags(C, ob, true, false);
}

bool paintface_deselect_all_visible(bContext *C, Object *ob, int action, bool flush_flags)
{
  using namespace blender;
  Mesh *me = BKE_mesh_from_object(ob);
  if (me == nullptr) {
    return false;
  }

  bke::MutableAttributeAccessor attributes = me->attributes_for_write();
  const VArray<bool> hide_poly = attributes.lookup_or_default<bool>(
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
  const Span<MPoly> polys = me->polys();
  const Span<MLoop> loops = me->loops();
  bke::AttributeAccessor attributes = me->attributes();
  const VArray<bool> hide_poly = attributes.lookup_or_default<bool>(
      ".hide_poly", ATTR_DOMAIN_FACE, false);
  const VArray<bool> select_poly = attributes.lookup_or_default<bool>(
      ".select_poly", ATTR_DOMAIN_FACE, false);

  for (int i = 0; i < me->totpoly; i++) {
    if (hide_poly[i] || !select_poly[i]) {
      continue;
    }

    const MPoly &poly = polys[i];
    const MLoop *ml = &loops[poly.loopstart];
    for (int b = 0; b < poly.totloop; b++, ml++) {
      mul_v3_m3v3(vec, bmat, positions[ml->v]);
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
  const VArray<bool> hide_poly = attributes.lookup_or_default<bool>(
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

  const VArray<bool> hide_vert_orig = attributes_orig.lookup_or_default<bool>(
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

  const VArray<bool> select_vert_orig = attributes_orig.lookup_or_default<bool>(
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
  const VArray<bool> hide_vert = attributes.lookup_or_default<bool>(
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
  const VArray<bool> hide_vert = attributes.lookup_or_default<bool>(
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
  const VArray<bool> hide_vert = attributes.lookup_or_default<bool>(
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
