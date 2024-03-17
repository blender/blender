/* SPDX-FileCopyrightText: 2010 by Nicholas Bishop. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edsculpt
 * Implements the PBVH node hiding operator.
 */

#include "MEM_guardedalloc.h"

#include "BLI_array_utils.hh"
#include "BLI_bit_span_ops.hh"
#include "BLI_enumerable_thread_specific.hh"
#include "BLI_math_geom.h"
#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"
#include "BLI_utildefines.h"
#include "BLI_vector.hh"

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

#include "bmesh.hh"

#include "paint_intern.hh"

/* For undo push. */
#include "sculpt_intern.hh"

using blender::Vector;
using blender::bke::dyntopo::DyntopoSet;
namespace blender::ed::sculpt_paint::hide {

void sync_all_from_faces(Object &object)
{
  SculptSession &ss = *object.sculpt;
  Mesh &mesh = *static_cast<Mesh *>(object.data);

  SCULPT_topology_islands_invalidate(&ss);

  switch (BKE_pbvh_type(ss.pbvh)) {
    case PBVH_FACES: {
      /* We may have adjusted the ".hide_poly" attribute, now make the hide status attributes for
       * vertices and edges consistent. */
      bke::mesh_hide_face_flush(mesh);
      break;
    }
    case PBVH_GRIDS: {
      /* In addition to making the hide status of the base mesh consistent, we also have to
       * propagate the status to the Multires grids. */
      bke::mesh_hide_face_flush(mesh);
      BKE_sculpt_sync_face_visibility_to_grids(&mesh, ss.subdiv_ccg);
      break;
    }
    case PBVH_BMESH: {
      BMesh &bm = *ss.bm;
      BMIter iter;
      BMFace *f;

      /* Hide all verts and edges attached to faces. */
      BM_ITER_MESH (f, &iter, &bm, BM_FACES_OF_MESH) {
        BMLoop *l = f->l_first;
        do {
          BM_elem_flag_enable(l->v, BM_ELEM_HIDDEN);
          BM_elem_flag_enable(l->e, BM_ELEM_HIDDEN);
        } while ((l = l->next) != f->l_first);
      }

      /* Unhide verts and edges attached to visible faces. */
      BM_ITER_MESH (f, &iter, &bm, BM_FACES_OF_MESH) {
        if (BM_elem_flag_test(f, BM_ELEM_HIDDEN)) {
          continue;
        }

        BMLoop *l = f->l_first;
        do {
          BM_elem_flag_disable(l->v, BM_ELEM_HIDDEN);
          BM_elem_flag_disable(l->e, BM_ELEM_HIDDEN);
        } while ((l = l->next) != f->l_first);
      }
      break;
    }
  }
}

void tag_update_visibility(const bContext &C)
{
  ARegion *region = CTX_wm_region(&C);
  ED_region_tag_redraw(region);

  Object *ob = CTX_data_active_object(&C);
  WM_event_add_notifier(&C, NC_OBJECT | ND_DRAW, ob);

  DEG_id_tag_update(&ob->id, ID_RECALC_SHADING);
  const RegionView3D *rv3d = CTX_wm_region_view3d(&C);
  if (!BKE_sculptsession_use_pbvh_draw(ob, rv3d)) {
    DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  }
}

enum class VisAction {
  Hide = 0,
  Show = 1,
};

struct HideShowOperation {
  gesture::Operation op;

  VisAction action;
};

static bool action_to_hide(const VisAction action)
{
  return action == VisAction::Hide;
}

void mesh_show_all(Object &object, const Span<PBVHNode *> nodes)
{
  Mesh &mesh = *static_cast<Mesh *>(object.data);
  bke::MutableAttributeAccessor attributes = mesh.attributes_for_write();
  if (const VArray<bool> attribute = *attributes.lookup<bool>(".hide_vert",
                                                              bke::AttrDomain::Point))
  {
    const VArraySpan hide_vert(attribute);
    threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
      for (PBVHNode *node : nodes.slice(range)) {
        const Span<int> verts = BKE_pbvh_node_get_vert_indices(node);
        if (std::any_of(verts.begin(), verts.end(), [&](const int i) { return hide_vert[i]; })) {
          undo::push_node(&object, node, undo::Type::HideVert);
          BKE_pbvh_node_mark_rebuild_draw(node);
        }
      }
    });
  }
  for (PBVHNode *node : nodes) {
    BKE_pbvh_node_fully_hidden_set(node, false);
  }
  attributes.remove(".hide_vert");
  bke::mesh_hide_vert_flush(mesh);
}

static void vert_hide_update(Object &object,
                             const Span<PBVHNode *> nodes,
                             const FunctionRef<void(Span<int>, MutableSpan<bool>)> calc_hide)
{
  Mesh &mesh = *static_cast<Mesh *>(object.data);
  bke::MutableAttributeAccessor attributes = mesh.attributes_for_write();
  bke::SpanAttributeWriter<bool> hide_vert = attributes.lookup_or_add_for_write_span<bool>(
      ".hide_vert", bke::AttrDomain::Point);

  bool any_changed = false;
  threading::EnumerableThreadSpecific<Vector<bool>> all_new_hide;
  threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
    Vector<bool> &new_hide = all_new_hide.local();
    for (PBVHNode *node : nodes.slice(range)) {
      const Span<int> verts = BKE_pbvh_node_get_unique_vert_indices(node);

      new_hide.reinitialize(verts.size());
      array_utils::gather(hide_vert.span.as_span(), verts, new_hide.as_mutable_span());
      calc_hide(verts, new_hide);
      if (!array_utils::indexed_data_equal<bool>(hide_vert.span, verts, new_hide)) {
        continue;
      }

      any_changed = true;
      undo::push_node(&object, node, undo::Type::HideVert);
      array_utils::scatter(new_hide.as_span(), verts, hide_vert.span);

      BKE_pbvh_node_mark_update_visibility(node);
      bke::pbvh::node_update_visibility_mesh(hide_vert.span, *node);
    }
  });

  hide_vert.finish();
  if (any_changed) {
    bke::mesh_hide_vert_flush(mesh);
  }
}

static void partialvis_all_update_mesh(Object &object,
                                       const VisAction action,
                                       const Span<PBVHNode *> nodes)
{
  Mesh &mesh = *static_cast<Mesh *>(object.data);
  bke::MutableAttributeAccessor attributes = mesh.attributes_for_write();
  if (action == VisAction::Show && !attributes.contains(".hide_vert")) {
    /* If everything is already visible, don't do anything. */
    return;
  }

  switch (action) {
    case VisAction::Hide:
      vert_hide_update(object, nodes, [&](const Span<int> /*verts*/, MutableSpan<bool> hide) {
        hide.fill(true);
      });
      break;
    case VisAction::Show:
      mesh_show_all(object, nodes);
      break;
  }
}

static void partialvis_masked_update_mesh(Object &object,
                                          const VisAction action,
                                          const Span<PBVHNode *> nodes)
{
  Mesh &mesh = *static_cast<Mesh *>(object.data);
  bke::MutableAttributeAccessor attributes = mesh.attributes_for_write();
  if (action == VisAction::Show && !attributes.contains(".hide_vert")) {
    /* If everything is already visible, don't do anything. */
    return;
  }

  const bool value = action_to_hide(action);
  const VArraySpan<float> mask = *attributes.lookup<float>(".sculpt_mask", bke::AttrDomain::Point);
  if (action == VisAction::Show && mask.is_empty()) {
    mesh_show_all(object, nodes);
  }
  else if (!mask.is_empty()) {
    vert_hide_update(object, nodes, [&](const Span<int> verts, MutableSpan<bool> hide) {
      for (const int i : verts.index_range()) {
        if (mask[verts[i]] > 0.5f) {
          hide[i] = value;
        }
      }
    });
  }
}

static void partialvis_gesture_update_mesh(gesture::GestureData &gesture_data)
{
  HideShowOperation *operation = reinterpret_cast<HideShowOperation *>(gesture_data.operation);
  Object *object = gesture_data.vc.obact;
  const VisAction action = operation->action;
  const Span<PBVHNode *> nodes = gesture_data.nodes;

  PBVH *pbvh = object->sculpt->pbvh;
  Mesh *mesh = static_cast<Mesh *>(object->data);
  bke::MutableAttributeAccessor attributes = mesh->attributes_for_write();
  if (action == VisAction::Show && !attributes.contains(".hide_vert")) {
    /* If everything is already visible, don't do anything. */
    return;
  }

  const bool value = action_to_hide(action);
  const Span<float3> positions = BKE_pbvh_get_vert_positions(pbvh);
  const Span<float3> normals = BKE_pbvh_get_vert_normals(pbvh);
  vert_hide_update(*object, nodes, [&](const Span<int> verts, MutableSpan<bool> hide) {
    for (const int i : verts.index_range()) {
      if (gesture::is_affected(gesture_data, positions[verts[i]], normals[verts[i]])) {
        hide[i] = value;
      }
    }
  });
}

void grids_show_all(Depsgraph &depsgraph, Object &object, const Span<PBVHNode *> nodes)
{
  Mesh &mesh = *static_cast<Mesh *>(object.data);
  PBVH &pbvh = *object.sculpt->pbvh;
  SubdivCCG &subdiv_ccg = *object.sculpt->subdiv_ccg;
  const BitGroupVector<> &grid_hidden = subdiv_ccg.grid_hidden;
  bool any_changed = false;
  if (!grid_hidden.is_empty()) {
    threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
      for (PBVHNode *node : nodes.slice(range)) {
        const Span<int> grids = BKE_pbvh_node_get_grid_indices(*node);
        if (std::any_of(grids.begin(), grids.end(), [&](const int i) {
              return bits::any_bit_set(grid_hidden[i]);
            }))
        {
          any_changed = true;
          undo::push_node(&object, node, undo::Type::HideVert);
          BKE_pbvh_node_mark_rebuild_draw(node);
        }
      }
    });
  }
  if (!any_changed) {
    return;
  }
  for (PBVHNode *node : nodes) {
    BKE_pbvh_node_fully_hidden_set(node, false);
  }
  BKE_subdiv_ccg_grid_hidden_free(subdiv_ccg);
  BKE_pbvh_sync_visibility_from_verts(&pbvh, &mesh);
  multires_mark_as_modified(&depsgraph, &object, MULTIRES_HIDDEN_MODIFIED);
}

static void grid_hide_update(Depsgraph &depsgraph,
                             Object &object,
                             const Span<PBVHNode *> nodes,
                             const FunctionRef<void(const int, MutableBoundedBitSpan)> calc_hide)
{
  Mesh &mesh = *static_cast<Mesh *>(object.data);
  PBVH &pbvh = *object.sculpt->pbvh;
  SubdivCCG &subdiv_ccg = *object.sculpt->subdiv_ccg;
  BitGroupVector<> &grid_hidden = BKE_subdiv_ccg_grid_hidden_ensure(subdiv_ccg);

  bool any_changed = false;
  threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
    for (PBVHNode *node : nodes.slice(range)) {
      const Span<int> grids = BKE_pbvh_node_get_grid_indices(*node);
      BitGroupVector<> new_hide(grids.size(), grid_hidden.group_size());
      for (const int i : grids.index_range()) {
        new_hide[i].copy_from(grid_hidden[grids[i]].as_span());
      }

      for (const int i : grids.index_range()) {
        calc_hide(grids[i], new_hide[i]);
      }

      if (std::all_of(grids.index_range().begin(), grids.index_range().end(), [&](const int i) {
            return bits::spans_equal(grid_hidden[grids[i]], new_hide[i]);
          }))
      {
        continue;
      }

      any_changed = true;
      undo::push_node(&object, node, undo::Type::HideVert);

      for (const int i : grids.index_range()) {
        grid_hidden[grids[i]].copy_from(new_hide[i].as_span());
      }

      BKE_pbvh_node_mark_update_visibility(node);
      bke::pbvh::node_update_visibility_grids(grid_hidden, *node);
    }
  });

  if (any_changed) {
    multires_mark_as_modified(&depsgraph, &object, MULTIRES_HIDDEN_MODIFIED);
    BKE_pbvh_sync_visibility_from_verts(&pbvh, &mesh);
  }
}

static void partialvis_masked_update_grids(Depsgraph &depsgraph,
                                           Object &object,
                                           const VisAction action,
                                           const Span<PBVHNode *> nodes)
{
  PBVH &pbvh = *object.sculpt->pbvh;
  SubdivCCG &subdiv_ccg = *object.sculpt->subdiv_ccg;

  const bool value = action_to_hide(action);
  const CCGKey key = *BKE_pbvh_get_grid_key(&pbvh);
  const Span<CCGElem *> grids = subdiv_ccg.grids;
  if (!key.has_mask) {
    grid_hide_update(depsgraph,
                     object,
                     nodes,
                     [&](const int /*verts*/, MutableBoundedBitSpan hide) { hide.fill(value); });
  }
  else {
    grid_hide_update(
        depsgraph, object, nodes, [&](const int grid_index, MutableBoundedBitSpan hide) {
          CCGElem *grid = grids[grid_index];
          for (const int y : IndexRange(key.grid_size)) {
            for (const int x : IndexRange(key.grid_size)) {
              CCGElem *elem = CCG_grid_elem(&key, grid, x, y);
              if (*CCG_elem_mask(&key, elem) > 0.5f) {
                hide[y * key.grid_size + x].set(value);
              }
            }
          }
        });
  }
}

static void partialvis_gesture_update_grids(Depsgraph &depsgraph,
                                            gesture::GestureData &gesture_data)
{
  HideShowOperation *operation = reinterpret_cast<HideShowOperation *>(gesture_data.operation);
  Object *object = gesture_data.vc.obact;
  const VisAction action = operation->action;
  const Span<PBVHNode *> nodes = gesture_data.nodes;

  PBVH *pbvh = object->sculpt->pbvh;
  SubdivCCG *subdiv_ccg = object->sculpt->subdiv_ccg;

  const bool value = action_to_hide(action);
  const CCGKey key = *BKE_pbvh_get_grid_key(pbvh);
  const Span<CCGElem *> grids = subdiv_ccg->grids;
  grid_hide_update(
      depsgraph, *object, nodes, [&](const int grid_index, MutableBoundedBitSpan hide) {
        CCGElem *grid = grids[grid_index];
        for (const int y : IndexRange(key.grid_size)) {
          for (const int x : IndexRange(key.grid_size)) {
            CCGElem *elem = CCG_grid_elem(&key, grid, x, y);
            if (gesture::is_affected(
                    gesture_data, CCG_elem_co(&key, elem), CCG_elem_no(&key, elem))) {
              hide[y * key.grid_size + x].set(value);
            }
          }
        }
      });
}
static void partialvis_all_update_grids(Depsgraph &depsgraph,
                                        Object &object,
                                        const VisAction action,
                                        const Span<PBVHNode *> nodes)
{
  switch (action) {
    case VisAction::Hide:
      grid_hide_update(depsgraph,
                       object,
                       nodes,
                       [&](const int /*verts*/, MutableBoundedBitSpan hide) { hide.fill(true); });
      break;
    case VisAction::Show:
      grids_show_all(depsgraph, object, nodes);
      break;
  }
}

static void partialvis_update_bmesh_verts(DyntopoSet<BMVert> &verts,
                                          const VisAction action,
                                          const FunctionRef<bool(const BMVert *v)> should_update,
                                          bool *any_changed,
                                          bool *any_visible)
{
  for (BMVert *v : verts) {
    if (should_update(v)) {
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

static void partialvis_update_bmesh_faces(DyntopoSet<BMFace> &faces)
{
  for (BMFace *f : faces) {
    bool hidden = false;

    BMLoop *l = f->l_first;
    do {
      hidden |= bool(BM_elem_flag_test(l->v, BM_ELEM_HIDDEN));
    } while ((l = l->next) != f->l_first);

    if (hidden) {
      BM_elem_flag_enable(f, BM_ELEM_HIDDEN);
    }
    else {
      BM_elem_flag_disable(f, BM_ELEM_HIDDEN);
    }
  }
}

static void partialvis_update_bmesh_nodes(Object *ob,
                                          const Span<PBVHNode *> nodes,
                                          const VisAction action,
                                          const FunctionRef<bool(const BMVert *v)> vert_test_fn)
{
  DyntopoSet<BMVert> *unique, *other;

  for (PBVHNode *node : nodes) {
    unique = &BKE_pbvh_bmesh_node_unique_verts(node);
    other = &BKE_pbvh_bmesh_node_other_verts(node);

    bool any_changed = false;
    bool any_visible = false;

    undo::push_node(ob, node, undo::Type::HideVert);

    partialvis_update_bmesh_verts(*unique, action, vert_test_fn, &any_changed, &any_visible);
    partialvis_update_bmesh_verts(*other, action, vert_test_fn, &any_changed, &any_visible);

    /* Finally loop over node faces and tag the ones that are fully hidden. */
    partialvis_update_bmesh_faces(BKE_pbvh_bmesh_node_faces(node));

    if (any_changed) {
      BKE_pbvh_node_mark_rebuild_draw(node);
      BKE_pbvh_node_fully_hidden_set(node, !any_visible);
      BKE_pbvh_vert_tag_update_normal_triangulation(node);
    }
  }
}

static void partialvis_masked_update_bmesh(Object *ob,
                                           PBVH *pbvh,
                                           const VisAction action,
                                           const Span<PBVHNode *> nodes)
{
  BMesh *bm = BKE_pbvh_get_bmesh(pbvh);
  const int mask_offset = CustomData_get_offset_named(&bm->vdata, CD_PROP_FLOAT, ".sculpt_mask");
  const auto mask_test_fn = [&](const BMVert *v) {
    const float vmask = BM_ELEM_CD_GET_FLOAT(v, mask_offset);
    return vmask > 0.5f;
  };

  partialvis_update_bmesh_nodes(ob, nodes, action, mask_test_fn);
}

static void partialvis_all_update_bmesh(Object *ob,
                                        const VisAction action,
                                        const Span<PBVHNode *> nodes)
{
  partialvis_update_bmesh_nodes(ob, nodes, action, [](const BMVert * /*vert*/) { return true; });
}

static void partialvis_gesture_update_bmesh(gesture::GestureData &gesture_data)
{
  const auto selection_test_fn = [&](const BMVert *v) {
    return gesture::is_affected(gesture_data, v->co, v->no);
  };

  HideShowOperation *operation = reinterpret_cast<HideShowOperation *>(gesture_data.operation);

  partialvis_update_bmesh_nodes(
      gesture_data.vc.obact, gesture_data.nodes, operation->action, selection_test_fn);
}

static void hide_show_begin(bContext &C, gesture::GestureData & /*gesture_data*/)
{
  Object *ob = CTX_data_active_object(&C);
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(&C);

  BKE_sculpt_object_pbvh_ensure(depsgraph, ob);
}

static void hide_show_apply_for_symmetry_pass(bContext &C, gesture::GestureData &gesture_data)
{
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(&C);

  switch (BKE_pbvh_type(gesture_data.ss->pbvh)) {
    case PBVH_FACES:
      partialvis_gesture_update_mesh(gesture_data);
      break;
    case PBVH_GRIDS:
      partialvis_gesture_update_grids(*depsgraph, gesture_data);
      break;
    case PBVH_BMESH:
      partialvis_gesture_update_bmesh(gesture_data);
      break;
  }
}
static void hide_show_end(bContext &C, gesture::GestureData &gesture_data)
{
  SCULPT_topology_islands_invalidate(gesture_data.vc.obact->sculpt);
  tag_update_visibility(C);
}

static void hide_show_init_properties(bContext & /*C*/,
                                      gesture::GestureData &gesture_data,
                                      wmOperator &op)
{
  gesture_data.operation = reinterpret_cast<gesture::Operation *>(
      MEM_cnew<HideShowOperation>(__func__));

  HideShowOperation *operation = reinterpret_cast<HideShowOperation *>(gesture_data.operation);

  operation->op.begin = hide_show_begin;
  operation->op.apply_for_symmetry_pass = hide_show_apply_for_symmetry_pass;
  operation->op.end = hide_show_end;

  operation->action = VisAction(RNA_enum_get(op.ptr, "action"));
  gesture_data.selection_type = gesture::SelectionType(RNA_enum_get(op.ptr, "area"));
}

static int hide_show_all_exec(bContext *C, wmOperator *op)
{
  Object *ob = CTX_data_active_object(C);
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);

  const VisAction action = VisAction(RNA_enum_get(op->ptr, "action"));

  PBVH *pbvh = BKE_sculpt_object_pbvh_ensure(depsgraph, ob);
  BKE_sculpt_hide_poly_ensure(ob);

  /* Start undo. */
  switch (action) {
    case VisAction::Hide:
      undo::push_begin_ex(ob, "Hide area");
      break;
    case VisAction::Show:
      undo::push_begin_ex(ob, "Show area");
      break;
  }

  Vector<PBVHNode *> nodes = bke::pbvh::search_gather(pbvh, {});

  switch (BKE_pbvh_type(pbvh)) {
    case PBVH_FACES:
      partialvis_all_update_mesh(*ob, action, nodes);
      break;
    case PBVH_GRIDS:
      partialvis_all_update_grids(*depsgraph, *ob, action, nodes);
      break;
    case PBVH_BMESH:
      partialvis_all_update_bmesh(ob, action, nodes);
      break;
  }

  /* End undo. */
  undo::push_end(ob);

  SCULPT_topology_islands_invalidate(ob->sculpt);
  tag_update_visibility(*C);

  return OPERATOR_FINISHED;
}

static int hide_show_masked_exec(bContext *C, wmOperator *op)
{
  Object *ob = CTX_data_active_object(C);
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);

  const VisAction action = VisAction(RNA_enum_get(op->ptr, "action"));

  PBVH *pbvh = BKE_sculpt_object_pbvh_ensure(depsgraph, ob);
  BLI_assert(BKE_object_sculpt_pbvh_get(ob) == pbvh);

  /* Start undo. */
  switch (action) {
    case VisAction::Hide:
      undo::push_begin_ex(ob, "Hide area");
      break;
    case VisAction::Show:
      undo::push_begin_ex(ob, "Show area");
      break;
  }

  Vector<PBVHNode *> nodes = bke::pbvh::search_gather(pbvh, {});

  switch (BKE_pbvh_type(pbvh)) {
    case PBVH_FACES:
      partialvis_masked_update_mesh(*ob, action, nodes);
      break;
    case PBVH_GRIDS:
      partialvis_masked_update_grids(*depsgraph, *ob, action, nodes);
      break;
    case PBVH_BMESH:
      partialvis_masked_update_bmesh(ob, pbvh, action, nodes);
      break;
  }

  /* End undo. */
  undo::push_end(ob);

  SCULPT_topology_islands_invalidate(ob->sculpt);
  tag_update_visibility(*C);

  return OPERATOR_FINISHED;
}

static int hide_show_gesture_box_exec(bContext *C, wmOperator *op)
{
  std::unique_ptr<gesture::GestureData> gesture_data = gesture::init_from_box(C, op);
  if (!gesture_data) {
    return OPERATOR_CANCELLED;
  }
  hide_show_init_properties(*C, *gesture_data, *op);
  gesture::apply(*C, *gesture_data, *op);
  return OPERATOR_FINISHED;
}

static int hide_show_gesture_lasso_exec(bContext *C, wmOperator *op)
{
  std::unique_ptr<gesture::GestureData> gesture_data = gesture::init_from_lasso(C, op);
  if (!gesture_data) {
    return OPERATOR_CANCELLED;
  }
  hide_show_init_properties(*C, *gesture_data, *op);
  gesture::apply(*C, *gesture_data, *op);
  return OPERATOR_FINISHED;
}

static void hide_show_operator_properties(wmOperatorType *ot)
{
  static const EnumPropertyItem action_items[] = {
      {int(VisAction::Hide), "HIDE", 0, "Hide", "Hide vertices"},
      {int(VisAction::Show), "SHOW", 0, "Show", "Show vertices"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  RNA_def_enum(ot->srna,
               "action",
               action_items,
               int(VisAction::Hide),
               "Visibility Action",
               "Whether to hide or show vertices");
}

static void hide_show_operator_gesture_properties(wmOperatorType *ot)
{
  static const EnumPropertyItem area_items[] = {
      {int(gesture::SelectionType::Outside),
       "OUTSIDE",
       0,
       "Outside",
       "Hide or show vertices outside the selection"},
      {int(gesture::SelectionType::Inside),
       "Inside",
       0,
       "Inside",
       "Hide or show vertices inside the selection"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  RNA_def_enum(ot->srna,
               "area",
               area_items,
               int(gesture::SelectionType::Inside),
               "Visibility Area",
               "Which vertices to hide or show");
}

void PAINT_OT_hide_show_masked(wmOperatorType *ot)
{
  ot->name = "Hide/Show Masked";
  ot->idname = "PAINT_OT_hide_show_masked";
  ot->description = "Hide/show all masked vertices above a threshold";

  ot->exec = hide_show_masked_exec;
  /* Sculpt-only for now. */
  ot->poll = SCULPT_mode_poll_view3d;

  ot->flag = OPTYPE_REGISTER;

  hide_show_operator_properties(ot);
}

void PAINT_OT_hide_show_all(wmOperatorType *ot)
{
  ot->name = "Hide/Show All";
  ot->idname = "PAINT_OT_hide_show_all";
  ot->description = "Hide/show all vertices";

  ot->exec = hide_show_all_exec;
  /* Sculpt-only for now. */
  ot->poll = SCULPT_mode_poll_view3d;

  ot->flag = OPTYPE_REGISTER;

  hide_show_operator_properties(ot);
}

void PAINT_OT_hide_show(wmOperatorType *ot)
{
  ot->name = "Hide/Show";
  ot->idname = "PAINT_OT_hide_show";
  ot->description = "Hide/show some vertices";

  ot->invoke = WM_gesture_box_invoke;
  ot->modal = WM_gesture_box_modal;
  ot->exec = hide_show_gesture_box_exec;
  /* Sculpt-only for now. */
  ot->poll = SCULPT_mode_poll_view3d;

  ot->flag = OPTYPE_REGISTER;

  WM_operator_properties_border(ot);
  hide_show_operator_properties(ot);
  hide_show_operator_gesture_properties(ot);
  gesture::operator_properties(ot);
}

void PAINT_OT_hide_show_lasso_gesture(wmOperatorType *ot)
{
  ot->name = "Hide/Show Lasso";
  ot->idname = "PAINT_OT_hide_show_lasso_gesture";
  ot->description = "Hide/show some vertices";

  ot->invoke = WM_gesture_lasso_invoke;
  ot->modal = WM_gesture_lasso_modal;
  ot->exec = hide_show_gesture_lasso_exec;
  /* Sculpt-only for now. */
  ot->poll = SCULPT_mode_poll_view3d;

  ot->flag = OPTYPE_REGISTER | OPTYPE_DEPENDS_ON_CURSOR;

  WM_operator_properties_gesture_lasso(ot);
  hide_show_operator_properties(ot);
  hide_show_operator_gesture_properties(ot);
  gesture::operator_properties(ot);
}

static void invert_visibility_mesh(Object &object, const Span<PBVHNode *> nodes)
{
  Mesh &mesh = *static_cast<Mesh *>(object.data);
  bke::MutableAttributeAccessor attributes = mesh.attributes_for_write();
  bke::SpanAttributeWriter<bool> hide_vert = attributes.lookup_or_add_for_write_span<bool>(
      ".hide_vert", bke::AttrDomain::Point);

  threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
    for (PBVHNode *node : nodes.slice(range)) {
      undo::push_node(&object, node, undo::Type::HideVert);
      for (const int vert : BKE_pbvh_node_get_unique_vert_indices(node)) {
        hide_vert.span[vert] = !hide_vert.span[vert];
      }
      BKE_pbvh_node_mark_update_visibility(node);
      bke::pbvh::node_update_visibility_mesh(hide_vert.span, *node);
    }
  });

  hide_vert.finish();
  bke::mesh_hide_vert_flush(mesh);
}

static void invert_visibility_grids(Depsgraph &depsgraph,
                                    Object &object,
                                    const Span<PBVHNode *> nodes)
{
  Mesh &mesh = *static_cast<Mesh *>(object.data);
  PBVH &pbvh = *object.sculpt->pbvh;
  SubdivCCG &subdiv_ccg = *object.sculpt->subdiv_ccg;
  BitGroupVector<> &grid_hidden = BKE_subdiv_ccg_grid_hidden_ensure(subdiv_ccg);

  threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
    for (PBVHNode *node : nodes.slice(range)) {
      undo::push_node(&object, node, undo::Type::HideVert);
      for (const int i : BKE_pbvh_node_get_grid_indices(*node)) {
        bits::invert(grid_hidden[i]);
      }
      BKE_pbvh_node_mark_update_visibility(node);
      bke::pbvh::node_update_visibility_grids(grid_hidden, *node);
    }
  });

  multires_mark_as_modified(&depsgraph, &object, MULTIRES_HIDDEN_MODIFIED);
  BKE_pbvh_sync_visibility_from_verts(&pbvh, &mesh);
}

static void invert_visibility_bmesh(Object &object, const Span<PBVHNode *> nodes)
{
  threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
    for (PBVHNode *node : nodes.slice(range)) {
      undo::push_node(&object, node, undo::Type::HideVert);
      bool fully_hidden = true;
      for (BMVert *vert : BKE_pbvh_bmesh_node_unique_verts(node)) {
        BM_elem_flag_toggle(vert, BM_ELEM_HIDDEN);
        fully_hidden &= BM_elem_flag_test_bool(vert, BM_ELEM_HIDDEN);
      }
      BKE_pbvh_node_fully_hidden_set(node, fully_hidden);
      BKE_pbvh_node_mark_rebuild_draw(node);
    }
  });
  threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
    for (PBVHNode *node : nodes.slice(range)) {
      partialvis_update_bmesh_faces(BKE_pbvh_bmesh_node_faces(node));
    }
  });
}

static int visibility_invert_exec(bContext *C, wmOperator *op)
{
  Object &object = *CTX_data_active_object(C);
  Depsgraph &depsgraph = *CTX_data_ensure_evaluated_depsgraph(C);

  PBVH *pbvh = BKE_sculpt_object_pbvh_ensure(&depsgraph, &object);
  BLI_assert(BKE_object_sculpt_pbvh_get(&object) == pbvh);

  Vector<PBVHNode *> nodes = bke::pbvh::search_gather(pbvh, {});
  undo::push_begin(&object, op);
  switch (BKE_pbvh_type(pbvh)) {
    case PBVH_FACES:
      invert_visibility_mesh(object, nodes);
      break;
    case PBVH_GRIDS:
      invert_visibility_grids(depsgraph, object, nodes);
      break;
    case PBVH_BMESH:
      invert_visibility_bmesh(object, nodes);
      break;
  }

  undo::push_end(&object);

  SCULPT_topology_islands_invalidate(object.sculpt);
  tag_update_visibility(*C);

  return OPERATOR_FINISHED;
}

void PAINT_OT_visibility_invert(wmOperatorType *ot)
{
  ot->name = "Invert Visibility";
  ot->idname = "PAINT_OT_visibility_invert";
  ot->description = "Invert the visibility of all vertices";

  ot->modal = WM_gesture_box_modal;
  ot->exec = visibility_invert_exec;
  ot->poll = SCULPT_mode_poll_view3d;

  ot->flag = OPTYPE_REGISTER;
}

}  // namespace blender::ed::sculpt_paint::hide
