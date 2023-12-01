/* SPDX-FileCopyrightText: 2010 by Nicholas Bishop. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edsculpt
 * Implements the PBVH node hiding operator.
 */

#include "MEM_guardedalloc.h"

#include "BLI_array_utils.hh"
#include "BLI_bitmap.h"
#include "BLI_enumerable_thread_specific.hh"
#include "BLI_math_geom.h"
#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"
#include "BLI_utildefines.h"
#include "BLI_vector.hh"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_attribute.hh"
#include "BKE_ccg.h"
#include "BKE_context.hh"
#include "BKE_mesh.hh"
#include "BKE_multires.hh"
#include "BKE_paint.hh"
#include "BKE_pbvh_api.hh"
#include "BKE_subdiv_ccg.hh"
#include "BKE_subsurf.hh"

#include "DEG_depsgraph.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "ED_screen.hh"
#include "ED_view3d.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "bmesh.h"

#include "paint_intern.hh"

/* For undo push. */
#include "sculpt_intern.hh"

namespace blender::ed::sculpt_paint::hide {

enum class VisAction {
  Hide = 0,
  Show = 1,
};

enum VisArea {
  Inside = 0,
  Outside = 1,
  All = 2,
  Masked = 3,
};

static bool action_to_hide(const VisAction action)
{
  return action == VisAction::Hide;
}

/* Return true if the element should be hidden/shown. */
static bool is_effected(const VisArea area,
                        const float planes[4][4],
                        const float co[3],
                        const float mask)
{
  if (area == VisArea::All) {
    return true;
  }
  if (area == VisArea::Masked) {
    return mask > 0.5f;
  }

  const bool inside = isect_point_planes_v3(planes, 4, co);
  return ((inside && area == VisArea::Inside) || (!inside && area == VisArea::Outside));
}

static void vert_show_all(Object &object, const Span<PBVHNode *> nodes)
{
  Mesh &mesh = *static_cast<Mesh *>(object.data);
  bke::MutableAttributeAccessor attributes = mesh.attributes_for_write();
  if (const VArray<bool> attribute = *attributes.lookup<bool>(".hide_vert", ATTR_DOMAIN_POINT)) {
    const VArraySpan hide_vert(attribute);
    for (PBVHNode *node : nodes) {
      const Span<int> verts = BKE_pbvh_node_get_vert_indices(node);
      if (std::any_of(verts.begin(), verts.end(), [&](const int i) { return hide_vert[i]; })) {
        SCULPT_undo_push_node(&object, node, SCULPT_UNDO_HIDDEN);
        BKE_pbvh_node_mark_rebuild_draw(node);
      }
    }
  }
  for (PBVHNode *node : nodes) {
    BKE_pbvh_node_fully_hidden_set(node, false);
  }
  attributes.remove(".hide_vert");
}

static bool vert_hide_is_changed(const Span<int> verts,
                                 const Span<bool> orig_hide,
                                 const Span<bool> new_hide)
{
  for (const int i : verts.index_range()) {
    if (orig_hide[verts[i]] != new_hide[i]) {
      return true;
    }
  }
  return false;
}

static void vert_hide_update(Object &object,
                             const Span<PBVHNode *> nodes,
                             FunctionRef<void(Span<int>, MutableSpan<bool>)> calc_hide)
{
  Mesh &mesh = *static_cast<Mesh *>(object.data);
  bke::MutableAttributeAccessor attributes = mesh.attributes_for_write();
  bke::SpanAttributeWriter<bool> hide_vert = attributes.lookup_or_add_for_write_span<bool>(
      ".hide_vert", ATTR_DOMAIN_POINT);

  threading::EnumerableThreadSpecific<Vector<bool>> all_new_hide;
  threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
    Vector<bool> &new_hide = all_new_hide.local();
    for (PBVHNode *node : nodes.slice(range)) {
      const Span<int> verts = BKE_pbvh_node_get_vert_indices(node);

      new_hide.reinitialize(verts.size());
      array_utils::gather(hide_vert.span.as_span(), verts, new_hide.as_mutable_span());
      calc_hide(verts, new_hide);
      if (!vert_hide_is_changed(verts, hide_vert.span, new_hide)) {
        continue;
      }

      SCULPT_undo_push_node(&object, node, SCULPT_UNDO_HIDDEN);

      /* Don't tag a visibility update, we handle updating the fully hidden status here. */
      BKE_pbvh_node_mark_rebuild_draw(node);
      BKE_pbvh_node_fully_hidden_set(node, !new_hide.contains(false));

      array_utils::scatter(new_hide.as_span(), verts, hide_vert.span);
    }
  });
  hide_vert.finish();
}

static void partialvis_update_mesh(Object &object,
                                   PBVH &pbvh,
                                   const VisAction action,
                                   const VisArea area,
                                   const float planes[4][4],
                                   const Span<PBVHNode *> nodes)
{
  Mesh &mesh = *static_cast<Mesh *>(object.data);
  bke::MutableAttributeAccessor attributes = mesh.attributes_for_write();
  if (action == VisAction::Show && !attributes.contains(".hide_vert")) {
    /* If everything is already visible, don't do anything.*/
    return;
  }

  const bool value = action_to_hide(action);
  switch (area) {
    case VisArea::Inside:
    case VisArea::Outside: {
      const Span<float3> positions = BKE_pbvh_get_vert_positions(&pbvh);
      vert_hide_update(object, nodes, [&](const Span<int> verts, MutableSpan<bool> hide) {
        for (const int i : verts.index_range()) {
          if (isect_point_planes_v3(planes, 4, positions[verts[i]])) {
            hide[i] = value;
          }
        }
      });
      break;
    }
    case VisArea::All:
      switch (action) {
        case VisAction::Hide:
          vert_hide_update(object, nodes, [&](const Span<int> /*verts*/, MutableSpan<bool> hide) {
            hide.fill(true);
          });
          break;
        case VisAction::Show:
          vert_show_all(object, nodes);
          break;
      }
      break;
    case VisArea::Masked:
      const VArraySpan<float> mask = *attributes.lookup<float>(".sculpt_mask", ATTR_DOMAIN_POINT);
      if (action == VisAction::Show && mask.is_empty()) {
        vert_show_all(object, nodes);
      }
      else {
        vert_hide_update(object, nodes, [&](const Span<int> verts, MutableSpan<bool> hide) {
          for (const int i : verts.index_range()) {
            if (mask[verts[i]] > 0.5f) {
              hide[i] = value;
            }
          }
        });
      }
      break;
  }

  BKE_mesh_flush_hidden_from_verts(&mesh);
  BKE_pbvh_update_hide_attributes_from_mesh(&pbvh);
}

/* Hide or show elements in multires grids with a special GridFlags
 * customdata layer. */
static void partialvis_update_grids(Depsgraph *depsgraph,
                                    Object *ob,
                                    PBVH *pbvh,
                                    const VisAction action,
                                    const VisArea area,
                                    const float planes[4][4],
                                    const Span<PBVHNode *> nodes)
{
  for (PBVHNode *node : nodes) {
    CCGElem *const *grids;
    const int *grid_indices;
    int totgrid;
    bool any_changed = false, any_visible = false;

    /* Get PBVH data. */
    BKE_pbvh_node_get_grids(pbvh, node, &grid_indices, &totgrid, nullptr, nullptr, &grids);

    SculptSession *ss = ob->sculpt;
    SubdivCCG *subdiv_ccg = ss->subdiv_ccg;
    MutableSpan<BLI_bitmap *> grid_hidden = subdiv_ccg->grid_hidden;
    const CCGKey key = *BKE_pbvh_get_grid_key(pbvh);

    SCULPT_undo_push_node(ob, node, SCULPT_UNDO_HIDDEN);

    for (int i = 0; i < totgrid; i++) {
      int any_hidden = 0;
      const int grid_index = grid_indices[i];
      BLI_bitmap *gh = grid_hidden[grid_index];
      if (!gh) {
        switch (action) {
          case VisAction::Hide:
            /* Create grid flags data. */
            gh = grid_hidden[grid_index] = BLI_BITMAP_NEW(key.grid_area,
                                                          "partialvis_update_grids");
            break;
          case VisAction::Show:
            /* Entire grid is visible, nothing to show. */
            continue;
        }
      }
      else if (action == VisAction::Show && area == VisArea::All) {
        /* Special case if we're showing all, just free the grid. */
        MEM_freeN(gh);
        grid_hidden[grid_index] = nullptr;
        any_changed = true;
        any_visible = true;
        continue;
      }

      for (int y = 0; y < key.grid_size; y++) {
        for (int x = 0; x < key.grid_size; x++) {
          CCGElem *elem = CCG_grid_elem(&key, grids[grid_index], x, y);
          const float *co = CCG_elem_co(&key, elem);
          float mask = key.has_mask ? *CCG_elem_mask(&key, elem) : 0.0f;

          /* Skip grid element if not in the effected area. */
          if (is_effected(area, planes, co, mask)) {
            /* Set or clear the hide flag. */
            BLI_BITMAP_SET(gh, y * key.grid_size + x, action == VisAction::Hide);

            any_changed = true;
          }

          /* Keep track of whether any elements are still hidden. */
          if (BLI_BITMAP_TEST(gh, y * key.grid_size + x)) {
            any_hidden = true;
          }
          else {
            any_visible = true;
          }
        }
      }

      /* If everything in the grid is now visible, free the grid flags. */
      if (!any_hidden) {
        MEM_freeN(gh);
        grid_hidden[grid_index] = nullptr;
      }
    }

    /* Mark updates if anything was hidden/shown. */
    if (any_changed) {
      BKE_pbvh_node_mark_rebuild_draw(node);
      BKE_pbvh_node_fully_hidden_set(node, !any_visible);
      multires_mark_as_modified(depsgraph, ob, MULTIRES_HIDDEN_MODIFIED);
    }
  }

  BKE_pbvh_sync_visibility_from_verts(pbvh, static_cast<Mesh *>(ob->data));
}

static void partialvis_update_bmesh_verts(BMesh *bm,
                                          const Set<BMVert *, 0> &verts,
                                          const VisAction action,
                                          const VisArea area,
                                          const float planes[4][4],
                                          bool *any_changed,
                                          bool *any_visible)
{
  const int mask_offset = CustomData_get_offset_named(&bm->vdata, CD_PROP_FLOAT, ".sculpt_mask");
  for (BMVert *v : verts) {
    const float vmask = BM_ELEM_CD_GET_FLOAT(v, mask_offset);

    /* Hide vertex if in the hide volume. */
    if (is_effected(area, planes, v->co, vmask)) {
      if (action == VisAction::Hide) {
        BM_elem_flag_enable(v, BM_ELEM_HIDDEN);
      }
      else {
        BM_elem_flag_disable(v, BM_ELEM_HIDDEN);
      }
      (*any_changed) = true;
    }

    if (!BM_elem_flag_test(v, BM_ELEM_HIDDEN)) {
      (*any_visible) = true;
    }
  }
}

static void partialvis_update_bmesh_faces(const Set<BMFace *, 0> &faces)
{
  for (BMFace *f : faces) {
    if (paint_is_bmesh_face_hidden(f)) {
      BM_elem_flag_enable(f, BM_ELEM_HIDDEN);
    }
    else {
      BM_elem_flag_disable(f, BM_ELEM_HIDDEN);
    }
  }
}

static void partialvis_update_bmesh(Object *ob,
                                    PBVH *pbvh,
                                    const VisAction action,
                                    const VisArea area,
                                    const float planes[4][4],
                                    const Span<PBVHNode *> nodes)
{
  for (PBVHNode *node : nodes) {
    bool any_changed = false;
    bool any_visible = false;

    BMesh *bm = BKE_pbvh_get_bmesh(pbvh);

    SCULPT_undo_push_node(ob, node, SCULPT_UNDO_HIDDEN);

    partialvis_update_bmesh_verts(bm,
                                  BKE_pbvh_bmesh_node_unique_verts(node),
                                  action,
                                  area,
                                  planes,
                                  &any_changed,
                                  &any_visible);

    partialvis_update_bmesh_verts(bm,
                                  BKE_pbvh_bmesh_node_other_verts(node),
                                  action,
                                  area,
                                  planes,
                                  &any_changed,
                                  &any_visible);

    /* Finally loop over node faces and tag the ones that are fully hidden. */
    partialvis_update_bmesh_faces(BKE_pbvh_bmesh_node_faces(node));

    if (any_changed) {
      BKE_pbvh_node_mark_rebuild_draw(node);
      BKE_pbvh_node_fully_hidden_set(node, !any_visible);
    }
  }
}

static void rect_from_props(rcti *rect, PointerRNA *ptr)
{
  rect->xmin = RNA_int_get(ptr, "xmin");
  rect->ymin = RNA_int_get(ptr, "ymin");
  rect->xmax = RNA_int_get(ptr, "xmax");
  rect->ymax = RNA_int_get(ptr, "ymax");
}

static void clip_planes_from_rect(bContext *C,
                                  Depsgraph *depsgraph,
                                  float clip_planes[4][4],
                                  const rcti *rect)
{
  view3d_operator_needs_opengl(C);
  ViewContext vc = ED_view3d_viewcontext_init(C, depsgraph);
  BoundBox bb;
  ED_view3d_clipping_calc(&bb, clip_planes, vc.region, vc.obact, rect);
}

/* If mode is inside, get all PBVH nodes that lie at least partially
 * inside the clip_planes volume. If mode is outside, get all nodes
 * that lie at least partially outside the volume. If showing all, get
 * all nodes. */
static Vector<PBVHNode *> get_pbvh_nodes(PBVH *pbvh,
                                         const float clip_planes[4][4],
                                         const VisArea area)
{
  PBVHFrustumPlanes frustum{};
  frustum.planes = const_cast<float(*)[4]>(clip_planes);
  frustum.num_planes = 4;
  return blender::bke::pbvh::search_gather(pbvh, [&](PBVHNode &node) {
    switch (area) {
      case VisArea::Inside:
        return BKE_pbvh_node_frustum_contain_AABB(&node, &frustum);
      case VisArea::Outside:
        return BKE_pbvh_node_frustum_exclude_AABB(&node, &frustum);
      case VisArea::All:
      case VisArea::Masked:
        return true;
    }
    BLI_assert_unreachable();
    return true;
  });
}

static int hide_show_exec(bContext *C, wmOperator *op)
{
  ARegion *region = CTX_wm_region(C);
  Object *ob = CTX_data_active_object(C);
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);

  /* Read operator properties. */
  const VisAction action = VisAction(RNA_enum_get(op->ptr, "action"));
  const VisArea area = VisArea(RNA_enum_get(op->ptr, "area"));

  rcti rect;
  rect_from_props(&rect, op->ptr);

  float clip_planes[4][4];
  clip_planes_from_rect(C, depsgraph, clip_planes, &rect);

  PBVH *pbvh = BKE_sculpt_object_pbvh_ensure(depsgraph, ob);
  BLI_assert(BKE_object_sculpt_pbvh_get(ob) == pbvh);

  Vector<PBVHNode *> nodes = get_pbvh_nodes(pbvh, clip_planes, area);
  const PBVHType pbvh_type = BKE_pbvh_type(pbvh);

  negate_m4(clip_planes);

  /* Start undo. */
  switch (action) {
    case VisAction::Hide:
      SCULPT_undo_push_begin_ex(ob, "Hide area");
      break;
    case VisAction::Show:
      SCULPT_undo_push_begin_ex(ob, "Show area");
      break;
  }

  switch (pbvh_type) {
    case PBVH_FACES:
      partialvis_update_mesh(*ob, *pbvh, action, area, clip_planes, nodes);
      break;
    case PBVH_GRIDS:
      partialvis_update_grids(depsgraph, ob, pbvh, action, area, clip_planes, nodes);
      break;
    case PBVH_BMESH:
      partialvis_update_bmesh(ob, pbvh, action, area, clip_planes, nodes);
      break;
  }

  /* End undo. */
  SCULPT_undo_push_end(ob);

  SCULPT_topology_islands_invalidate(ob->sculpt);

  RegionView3D *rv3d = CTX_wm_region_view3d(C);
  if (!BKE_sculptsession_use_pbvh_draw(ob, rv3d)) {
    DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  }

  DEG_id_tag_update(&ob->id, ID_RECALC_SHADING);
  ED_region_tag_redraw(region);

  return OPERATOR_FINISHED;
}

static int hide_show_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  const VisArea area = VisArea(RNA_enum_get(op->ptr, "area"));
  if (!ELEM(area, VisArea::All, VisArea::Masked)) {
    return WM_gesture_box_invoke(C, op, event);
  }
  return op->type->exec(C, op);
}

void PAINT_OT_hide_show(wmOperatorType *ot)
{
  static const EnumPropertyItem action_items[] = {
      {int(VisAction::Hide), "HIDE", 0, "Hide", "Hide vertices"},
      {int(VisAction::Show), "SHOW", 0, "Show", "Show vertices"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem area_items[] = {
      {int(VisArea::Outside),
       "OUTSIDE",
       0,
       "Outside",
       "Hide or show vertices outside the selection"},
      {int(VisArea::Inside), "INSIDE", 0, "Inside", "Hide or show vertices inside the selection"},
      {int(VisArea::All), "ALL", 0, "All", "Hide or show all vertices"},
      {int(VisArea::Masked),
       "MASKED",
       0,
       "Masked",
       "Hide or show vertices that are masked (minimum mask value of 0.5)"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  ot->name = "Hide/Show";
  ot->idname = "PAINT_OT_hide_show";
  ot->description = "Hide/show some vertices";

  ot->invoke = hide_show_invoke;
  ot->modal = WM_gesture_box_modal;
  ot->exec = hide_show_exec;
  /* Sculpt-only for now. */
  ot->poll = SCULPT_mode_poll_view3d;

  ot->flag = OPTYPE_REGISTER;

  RNA_def_enum(ot->srna,
               "action",
               action_items,
               int(VisAction::Hide),
               "VisAction",
               "Whether to hide or show vertices");
  RNA_def_enum(
      ot->srna, "area", area_items, VisArea::Inside, "VisArea", "Which vertices to hide or show");
  WM_operator_properties_border(ot);
}

}  // namespace blender::ed::sculpt_paint::hide
