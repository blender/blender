/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spview3d
 */

#include <cfloat>
#include <cstring>
#include <optional>

#include "DNA_action_types.h"
#include "DNA_armature_types.h"
#include "DNA_curve_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_meta_types.h"
#include "DNA_object_types.h"
#include "DNA_pointcloud_types.h"
#include "DNA_scene_types.h"
#include "DNA_tracking_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_bitmap.h"
#include "BLI_function_ref.hh"
#include "BLI_lasso_2d.hh"
#include "BLI_listbase.h"
#include "BLI_math_bits.h"
#include "BLI_math_geom.h"
#include "BLI_rect.h"
#include "BLI_span.hh"
#include "BLI_string_utf8.h"
#include "BLI_task.hh"
#include "BLI_utildefines.h"
#include "BLI_vector.hh"

#include "BLT_translation.hh"

#include "BKE_action.hh"
#include "BKE_armature.hh"
#include "BKE_attribute.hh"
#include "BKE_context.hh"
#include "BKE_crazyspace.hh"
#include "BKE_curve.hh"
#include "BKE_curves.hh"
#include "BKE_editmesh.hh"
#include "BKE_global.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_layer.hh"
#include "BKE_main.hh"
#include "BKE_mball.hh"
#include "BKE_mesh.hh"
#include "BKE_object.hh"
#include "BKE_paint.hh"
#include "BKE_scene.hh"
#include "BKE_tracking.h"
#include "BKE_workspace.hh"

#include "WM_api.hh"
#include "WM_toolsystem.hh"
#include "WM_types.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_enum_types.hh"

#include "ED_armature.hh"
#include "ED_curve.hh"
#include "ED_curves.hh"
#include "ED_gpencil_legacy.hh"
#include "ED_grease_pencil.hh"
#include "ED_lattice.hh"
#include "ED_mball.hh"
#include "ED_mesh.hh"
#include "ED_object.hh"
#include "ED_outliner.hh"
#include "ED_particle.hh"
#include "ED_pointcloud.hh"
#include "ED_screen.hh"
#include "ED_sculpt.hh"
#include "ED_select_utils.hh"
#include "ED_uvedit.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "GPU_matrix.hh"
#include "GPU_select.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_query.hh"

#include "DRW_engine.hh"
#include "DRW_select_buffer.hh"

#include "ANIM_armature.hh"
#include "ANIM_bone_collections.hh"

#include "view3d_intern.hh" /* own include */

// #include "BLI_time_utildefines.h"

using blender::Array;
using blender::int2;
using blender::Span;
using blender::Vector;
using blender::ed::uv::UVSyncSelectFromMesh;

/* -------------------------------------------------------------------- */
/** \name Public Utilities
 * \{ */

float ED_view3d_select_dist_px()
{
  return 75.0f * U.pixelsize;
}

ViewContext ED_view3d_viewcontext_init(bContext *C, Depsgraph *depsgraph)
{
  /* TODO: should return whether there is valid context to continue. */
  ViewContext vc = {};
  vc.C = C;
  vc.region = CTX_wm_region(C);
  vc.bmain = CTX_data_main(C);
  vc.depsgraph = depsgraph;
  vc.scene = CTX_data_scene(C);
  vc.view_layer = CTX_data_view_layer(C);
  vc.v3d = CTX_wm_view3d(C);
  vc.win = CTX_wm_window(C);
  vc.rv3d = CTX_wm_region_view3d(C);
  vc.obact = CTX_data_active_object(C);
  vc.obedit = CTX_data_edit_object(C);
  return vc;
}

void ED_view3d_viewcontext_init_object(ViewContext *vc, Object *obact)
{
  vc->obact = obact;
  /* See public doc-string for rationale on checking the existing values first. */
  if (vc->obedit) {
    BLI_assert(BKE_object_is_in_editmode(obact));
    vc->obedit = obact;
    if (vc->em) {
      vc->em = BKE_editmesh_from_object(vc->obedit);
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Internal Object Utilities
 * \{ */

static bool object_deselect_all_visible(const Scene *scene, ViewLayer *view_layer, View3D *v3d)
{
  bool changed = false;
  BKE_view_layer_synced_ensure(scene, view_layer);
  LISTBASE_FOREACH (Base *, base, BKE_view_layer_object_bases_get(view_layer)) {
    if (base->flag & BASE_SELECTED) {
      if (BASE_SELECTABLE(v3d, base)) {
        blender::ed::object::base_select(base, blender::ed::object::BA_DESELECT);
        changed = true;
      }
    }
  }
  return changed;
}

/* deselect all except b */
static bool object_deselect_all_except(const Scene *scene, ViewLayer *view_layer, Base *b)
{
  bool changed = false;
  BKE_view_layer_synced_ensure(scene, view_layer);
  LISTBASE_FOREACH (Base *, base, BKE_view_layer_object_bases_get(view_layer)) {
    if (base->flag & BASE_SELECTED) {
      if (b != base) {
        blender::ed::object::base_select(base, blender::ed::object::BA_DESELECT);
        changed = true;
      }
    }
  }
  return changed;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Internal Edit-Mesh Select Buffer Wrapper
 *
 * Avoid duplicate code when using edit-mode selection,
 * actual logic is handled outside of this function.
 *
 * \note Currently this #EDBMSelectID_Context which is mesh specific
 * however the logic could also be used for non-meshes too.
 *
 * \{ */

struct EditSelectBuf_Cache {
  BLI_bitmap *select_bitmap;
};

static void editselect_buf_cache_init(const ViewContext *vc, short select_mode)
{
  if (vc->obedit) {
    Vector<Base *> bases = BKE_view_layer_array_from_bases_in_edit_mode(
        vc->scene, vc->view_layer, vc->v3d);

    DRW_select_buffer_context_create(vc->depsgraph, bases, select_mode);
  }
  else {
    /* Use for paint modes, currently only a single object at a time. */
    if (vc->obact) {
      BKE_view_layer_synced_ensure(vc->scene, vc->view_layer);
      Base *base = BKE_view_layer_base_find(vc->view_layer, vc->obact);
      DRW_select_buffer_context_create(vc->depsgraph, {base}, select_mode);
    }
  }
}

static void editselect_buf_cache_free(EditSelectBuf_Cache *esel)
{
  MEM_SAFE_FREE(esel->select_bitmap);
}

static void editselect_buf_cache_free_voidp(void *esel_voidp)
{
  editselect_buf_cache_free(static_cast<EditSelectBuf_Cache *>(esel_voidp));
  MEM_freeN(static_cast<EditSelectBuf_Cache *>(esel_voidp));
}

static void editselect_buf_cache_init_with_generic_userdata(wmGenericUserData *wm_userdata,
                                                            const ViewContext *vc,
                                                            short select_mode)
{
  EditSelectBuf_Cache *esel = MEM_callocN<EditSelectBuf_Cache>(__func__);
  wm_userdata->data = esel;
  wm_userdata->free_fn = editselect_buf_cache_free_voidp;
  wm_userdata->use_free = true;
  editselect_buf_cache_init(vc, select_mode);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Internal Edit-Mesh Utilities
 * \{ */

static bool edbm_backbuf_check_and_select_verts(EditSelectBuf_Cache *esel,
                                                Depsgraph *depsgraph,
                                                Object *ob,
                                                BMEditMesh *em,
                                                UVSyncSelectFromMesh *uv_selctx,
                                                const eSelectOp sel_op)
{
  BMVert *eve;
  BMIter iter;
  bool changed = false;

  const BLI_bitmap *select_bitmap = esel->select_bitmap;
  uint index = DRW_select_buffer_context_offset_for_object_elem(depsgraph, ob, SCE_SELECT_VERTEX);
  if (index == 0) {
    return false;
  }

  index -= 1;
  BM_ITER_MESH (eve, &iter, em->bm, BM_VERTS_OF_MESH) {
    if (!BM_elem_flag_test(eve, BM_ELEM_HIDDEN)) {
      const bool is_select = BM_elem_flag_test(eve, BM_ELEM_SELECT);
      const bool is_inside = BLI_BITMAP_TEST_BOOL(select_bitmap, index);
      const int sel_op_result = ED_select_op_action_deselected(sel_op, is_select, is_inside);
      if (sel_op_result != -1) {
        BM_vert_select_set(em->bm, eve, sel_op_result);
        if (uv_selctx) {
          uv_selctx->vert_select_set(eve, sel_op_result);
        }

        changed = true;
      }
    }
    index++;
  }
  return changed;
}

static bool edbm_backbuf_check_and_select_edges(EditSelectBuf_Cache *esel,
                                                Depsgraph *depsgraph,
                                                Object *ob,
                                                BMEditMesh *em,
                                                UVSyncSelectFromMesh *uv_selctx,
                                                const eSelectOp sel_op)
{
  BMEdge *eed;
  BMIter iter;
  bool changed = false;

  const BLI_bitmap *select_bitmap = esel->select_bitmap;
  uint index = DRW_select_buffer_context_offset_for_object_elem(depsgraph, ob, SCE_SELECT_EDGE);
  if (index == 0) {
    return false;
  }

  index -= 1;
  BM_ITER_MESH (eed, &iter, em->bm, BM_EDGES_OF_MESH) {
    if (!BM_elem_flag_test(eed, BM_ELEM_HIDDEN)) {
      const bool is_select = BM_elem_flag_test(eed, BM_ELEM_SELECT);
      const bool is_inside = BLI_BITMAP_TEST_BOOL(select_bitmap, index);
      const int sel_op_result = ED_select_op_action_deselected(sel_op, is_select, is_inside);
      if (sel_op_result != -1) {
        BM_edge_select_set(em->bm, eed, sel_op_result);
        changed = true;

        if (uv_selctx) {
          uv_selctx->edge_select_set(eed, sel_op_result);
        }
      }
    }
    index++;
  }
  return changed;
}

static bool edbm_backbuf_check_and_select_faces(EditSelectBuf_Cache *esel,
                                                Depsgraph *depsgraph,
                                                Object *ob,
                                                BMEditMesh *em,
                                                UVSyncSelectFromMesh *uv_selctx,
                                                const eSelectOp sel_op)
{
  BMFace *efa;
  BMIter iter;
  bool changed = false;

  const BLI_bitmap *select_bitmap = esel->select_bitmap;
  uint index = DRW_select_buffer_context_offset_for_object_elem(depsgraph, ob, SCE_SELECT_FACE);
  if (index == 0) {
    return false;
  }

  index -= 1;
  BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
    if (!BM_elem_flag_test(efa, BM_ELEM_HIDDEN)) {
      const bool is_select = BM_elem_flag_test(efa, BM_ELEM_SELECT);
      const bool is_inside = BLI_BITMAP_TEST_BOOL(select_bitmap, index);
      const int sel_op_result = ED_select_op_action_deselected(sel_op, is_select, is_inside);
      if (sel_op_result != -1) {
        BM_face_select_set(em->bm, efa, sel_op_result);
        if (uv_selctx) {
          uv_selctx->face_select_set(efa, sel_op_result);
        }

        changed = true;
      }
    }
    index++;
  }
  return changed;
}

/* object mode, edbm_ prefix is confusing here, rename? */
static bool edbm_backbuf_check_and_select_verts_obmode(Mesh *mesh,
                                                       EditSelectBuf_Cache *esel,
                                                       const eSelectOp sel_op)
{
  using namespace blender;
  bool changed = false;

  const BLI_bitmap *select_bitmap = esel->select_bitmap;

  bke::MutableAttributeAccessor attributes = mesh->attributes_for_write();
  bke::SpanAttributeWriter<bool> select_vert = attributes.lookup_or_add_for_write_span<bool>(
      ".select_vert", bke::AttrDomain::Point);
  const VArray<bool> hide_vert = *attributes.lookup_or_default<bool>(
      ".hide_vert", bke::AttrDomain::Point, false);

  for (int index = 0; index < mesh->verts_num; index++) {
    if (!hide_vert[index]) {
      const bool is_select = select_vert.span[index];
      const bool is_inside = BLI_BITMAP_TEST_BOOL(select_bitmap, index);
      const int sel_op_result = ED_select_op_action_deselected(sel_op, is_select, is_inside);
      if (sel_op_result != -1) {
        select_vert.span[index] = sel_op_result == 1;
        changed = true;
      }
    }
  }
  select_vert.finish();
  return changed;
}

/* object mode, edbm_ prefix is confusing here, rename? */
static bool edbm_backbuf_check_and_select_faces_obmode(Mesh *mesh,
                                                       EditSelectBuf_Cache *esel,
                                                       const eSelectOp sel_op)
{
  using namespace blender;
  bool changed = false;

  const BLI_bitmap *select_bitmap = esel->select_bitmap;

  bke::MutableAttributeAccessor attributes = mesh->attributes_for_write();
  bke::SpanAttributeWriter<bool> select_poly = attributes.lookup_or_add_for_write_span<bool>(
      ".select_poly", bke::AttrDomain::Face);
  const VArray<bool> hide_poly = *attributes.lookup_or_default<bool>(
      ".hide_poly", bke::AttrDomain::Face, false);

  for (int index = 0; index < mesh->faces_num; index++) {
    if (!hide_poly[index]) {
      const bool is_select = select_poly.span[index];
      const bool is_inside = BLI_BITMAP_TEST_BOOL(select_bitmap, index);
      const int sel_op_result = ED_select_op_action_deselected(sel_op, is_select, is_inside);
      if (sel_op_result != -1) {
        select_poly.span[index] = sel_op_result == 1;
        changed = true;
      }
    }
  }
  select_poly.finish();
  return changed;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Lasso Select
 * \{ */

struct LassoSelectUserData {
  const ViewContext *vc;
  const rcti *rect;
  const rctf *rect_fl;
  rctf _rect_fl;
  Span<int2> mcoords;
  eSelectOp sel_op;
  eBezTriple_Flag select_flag;

  /** Only for edit-mesh selection. */
  UVSyncSelectFromMesh *uv_selctx = nullptr;

  /* runtime */
  int pass;
  bool is_done;
  bool is_changed;
};

static void view3d_userdata_lassoselect_init(LassoSelectUserData *r_data,
                                             const ViewContext *vc,
                                             const rcti *rect,
                                             const Span<int2> mcoords,
                                             const eSelectOp sel_op)
{
  r_data->vc = vc;

  r_data->rect = rect;
  r_data->rect_fl = &r_data->_rect_fl;
  BLI_rctf_rcti_copy(&r_data->_rect_fl, rect);

  r_data->mcoords = mcoords;
  r_data->sel_op = sel_op;
  /* SELECT by default, but can be changed if needed (only few cases use and respect this). */
  r_data->select_flag = (eBezTriple_Flag)SELECT;

  /* runtime */
  r_data->pass = 0;
  r_data->is_done = false;
  r_data->is_changed = false;
}

static bool view3d_selectable_data(bContext *C)
{
  if (!ED_operator_region_view3d_active(C)) {
    return false;
  }
  if (Object *ob = CTX_data_active_object(C)) {
    if (ob->mode & OB_MODE_EDIT) {
      return ob->type != OB_FONT;
    }
    if (ob->mode & (OB_MODE_VERTEX_PAINT | OB_MODE_TEXTURE_PAINT | OB_MODE_SCULPT_GREASE_PENCIL)) {
      return BKE_paint_select_elem_test(ob);
    }
    if (ob->mode & OB_MODE_WEIGHT_PAINT) {
      return BKE_paint_select_elem_test(ob) || BKE_object_pose_armature_get_with_wpaint_check(ob);
    }
  }

  return true;
}

/* helper also for box_select */
static bool edge_fully_inside_rect(const rctf *rect, const float v1[2], const float v2[2])
{
  return BLI_rctf_isect_pt_v(rect, v1) && BLI_rctf_isect_pt_v(rect, v2);
}

static bool edge_inside_rect(const rctf *rect, const float v1[2], const float v2[2])
{
  int d1, d2, d3, d4;

  /* check points in rect */
  if (edge_fully_inside_rect(rect, v1, v2)) {
    return true;
  }

  /* check points completely out rect */
  if (v1[0] < rect->xmin && v2[0] < rect->xmin) {
    return false;
  }
  if (v1[0] > rect->xmax && v2[0] > rect->xmax) {
    return false;
  }
  if (v1[1] < rect->ymin && v2[1] < rect->ymin) {
    return false;
  }
  if (v1[1] > rect->ymax && v2[1] > rect->ymax) {
    return false;
  }

  /* simple check lines intersecting. */
  d1 = (v1[1] - v2[1]) * (v1[0] - rect->xmin) + (v2[0] - v1[0]) * (v1[1] - rect->ymin);
  d2 = (v1[1] - v2[1]) * (v1[0] - rect->xmin) + (v2[0] - v1[0]) * (v1[1] - rect->ymax);
  d3 = (v1[1] - v2[1]) * (v1[0] - rect->xmax) + (v2[0] - v1[0]) * (v1[1] - rect->ymax);
  d4 = (v1[1] - v2[1]) * (v1[0] - rect->xmax) + (v2[0] - v1[0]) * (v1[1] - rect->ymin);

  if (d1 < 0 && d2 < 0 && d3 < 0 && d4 < 0) {
    return false;
  }
  if (d1 > 0 && d2 > 0 && d3 > 0 && d4 > 0) {
    return false;
  }

  return true;
}

static void do_lasso_select_pose__do_tag(void *user_data,
                                         bPoseChannel *pchan,
                                         const float screen_co_a[2],
                                         const float screen_co_b[2])
{
  LassoSelectUserData *data = static_cast<LassoSelectUserData *>(user_data);
  const bArmature *arm = static_cast<bArmature *>(data->vc->obact->data);
  if (!blender::animrig::bone_is_selectable(arm, pchan)) {
    return;
  }

  if (BLI_rctf_isect_segment(data->rect_fl, screen_co_a, screen_co_b) &&
      BLI_lasso_is_edge_inside(data->mcoords, UNPACK2(screen_co_a), UNPACK2(screen_co_b), INT_MAX))
  {
    pchan->runtime.flag |= POSE_RUNTIME_IN_SELECTION_AREA;
    data->is_changed = true;
  }
}
static void do_lasso_tag_pose(const ViewContext *vc, const Span<int2> mcoords)
{
  LassoSelectUserData data;
  rcti rect;

  if ((vc->obact->type != OB_ARMATURE) || (vc->obact->pose == nullptr)) {
    return;
  }

  BLI_lasso_boundbox(&rect, mcoords);

  view3d_userdata_lassoselect_init(&data, vc, &rect, mcoords, static_cast<eSelectOp>(0));

  ED_view3d_init_mats_rv3d(vc->obact, vc->rv3d);

  /* Treat bones as clipped segments (no joints). */
  pose_foreachScreenBone(vc,
                         do_lasso_select_pose__do_tag,
                         &data,
                         V3D_PROJ_TEST_CLIP_DEFAULT | V3D_PROJ_TEST_CLIP_CONTENT_DEFAULT);
}

static bool do_lasso_select_objects(const ViewContext *vc,
                                    const Span<int2> mcoords,
                                    const eSelectOp sel_op)
{
  View3D *v3d = vc->v3d;

  bool changed = false;
  if (SEL_OP_USE_PRE_DESELECT(sel_op)) {
    changed |= object_deselect_all_visible(vc->scene, vc->view_layer, vc->v3d);
  }
  BKE_view_layer_synced_ensure(vc->scene, vc->view_layer);
  LISTBASE_FOREACH (Base *, base, BKE_view_layer_object_bases_get(vc->view_layer)) {
    if (BASE_SELECTABLE(v3d, base)) { /* Use this to avoid unnecessary lasso look-ups. */
      float region_co[2];
      const bool is_select = base->flag & BASE_SELECTED;
      const bool is_inside = (ED_view3d_project_base(vc->region, base, region_co) ==
                              V3D_PROJ_RET_OK) &&
                             BLI_lasso_is_point_inside(mcoords,
                                                       int(region_co[0]),
                                                       int(region_co[1]),
                                                       /* Dummy value. */
                                                       INT_MAX);
      const int sel_op_result = ED_select_op_action_deselected(sel_op, is_select, is_inside);
      if (sel_op_result != -1) {
        blender::ed::object::base_select(base,
                                         sel_op_result ? blender::ed::object::BA_SELECT :
                                                         blender::ed::object::BA_DESELECT);
        changed = true;
      }
    }
  }

  if (changed) {
    DEG_id_tag_update(&vc->scene->id, ID_RECALC_SELECT);
    WM_main_add_notifier(NC_SCENE | ND_OB_SELECT, vc->scene);
  }
  return changed;
}

/**
 * Use for lasso & box select.
 */
static blender::Vector<Base *> do_pose_tag_select_op_prepare(const ViewContext *vc)
{
  auto bases_tag_and_append_fn = [](blender::Vector<Base *> &bases, Base *base) {
    Object *ob = base->object;
    bArmature *arm = static_cast<bArmature *>(ob->data);
    LISTBASE_FOREACH (bPoseChannel *, pchan, &ob->pose->chanbase) {
      pchan->runtime.flag &= ~POSE_RUNTIME_IN_SELECTION_AREA;
    }
    arm->id.tag |= ID_TAG_DOIT;
    ob->id.tag &= ~ID_TAG_DOIT;
    bases.append(base);
  };

  blender::Vector<Base *> bases;

  /* Special case, pose + weight paint mode. */
  if (vc->obact && (vc->obact->mode & OB_MODE_WEIGHT_PAINT)) {
    Object *ob_pose = BKE_object_pose_armature_get_with_wpaint_check(vc->obact);
    BLI_assert(ob_pose != nullptr); /* Caller is expected to check. */
    Base *base = BKE_view_layer_base_find(vc->view_layer, ob_pose);
    if (base) {
      bases_tag_and_append_fn(bases, base);
    }
  }
  else {
    FOREACH_BASE_IN_MODE_BEGIN (
        vc->scene, vc->view_layer, vc->v3d, OB_ARMATURE, OB_MODE_POSE, base_iter)
    {
      bases_tag_and_append_fn(bases, base_iter);
    }
    FOREACH_BASE_IN_MODE_END;
  }
  return bases;
}

static bool do_pose_tag_select_op_exec(blender::MutableSpan<Base *> bases, const eSelectOp sel_op)
{
  bool changed_multi = false;

  if (SEL_OP_USE_PRE_DESELECT(sel_op)) {
    for (const int i : bases.index_range()) {
      Base *base_iter = bases[i];
      Object *ob_iter = base_iter->object;
      if (ED_pose_deselect_all(ob_iter, SEL_DESELECT, false)) {
        ED_pose_bone_select_tag_update(ob_iter);
        changed_multi = true;
      }
    }
  }

  for (const int i : bases.index_range()) {
    Base *base_iter = bases[i];
    Object *ob_iter = base_iter->object;
    bArmature *arm = static_cast<bArmature *>(ob_iter->data);

    bool changed = false;
    LISTBASE_FOREACH (bPoseChannel *, pchan, &ob_iter->pose->chanbase) {
      Bone *bone = pchan->bone;
      if ((bone->flag & BONE_UNSELECTABLE) == 0) {
        const bool is_select = pchan->flag & POSE_SELECTED;
        const bool is_inside = pchan->runtime.flag & POSE_RUNTIME_IN_SELECTION_AREA;
        const int sel_op_result = ED_select_op_action_deselected(sel_op, is_select, is_inside);
        if (sel_op_result != -1) {
          SET_FLAG_FROM_TEST(pchan->flag, sel_op_result, POSE_SELECTED);
          if (sel_op_result == 0) {
            if (arm->act_bone == bone) {
              arm->act_bone = nullptr;
            }
          }
          changed = true;
        }
      }
    }
    if (changed) {
      ED_pose_bone_select_tag_update(ob_iter);
      changed_multi = true;
    }
  }
  return changed_multi;
}

static bool do_lasso_select_pose(const ViewContext *vc,
                                 const Span<int2> mcoords,
                                 const eSelectOp sel_op)
{
  blender::Vector<Base *> bases = do_pose_tag_select_op_prepare(vc);

  ViewContext vc_temp = *vc;

  for (const int i : bases.index_range()) {
    Base *base_iter = bases[i];
    Object *ob_iter = base_iter->object;
    ED_view3d_viewcontext_init_object(&vc_temp, ob_iter);
    do_lasso_tag_pose(&vc_temp, mcoords);
  }

  const bool changed_multi = do_pose_tag_select_op_exec(bases, sel_op);
  if (changed_multi) {
    DEG_id_tag_update(&vc->scene->id, ID_RECALC_SELECT);
    WM_main_add_notifier(NC_SCENE | ND_OB_SELECT, vc->scene);
  }

  return changed_multi;
}

static void do_lasso_select_mesh__doSelectVert(void *user_data,
                                               BMVert *eve,
                                               const float screen_co[2],
                                               int /*index*/)
{
  LassoSelectUserData *data = static_cast<LassoSelectUserData *>(user_data);
  const bool is_select = BM_elem_flag_test(eve, BM_ELEM_SELECT);
  const bool is_inside = (BLI_rctf_isect_pt_v(data->rect_fl, screen_co) &&
                          BLI_lasso_is_point_inside(
                              data->mcoords, screen_co[0], screen_co[1], IS_CLIPPED));
  const int sel_op_result = ED_select_op_action_deselected(data->sel_op, is_select, is_inside);
  if (sel_op_result != -1) {
    BM_vert_select_set(data->vc->em->bm, eve, sel_op_result);
    if (data->uv_selctx) {
      data->uv_selctx->vert_select_set(eve, sel_op_result);
    }

    data->is_changed = true;
  }
}
struct LassoSelectUserData_ForMeshEdge {
  LassoSelectUserData *data;
  EditSelectBuf_Cache *esel;
  uint backbuf_offset;
};
static void do_lasso_select_mesh__doSelectEdge_pass0(void *user_data,
                                                     BMEdge *eed,
                                                     const float screen_co_a[2],
                                                     const float screen_co_b[2],
                                                     int index)
{
  LassoSelectUserData_ForMeshEdge *data_for_edge = static_cast<LassoSelectUserData_ForMeshEdge *>(
      user_data);
  LassoSelectUserData *data = data_for_edge->data;
  bool is_visible = true;
  if (data_for_edge->backbuf_offset) {
    uint bitmap_inedx = data_for_edge->backbuf_offset + index - 1;
    is_visible = BLI_BITMAP_TEST_BOOL(data_for_edge->esel->select_bitmap, bitmap_inedx);
  }

  const bool is_select = BM_elem_flag_test(eed, BM_ELEM_SELECT);
  const bool is_inside =
      (is_visible && edge_fully_inside_rect(data->rect_fl, screen_co_a, screen_co_b) &&
       BLI_lasso_is_point_inside(data->mcoords, UNPACK2(screen_co_a), IS_CLIPPED) &&
       BLI_lasso_is_point_inside(data->mcoords, UNPACK2(screen_co_b), IS_CLIPPED));
  const int sel_op_result = ED_select_op_action_deselected(data->sel_op, is_select, is_inside);
  if (sel_op_result != -1) {
    BM_edge_select_set(data->vc->em->bm, eed, sel_op_result);
    if (data->uv_selctx) {
      data->uv_selctx->edge_select_set(eed, sel_op_result);
    }

    data->is_done = true;
    data->is_changed = true;
  }
}
static void do_lasso_select_mesh__doSelectEdge_pass1(void *user_data,
                                                     BMEdge *eed,
                                                     const float screen_co_a[2],
                                                     const float screen_co_b[2],
                                                     int index)
{
  LassoSelectUserData_ForMeshEdge *data_for_edge = static_cast<LassoSelectUserData_ForMeshEdge *>(
      user_data);
  LassoSelectUserData *data = data_for_edge->data;
  bool is_visible = true;
  if (data_for_edge->backbuf_offset) {
    uint bitmap_inedx = data_for_edge->backbuf_offset + index - 1;
    is_visible = BLI_BITMAP_TEST_BOOL(data_for_edge->esel->select_bitmap, bitmap_inedx);
  }

  const bool is_select = BM_elem_flag_test(eed, BM_ELEM_SELECT);
  const bool is_inside = (is_visible && BLI_lasso_is_edge_inside(data->mcoords,
                                                                 UNPACK2(screen_co_a),
                                                                 UNPACK2(screen_co_b),
                                                                 IS_CLIPPED));
  const int sel_op_result = ED_select_op_action_deselected(data->sel_op, is_select, is_inside);
  if (sel_op_result != -1) {
    BM_edge_select_set(data->vc->em->bm, eed, sel_op_result);
    if (data->uv_selctx) {
      data->uv_selctx->edge_select_set(eed, sel_op_result);
    }

    data->is_changed = true;
  }
}

static void do_lasso_select_mesh__doSelectFace(void *user_data,
                                               BMFace *efa,
                                               const float screen_co[2],
                                               int /*index*/)
{
  LassoSelectUserData *data = static_cast<LassoSelectUserData *>(user_data);
  const bool is_select = BM_elem_flag_test(efa, BM_ELEM_SELECT);
  const bool is_inside = (BLI_rctf_isect_pt_v(data->rect_fl, screen_co) &&
                          BLI_lasso_is_point_inside(
                              data->mcoords, screen_co[0], screen_co[1], IS_CLIPPED));
  const int sel_op_result = ED_select_op_action_deselected(data->sel_op, is_select, is_inside);
  if (sel_op_result != -1) {
    BM_face_select_set(data->vc->em->bm, efa, sel_op_result);
    if (data->uv_selctx) {
      data->uv_selctx->face_select_set(efa, sel_op_result);
    }

    data->is_changed = true;
  }
}

static bool do_lasso_select_mesh(const ViewContext *vc,
                                 wmGenericUserData *wm_userdata,
                                 const Span<int2> mcoords,
                                 const eSelectOp sel_op)
{
  LassoSelectUserData data;
  ToolSettings *ts = vc->scene->toolsettings;
  rcti rect;

  BLI_assert(vc->em == BKE_editmesh_from_object(vc->obedit));

  BLI_lasso_boundbox(&rect, mcoords);

  view3d_userdata_lassoselect_init(&data, vc, &rect, mcoords, sel_op);

  if (SEL_OP_USE_PRE_DESELECT(sel_op)) {
    if (vc->em->bm->totvertsel) {
      EDBM_flag_disable_all(vc->em, BM_ELEM_SELECT);
      data.is_changed = true;
    }
  }

  std::unique_ptr<UVSyncSelectFromMesh> uv_selctx = UVSyncSelectFromMesh::create_if_needed(
      *ts, *vc->em->bm);
  data.uv_selctx = uv_selctx.get();

  /* for non zbuf projections, don't change the GL state */
  ED_view3d_init_mats_rv3d(vc->obedit, vc->rv3d);

  GPU_matrix_set(vc->rv3d->viewmat);

  const bool use_zbuf = !XRAY_FLAG_ENABLED(vc->v3d);

  EditSelectBuf_Cache *esel = static_cast<EditSelectBuf_Cache *>(wm_userdata->data);
  if (use_zbuf) {
    if (wm_userdata->data == nullptr) {
      editselect_buf_cache_init_with_generic_userdata(wm_userdata, vc, ts->selectmode);
      esel = static_cast<EditSelectBuf_Cache *>(wm_userdata->data);
      esel->select_bitmap = DRW_select_buffer_bitmap_from_poly(
          vc->depsgraph, vc->region, vc->v3d, mcoords, &rect, nullptr);
    }
  }

  if (ts->selectmode & SCE_SELECT_VERTEX) {
    if (use_zbuf) {
      data.is_changed |= edbm_backbuf_check_and_select_verts(
          esel, vc->depsgraph, vc->obedit, vc->em, data.uv_selctx, sel_op);
    }
    else {
      mesh_foreachScreenVert(
          vc, do_lasso_select_mesh__doSelectVert, &data, V3D_PROJ_TEST_CLIP_DEFAULT);
    }
  }
  if (ts->selectmode & SCE_SELECT_EDGE) {
    /* Does both use_zbuf and non-use_zbuf versions (need screen cos for both) */
    LassoSelectUserData_ForMeshEdge data_for_edge{};
    data_for_edge.data = &data;
    data_for_edge.esel = use_zbuf ? esel : nullptr;
    data_for_edge.backbuf_offset = use_zbuf ? DRW_select_buffer_context_offset_for_object_elem(
                                                  vc->depsgraph, vc->obedit, SCE_SELECT_EDGE) :
                                              0;

    const eV3DProjTest clip_flag = V3D_PROJ_TEST_CLIP_NEAR |
                                   (use_zbuf ? (eV3DProjTest)0 : V3D_PROJ_TEST_CLIP_BB);
    /* Fully inside. */
    mesh_foreachScreenEdge_clip_bb_segment(
        vc, do_lasso_select_mesh__doSelectEdge_pass0, &data_for_edge, clip_flag);
    if (data.is_done == false) {
      /* Fall back to partially inside.
       * Clip content to account for edges partially behind the view. */
      mesh_foreachScreenEdge_clip_bb_segment(vc,
                                             do_lasso_select_mesh__doSelectEdge_pass1,
                                             &data_for_edge,
                                             clip_flag | V3D_PROJ_TEST_CLIP_CONTENT_DEFAULT);
    }
  }

  if (ts->selectmode & SCE_SELECT_FACE) {
    if (use_zbuf) {
      data.is_changed |= edbm_backbuf_check_and_select_faces(
          esel, vc->depsgraph, vc->obedit, vc->em, data.uv_selctx, sel_op);
    }
    else {
      mesh_foreachScreenFace(
          vc, do_lasso_select_mesh__doSelectFace, &data, V3D_PROJ_TEST_CLIP_DEFAULT);
    }
  }

  if (data.is_changed) {
    EDBM_selectmode_flush(vc->em);
  }

  if (data.uv_selctx) {
    data.uv_selctx->apply();
  }

  return data.is_changed;
}

static void do_lasso_select_curve__doSelect(void *user_data,
                                            Nurb * /*nu*/,
                                            BPoint *bp,
                                            BezTriple *bezt,
                                            int beztindex,
                                            bool handles_visible,
                                            const float screen_co[2])
{
  LassoSelectUserData *data = static_cast<LassoSelectUserData *>(user_data);

  const bool is_inside = BLI_lasso_is_point_inside(
      data->mcoords, screen_co[0], screen_co[1], IS_CLIPPED);
  if (bp) {
    const bool is_select = bp->f1 & SELECT;
    const int sel_op_result = ED_select_op_action_deselected(data->sel_op, is_select, is_inside);
    if (sel_op_result != -1) {
      SET_FLAG_FROM_TEST(bp->f1, sel_op_result, data->select_flag);
      data->is_changed = true;
    }
  }
  else {
    if (!handles_visible) {
      /* can only be (beztindex == 1) here since handles are hidden */
      const bool is_select = bezt->f2 & SELECT;
      const int sel_op_result = ED_select_op_action_deselected(data->sel_op, is_select, is_inside);
      if (sel_op_result != -1) {
        SET_FLAG_FROM_TEST(bezt->f2, sel_op_result, data->select_flag);
      }
      bezt->f1 = bezt->f3 = bezt->f2;
      data->is_changed = true;
    }
    else {
      uint8_t *flag_p = (&bezt->f1) + beztindex;
      const bool is_select = *flag_p & SELECT;
      const int sel_op_result = ED_select_op_action_deselected(data->sel_op, is_select, is_inside);
      if (sel_op_result != -1) {
        SET_FLAG_FROM_TEST(*flag_p, sel_op_result, data->select_flag);
        data->is_changed = true;
      }
    }
  }
}

static bool do_lasso_select_curve(const ViewContext *vc,
                                  const Span<int2> mcoords,
                                  const eSelectOp sel_op)
{
  const bool deselect_all = (sel_op == SEL_OP_SET);
  LassoSelectUserData data;
  rcti rect;

  BLI_lasso_boundbox(&rect, mcoords);

  view3d_userdata_lassoselect_init(&data, vc, &rect, mcoords, sel_op);

  Curve *curve = (Curve *)vc->obedit->data;
  ListBase *nurbs = BKE_curve_editNurbs_get(curve);

  /* For deselect all, items to be selected are tagged with temp flag. Clear that first. */
  if (deselect_all) {
    BKE_nurbList_flag_set(nurbs, BEZT_FLAG_TEMP_TAG, false);
    data.select_flag = BEZT_FLAG_TEMP_TAG;
  }

  ED_view3d_init_mats_rv3d(vc->obedit, vc->rv3d); /* for foreach's screen/vert projection */
  nurbs_foreachScreenVert(vc, do_lasso_select_curve__doSelect, &data, V3D_PROJ_TEST_CLIP_DEFAULT);

  /* Deselect items that were not added to selection (indicated by temp flag). */
  if (deselect_all) {
    data.is_changed |= BKE_nurbList_flag_set_from_flag(nurbs, BEZT_FLAG_TEMP_TAG, SELECT);
  }

  if (data.is_changed) {
    BKE_curve_nurb_vert_active_validate(static_cast<Curve *>(vc->obedit->data));
  }
  return data.is_changed;
}

static void do_lasso_select_lattice__doSelect(void *user_data,
                                              BPoint *bp,
                                              const float screen_co[2])
{
  LassoSelectUserData *data = static_cast<LassoSelectUserData *>(user_data);
  const bool is_select = bp->f1 & SELECT;
  const bool is_inside = (BLI_rctf_isect_pt_v(data->rect_fl, screen_co) &&
                          BLI_lasso_is_point_inside(
                              data->mcoords, screen_co[0], screen_co[1], IS_CLIPPED));
  const int sel_op_result = ED_select_op_action_deselected(data->sel_op, is_select, is_inside);
  if (sel_op_result != -1) {
    SET_FLAG_FROM_TEST(bp->f1, sel_op_result, SELECT);
    data->is_changed = true;
  }
}
static bool do_lasso_select_lattice(const ViewContext *vc,
                                    const Span<int2> mcoords,
                                    const eSelectOp sel_op)
{
  LassoSelectUserData data;
  rcti rect;

  BLI_lasso_boundbox(&rect, mcoords);

  view3d_userdata_lassoselect_init(&data, vc, &rect, mcoords, sel_op);

  if (SEL_OP_USE_PRE_DESELECT(sel_op)) {
    data.is_changed |= ED_lattice_flags_set(vc->obedit, 0);
  }

  ED_view3d_init_mats_rv3d(vc->obedit, vc->rv3d); /* for foreach's screen/vert projection */
  lattice_foreachScreenVert(
      vc, do_lasso_select_lattice__doSelect, &data, V3D_PROJ_TEST_CLIP_DEFAULT);
  return data.is_changed;
}

static void do_lasso_select_armature__doSelectBone(void *user_data,
                                                   EditBone *ebone,
                                                   const float screen_co_a[2],
                                                   const float screen_co_b[2])
{
  LassoSelectUserData *data = static_cast<LassoSelectUserData *>(user_data);
  const bArmature *arm = static_cast<const bArmature *>(data->vc->obedit->data);
  if (!blender::animrig::bone_is_visible(arm, ebone)) {
    return;
  }

  int is_ignore_flag = 0;
  int is_inside_flag = 0;

  if (screen_co_a[0] != IS_CLIPPED) {
    if (BLI_rcti_isect_pt(data->rect, UNPACK2(screen_co_a)) &&
        BLI_lasso_is_point_inside(data->mcoords, UNPACK2(screen_co_a), INT_MAX))
    {
      is_inside_flag |= BONESEL_ROOT;
    }
  }
  else {
    is_ignore_flag |= BONESEL_ROOT;
  }

  if (screen_co_b[0] != IS_CLIPPED) {
    if (BLI_rcti_isect_pt(data->rect, UNPACK2(screen_co_b)) &&
        BLI_lasso_is_point_inside(data->mcoords, UNPACK2(screen_co_b), INT_MAX))
    {
      is_inside_flag |= BONESEL_TIP;
    }
  }
  else {
    is_ignore_flag |= BONESEL_TIP;
  }

  if (is_ignore_flag == 0) {
    if (is_inside_flag == (BONE_ROOTSEL | BONE_TIPSEL) ||
        BLI_lasso_is_edge_inside(
            data->mcoords, UNPACK2(screen_co_a), UNPACK2(screen_co_b), INT_MAX))
    {
      is_inside_flag |= BONESEL_BONE;
    }
  }

  ebone->temp.i = is_inside_flag | (is_ignore_flag >> 16);
}
static void do_lasso_select_armature__doSelectBone_clip_content(void *user_data,
                                                                EditBone *ebone,
                                                                const float screen_co_a[2],
                                                                const float screen_co_b[2])
{
  LassoSelectUserData *data = static_cast<LassoSelectUserData *>(user_data);
  bArmature *arm = static_cast<bArmature *>(data->vc->obedit->data);
  if (!blender::animrig::bone_is_visible(arm, ebone)) {
    return;
  }

  const int is_ignore_flag = ebone->temp.i << 16;
  int is_inside_flag = ebone->temp.i & ~0xFFFF;

  /* - When #BONESEL_BONE is set, there is nothing to do.
   * - When #BONE_ROOTSEL or #BONE_TIPSEL have been set - they take priority over bone selection.
   */
  if (is_inside_flag & (BONESEL_BONE | BONE_ROOTSEL | BONE_TIPSEL)) {
    return;
  }

  if (BLI_lasso_is_edge_inside(data->mcoords, UNPACK2(screen_co_a), UNPACK2(screen_co_b), INT_MAX))
  {
    is_inside_flag |= BONESEL_BONE;
  }

  ebone->temp.i = is_inside_flag | (is_ignore_flag >> 16);
}

static bool do_lasso_select_armature(const ViewContext *vc,
                                     const Span<int2> mcoords,
                                     const eSelectOp sel_op)
{
  LassoSelectUserData data;
  rcti rect;

  BLI_lasso_boundbox(&rect, mcoords);

  view3d_userdata_lassoselect_init(&data, vc, &rect, mcoords, sel_op);

  if (SEL_OP_USE_PRE_DESELECT(sel_op)) {
    data.is_changed |= ED_armature_edit_deselect_all_visible(vc->obedit);
  }

  bArmature *arm = static_cast<bArmature *>(vc->obedit->data);

  ED_armature_ebone_listbase_temp_clear(arm->edbo);

  ED_view3d_init_mats_rv3d(vc->obedit, vc->rv3d);

  /* Operate on fully visible (non-clipped) points. */
  armature_foreachScreenBone(
      vc, do_lasso_select_armature__doSelectBone, &data, V3D_PROJ_TEST_CLIP_DEFAULT);

  /* Operate on bones as segments clipped to the viewport bounds
   * (needed to handle bones with both points outside the view).
   * A separate pass is needed since clipped coordinates can't be used for selecting joints. */
  armature_foreachScreenBone(vc,
                             do_lasso_select_armature__doSelectBone_clip_content,
                             &data,
                             V3D_PROJ_TEST_CLIP_DEFAULT | V3D_PROJ_TEST_CLIP_CONTENT_DEFAULT);

  data.is_changed |= ED_armature_edit_select_op_from_tagged(arm, sel_op);

  if (data.is_changed) {
    WM_main_add_notifier(NC_OBJECT | ND_BONE_SELECT, vc->obedit);
  }
  return data.is_changed;
}

static void do_lasso_select_mball__doSelectElem(void *user_data,
                                                MetaElem *ml,
                                                const float screen_co[2])
{
  LassoSelectUserData *data = static_cast<LassoSelectUserData *>(user_data);
  const bool is_select = ml->flag & SELECT;
  const bool is_inside = (BLI_rctf_isect_pt_v(data->rect_fl, screen_co) &&
                          BLI_lasso_is_point_inside(
                              data->mcoords, screen_co[0], screen_co[1], INT_MAX));
  const int sel_op_result = ED_select_op_action_deselected(data->sel_op, is_select, is_inside);
  if (sel_op_result != -1) {
    SET_FLAG_FROM_TEST(ml->flag, sel_op_result, SELECT);
    data->is_changed = true;
  }
}
static bool do_lasso_select_meta(const ViewContext *vc,
                                 const Span<int2> mcoords,
                                 const eSelectOp sel_op)
{
  LassoSelectUserData data;
  rcti rect;

  MetaBall *mb = (MetaBall *)vc->obedit->data;

  BLI_lasso_boundbox(&rect, mcoords);

  view3d_userdata_lassoselect_init(&data, vc, &rect, mcoords, sel_op);

  if (SEL_OP_USE_PRE_DESELECT(sel_op)) {
    data.is_changed |= BKE_mball_deselect_all(mb);
  }

  ED_view3d_init_mats_rv3d(vc->obedit, vc->rv3d);

  mball_foreachScreenElem(
      vc, do_lasso_select_mball__doSelectElem, &data, V3D_PROJ_TEST_CLIP_DEFAULT);

  return data.is_changed;
}

static bool do_lasso_select_grease_pencil(const ViewContext *vc,
                                          const Span<int2> mcoords,
                                          const eSelectOp sel_op)
{
  using namespace blender;
  Object *object = (vc->obedit ? vc->obedit : vc->obact);
  const Object *ob_eval = DEG_get_evaluated(vc->depsgraph, object);
  const GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object->data);
  const bke::AttrDomain selection_domain = ED_grease_pencil_selection_domain_get(
      vc->scene->toolsettings, object);

  return ed::greasepencil::selection_update(
      vc,
      sel_op,
      [&](const ed::greasepencil::MutableDrawingInfo &info,
          const IndexMask &mask,
          const StringRef attribute_name,
          IndexMaskMemory &memory) {
        bke::CurvesGeometry &curves = info.drawing.strokes_for_write();
        const bke::greasepencil::Layer &layer = grease_pencil.layer(info.layer_index);
        const bke::crazyspace::GeometryDeformation deformation =
            bke::crazyspace::get_evaluated_grease_pencil_drawing_deformation(
                ob_eval, *object, info.drawing);
        const IndexMask visible_handle_elements =
            ed::greasepencil::retrieve_visible_bezier_handle_elements(
                *object,
                info.drawing,
                info.layer_index,
                selection_domain,
                vc->v3d->overlay.handle_display,
                memory);
        const float4x4 layer_to_world = layer.to_world_space(*ob_eval);
        const float4x4 projection = ED_view3d_ob_project_mat_get_from_obmat(vc->rv3d,
                                                                            layer_to_world);

        return ed::curves::select_lasso_mask(*vc,
                                             curves,
                                             deformation,
                                             projection,
                                             mask,
                                             visible_handle_elements,
                                             selection_domain,
                                             attribute_name,
                                             mcoords,
                                             memory);
      });
}

struct LassoSelectUserData_ForMeshObjectVert {
  LassoSelectUserData lasso_data;
  blender::MutableSpan<bool> select_vert;
};
static void do_lasso_select_meshobject__doSelectVert(void *user_data,
                                                     const float screen_co[2],
                                                     int index)
{
  using namespace blender;
  LassoSelectUserData_ForMeshObjectVert *mesh_data =
      static_cast<LassoSelectUserData_ForMeshObjectVert *>(user_data);
  LassoSelectUserData *data = &mesh_data->lasso_data;
  const bool is_select = mesh_data->select_vert[index];
  const bool is_inside = (BLI_rctf_isect_pt_v(data->rect_fl, screen_co) &&
                          BLI_lasso_is_point_inside(
                              data->mcoords, screen_co[0], screen_co[1], IS_CLIPPED));
  const int sel_op_result = ED_select_op_action_deselected(data->sel_op, is_select, is_inside);
  if (sel_op_result != -1) {
    mesh_data->select_vert[index] = sel_op_result == 1;
    data->is_changed = true;
  }
}
static bool do_lasso_select_paintvert(const ViewContext *vc,
                                      wmGenericUserData *wm_userdata,
                                      const Span<int2> mcoords,
                                      const eSelectOp sel_op)
{
  using namespace blender;
  const bool use_zbuf = !XRAY_ENABLED(vc->v3d);
  Object *ob = vc->obact;
  Mesh *mesh = static_cast<Mesh *>(ob->data);
  rcti rect;

  if (mesh == nullptr || mesh->verts_num == 0) {
    return false;
  }

  bool changed = false;
  if (SEL_OP_USE_PRE_DESELECT(sel_op)) {
    /* flush selection at the end */
    changed |= paintvert_deselect_all_visible(ob, SEL_DESELECT, false);
  }

  BLI_lasso_boundbox(&rect, mcoords);

  EditSelectBuf_Cache *esel = static_cast<EditSelectBuf_Cache *>(wm_userdata->data);
  if (use_zbuf) {
    if (wm_userdata->data == nullptr) {
      editselect_buf_cache_init_with_generic_userdata(wm_userdata, vc, SCE_SELECT_VERTEX);
      esel = static_cast<EditSelectBuf_Cache *>(wm_userdata->data);
      esel->select_bitmap = DRW_select_buffer_bitmap_from_poly(
          vc->depsgraph, vc->region, vc->v3d, mcoords, &rect, nullptr);
    }
  }

  if (use_zbuf) {
    if (esel->select_bitmap != nullptr) {
      changed |= edbm_backbuf_check_and_select_verts_obmode(mesh, esel, sel_op);
    }
  }
  else {
    bke::MutableAttributeAccessor attributes = mesh->attributes_for_write();
    bke::SpanAttributeWriter<bool> select_vert = attributes.lookup_or_add_for_write_span<bool>(
        ".select_vert", bke::AttrDomain::Point);

    LassoSelectUserData_ForMeshObjectVert data;
    data.select_vert = select_vert.span;

    view3d_userdata_lassoselect_init(&data.lasso_data, vc, &rect, mcoords, sel_op);

    ED_view3d_init_mats_rv3d(vc->obact, vc->rv3d);

    meshobject_foreachScreenVert(
        vc, do_lasso_select_meshobject__doSelectVert, &data, V3D_PROJ_TEST_CLIP_DEFAULT);

    changed |= data.lasso_data.is_changed;
    select_vert.finish();
  }

  if (changed) {
    if (SEL_OP_CAN_DESELECT(sel_op)) {
      BKE_mesh_mselect_validate(mesh);
    }
    paintvert_flush_flags(ob);
    paintvert_tag_select_update(vc->C, ob);
  }

  return changed;
}
static bool do_lasso_select_paintface(const ViewContext *vc,
                                      wmGenericUserData *wm_userdata,
                                      const Span<int2> mcoords,
                                      const eSelectOp sel_op)
{
  Object *ob = vc->obact;
  Mesh *mesh = static_cast<Mesh *>(ob->data);
  rcti rect;

  if (mesh == nullptr || mesh->faces_num == 0) {
    return false;
  }

  bool changed = false;
  if (SEL_OP_USE_PRE_DESELECT(sel_op)) {
    /* flush selection at the end */
    changed |= paintface_deselect_all_visible(vc->C, ob, SEL_DESELECT, false);
  }

  BLI_lasso_boundbox(&rect, mcoords);

  EditSelectBuf_Cache *esel = static_cast<EditSelectBuf_Cache *>(wm_userdata->data);
  if (esel == nullptr) {
    editselect_buf_cache_init_with_generic_userdata(wm_userdata, vc, SCE_SELECT_FACE);
    esel = static_cast<EditSelectBuf_Cache *>(wm_userdata->data);
    esel->select_bitmap = DRW_select_buffer_bitmap_from_poly(
        vc->depsgraph, vc->region, vc->v3d, mcoords, &rect, nullptr);
  }

  if (esel->select_bitmap) {
    changed |= edbm_backbuf_check_and_select_faces_obmode(mesh, esel, sel_op);
  }

  if (changed) {
    paintface_flush_flags(vc->C, ob, true, false);
  }
  return changed;
}

static bool view3d_lasso_select(bContext *C,
                                ViewContext *vc,
                                const Span<int2> mcoords,
                                const eSelectOp sel_op)
{
  using namespace blender;
  Object *ob = CTX_data_active_object(C);
  bool changed_multi = false;

  wmGenericUserData wm_userdata_buf = {nullptr, nullptr, false};
  wmGenericUserData *wm_userdata = &wm_userdata_buf;

  if (vc->obedit == nullptr) { /* Object Mode */
    if (BKE_paint_select_face_test(ob)) {
      changed_multi |= do_lasso_select_paintface(vc, wm_userdata, mcoords, sel_op);
    }
    else if (BKE_paint_select_vert_test(ob)) {
      changed_multi |= do_lasso_select_paintvert(vc, wm_userdata, mcoords, sel_op);
    }
    else if (BKE_paint_select_grease_pencil_test(ob)) {
      changed_multi |= do_lasso_select_grease_pencil(vc, mcoords, sel_op);
    }
    else if (ob && (ob->mode & OB_MODE_PARTICLE_EDIT)) {
      changed_multi |= PE_lasso_select(C,
                                       reinterpret_cast<const int (*)[2]>(mcoords.data()),
                                       mcoords.size(),
                                       sel_op) != OPERATOR_CANCELLED;
    }
    else if (ob &&
             ((ob->mode & OB_MODE_POSE) | ((ob->mode & OB_MODE_WEIGHT_PAINT) &&
                                           BKE_object_pose_armature_get_with_wpaint_check(ob))))
    {
      changed_multi |= do_lasso_select_pose(vc, mcoords, sel_op);
      if (changed_multi) {
        ED_outliner_select_sync_from_pose_bone_tag(C);
      }
    }
    else if (ob &&
             (ob->mode & (OB_MODE_VERTEX_PAINT | OB_MODE_WEIGHT_PAINT | OB_MODE_TEXTURE_PAINT)))
    {
      /* pass */
    }
    else {
      changed_multi |= do_lasso_select_objects(vc, mcoords, sel_op);
      if (changed_multi) {
        ED_outliner_select_sync_from_object_tag(C);
      }
    }
  }
  else { /* Edit Mode */
    if (vc->obedit->type == OB_MESH) {
      vc->em = BKE_editmesh_from_object(vc->obedit);
    }

    FOREACH_OBJECT_IN_MODE_BEGIN (vc->scene, vc->view_layer, vc->v3d, ob->type, ob->mode, ob_iter)
    {
      ED_view3d_viewcontext_init_object(vc, ob_iter);
      bool changed = false;

      switch (vc->obedit->type) {
        case OB_MESH:
          changed = do_lasso_select_mesh(vc, wm_userdata, mcoords, sel_op);
          break;
        case OB_CURVES_LEGACY:
        case OB_SURF:
          changed = do_lasso_select_curve(vc, mcoords, sel_op);
          break;
        case OB_LATTICE:
          changed = do_lasso_select_lattice(vc, mcoords, sel_op);
          break;
        case OB_ARMATURE:
          changed = do_lasso_select_armature(vc, mcoords, sel_op);
          if (changed) {
            ED_outliner_select_sync_from_edit_bone_tag(C);
          }
          break;
        case OB_MBALL:
          changed = do_lasso_select_meta(vc, mcoords, sel_op);
          break;
        case OB_CURVES: {
          Curves &curves_id = *static_cast<Curves *>(vc->obedit->data);
          bke::CurvesGeometry &curves = curves_id.geometry.wrap();
          bke::crazyspace::GeometryDeformation deformation =
              bke::crazyspace::get_evaluated_curves_deformation(*vc->depsgraph, *vc->obedit);
          const bke::AttrDomain selection_domain = bke::AttrDomain(curves_id.selection_domain);
          const IndexRange elements(curves.attributes().domain_size(selection_domain));
          const float4x4 projection = ED_view3d_ob_project_mat_get(vc->rv3d, vc->obedit);
          changed = ed::curves::select_lasso(*vc,
                                             curves,
                                             deformation,
                                             projection,
                                             elements,
                                             elements,
                                             selection_domain,
                                             mcoords,
                                             sel_op);
          if (changed) {
            /* Use #ID_RECALC_GEOMETRY instead of #ID_RECALC_SELECT because it is handled as a
             * generic attribute for now. */
            DEG_id_tag_update(static_cast<ID *>(vc->obedit->data), ID_RECALC_GEOMETRY);
            WM_event_add_notifier(C, NC_GEOM | ND_DATA, vc->obedit->data);
          }
          break;
        }
        case OB_POINTCLOUD: {
          PointCloud &pointcloud = *static_cast<PointCloud *>(vc->obedit->data);
          const float4x4 projection = ED_view3d_ob_project_mat_get(vc->rv3d, vc->obedit);
          changed = ed::pointcloud::select_lasso(
              pointcloud, *vc->region, projection, mcoords, sel_op);
          if (changed) {
            /* Use #ID_RECALC_GEOMETRY instead of #ID_RECALC_SELECT because it is handled as a
             * generic attribute for now. */
            DEG_id_tag_update(static_cast<ID *>(vc->obedit->data), ID_RECALC_GEOMETRY);
            WM_event_add_notifier(C, NC_GEOM | ND_DATA, vc->obedit->data);
          }
          break;
        }
        case OB_GREASE_PENCIL: {
          changed = do_lasso_select_grease_pencil(vc, mcoords, sel_op);
          break;
        }
        default:
          BLI_assert_msg(0, "lasso select on incorrect object type");
          break;
      }

      if (changed) {
        DEG_id_tag_update(static_cast<ID *>(vc->obedit->data), ID_RECALC_SELECT);
        WM_event_add_notifier(C, NC_GEOM | ND_SELECT, vc->obedit->data);
        changed_multi = true;
      }
    }
    FOREACH_OBJECT_IN_MODE_END;
  }

  WM_generic_user_data_free(wm_userdata);

  return changed_multi;
}

/* lasso operator gives properties, but since old code works
 * with short array we convert */
static wmOperatorStatus view3d_lasso_select_exec(bContext *C, wmOperator *op)
{
  Array<int2> mcoords = WM_gesture_lasso_path_to_array(C, op);
  if (mcoords.is_empty()) {
    return OPERATOR_PASS_THROUGH;
  }

  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  view3d_operator_needs_gpu(C);
  BKE_object_update_select_id(CTX_data_main(C));

  /* setup view context for argument to callbacks */
  ViewContext vc = ED_view3d_viewcontext_init(C, depsgraph);

  eSelectOp sel_op = static_cast<eSelectOp>(RNA_enum_get(op->ptr, "mode"));
  bool changed_multi = view3d_lasso_select(C, &vc, mcoords, sel_op);

  if (changed_multi) {
    return OPERATOR_FINISHED;
  }
  return OPERATOR_CANCELLED;
}

void VIEW3D_OT_select_lasso(wmOperatorType *ot)
{
  ot->name = "Lasso Select";
  ot->description = "Select items using lasso selection";
  ot->idname = "VIEW3D_OT_select_lasso";

  ot->invoke = WM_gesture_lasso_invoke;
  ot->modal = WM_gesture_lasso_modal;
  ot->exec = view3d_lasso_select_exec;
  ot->poll = view3d_selectable_data;
  ot->cancel = WM_gesture_lasso_cancel;

  /* flags */
  ot->flag = OPTYPE_UNDO | OPTYPE_DEPENDS_ON_CURSOR;

  /* properties */
  WM_operator_properties_gesture_lasso(ot);
  WM_operator_properties_select_operation(ot);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Cursor Picking
 * \{ */

/* The max number of menu items in an object select menu */
struct SelMenuItemF {
  char idname[MAX_ID_NAME - 2];
  int icon;
  Base *base_ptr;
  void *item_ptr;
};

#define SEL_MENU_SIZE 22
static SelMenuItemF object_mouse_select_menu_data[SEL_MENU_SIZE];

/* special (crappy) operator only for menu select */
static const EnumPropertyItem *object_select_menu_enum_itemf(bContext *C,
                                                             PointerRNA * /*ptr*/,
                                                             PropertyRNA * /*prop*/,
                                                             bool *r_free)
{
  EnumPropertyItem *item = nullptr, item_tmp = {0};
  int totitem = 0;
  int i = 0;

  /* Don't need context but avoid API doc-generation using this. */
  if (C == nullptr || object_mouse_select_menu_data[i].idname[0] == '\0') {
    return rna_enum_dummy_NULL_items;
  }

  for (; i < SEL_MENU_SIZE && object_mouse_select_menu_data[i].idname[0] != '\0'; i++) {
    item_tmp.name = object_mouse_select_menu_data[i].idname;
    item_tmp.identifier = object_mouse_select_menu_data[i].idname;
    item_tmp.value = i;
    item_tmp.icon = object_mouse_select_menu_data[i].icon;
    RNA_enum_item_add(&item, &totitem, &item_tmp);
  }

  RNA_enum_item_end(&item, &totitem);
  *r_free = true;

  return item;
}

static wmOperatorStatus object_select_menu_exec(bContext *C, wmOperator *op)
{
  const int name_index = RNA_enum_get(op->ptr, "name");
  const bool extend = RNA_boolean_get(op->ptr, "extend");
  const bool deselect = RNA_boolean_get(op->ptr, "deselect");
  const bool toggle = RNA_boolean_get(op->ptr, "toggle");
  bool changed = false;
  const char *name = object_mouse_select_menu_data[name_index].idname;

  View3D *v3d = CTX_wm_view3d(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  BKE_view_layer_synced_ensure(scene, view_layer);
  const Base *oldbasact = BKE_view_layer_active_base_get(view_layer);

  Base *basact = nullptr;
  CTX_DATA_BEGIN (C, Base *, base, selectable_bases) {
    /* This is a bit dodgy, there should only be ONE object with this name,
     * but library objects can mess this up. */
    if (STREQ(name, base->object->id.name + 2)) {
      basact = base;
      break;
    }
  }
  CTX_DATA_END;

  if (basact == nullptr) {
    return OPERATOR_CANCELLED;
  }
  UNUSED_VARS_NDEBUG(v3d);
  BLI_assert(BASE_SELECTABLE(v3d, basact));

  if (extend) {
    blender::ed::object::base_select(basact, blender::ed::object::BA_SELECT);
    changed = true;
  }
  else if (deselect) {
    blender::ed::object::base_select(basact, blender::ed::object::BA_DESELECT);
    changed = true;
  }
  else if (toggle) {
    if (basact->flag & BASE_SELECTED) {
      if (basact == oldbasact) {
        blender::ed::object::base_select(basact, blender::ed::object::BA_DESELECT);
        changed = true;
      }
    }
    else {
      blender::ed::object::base_select(basact, blender::ed::object::BA_SELECT);
      changed = true;
    }
  }
  else {
    object_deselect_all_except(scene, view_layer, basact);
    blender::ed::object::base_select(basact, blender::ed::object::BA_SELECT);
    changed = true;
  }

  if (oldbasact != basact) {
    blender::ed::object::base_activate(C, basact);
  }

  /* weak but ensures we activate menu again before using the enum */
  memset(object_mouse_select_menu_data, 0, sizeof(object_mouse_select_menu_data));

  /* undo? */
  if (changed) {
    Scene *scene = CTX_data_scene(C);
    DEG_id_tag_update(&scene->id, ID_RECALC_SELECT);
    WM_event_add_notifier(C, NC_SCENE | ND_OB_SELECT, scene);

    ED_outliner_select_sync_from_object_tag(C);

    return OPERATOR_FINISHED;
  }
  return OPERATOR_CANCELLED;
}

static std::string object_select_menu_get_name(wmOperatorType * /*ot*/, PointerRNA *ptr)
{
  if (RNA_boolean_get(ptr, "deselect")) {
    return CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Deselect Object");
  }
  if (RNA_boolean_get(ptr, "toggle")) {
    return CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Toggle Object Selection");
  }
  return CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Select Object");
}

void VIEW3D_OT_select_menu(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Select Menu";
  ot->description = "Menu object selection";
  ot->idname = "VIEW3D_OT_select_menu";

  /* API callbacks. */
  ot->invoke = WM_menu_invoke;
  ot->exec = object_select_menu_exec;
  ot->get_name = object_select_menu_get_name;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* #Object.id.name to select (dynamic enum). */
  prop = RNA_def_enum(ot->srna, "name", rna_enum_dummy_NULL_items, 0, "Object Name", "");
  RNA_def_enum_funcs(prop, object_select_menu_enum_itemf);
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_ENUM_NO_TRANSLATE);
  ot->prop = prop;

  prop = RNA_def_boolean(ot->srna, "extend", false, "Extend", "");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  prop = RNA_def_boolean(ot->srna, "deselect", false, "Deselect", "");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  prop = RNA_def_boolean(ot->srna, "toggle", false, "Toggle", "");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/**
 * \return True when a menu was activated.
 */
static bool object_mouse_select_menu(bContext *C,
                                     const ViewContext *vc,
                                     const blender::Span<GPUSelectResult> hit_results,
                                     const int mval[2],
                                     const SelectPick_Params &params,
                                     Base **r_basact)
{

  const float mval_fl[2] = {float(mval[0]), float(mval[1])};
  /* Distance from object center to use for selection. */
  const float dist_threshold_sq = square_f(15 * U.pixelsize);
  int base_count = 0;

  struct BaseRefWithDepth {
    BaseRefWithDepth *next, *prev;
    Base *base;
    /** The scale isn't defined, simply use for sorting. */
    uint depth_id;
  };
  ListBase base_ref_list = {nullptr, nullptr}; /* List of #BaseRefWithDepth. */

  /* handle base->object->select_id */
  CTX_DATA_BEGIN (C, Base *, base, selectable_bases) {
    bool ok = false;
    uint depth_id;

    /* two selection methods, the CTRL select uses max dist of 15 */
    if (!hit_results.is_empty()) {
      for (const GPUSelectResult &hit_result : hit_results) {
        /* index was converted */
        if (base->object->runtime->select_id == (hit_result.id & ~0xFFFF0000)) {
          ok = true;
          depth_id = hit_result.depth;
          break;
        }
      }
    }
    else {
      float region_co[2];
      if (ED_view3d_project_base(vc->region, base, region_co) == V3D_PROJ_RET_OK) {
        const float dist_test_sq = len_squared_v2v2(mval_fl, region_co);
        if (dist_test_sq < dist_threshold_sq) {
          ok = true;
          /* Match GPU depth logic, as the float is always positive, it can be sorted as an int. */
          depth_id = float_as_uint(dist_test_sq);
        }
      }
    }

    if (ok) {
      base_count++;
      BaseRefWithDepth *base_ref = MEM_callocN<BaseRefWithDepth>(__func__);
      base_ref->base = base;
      base_ref->depth_id = depth_id;
      BLI_addtail(&base_ref_list, (void *)base_ref);
    }
  }
  CTX_DATA_END;

  *r_basact = nullptr;

  if (base_count == 0) {
    return false;
  }
  if (base_count == 1) {
    Base *base = ((BaseRefWithDepth *)base_ref_list.first)->base;
    BLI_freelistN(&base_ref_list);
    *r_basact = base;
    return false;
  }

  /* Sort by depth or distance to cursor. */
  BLI_listbase_sort(&base_ref_list, [](const void *a, const void *b) {
    return int(static_cast<const BaseRefWithDepth *>(a)->depth_id >
               static_cast<const BaseRefWithDepth *>(b)->depth_id);
  });

  while (base_count > SEL_MENU_SIZE) {
    BLI_freelinkN(&base_ref_list, base_ref_list.last);
    base_count -= 1;
  }

  /* UI, full in static array values that we later use in an enum function */

  memset(object_mouse_select_menu_data, 0, sizeof(object_mouse_select_menu_data));

  int i;
  LISTBASE_FOREACH_INDEX (BaseRefWithDepth *, base_ref, &base_ref_list, i) {
    Base *base = base_ref->base;
    Object *ob = base->object;
    const char *name = ob->id.name + 2;

    BLI_strncpy_utf8(object_mouse_select_menu_data[i].idname, name, MAX_ID_NAME - 2);
    object_mouse_select_menu_data[i].icon = UI_icon_from_id(&ob->id);
  }

  wmOperatorType *ot = WM_operatortype_find("VIEW3D_OT_select_menu", false);
  PointerRNA ptr;

  WM_operator_properties_create_ptr(&ptr, ot);
  RNA_boolean_set(&ptr, "extend", params.sel_op == SEL_OP_ADD);
  RNA_boolean_set(&ptr, "deselect", params.sel_op == SEL_OP_SUB);
  RNA_boolean_set(&ptr, "toggle", params.sel_op == SEL_OP_XOR);
  WM_operator_name_call_ptr(C, ot, blender::wm::OpCallContext::InvokeDefault, &ptr, nullptr);
  WM_operator_properties_free(&ptr);

  BLI_freelistN(&base_ref_list);
  return true;
}

static wmOperatorStatus bone_select_menu_exec(bContext *C, wmOperator *op)
{
  const int name_index = RNA_enum_get(op->ptr, "name");

  SelectPick_Params params{};
  params.sel_op = ED_select_op_from_operator(op->ptr);

  View3D *v3d = CTX_wm_view3d(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  BKE_view_layer_synced_ensure(scene, view_layer);
  const Base *oldbasact = BKE_view_layer_active_base_get(view_layer);

  Base *basact = object_mouse_select_menu_data[name_index].base_ptr;

  if (basact == nullptr) {
    return OPERATOR_CANCELLED;
  }

  BLI_assert(BASE_SELECTABLE(v3d, basact));

  if (basact->object->mode & OB_MODE_EDIT) {
    EditBone *ebone = (EditBone *)object_mouse_select_menu_data[name_index].item_ptr;
    ED_armature_edit_select_pick_bone(C, basact, ebone, BONE_SELECTED, params);
  }
  else {
    bPoseChannel *pchan = (bPoseChannel *)object_mouse_select_menu_data[name_index].item_ptr;
    ED_armature_pose_select_pick_bone(scene, view_layer, v3d, basact->object, pchan, params);
  }

  /* Weak but ensures we activate the menu again before using the enum. */
  memset(object_mouse_select_menu_data, 0, sizeof(object_mouse_select_menu_data));

  /* We make the armature selected:
   * Not-selected active object in pose-mode won't work well for tools. */
  blender::ed::object::base_select(basact, blender::ed::object::BA_SELECT);

  WM_event_add_notifier(C, NC_OBJECT | ND_BONE_SELECT, basact->object);
  WM_event_add_notifier(C, NC_OBJECT | ND_BONE_ACTIVE, basact->object);

  /* In weight-paint, we use selected bone to select vertex-group,
   * so don't switch to new active object. */
  if (oldbasact) {
    if (basact->object->mode & OB_MODE_EDIT) {
      /* Pass. */
    }
    else if (oldbasact->object->mode & OB_MODE_ALL_WEIGHT_PAINT) {
      /* Prevent activating.
       * Selection causes this to be considered the 'active' pose in weight-paint mode.
       * Eventually this limitation may be removed.
       * For now, de-select all other pose objects deforming this mesh. */
      ED_armature_pose_select_in_wpaint_mode(scene, view_layer, basact);
    }
    else {
      if (oldbasact != basact) {
        blender::ed::object::base_activate(C, basact);
      }
    }
  }

  /* Undo? */
  DEG_id_tag_update(&scene->id, ID_RECALC_SELECT);
  DEG_id_tag_update(&scene->id, ID_RECALC_BASE_FLAGS);
  WM_event_add_notifier(C, NC_SCENE | ND_OB_SELECT, scene);

  ED_outliner_select_sync_from_object_tag(C);

  return OPERATOR_FINISHED;
}

void VIEW3D_OT_bone_select_menu(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Select Menu";
  ot->description = "Menu bone selection";
  ot->idname = "VIEW3D_OT_bone_select_menu";

  /* API callbacks. */
  ot->invoke = WM_menu_invoke;
  ot->exec = bone_select_menu_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* #Object.id.name to select (dynamic enum). */
  prop = RNA_def_enum(ot->srna, "name", rna_enum_dummy_NULL_items, 0, "Bone Name", "");
  RNA_def_enum_funcs(prop, object_select_menu_enum_itemf);
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_ENUM_NO_TRANSLATE);
  ot->prop = prop;

  prop = RNA_def_boolean(ot->srna, "extend", false, "Extend", "");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  prop = RNA_def_boolean(ot->srna, "deselect", false, "Deselect", "");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  prop = RNA_def_boolean(ot->srna, "toggle", false, "Toggle", "");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/**
 * \return True when a menu was activated.
 */
static bool bone_mouse_select_menu(bContext *C,
                                   const blender::Span<GPUSelectResult> hit_results,
                                   const bool is_editmode,
                                   const SelectPick_Params &params)
{
  int bone_count = 0;

  struct BoneRefWithDepth {
    BoneRefWithDepth *next, *prev;
    Base *base;
    union {
      EditBone *ebone;
      bPoseChannel *pchan;
      void *bone_ptr;
    };
    /** The scale isn't defined, simply use for sorting. */
    uint depth_id;
  };
  ListBase bone_ref_list = {nullptr, nullptr};

  GSet *added_bones = BLI_gset_ptr_new("Bone mouse select menu");

  /* Select logic taken from #ed_armature_pick_bone_from_selectbuffer_impl
   * in `armature_select.cc`. */
  for (const GPUSelectResult &hit_result : hit_results) {
    void *bone_ptr = nullptr;
    Base *bone_base = nullptr;
    uint select_id = hit_result.id;

    if (!(select_id & BONESEL_ANY)) {
      /* To avoid including objects in selection. */
      continue;
    }

    select_id &= ~BONESEL_ANY;
    const uint hit_object = select_id & 0xFFFF;

    /* Find the hit bone base (armature object). */
    CTX_DATA_BEGIN (C, Base *, base, selectable_bases) {
      if (base->object->runtime->select_id == hit_object) {
        bone_base = base;
        break;
      }
    }
    CTX_DATA_END;

    if (!bone_base) {
      continue;
    }

    /* Determine what the current bone is */
    if (is_editmode) {
      const uint hit_bone = (select_id & ~BONESEL_ANY) >> 16;
      bArmature *arm = static_cast<bArmature *>(bone_base->object->data);
      EditBone *ebone = static_cast<EditBone *>(BLI_findlink(arm->edbo, hit_bone));
      if (ebone && !(ebone->flag & BONE_UNSELECTABLE)) {
        bone_ptr = ebone;
      }
    }
    else {
      const uint hit_bone = (select_id & ~BONESEL_ANY) >> 16;
      bPoseChannel *pchan = static_cast<bPoseChannel *>(
          BLI_findlink(&bone_base->object->pose->chanbase, hit_bone));
      if (pchan && !(pchan->bone->flag & BONE_UNSELECTABLE)) {
        bone_ptr = pchan;
      }
    }

    if (!bone_ptr) {
      continue;
    }
    /* We can hit a bone multiple times, so make sure we are not adding an already included bone
     * to the list. */
    const bool is_duplicate_bone = BLI_gset_haskey(added_bones, bone_ptr);

    if (!is_duplicate_bone) {
      bone_count++;
      BoneRefWithDepth *bone_ref = MEM_callocN<BoneRefWithDepth>(__func__);
      bone_ref->base = bone_base;
      bone_ref->bone_ptr = bone_ptr;
      bone_ref->depth_id = hit_result.depth;
      BLI_addtail(&bone_ref_list, (void *)bone_ref);

      BLI_gset_insert(added_bones, bone_ptr);
    }
  }

  BLI_gset_free(added_bones, nullptr);

  if (bone_count == 0) {
    return false;
  }
  if (bone_count == 1) {
    BLI_freelistN(&bone_ref_list);
    return false;
  }

  /* Sort by depth or distance to cursor. */
  BLI_listbase_sort(&bone_ref_list, [](const void *a, const void *b) {
    return int(static_cast<const BoneRefWithDepth *>(a)->depth_id >
               static_cast<const BoneRefWithDepth *>(b)->depth_id);
  });

  while (bone_count > SEL_MENU_SIZE) {
    BLI_freelinkN(&bone_ref_list, bone_ref_list.last);
    bone_count -= 1;
  }

  /* UI, full in static array values that we later use in an enum function */
  memset(object_mouse_select_menu_data, 0, sizeof(object_mouse_select_menu_data));

  int i;
  LISTBASE_FOREACH_INDEX (BoneRefWithDepth *, bone_ref, &bone_ref_list, i) {
    char *name;

    object_mouse_select_menu_data[i].base_ptr = bone_ref->base;

    if (is_editmode) {
      EditBone *ebone = bone_ref->ebone;
      object_mouse_select_menu_data[i].item_ptr = static_cast<void *>(ebone);
      name = ebone->name;
    }
    else {
      bPoseChannel *pchan = bone_ref->pchan;
      object_mouse_select_menu_data[i].item_ptr = static_cast<void *>(pchan);
      name = pchan->name;
    }

    BLI_strncpy_utf8(object_mouse_select_menu_data[i].idname, name, MAX_ID_NAME - 2);
    object_mouse_select_menu_data[i].icon = ICON_BONE_DATA;
  }

  wmOperatorType *ot = WM_operatortype_find("VIEW3D_OT_bone_select_menu", false);
  PointerRNA ptr;

  WM_operator_properties_create_ptr(&ptr, ot);
  RNA_boolean_set(&ptr, "extend", params.sel_op == SEL_OP_ADD);
  RNA_boolean_set(&ptr, "deselect", params.sel_op == SEL_OP_SUB);
  RNA_boolean_set(&ptr, "toggle", params.sel_op == SEL_OP_XOR);
  WM_operator_name_call_ptr(C, ot, blender::wm::OpCallContext::InvokeDefault, &ptr, nullptr);
  WM_operator_properties_free(&ptr);

  BLI_freelistN(&bone_ref_list);
  return true;
}

static bool selectbuffer_has_bones(const blender::Span<GPUSelectResult> hit_results)
{
  for (const GPUSelectResult &hit_result : hit_results) {
    if (hit_result.id & 0xFFFF0000) {
      return true;
    }
  }
  return false;
}

/* utility function for mixed_bones_object_selectbuffer */
static int selectbuffer_ret_hits_15(blender::MutableSpan<GPUSelectResult> /*hit_results*/,
                                    const int hits15)
{
  return hits15;
}

static int selectbuffer_ret_hits_9(blender::MutableSpan<GPUSelectResult> hit_results,
                                   const int hits15,
                                   const int hits9)
{
  const int ofs = hits15;
  /* Shift results to beginning. */
  hit_results.slice(0, hits9).copy_from(hit_results.slice(ofs, hits9));
  return hits9;
}

static int selectbuffer_ret_hits_5(blender::MutableSpan<GPUSelectResult> hit_results,
                                   const int hits15,
                                   const int hits9,
                                   const int hits5)
{
  const int ofs = hits15 + hits9;
  /* Shift results to beginning. */
  hit_results.slice(0, hits5).copy_from(hit_results.slice(ofs, hits5));
  return hits5;
}

/**
 * Populate a select buffer with objects and bones, if there are any.
 * Checks three selection levels and compare.
 *
 * \param do_nearest_xray: When set, read in hits that don't stop
 * at the nearest surface. The hits must still be ordered by depth.
 * Needed so we can step to the next, non-active object when it's already selected, see: #76445.
 */
static int mixed_bones_object_selectbuffer(const ViewContext *vc,
                                           GPUSelectBuffer *buffer,
                                           const int mval[2],
                                           eV3DSelectObjectFilter select_filter,
                                           bool do_nearest,
                                           bool do_nearest_xray,
                                           const bool do_material_slot_selection)
{
  rcti rect;
  int hits15, hits9 = 0, hits5 = 0;
  bool has_bones15 = false, has_bones9 = false, has_bones5 = false;

  eV3DSelectMode select_mode = (do_nearest ? VIEW3D_SELECT_PICK_NEAREST : VIEW3D_SELECT_PICK_ALL);
  int hits = 0;

  if (do_nearest_xray) {
    select_mode = VIEW3D_SELECT_PICK_ALL;
  }

  /* we _must_ end cache before return, use 'goto finally' */
  view3d_gpu_select_cache_begin();

  GPUSelectStorage &storage = buffer->storage;
  BLI_rcti_init_pt_radius(&rect, mval, 14);
  hits15 = view3d_gpu_select_ex(
      vc, buffer, &rect, select_mode, select_filter, do_material_slot_selection);
  if (hits15 == 1) {
    hits = selectbuffer_ret_hits_15(storage.as_mutable_span(), hits15);
    goto finally;
  }
  else if (hits15 > 0) {
    int ofs;
    has_bones15 = selectbuffer_has_bones(storage.as_span().slice(0, hits15));

    ofs = hits15;
    BLI_rcti_init_pt_radius(&rect, mval, 9);
    hits9 = view3d_gpu_select(vc, buffer, &rect, select_mode, select_filter);
    if (hits9 == 1) {
      hits = selectbuffer_ret_hits_9(storage.as_mutable_span(), hits15, hits9);
      goto finally;
    }
    else if (hits9 > 0) {
      has_bones9 = selectbuffer_has_bones(storage.as_span().slice(ofs, hits9));

      ofs += hits9;
      BLI_rcti_init_pt_radius(&rect, mval, 5);
      hits5 = view3d_gpu_select(vc, buffer, &rect, select_mode, select_filter);
      if (hits5 == 1) {
        hits = selectbuffer_ret_hits_5(storage.as_mutable_span(), hits15, hits9, hits5);
        goto finally;
      }
      else if (hits5 > 0) {
        has_bones5 = selectbuffer_has_bones(storage.as_span().slice(ofs, hits5));
      }
    }

    if (has_bones5) {
      hits = selectbuffer_ret_hits_5(storage.as_mutable_span(), hits15, hits9, hits5);
      goto finally;
    }
    else if (has_bones9) {
      hits = selectbuffer_ret_hits_9(storage.as_mutable_span(), hits15, hits9);
      goto finally;
    }
    else if (has_bones15) {
      hits = selectbuffer_ret_hits_15(storage.as_mutable_span(), hits15);
      goto finally;
    }

    if (hits5 > 0) {
      hits = selectbuffer_ret_hits_5(storage.as_mutable_span(), hits15, hits9, hits5);
      goto finally;
    }
    else if (hits9 > 0) {
      hits = selectbuffer_ret_hits_9(storage.as_mutable_span(), hits15, hits9);
      goto finally;
    }
    else {
      hits = selectbuffer_ret_hits_15(storage.as_mutable_span(), hits15);
      goto finally;
    }
  }

finally:
  view3d_gpu_select_cache_end();
  return hits;
}

static int mixed_bones_object_selectbuffer_extended(const ViewContext *vc,
                                                    GPUSelectBuffer *buffer,
                                                    const int mval[2],
                                                    eV3DSelectObjectFilter select_filter,
                                                    bool use_cycle,
                                                    bool enumerate,
                                                    bool *r_do_nearest)
{
  bool do_nearest = false;
  View3D *v3d = vc->v3d;

  /* define if we use solid nearest select or not */
  if (use_cycle) {
    /* Update the coordinates (even if the return value isn't used). */
    const bool has_motion = WM_cursor_test_motion_and_update(mval);
    if (!XRAY_ACTIVE(v3d)) {
      do_nearest = has_motion;
    }
  }
  else {
    if (!XRAY_ACTIVE(v3d)) {
      do_nearest = true;
    }
  }

  if (r_do_nearest) {
    *r_do_nearest = do_nearest;
  }

  do_nearest = do_nearest && !enumerate;

  int hits = mixed_bones_object_selectbuffer(
      vc, buffer, mval, select_filter, do_nearest, true, false);

  return hits;
}

/**
 * Compare result of `GPU_select`: #GPUSelectResult,
 * Needed for stable sorting, so cycling through all items near the cursor behaves predictably.
 */
static int gpu_select_buffer_depth_id_cmp(const void *sel_a_p, const void *sel_b_p)
{
  GPUSelectResult *a = (GPUSelectResult *)sel_a_p;
  GPUSelectResult *b = (GPUSelectResult *)sel_b_p;

  if (a->depth < b->depth) {
    return -1;
  }
  if (a->depth > b->depth) {
    return 1;
  }

  /* Depths match, sort by id. */
  /* NOTE: this is endianness-sensitive.
   * GPUSelectResult values are always expected to be little-endian. */
  uint sel_a = a->id;
  uint sel_b = b->id;

  if (sel_a < sel_b) {
    return -1;
  }
  if (sel_a > sel_b) {
    return 1;
  }
  return 0;
}

/**
 * \param has_bones: When true, skip non-bone hits, also allow bases to be used
 * that are visible but not select-able,
 * since you may be in pose mode with an un-selectable object.
 *
 * \return the active base or nullptr.
 */
static Base *mouse_select_eval_buffer(const ViewContext *vc,
                                      const GPUSelectBuffer &buffer,
                                      int hits,
                                      bool do_nearest,
                                      bool has_bones,
                                      bool do_bones_get_priotity,
                                      int *r_select_id_subelem)
{
  Scene *scene = vc->scene;
  ViewLayer *view_layer = vc->view_layer;
  View3D *v3d = vc->v3d;
  int a;

  bool found = false;
  int select_id = 0;
  int select_id_subelem = 0;

  if (do_nearest) {
    uint min = 0xFFFFFFFF;
    int hit_index = -1;

    if (has_bones && do_bones_get_priotity) {
      /* we skip non-bone hits */
      for (a = 0; a < hits; a++) {
        if (min > buffer.storage[a].depth && (buffer.storage[a].id & 0xFFFF0000)) {
          min = buffer.storage[a].depth;
          hit_index = a;
        }
      }
    }
    else {

      for (a = 0; a < hits; a++) {
        /* Any object. */
        if (min > buffer.storage[a].depth) {
          min = buffer.storage[a].depth;
          hit_index = a;
        }
      }
    }

    if (hit_index != -1) {
      select_id = buffer.storage[hit_index].id & 0xFFFF;
      select_id_subelem = (buffer.storage[hit_index].id & 0xFFFF0000) >> 16;
      found = true;
      /* No need to set `min` to `buffer.storage[hit_index].depth`, it's not used from now on. */
    }
  }
  else {

    GPUSelectStorage buffer_sorted = buffer.storage;
    {
      buffer_sorted.resize(hits);
      /* Remove non-bone objects. */
      if (has_bones && do_bones_get_priotity) {
        /* Loop backwards to reduce re-ordering. */
        for (a = hits - 1; a >= 0; a--) {
          if ((buffer_sorted[a].id & 0xFFFF0000) == 0) {
            buffer_sorted[a] = buffer_sorted[--hits];
          }
        }
      }
      qsort(buffer_sorted.data(), hits, sizeof(GPUSelectResult), gpu_select_buffer_depth_id_cmp);
    }

    int hit_index = -1;

    /* It's possible there are no hits (all objects contained bones). */
    if (hits > 0) {
      /* Only exclude active object when it is selected. */
      BKE_view_layer_synced_ensure(scene, view_layer);
      Base *base = BKE_view_layer_active_base_get(view_layer);
      if (base && (base->flag & BASE_SELECTED)) {
        const int select_id_active = base->object->runtime->select_id;
        for (int i_next = 0, i_prev = hits - 1; i_next < hits; i_prev = i_next++) {
          if ((select_id_active == (buffer_sorted[i_prev].id & 0xFFFF)) &&
              (select_id_active != (buffer_sorted[i_next].id & 0xFFFF)))
          {
            hit_index = i_next;
            break;
          }
        }
      }

      /* When the active object is unselected or not in `buffer`, use the nearest. */
      if (hit_index == -1) {
        /* Just pick the nearest. */
        hit_index = 0;
      }
    }

    if (hit_index != -1) {
      select_id = buffer_sorted[hit_index].id & 0xFFFF;
      select_id_subelem = (buffer_sorted[hit_index].id & 0xFFFF0000) >> 16;
      found = true;
    }
  }

  Base *basact = nullptr;
  if (found) {
    BKE_view_layer_synced_ensure(scene, view_layer);
    LISTBASE_FOREACH (Base *, base, BKE_view_layer_object_bases_get(view_layer)) {
      if (has_bones ? BASE_VISIBLE(v3d, base) : BASE_SELECTABLE(v3d, base)) {
        if (base->object->runtime->select_id == select_id) {
          basact = base;
          break;
        }
      }
    }

    if (basact && r_select_id_subelem) {
      *r_select_id_subelem = select_id_subelem;
    }
  }

  return basact;
}

static Base *mouse_select_object_center(const ViewContext *vc, Base *startbase, const int mval[2])
{
  ARegion *region = vc->region;
  Scene *scene = vc->scene;
  ViewLayer *view_layer = vc->view_layer;
  View3D *v3d = vc->v3d;

  BKE_view_layer_synced_ensure(scene, view_layer);
  Base *oldbasact = BKE_view_layer_active_base_get(view_layer);

  const float mval_fl[2] = {float(mval[0]), float(mval[1])};
  float dist = ED_view3d_select_dist_px() * 1.3333f;
  Base *basact = nullptr;

  /* Put the active object at a disadvantage to cycle through other objects. */
  const float penalty_dist = 10.0f * UI_SCALE_FAC;
  Base *base = startbase;
  while (base) {
    if (BASE_SELECTABLE(v3d, base)) {
      float screen_co[2];
      if (ED_view3d_project_float_global(region,
                                         base->object->object_to_world().location(),
                                         screen_co,
                                         V3D_PROJ_TEST_CLIP_DEFAULT) == V3D_PROJ_RET_OK)
      {
        float dist_test = len_manhattan_v2v2(mval_fl, screen_co);
        if (base == oldbasact) {
          dist_test += penalty_dist;
        }
        if (dist_test < dist) {
          dist = dist_test;
          basact = base;
        }
      }
    }
    base = base->next;

    if (base == nullptr) {
      base = static_cast<Base *>(BKE_view_layer_object_bases_get(view_layer)->first);
    }
    if (base == startbase) {
      break;
    }
  }
  return basact;
}

static Base *ed_view3d_give_base_under_cursor_ex(bContext *C,
                                                 const int mval[2],
                                                 int *r_material_slot)
{
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Base *basact = nullptr;
  GPUSelectBuffer buffer;

  /* setup view context for argument to callbacks */
  view3d_operator_needs_gpu(C);
  BKE_object_update_select_id(CTX_data_main(C));

  const ViewContext vc = ED_view3d_viewcontext_init(C, depsgraph);

  const bool do_nearest = !XRAY_ACTIVE(vc.v3d);
  const bool do_material_slot_selection = r_material_slot != nullptr;
  const int hits = mixed_bones_object_selectbuffer(
      &vc, &buffer, mval, VIEW3D_SELECT_FILTER_NOP, do_nearest, false, do_material_slot_selection);

  if (hits > 0) {
    const bool has_bones = (r_material_slot == nullptr) &&
                           selectbuffer_has_bones(buffer.storage.as_span().slice(0, hits));
    basact = mouse_select_eval_buffer(
        &vc, buffer, hits, do_nearest, has_bones, true, r_material_slot);
  }

  return basact;
}

Base *ED_view3d_give_base_under_cursor(bContext *C, const int mval[2])
{
  return ed_view3d_give_base_under_cursor_ex(C, mval, nullptr);
}

Object *ED_view3d_give_object_under_cursor(bContext *C, const int mval[2])
{
  Base *base = ED_view3d_give_base_under_cursor(C, mval);
  if (base) {
    return base->object;
  }
  return nullptr;
}

Object *ED_view3d_give_material_slot_under_cursor(bContext *C,
                                                  const int mval[2],
                                                  int *r_material_slot)
{
  Base *base = ed_view3d_give_base_under_cursor_ex(C, mval, r_material_slot);
  if (base) {
    return base->object;
  }
  return nullptr;
}

bool ED_view3d_is_object_under_cursor(bContext *C, const int mval[2])
{
  return ED_view3d_give_object_under_cursor(C, mval) != nullptr;
}

static void deselect_all_tracks(MovieTracking *tracking)
{
  LISTBASE_FOREACH (MovieTrackingObject *, tracking_object, &tracking->objects) {
    LISTBASE_FOREACH (MovieTrackingTrack *, track, &tracking_object->tracks) {
      BKE_tracking_track_deselect(track, TRACK_AREA_ALL);
    }
  }
}

static bool ed_object_select_pick_camera_track(bContext *C,
                                               Scene *scene,
                                               Base *basact,
                                               MovieClip *clip,
                                               const GPUSelectBuffer &buffer,
                                               const short hits,
                                               const SelectPick_Params &params)
{
  bool changed = false;
  bool found = false;

  MovieTracking *tracking = &clip->tracking;
  ListBase *tracksbase = nullptr;
  MovieTrackingTrack *track = nullptr;

  for (int i = 0; i < hits; i++) {
    const int select_id = buffer.storage[i].id;

    /* If there's bundles in buffer select bundles first,
     * so non-camera elements should be ignored in buffer. */
    if (basact->object->runtime->select_id != (select_id & 0xFFFF)) {
      continue;
    }
    /* Index of bundle is 1<<16-based. if there's no "bone" index
     * in height word, this buffer value belongs to camera. not to bundle. */
    if ((select_id & 0xFFFF0000) == 0) {
      continue;
    }

    track = BKE_tracking_track_get_for_selection_index(
        &clip->tracking, select_id >> 16, &tracksbase);
    found = true;
    break;
  }

  /* Note `params.deselect_all` is ignored for tracks as in this case
   * all objects will be de-selected (not tracks). */
  if (params.sel_op == SEL_OP_SET) {
    if ((found && params.select_passthrough) && TRACK_SELECTED(track)) {
      found = false;
    }
    else if (found /* `|| params.deselect_all` */) {
      /* Deselect everything. */
      deselect_all_tracks(tracking);
      changed = true;
    }
  }

  if (found) {
    switch (params.sel_op) {
      case SEL_OP_ADD: {
        BKE_tracking_track_select(tracksbase, track, TRACK_AREA_ALL, true);
        break;
      }
      case SEL_OP_SUB: {
        BKE_tracking_track_deselect(track, TRACK_AREA_ALL);
        break;
      }
      case SEL_OP_XOR: {
        if (TRACK_SELECTED(track)) {
          BKE_tracking_track_deselect(track, TRACK_AREA_ALL);
        }
        else {
          BKE_tracking_track_select(tracksbase, track, TRACK_AREA_ALL, true);
        }
        break;
      }
      case SEL_OP_SET: {
        BKE_tracking_track_select(tracksbase, track, TRACK_AREA_ALL, false);
        break;
      }
      case SEL_OP_AND: {
        BLI_assert_unreachable(); /* Doesn't make sense for picking. */
        break;
      }
    }

    DEG_id_tag_update(&scene->id, ID_RECALC_SELECT);
    DEG_id_tag_update(&clip->id, ID_RECALC_SELECT);
    WM_event_add_notifier(C, NC_MOVIECLIP | ND_SELECT, track);
    WM_event_add_notifier(C, NC_SCENE | ND_OB_SELECT, scene);

    changed = true;
  }

  return changed || found;
}

/**
 * Cursor selection picking for object & pose-mode.
 *
 * \param mval: Region relative cursor coordinates.
 * \param params: Selection parameters.
 * \param center: Select by the cursors on-screen distances to the center/origin
 * instead of the geometry any other contents of the item being selected.
 * This could be used to select by bones by their origin too, currently it's only used for objects.
 * \param enumerate: Show a menu for objects at the cursor location.
 * Otherwise fall-through to non-menu selection.
 * \param object_only: Only select objects (not bones / track markers).
 */
static bool ed_object_select_pick(bContext *C,
                                  const int mval[2],
                                  const SelectPick_Params &params,
                                  const bool center,
                                  const bool enumerate,
                                  const bool object_only)
{
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  /* Setup view context for argument to callbacks. */
  ViewContext vc = ED_view3d_viewcontext_init(C, depsgraph);

  Scene *scene = vc.scene;
  View3D *v3d = vc.v3d;

  /* Menu activation may find a base to make active (if it only finds a single item to select). */
  Base *basact_override = nullptr;

  const bool is_obedit = (vc.obedit != nullptr);
  if (object_only) {
    /* Signal for #view3d_gpu_select to skip edit-mode objects. */
    vc.obedit = nullptr;
  }

  /* Set for GPU depth buffer picking, leave null when selecting by center. */
  struct GPUData {
    GPUSelectBuffer buffer;
    int hits;
    bool do_nearest;
    bool has_bones;
  } *gpu = nullptr;

  /* First handle menu selection, early exit if a menu opens
   * since this takes ownership of the selection action.
   *
   * Even when there is no menu `basact_override` may be set to avoid having to re-find
   * the item under the cursor. */

  if (center == false) {
    gpu = MEM_new<GPUData>(__func__);
    gpu->do_nearest = false;
    gpu->has_bones = false;

    /* If objects have pose-mode set, the bones are in the same selection buffer. */
    const eV3DSelectObjectFilter select_filter = ((object_only == false) ?
                                                      ED_view3d_select_filter_from_mode(scene,
                                                                                        vc.obact) :
                                                      VIEW3D_SELECT_FILTER_NOP);
    gpu->hits = mixed_bones_object_selectbuffer_extended(
        &vc, &gpu->buffer, mval, select_filter, true, enumerate, &gpu->do_nearest);
    gpu->has_bones = (object_only && gpu->hits > 0) ?
                         false :
                         selectbuffer_has_bones(gpu->buffer.storage.as_span().slice(0, gpu->hits));
  }

  /* First handle menu selection, early exit when a menu was opened.
   * Otherwise fall through to regular selection. */
  if (enumerate) {
    bool has_menu = false;
    if (center) {
      if (object_mouse_select_menu(C, &vc, {}, mval, params, &basact_override)) {
        has_menu = true;
      }
    }
    else {
      if (gpu->hits != 0) {
        const blender::Span<GPUSelectResult> hit_results = gpu->buffer.storage.as_span().slice(
            0, gpu->hits);
        if (gpu->has_bones && bone_mouse_select_menu(C, hit_results, false, params)) {
          has_menu = true;
        }
        else if (object_mouse_select_menu(C, &vc, hit_results, mval, params, &basact_override)) {
          has_menu = true;
        }
      }
    }

    /* Let the menu handle any further actions. */
    if (has_menu) {
      if (gpu != nullptr) {
        MEM_delete(gpu);
      }
      return false;
    }
  }

  /* No menu, continue with selection. */

  ViewLayer *view_layer = vc.view_layer;
  BKE_view_layer_synced_ensure(scene, view_layer);
  /* Don't set when the context has no active object (hidden), see: #60807. */
  const Base *oldbasact = vc.obact ? BKE_view_layer_active_base_get(view_layer) : nullptr;
  /* Always start list from `basact` when cycling the selection. */
  Base *startbase = (oldbasact && oldbasact->next) ?
                        oldbasact->next :
                        static_cast<Base *>(BKE_view_layer_object_bases_get(view_layer)->first);

  /* The next object's base to make active. */
  Base *basact = nullptr;
  const eObjectMode object_mode = oldbasact ? static_cast<eObjectMode>(oldbasact->object->mode) :
                                              OB_MODE_OBJECT;
  /* For the most part this is equivalent to `(object_mode & OB_MODE_POSE) != 0`
   * however this logic should also run with weight-paint + pose selection.
   * Without this, selection in weight-paint mode can de-select armatures which isn't useful,
   * see: #101686. */
  const bool has_pose_old = (oldbasact &&
                             BKE_object_pose_armature_get_with_wpaint_check(oldbasact->object));

  /* When enabled, don't attempt any further selection. */
  bool handled = false;

  /* Split `changed` into data-types so their associated updates can be properly performed.
   * This is also needed as multiple changes may happen at once.
   * Selecting a pose-bone or track can also select the object for example */
  bool changed_object = false;
  bool changed_pose = false;
  bool changed_track = false;

  /* Handle setting the new base active (even when `handled == true`). */
  bool use_activate_selected_base = false;

  if (center) {
    if (basact_override) {
      basact = basact_override;
    }
    else {
      basact = mouse_select_object_center(&vc, startbase, mval);
    }
  }
  else {
    if (basact_override) {
      basact = basact_override;
    }
    else {
      /* Regarding bone priority.
       *
       * - When in pose-bone, it's useful that any selection containing a bone
       *   gets priority over other geometry (background scenery for example).
       *
       * - When in object-mode, don't prioritize bones as it would cause
       *   pose-objects behind other objects to get priority
       *   (mainly noticeable when #SCE_OBJECT_MODE_LOCK is disabled).
       *
       * This way prioritizing based on pose-mode has a bias to stay in pose-mode
       * without having to enforce this through locking the object mode. */
      bool do_bones_get_priotity = has_pose_old;

      basact = (gpu->hits > 0) ? mouse_select_eval_buffer(&vc,
                                                          gpu->buffer,
                                                          gpu->hits,
                                                          gpu->do_nearest,
                                                          gpu->has_bones,
                                                          do_bones_get_priotity,
                                                          nullptr) :
                                 nullptr;
    }

    /* See comment for `has_pose_old`, the same rationale applies here. */
    const bool has_pose_new = (basact &&
                               BKE_object_pose_armature_get_with_wpaint_check(basact->object));

    /* Select pose-bones or camera-tracks. */
    if (((gpu->hits > 0) && gpu->has_bones) ||
        /* Special case, even when there are no hits, pose logic may de-select all bones. */
        ((gpu->hits == 0) && has_pose_old))
    {
      /* Regarding the `basact` null checks.
       * While it's unlikely there are GPU hits *without* `basact` being found,
       * it's possible looking up the selection index fails, see: #143161. */

      if (basact && (gpu->has_bones && (basact->object->type == OB_CAMERA))) {
        MovieClip *clip = BKE_object_movieclip_get(scene, basact->object, false);
        if (clip != nullptr) {
          if (ed_object_select_pick_camera_track(
                  C, scene, basact, clip, gpu->buffer, gpu->hits, params))
          {
            blender::ed::object::base_select(basact, blender::ed::object::BA_SELECT);
            /* Don't set `handled` here as the object activation may be necessary. */
            changed_object = true;

            changed_track = true;
          }
          else {
            /* Fallback to regular object selection if no new bundles were selected,
             * allows to select object parented to reconstruction object. */
            basact = mouse_select_eval_buffer(
                &vc, gpu->buffer, gpu->hits, gpu->do_nearest, false, false, nullptr);
          }
        }
      }
      else if ((basact || oldbasact) &&
               ED_armature_pose_select_pick_with_buffer(scene,
                                                        view_layer,
                                                        v3d,
                                                        basact ? basact : (Base *)oldbasact,
                                                        gpu->buffer.storage.data(),
                                                        gpu->hits,
                                                        params,
                                                        gpu->do_nearest))
      {

        changed_pose = true;

        /* When there is no `baseact` this will have operated on `oldbasact`,
         * allowing #SelectPick_Params.deselect_all work in pose-mode.
         * In this case no object operations are needed. */
        if (basact == nullptr) {
          handled = true;
        }
        else {
          /* By convention the armature-object is selected when in pose-mode.
           * While leaving it unselected will work, leaving pose-mode would leave the object
           * active + unselected which isn't ideal when performing other actions on the object. */
          blender::ed::object::base_select(basact, blender::ed::object::BA_SELECT);
          changed_object = true;

          WM_event_add_notifier(C, NC_OBJECT | ND_BONE_SELECT, basact->object);
          WM_event_add_notifier(C, NC_OBJECT | ND_BONE_ACTIVE, basact->object);

          /* In weight-paint, we use selected bone to select vertex-group.
           * In this case the active object mustn't change as it would leave weight-paint mode. */
          if (oldbasact) {
            if (oldbasact->object->mode & OB_MODE_ALL_WEIGHT_PAINT) {
              /* Prevent activating.
               * Selection causes this to be considered the 'active' pose in weight-paint mode.
               * Eventually this limitation may be removed.
               * For now, de-select all other pose objects deforming this mesh. */
              ED_armature_pose_select_in_wpaint_mode(scene, view_layer, basact);

              handled = true;
            }
            else if (has_pose_old && has_pose_new) {
              /* Within pose-mode, keep the current selection when switching pose bones,
               * this is noticeable when in pose mode with multiple objects at once.
               * Where selecting the bone of a different object would de-select this one.
               * After that, exiting pose-mode would only have the active armature selected.
               * This matches multi-object edit-mode behavior. */
              handled = true;

              if (oldbasact != basact) {
                use_activate_selected_base = true;
              }
            }
            else {
              /* Don't set `handled` here as the object selection may be necessary
               * when starting out in object-mode and moving into pose-mode,
               * when moving from pose to object-mode using object selection also makes sense. */
            }
          }
        }
      }
      /* Prevent bone/track selecting to pass on to object selecting. */
      if (basact == oldbasact) {
        handled = true;
      }
    }
  }

  bool changed_object_mode = false;

  if (handled == false) {
    if (scene->toolsettings->object_flag & SCE_OBJECT_MODE_LOCK) {
      /* No special logic in edit-mode. */
      if (is_obedit == false) {
        if (basact && !BKE_object_is_mode_compat(basact->object, object_mode)) {
          if (object_mode == OB_MODE_OBJECT) {
            Main *bmain = vc.bmain;
            blender::ed::object::mode_generic_exit(bmain, vc.depsgraph, scene, basact->object);
          }
          if (!BKE_object_is_mode_compat(basact->object, object_mode)) {
            basact = nullptr;
          }
        }

        /* Disallow switching modes,
         * special exception for edit-mode - vertex-parent operator. */
        if (basact && oldbasact) {
          if ((oldbasact->object->mode != basact->object->mode) &&
              (oldbasact->object->mode & basact->object->mode) == 0)
          {
            basact = nullptr;
          }
        }
      }
    }
    else {
      if (basact) {
        if (!BKE_object_is_mode_compat(basact->object, object_mode)) {
          changed_object_mode = true;
        }
      }
      else if (object_mode != OB_MODE_OBJECT) {
        changed_object_mode = true;
      }
    }
  }

  /* Ensure code above doesn't change the active base. This code is already fairly involved,
   * it's best if changing the active object is localized to a single place. */
  BLI_assert(oldbasact == (vc.obact ? BKE_view_layer_active_base_get(view_layer) : nullptr));

  if (handled) {
    /* Pass. */
  }
  else if (basact && vc.obedit) {
    /* Only do the select (use for setting vertex parents & hooks).
     * In edit-mode do not activate. */
    object_deselect_all_except(scene, view_layer, basact);
    blender::ed::object::base_select(basact, blender::ed::object::BA_SELECT);

    changed_object = true;
  }
  else {
    /* Object-mode (pose mode will have been handled already)
     * unless entering pose-mode from object selection (handled by `changed_object_mode`). */

    /* NOTE(@ideasman42): When select changes object-mode it doesn't make sense to use
     * pass-through.
     *
     * - When object-mode locking is disabled:
     *   Selecting another already selected object does not need to make it active,
     *   allowing a cursor-drag to move this objects as well as other selected objects.
     * - When object-mode locking is enabled:
     *   Selecting an object in a different mode (typically pose-mode) must prioritize
     *   entering that mode which requires making the object active, further, the selection
     *   before switching modes wont make sense in the newly entered mode,
     *   so it makes sense to disable pass-through logic in this case.
     *
     * See: #115181 for details. */
    const bool select_passthrough = params.select_passthrough && (changed_object_mode == false);

    bool found = (basact != nullptr) && BASE_SELECTABLE(v3d, basact);
    if (params.sel_op == SEL_OP_SET) {
      if ((found && select_passthrough) && (basact->flag & BASE_SELECTED)) {
        found = false;
      }
      else if (found || params.deselect_all) {
        /* Deselect everything. */
        /* `basact` may be nullptr. */
        if (object_deselect_all_except(scene, view_layer, basact)) {
          changed_object = true;
        }
      }
    }

    if (found) {
      use_activate_selected_base |= (oldbasact != basact) && (is_obedit == false);

      switch (params.sel_op) {
        case SEL_OP_ADD: {
          blender::ed::object::base_select(basact, blender::ed::object::BA_SELECT);
          break;
        }
        case SEL_OP_SUB: {
          blender::ed::object::base_select(basact, blender::ed::object::BA_DESELECT);
          break;
        }
        case SEL_OP_XOR: {
          if (basact->flag & BASE_SELECTED) {
            /* Keep selected if the base is to be activated. */
            if (use_activate_selected_base == false) {
              blender::ed::object::base_select(basact, blender::ed::object::BA_DESELECT);
            }
          }
          else {
            blender::ed::object::base_select(basact, blender::ed::object::BA_SELECT);
          }
          break;
        }
        case SEL_OP_SET: {
          /* Deselect has already been performed. */
          blender::ed::object::base_select(basact, blender::ed::object::BA_SELECT);
          break;
        }
        case SEL_OP_AND: {
          BLI_assert_unreachable(); /* Doesn't make sense for picking. */
          break;
        }
      }

      changed_object = true;
    }
  }

  /* Perform the activation even when 'handled', since this is used to ensure
   * the object from the pose-bone selected is also activated. */
  if (use_activate_selected_base && (basact != nullptr)) {
    changed_object = true;
    blender::ed::object::base_activate(C, basact); /* adds notifier */
    if ((scene->toolsettings->object_flag & SCE_OBJECT_MODE_LOCK) == 0) {
      WM_toolsystem_update_from_context_view3d(C);
    }
  }

  if (changed_object) {
    DEG_id_tag_update(&scene->id, ID_RECALC_SELECT);
    WM_event_add_notifier(C, NC_SCENE | ND_OB_SELECT, scene);

    ED_outliner_select_sync_from_object_tag(C);
  }

  if (changed_pose) {
    ED_outliner_select_sync_from_pose_bone_tag(C);
  }

  if (gpu != nullptr) {
    MEM_delete(gpu);
  }

  return (changed_object || changed_pose || changed_track);
}

/**
 * Mouse selection in weight paint.
 * Called via generic mouse select operator.
 *
 * \return True when pick finds an element or the selection changed.
 */
static bool ed_wpaint_vertex_select_pick(bContext *C,
                                         const int mval[2],
                                         const SelectPick_Params &params,
                                         Object *obact)
{
  using namespace blender;
  View3D *v3d = CTX_wm_view3d(C);
  const bool use_zbuf = !XRAY_ENABLED(v3d);

  Mesh *mesh = static_cast<Mesh *>(obact->data); /* already checked for nullptr */
  uint index = 0;
  bool changed = false;

  bool found = ED_mesh_pick_vert(C, obact, mval, ED_MESH_PICK_DEFAULT_VERT_DIST, use_zbuf, &index);

  bke::MutableAttributeAccessor attributes = mesh->attributes_for_write();
  bke::AttributeWriter<bool> select_vert = attributes.lookup_or_add_for_write<bool>(
      ".select_vert", bke::AttrDomain::Point);

  if (params.sel_op == SEL_OP_SET) {
    if ((found && params.select_passthrough) && select_vert.varray[index]) {
      found = false;
    }
    else if (found || params.deselect_all) {
      /* Deselect everything. */
      changed |= paintface_deselect_all_visible(C, obact, SEL_DESELECT, false);
    }
  }

  if (found) {
    switch (params.sel_op) {
      case SEL_OP_ADD: {
        select_vert.varray.set(index, true);
        break;
      }
      case SEL_OP_SUB: {
        select_vert.varray.set(index, false);
        break;
      }
      case SEL_OP_XOR: {
        select_vert.varray.set(index, !select_vert.varray[index]);
        break;
      }
      case SEL_OP_SET: {
        paintvert_deselect_all_visible(obact, SEL_DESELECT, false);
        select_vert.varray.set(index, true);
        break;
      }
      case SEL_OP_AND: {
        BLI_assert_unreachable(); /* Doesn't make sense for picking. */
        break;
      }
    }

    /* update mselect */
    if (select_vert.varray[index]) {
      BKE_mesh_mselect_active_set(mesh, index, ME_VSEL);
    }
    else {
      BKE_mesh_mselect_validate(mesh);
    }

    select_vert.finish();

    paintvert_flush_flags(obact);

    changed = true;
  }
  else {
    select_vert.finish();
  }

  if (changed) {
    paintvert_tag_select_update(C, obact);
  }

  return changed || found;
}

struct ClosestPointCloud {
  PointCloud *pointcloud = nullptr;
  blender::ed::pointcloud::FindClosestData elem;
};

/**
 * Cursor selection for all point cloud objects in edit mode.
 *
 * \returns true if the selection changed.
 */
static bool pointcloud_select_pick(bContext &C, const int2 mval, const SelectPick_Params &params)
{
  using namespace blender;
  using namespace blender::ed;
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(&C);
  /* Setup view context for argument to callbacks. */
  const ViewContext vc = ED_view3d_viewcontext_init(&C, depsgraph);

  const Vector<Base *> bases = BKE_view_layer_array_from_bases_in_edit_mode_unique_data(
      vc.scene, vc.view_layer, vc.v3d);

  const ClosestPointCloud closest = threading::parallel_reduce(
      bases.index_range(),
      1L,
      ClosestPointCloud(),
      [&](const IndexRange range, const ClosestPointCloud &init) {
        ClosestPointCloud new_closest = init;
        for (Base *base : bases.as_span().slice(range)) {
          Object &object = *base->object;
          PointCloud &pointcloud = *static_cast<PointCloud *>(object.data);
          const float4x4 projection = ED_view3d_ob_project_mat_get(vc.rv3d, &object);

          std::optional<pointcloud::FindClosestData> new_closest_elem =
              pointcloud::find_closest_point_to_screen_co(*vc.region,
                                                          pointcloud.positions(),
                                                          projection,
                                                          IndexMask(pointcloud.totpoint),
                                                          float2(mval),
                                                          ED_view3d_select_dist_px(),
                                                          new_closest.elem);
          if (new_closest_elem) {
            new_closest.elem = *new_closest_elem;
            new_closest.pointcloud = &pointcloud;
          }
        }
        return new_closest;
      },
      [](const ClosestPointCloud &a, const ClosestPointCloud &b) {
        return (a.elem.distance_sq < b.elem.distance_sq) ? a : b;
      });

  Array<bool> changed(bases.size(), false);
  if (params.deselect_all || params.sel_op == SEL_OP_SET) {
    threading::parallel_for(bases.index_range(), 1L, [&](const IndexRange range) {
      for (const int i : range) {
        PointCloud &pointcloud = *static_cast<PointCloud *>(bases[i]->object->data);
        if (!pointcloud::has_anything_selected(pointcloud)) {
          continue;
        }

        bke::GSpanAttributeWriter selection = pointcloud::ensure_selection_attribute(
            pointcloud, bke::AttrType::Bool);
        pointcloud::fill_selection_false(selection.span, IndexMask(pointcloud.totpoint));
        selection.finish();

        changed[i] = true;
      }
    });

    for (const int i : bases.index_range()) {
      if (changed[i]) {
        PointCloud &pointcloud = *static_cast<PointCloud *>(bases[i]->object->data);
        /* Use #ID_RECALC_GEOMETRY instead of #ID_RECALC_SELECT because it is handled as a
         * generic attribute for now. */
        DEG_id_tag_update(&pointcloud.id, ID_RECALC_GEOMETRY);
        WM_event_add_notifier(&C, NC_GEOM | ND_DATA, &pointcloud);
      }
    }
  }

  if (!closest.pointcloud) {
    return changed.as_span().contains(true);
  }

  bke::GSpanAttributeWriter selection = pointcloud::ensure_selection_attribute(
      *closest.pointcloud, bke::AttrType::Bool);
  curves::apply_selection_operation_at_index(selection.span, closest.elem.index, params.sel_op);
  selection.finish();

  /* Use #ID_RECALC_GEOMETRY instead of #ID_RECALC_SELECT because it is handled as a
   * generic attribute for now. */
  DEG_id_tag_update(&closest.pointcloud->id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(&C, NC_GEOM | ND_DATA, closest.pointcloud);

  return true;
}

struct ClosestCurveDataBlock {
  blender::StringRef selection_attribute_name;
  Curves *curves_id = nullptr;
  blender::ed::curves::FindClosestData elem;
};

/**
 * Cursor selection for all Curves objects in edit mode.
 *
 * \returns true if the selection changed.
 */
static bool ed_curves_select_pick(bContext &C, const int mval[2], const SelectPick_Params &params)
{
  using namespace blender;
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(&C);
  /* Setup view context for argument to callbacks. */
  const ViewContext vc = ED_view3d_viewcontext_init(&C, depsgraph);

  const Vector<Base *> bases = BKE_view_layer_array_from_bases_in_edit_mode_unique_data(
      vc.scene, vc.view_layer, vc.v3d);

  Curves &active_curves_id = *static_cast<Curves *>(vc.obedit->data);
  const bke::AttrDomain selection_domain = bke::AttrDomain(active_curves_id.selection_domain);

  const ClosestCurveDataBlock closest = threading::parallel_reduce(
      bases.index_range(),
      1L,
      ClosestCurveDataBlock(),
      [&](const IndexRange range, const ClosestCurveDataBlock &init) {
        ClosestCurveDataBlock new_closest = init;
        for (Base *base : bases.as_span().slice(range)) {
          Object &curves_ob = *base->object;
          Curves &curves_id = *static_cast<Curves *>(curves_ob.data);
          bke::crazyspace::GeometryDeformation deformation =
              bke::crazyspace::get_evaluated_curves_deformation(*vc.depsgraph, curves_ob);
          const bke::CurvesGeometry &curves = curves_id.geometry.wrap();
          const float4x4 projection = ED_view3d_ob_project_mat_get(vc.rv3d, &curves_ob);
          const IndexMask elements(curves.attributes().domain_size(selection_domain));
          const auto range_consumer =
              [&](IndexRange range, Span<float3> positions, StringRef selection_attribute_name) {
                IndexMask mask = elements.slice_content(range);

                std::optional<ed::curves::FindClosestData> new_closest_elem =
                    ed::curves::closest_elem_find_screen_space(vc,
                                                               curves.points_by_curve(),
                                                               positions,
                                                               curves.cyclic(),
                                                               projection,
                                                               mask,
                                                               selection_domain,
                                                               mval,
                                                               new_closest.elem);
                if (new_closest_elem) {
                  new_closest.selection_attribute_name = selection_attribute_name;
                  new_closest.elem = *new_closest_elem;
                  new_closest.curves_id = &curves_id;
                }
              };

          if (selection_domain == bke::AttrDomain::Point) {
            ed::curves::foreach_selectable_point_range(
                curves,
                deformation,
                eHandleDisplay(vc.v3d->overlay.handle_display),
                range_consumer);
          }
          else if (selection_domain == bke::AttrDomain::Curve) {
            ed::curves::foreach_selectable_curve_range(
                curves,
                deformation,
                eHandleDisplay(vc.v3d->overlay.handle_display),
                range_consumer);
          };
        }
        return new_closest;
      },
      [](const ClosestCurveDataBlock &a, const ClosestCurveDataBlock &b) {
        return (a.elem.distance_sq < b.elem.distance_sq) ? a : b;
      });

  Array<bool> changed(bases.size(), false);
  if (params.deselect_all || params.sel_op == SEL_OP_SET) {
    threading::parallel_for(bases.index_range(), 1L, [&](const IndexRange range) {
      for (const int i : range) {
        Curves &curves_id = *static_cast<Curves *>(bases[i]->object->data);
        bke::CurvesGeometry &curves = curves_id.geometry.wrap();
        if (!ed::curves::has_anything_selected(curves, selection_domain)) {
          continue;
        }

        ed::curves::foreach_selection_attribute_writer(
            curves, selection_domain, [](bke::GSpanAttributeWriter &selection) {
              ed::curves::fill_selection_false(selection.span);
            });

        changed[i] = true;
      }
    });

    for (const int i : bases.index_range()) {
      if (changed[i]) {
        Curves &curves_id = *static_cast<Curves *>(bases[i]->object->data);
        /* Use #ID_RECALC_GEOMETRY instead of #ID_RECALC_SELECT because it is handled as a
         * generic attribute for now. */
        DEG_id_tag_update(&curves_id.id, ID_RECALC_GEOMETRY);
        WM_event_add_notifier(&C, NC_GEOM | ND_DATA, &curves_id);
      }
    }
  }

  if (!closest.curves_id) {
    return changed.as_span().contains(true);
  }

  if (selection_domain == bke::AttrDomain::Point) {
    bke::GSpanAttributeWriter selection = ed::curves::ensure_selection_attribute(
        closest.curves_id->geometry.wrap(),
        bke::AttrDomain::Point,
        bke::AttrType::Bool,
        closest.selection_attribute_name);
    ed::curves::apply_selection_operation_at_index(
        selection.span, closest.elem.index, params.sel_op);
    selection.finish();
  }
  else if (selection_domain == bke::AttrDomain::Curve) {
    ed::curves::foreach_selection_attribute_writer(
        closest.curves_id->geometry.wrap(),
        bke::AttrDomain::Curve,
        [&](bke::GSpanAttributeWriter &selection) {
          ed::curves::apply_selection_operation_at_index(
              selection.span, closest.elem.index, params.sel_op);
        });
  }

  /* Use #ID_RECALC_GEOMETRY instead of #ID_RECALC_SELECT because it is handled as a
   * generic attribute for now. */
  DEG_id_tag_update(&closest.curves_id->id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(&C, NC_GEOM | ND_DATA, closest.curves_id);

  return true;
}

struct ClosestGreasePencilDrawing {
  blender::StringRef selection_attribute_name;
  int info_index = -1;
  blender::bke::greasepencil::Drawing *drawing = nullptr;
  blender::ed::curves::FindClosestData elem;
};

/**
 * Cursor selection for all Grease Pencil curves in edit mode.
 *
 * \returns true if the selection changed.
 */
static bool ed_grease_pencil_select_pick(bContext *C,
                                         const int mval[2],
                                         const SelectPick_Params &params)
{
  using namespace blender;
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  /* Setup view context for argument to callbacks. */
  const ViewContext vc = ED_view3d_viewcontext_init(C, depsgraph);
  Object *object = (vc.obedit ? vc.obedit : vc.obact);

  /* Collect editable drawings. */
  const Object *ob_eval = DEG_get_evaluated(vc.depsgraph, object);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object->data);
  const Vector<ed::greasepencil::MutableDrawingInfo> drawings =
      ed::greasepencil::retrieve_editable_drawings(*vc.scene, grease_pencil);

  /* Get selection domain from tool settings. */
  const bke::AttrDomain selection_domain = ED_grease_pencil_selection_domain_get(
      vc.scene->toolsettings, object);

  const ClosestGreasePencilDrawing closest = threading::parallel_reduce(
      drawings.index_range(),
      1L,
      ClosestGreasePencilDrawing(),
      [&](const IndexRange range, const ClosestGreasePencilDrawing &init) {
        ClosestGreasePencilDrawing new_closest = init;
        for (const int i : range) {
          ed::greasepencil::MutableDrawingInfo info = drawings[i];
          const bke::greasepencil::Layer &layer = grease_pencil.layer(info.layer_index);
          /* Get deformation by modifiers. */
          bke::crazyspace::GeometryDeformation deformation =
              bke::crazyspace::get_evaluated_grease_pencil_drawing_deformation(
                  ob_eval, *object, info.drawing);

          IndexMaskMemory memory;
          const IndexMask elements = ed::greasepencil::retrieve_editable_elements(
              *object, info, selection_domain, memory);
          if (elements.is_empty()) {
            continue;
          }
          const IndexMask visible_handle_elements =
              ed::greasepencil::retrieve_visible_bezier_handle_elements(
                  *object,
                  info.drawing,
                  info.layer_index,
                  selection_domain,
                  vc.v3d->overlay.handle_display,
                  memory);
          const bke::CurvesGeometry &curves = info.drawing.strokes();
          const float4x4 layer_to_world = layer.to_world_space(*ob_eval);
          const float4x4 projection = ED_view3d_ob_project_mat_get_from_obmat(vc.rv3d,
                                                                              layer_to_world);
          const auto range_consumer = [&](const IndexRange range,
                                          const Span<float3> positions,
                                          const StringRef selection_attribute_name) {
            const IndexMask mask = ((selection_attribute_name == ".selection") ?
                                        elements :
                                        visible_handle_elements)
                                       .slice_content(range);

            std::optional<ed::curves::FindClosestData> new_closest_elem =
                ed::curves::closest_elem_find_screen_space(vc,
                                                           curves.points_by_curve(),
                                                           positions,
                                                           curves.cyclic(),
                                                           projection,
                                                           mask,
                                                           selection_domain,
                                                           mval,
                                                           new_closest.elem);
            if (new_closest_elem) {
              new_closest.selection_attribute_name = selection_attribute_name;
              new_closest.elem = *new_closest_elem;
              new_closest.info_index = i;
              new_closest.drawing = &info.drawing;
            }
          };

          if (selection_domain == bke::AttrDomain::Point) {
            ed::curves::foreach_selectable_point_range(
                curves,
                deformation,
                eHandleDisplay(vc.v3d->overlay.handle_display),
                range_consumer);
          }
          else if (selection_domain == bke::AttrDomain::Curve) {
            ed::curves::foreach_selectable_curve_range(
                curves,
                deformation,
                eHandleDisplay(vc.v3d->overlay.handle_display),
                range_consumer);
          };
        }
        return new_closest;
      },
      [](const ClosestGreasePencilDrawing &a, const ClosestGreasePencilDrawing &b) {
        return (a.elem.distance_sq < b.elem.distance_sq) ? a : b;
      });

  std::atomic<bool> deselected = false;
  if (params.deselect_all || params.sel_op == SEL_OP_SET) {
    threading::parallel_for(drawings.index_range(), 1L, [&](const IndexRange range) {
      for (const int i : range) {
        ed::greasepencil::MutableDrawingInfo info = drawings[i];
        IndexMaskMemory memory;
        const IndexMask elements = ed::greasepencil::retrieve_editable_elements(
            *object, info, selection_domain, memory);
        if (elements.is_empty()) {
          continue;
        }
        bke::CurvesGeometry &curves = info.drawing.strokes_for_write();
        if (!ed::curves::has_anything_selected(curves, selection_domain, elements)) {
          continue;
        }

        ed::curves::foreach_selection_attribute_writer(
            curves, selection_domain, [](bke::GSpanAttributeWriter &selection) {
              ed::curves::fill_selection_false(selection.span);
            });

        deselected = true;
      }
    });
  }

  if (!closest.drawing) {
    if (deselected) {
      /* Use #ID_RECALC_GEOMETRY instead of #ID_RECALC_SELECT because it is handled as a
       * generic attribute for now. */
      DEG_id_tag_update(&grease_pencil.id, ID_RECALC_GEOMETRY);
      WM_event_add_notifier(C, NC_GEOM | ND_DATA, &grease_pencil);
    }
    return deselected;
  }

  const IndexMask selection_mask = IndexRange::from_single(closest.elem.index);

  ed::greasepencil::selection_update(&vc,
                                     params.sel_op,
                                     [&](const ed::greasepencil::MutableDrawingInfo &info,
                                         const IndexMask & /*universe*/,
                                         StringRef attribute_name,
                                         IndexMaskMemory & /*memory*/) -> IndexMask {
                                       /* Selection update mask is already known, but only applies
                                        * to a specific drawing. */
                                       if (&info.drawing == closest.drawing &&
                                           attribute_name == closest.selection_attribute_name) {
                                         return selection_mask;
                                       }
                                       return {};
                                     });

  /* Use #ID_RECALC_GEOMETRY instead of #ID_RECALC_SELECT because it is handled as a
   * generic attribute for now. */
  DEG_id_tag_update(&grease_pencil.id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GEOM | ND_DATA, &grease_pencil);

  return true;
}

static wmOperatorStatus view3d_select_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  Object *obedit = CTX_data_edit_object(C);
  Object *obact = CTX_data_active_object(C);

  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  const ViewContext vc = ED_view3d_viewcontext_init(C, depsgraph);

  const SelectPick_Params params = ED_select_pick_params_from_operator(op->ptr);

  bool center = RNA_boolean_get(op->ptr, "center");
  bool enumerate = RNA_boolean_get(op->ptr, "enumerate");
  /* Only force object select for edit-mode to support vertex parenting,
   * or paint-select to allow pose bone select with vert/face select. */
  bool object_only = (RNA_boolean_get(op->ptr, "object") &&
                      (obedit || BKE_paint_select_elem_test(obact) ||
                       /* so its possible to select bones in weight-paint mode (LMB select) */
                       (obact && (obact->mode & OB_MODE_ALL_WEIGHT_PAINT) &&
                        BKE_object_pose_armature_get(obact))));

  /* This could be called "changed_or_found" since this is true when there is an element
   * under the cursor to select, even if it happens that the selection & active state doesn't
   * actually change. This is important so undo pushes are predictable. */
  bool changed = false;
  int mval[2];

  if (object_only) {
    obedit = nullptr;
    obact = nullptr;

    /* ack, this is incorrect but to do this correctly we would need an
     * alternative edit-mode/object-mode keymap, this copies the functionality
     * from 2.4x where Ctrl+Select in edit-mode does object select only. */
    center = false;
  }

  if (obedit && enumerate) {
    /* Enumerate makes no sense in edit-mode unless also explicitly picking objects or bones.
     * Pass the event through so the event may be handled by loop-select for example. See: #100204.
     */
    if (obedit->type != OB_ARMATURE) {
      return OPERATOR_PASS_THROUGH | OPERATOR_CANCELLED;
    }
  }

  RNA_int_get_array(op->ptr, "location", mval);

  view3d_operator_needs_gpu(C);
  BKE_object_update_select_id(CTX_data_main(C));

  if (obedit && object_only == false) {
    if (obedit->type == OB_MESH) {
      changed = EDBM_select_pick(C, mval, params);
    }
    else if (obedit->type == OB_ARMATURE) {
      if (enumerate) {
        GPUSelectBuffer buffer;
        const int hits = mixed_bones_object_selectbuffer(
            &vc, &buffer, mval, VIEW3D_SELECT_FILTER_NOP, false, true, false);
        changed = bone_mouse_select_menu(
            C, buffer.storage.as_span().take_front(hits), true, params);
      }
      if (!changed) {
        changed = ED_armature_edit_select_pick(C, mval, params);
      }
    }
    else if (obedit->type == OB_LATTICE) {
      changed = ED_lattice_select_pick(C, mval, params);
    }
    else if (ELEM(obedit->type, OB_CURVES_LEGACY, OB_SURF)) {
      changed = ED_curve_editnurb_select_pick(C, mval, ED_view3d_select_dist_px(), params);
    }
    else if (obedit->type == OB_MBALL) {
      changed = ED_mball_select_pick(C, mval, params);
    }
    else if (obedit->type == OB_FONT) {
      changed = ED_curve_editfont_select_pick(C, mval, params);
    }
    else if (obedit->type == OB_POINTCLOUD) {
      changed = pointcloud_select_pick(*C, mval, params);
    }
    else if (obedit->type == OB_CURVES) {
      changed = ed_curves_select_pick(*C, mval, params);
    }
    else if (obedit->type == OB_GREASE_PENCIL) {
      changed = ed_grease_pencil_select_pick(C, mval, params);
    }
  }
  else if (obact && obact->mode & OB_MODE_PARTICLE_EDIT) {
    changed = PE_mouse_particles(C, mval, params);
  }
  else if (obact && BKE_paint_select_face_test(obact)) {
    changed = paintface_mouse_select(C, mval, params, obact);
  }
  else if (BKE_paint_select_vert_test(obact)) {
    changed = ed_wpaint_vertex_select_pick(C, mval, params, obact);
  }
  else if (BKE_paint_select_grease_pencil_test(obact)) {
    changed = ed_grease_pencil_select_pick(C, mval, params);
  }
  else {
    changed = ed_object_select_pick(C, mval, params, center, enumerate, object_only);
  }

  /* Pass-through flag may be cleared, see #WM_operator_flag_only_pass_through_on_press. */

  /* Pass-through allows tweaks
   * FINISHED to signal one operator worked */
  if (changed) {
    WM_event_add_notifier(C, NC_SCENE | ND_OB_SELECT, scene);
    return OPERATOR_PASS_THROUGH | OPERATOR_FINISHED;
  }
  /* Nothing selected, just passthrough. */
  return OPERATOR_PASS_THROUGH | OPERATOR_CANCELLED;
}

static wmOperatorStatus view3d_select_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  RNA_int_set_array(op->ptr, "location", event->mval);

  const wmOperatorStatus retval = view3d_select_exec(C, op);

  return WM_operator_flag_only_pass_through_on_press(retval, event);
}

void VIEW3D_OT_select(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Select";
  ot->description = "Select and activate item(s)";
  ot->idname = "VIEW3D_OT_select";

  /* API callbacks. */
  ot->invoke = view3d_select_invoke;
  ot->exec = view3d_select_exec;
  ot->poll = ED_operator_view3d_active;
  ot->get_name = ED_select_pick_get_name;

  /* flags */
  ot->flag = OPTYPE_UNDO;

  /* properties */
  WM_operator_properties_mouse_select(ot);

  prop = RNA_def_boolean(
      ot->srna,
      "center",
      false,
      "Center",
      "Use the object center when selecting, in edit mode used to extend object selection");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  prop = RNA_def_boolean(ot->srna,
                         "enumerate",
                         false,
                         "Enumerate",
                         "List objects under the mouse (object mode only)");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  prop = RNA_def_boolean(
      ot->srna, "object", false, "Object", "Use object selection (edit mode only)");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  prop = RNA_def_int_vector(ot->srna,
                            "location",
                            2,
                            nullptr,
                            INT_MIN,
                            INT_MAX,
                            "Location",
                            "Mouse location",
                            INT_MIN,
                            INT_MAX);
  RNA_def_property_flag(prop, PROP_HIDDEN);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Box Select
 * \{ */

struct BoxSelectUserData {
  const ViewContext *vc;
  const rcti *rect;
  const rctf *rect_fl;
  rctf _rect_fl;
  eSelectOp sel_op;
  eBezTriple_Flag select_flag;

  /** Only for edit-mesh selection. */
  UVSyncSelectFromMesh *uv_selctx;

  /* runtime */
  bool is_done;
  bool is_changed;
};

static void view3d_userdata_boxselect_init(BoxSelectUserData *r_data,
                                           const ViewContext *vc,
                                           const rcti *rect,
                                           const eSelectOp sel_op)
{
  r_data->vc = vc;

  r_data->rect = rect;
  r_data->rect_fl = &r_data->_rect_fl;
  BLI_rctf_rcti_copy(&r_data->_rect_fl, rect);

  r_data->sel_op = sel_op;
  /* SELECT by default, but can be changed if needed (only few cases use and respect this). */
  r_data->select_flag = (eBezTriple_Flag)SELECT;

  /* runtime */
  r_data->is_done = false;
  r_data->is_changed = false;
}

bool edge_inside_circle(const float cent[2],
                        float radius,
                        const float screen_co_a[2],
                        const float screen_co_b[2])
{
  const float radius_squared = radius * radius;
  return (dist_squared_to_line_segment_v2(cent, screen_co_a, screen_co_b) < radius_squared);
}

struct BoxSelectUserData_ForMeshObjectVert {
  BoxSelectUserData box_data;
  blender::MutableSpan<bool> select_vert;
};
static void do_paintvert_box_select__doSelectVert(void *user_data,
                                                  const float screen_co[2],
                                                  int index)
{
  BoxSelectUserData_ForMeshObjectVert *mesh_data =
      static_cast<BoxSelectUserData_ForMeshObjectVert *>(user_data);
  BoxSelectUserData *data = &mesh_data->box_data;
  const bool is_select = mesh_data->select_vert[index];
  const bool is_inside = BLI_rctf_isect_pt_v(data->rect_fl, screen_co);
  const int sel_op_result = ED_select_op_action_deselected(data->sel_op, is_select, is_inside);
  if (sel_op_result != -1) {
    mesh_data->select_vert[index] = sel_op_result == 1;
    data->is_changed = true;
  }
}
static bool do_paintvert_box_select(const ViewContext *vc,
                                    wmGenericUserData *wm_userdata,
                                    const rcti *rect,
                                    const eSelectOp sel_op)
{
  using namespace blender;
  const bool use_zbuf = !XRAY_ENABLED(vc->v3d);

  Mesh *mesh = static_cast<Mesh *>(vc->obact->data);
  if ((mesh == nullptr) || (mesh->verts_num == 0)) {
    return false;
  }

  bool changed = false;
  if (SEL_OP_USE_PRE_DESELECT(sel_op)) {
    changed |= paintvert_deselect_all_visible(vc->obact, SEL_DESELECT, false);
  }

  if (BLI_rcti_is_empty(rect)) {
    /* pass */
  }
  else if (use_zbuf) {
    EditSelectBuf_Cache *esel = static_cast<EditSelectBuf_Cache *>(wm_userdata->data);
    if (wm_userdata->data == nullptr) {
      editselect_buf_cache_init_with_generic_userdata(wm_userdata, vc, SCE_SELECT_VERTEX);
      esel = static_cast<EditSelectBuf_Cache *>(wm_userdata->data);
      esel->select_bitmap = DRW_select_buffer_bitmap_from_rect(
          vc->depsgraph, vc->region, vc->v3d, rect, nullptr);
    }
    if (esel->select_bitmap != nullptr) {
      changed |= edbm_backbuf_check_and_select_verts_obmode(mesh, esel, sel_op);
    }
  }
  else {
    bke::MutableAttributeAccessor attributes = mesh->attributes_for_write();
    bke::SpanAttributeWriter<bool> select_vert = attributes.lookup_or_add_for_write_span<bool>(
        ".select_vert", bke::AttrDomain::Point);

    BoxSelectUserData_ForMeshObjectVert data;
    data.select_vert = select_vert.span;

    view3d_userdata_boxselect_init(&data.box_data, vc, rect, sel_op);

    ED_view3d_init_mats_rv3d(vc->obact, vc->rv3d);

    meshobject_foreachScreenVert(
        vc, do_paintvert_box_select__doSelectVert, &data, V3D_PROJ_TEST_CLIP_DEFAULT);
    changed |= data.box_data.is_changed;
    select_vert.finish();
  }

  if (changed) {
    if (SEL_OP_CAN_DESELECT(sel_op)) {
      BKE_mesh_mselect_validate(mesh);
    }
    paintvert_flush_flags(vc->obact);
    paintvert_tag_select_update(vc->C, vc->obact);
  }
  return changed;
}

static bool do_paintface_box_select(const ViewContext *vc,
                                    wmGenericUserData *wm_userdata,
                                    const rcti *rect,
                                    eSelectOp sel_op)
{
  Object *ob = vc->obact;
  Mesh *mesh;

  mesh = BKE_mesh_from_object(ob);
  if ((mesh == nullptr) || (mesh->faces_num == 0)) {
    return false;
  }

  bool changed = false;
  if (SEL_OP_USE_PRE_DESELECT(sel_op)) {
    changed |= paintface_deselect_all_visible(vc->C, vc->obact, SEL_DESELECT, false);
  }

  if (BLI_rcti_is_empty(rect)) {
    /* pass */
  }
  else {
    EditSelectBuf_Cache *esel = static_cast<EditSelectBuf_Cache *>(wm_userdata->data);
    if (wm_userdata->data == nullptr) {
      editselect_buf_cache_init_with_generic_userdata(wm_userdata, vc, SCE_SELECT_FACE);
      esel = static_cast<EditSelectBuf_Cache *>(wm_userdata->data);
      esel->select_bitmap = DRW_select_buffer_bitmap_from_rect(
          vc->depsgraph, vc->region, vc->v3d, rect, nullptr);
    }
    if (esel->select_bitmap != nullptr) {
      changed |= edbm_backbuf_check_and_select_faces_obmode(mesh, esel, sel_op);
    }
  }

  if (changed) {
    paintface_flush_flags(vc->C, vc->obact, true, false);
  }
  return changed;
}

static void do_nurbs_box_select__doSelect(void *user_data,
                                          Nurb * /*nu*/,
                                          BPoint *bp,
                                          BezTriple *bezt,
                                          int beztindex,
                                          bool handles_visible,
                                          const float screen_co[2])
{
  BoxSelectUserData *data = static_cast<BoxSelectUserData *>(user_data);

  const bool is_inside = BLI_rctf_isect_pt_v(data->rect_fl, screen_co);
  if (bp) {
    const bool is_select = bp->f1 & SELECT;
    const int sel_op_result = ED_select_op_action_deselected(data->sel_op, is_select, is_inside);
    if (sel_op_result != -1) {
      SET_FLAG_FROM_TEST(bp->f1, sel_op_result, data->select_flag);
      data->is_changed = true;
    }
  }
  else {
    if (!handles_visible) {
      /* can only be (beztindex == 1) here since handles are hidden */
      const bool is_select = bezt->f2 & SELECT;
      const int sel_op_result = ED_select_op_action_deselected(data->sel_op, is_select, is_inside);
      if (sel_op_result != -1) {
        SET_FLAG_FROM_TEST(bezt->f2, sel_op_result, data->select_flag);
        data->is_changed = true;
      }
      bezt->f1 = bezt->f3 = bezt->f2;
    }
    else {
      uint8_t *flag_p = (&bezt->f1) + beztindex;
      const bool is_select = *flag_p & SELECT;
      const int sel_op_result = ED_select_op_action_deselected(data->sel_op, is_select, is_inside);
      if (sel_op_result != -1) {
        SET_FLAG_FROM_TEST(*flag_p, sel_op_result, data->select_flag);
        data->is_changed = true;
      }
    }
  }
}
static bool do_nurbs_box_select(const ViewContext *vc, const rcti *rect, const eSelectOp sel_op)
{
  const bool deselect_all = (sel_op == SEL_OP_SET);
  BoxSelectUserData data;

  view3d_userdata_boxselect_init(&data, vc, rect, sel_op);

  Curve *curve = (Curve *)vc->obedit->data;
  ListBase *nurbs = BKE_curve_editNurbs_get(curve);

  /* For deselect all, items to be selected are tagged with temp flag. Clear that first. */
  if (deselect_all) {
    BKE_nurbList_flag_set(nurbs, BEZT_FLAG_TEMP_TAG, false);
    data.select_flag = BEZT_FLAG_TEMP_TAG;
  }

  ED_view3d_init_mats_rv3d(vc->obedit, vc->rv3d); /* for foreach's screen/vert projection */
  nurbs_foreachScreenVert(vc, do_nurbs_box_select__doSelect, &data, V3D_PROJ_TEST_CLIP_DEFAULT);

  /* Deselect items that were not added to selection (indicated by temp flag). */
  if (deselect_all) {
    data.is_changed |= BKE_nurbList_flag_set_from_flag(nurbs, BEZT_FLAG_TEMP_TAG, SELECT);
  }

  BKE_curve_nurb_vert_active_validate(curve);

  return data.is_changed;
}

static void do_lattice_box_select__doSelect(void *user_data, BPoint *bp, const float screen_co[2])
{
  BoxSelectUserData *data = static_cast<BoxSelectUserData *>(user_data);
  const bool is_select = bp->f1 & SELECT;
  const bool is_inside = BLI_rctf_isect_pt_v(data->rect_fl, screen_co);
  const int sel_op_result = ED_select_op_action_deselected(data->sel_op, is_select, is_inside);
  if (sel_op_result != -1) {
    SET_FLAG_FROM_TEST(bp->f1, sel_op_result, SELECT);
    data->is_changed = true;
  }
}
static bool do_lattice_box_select(const ViewContext *vc, const rcti *rect, const eSelectOp sel_op)
{
  BoxSelectUserData data;

  view3d_userdata_boxselect_init(&data, vc, rect, sel_op);

  if (SEL_OP_USE_PRE_DESELECT(sel_op)) {
    data.is_changed |= ED_lattice_flags_set(vc->obedit, 0);
  }

  ED_view3d_init_mats_rv3d(vc->obedit, vc->rv3d); /* for foreach's screen/vert projection */
  lattice_foreachScreenVert(
      vc, do_lattice_box_select__doSelect, &data, V3D_PROJ_TEST_CLIP_DEFAULT);

  return data.is_changed;
}

static void do_mesh_box_select__doSelectVert(void *user_data,
                                             BMVert *eve,
                                             const float screen_co[2],
                                             int /*index*/)
{
  BoxSelectUserData *data = static_cast<BoxSelectUserData *>(user_data);
  const bool is_select = BM_elem_flag_test(eve, BM_ELEM_SELECT);
  const bool is_inside = BLI_rctf_isect_pt_v(data->rect_fl, screen_co);
  const int sel_op_result = ED_select_op_action_deselected(data->sel_op, is_select, is_inside);
  if (sel_op_result != -1) {
    BM_vert_select_set(data->vc->em->bm, eve, sel_op_result);
    if (data->uv_selctx) {
      data->uv_selctx->vert_select_set(eve, sel_op_result);
    }

    data->is_changed = true;
  }
}
struct BoxSelectUserData_ForMeshEdge {
  BoxSelectUserData *data;
  EditSelectBuf_Cache *esel;
  uint backbuf_offset;
};
/**
 * Pass 0 operates on edges when fully inside.
 */
static void do_mesh_box_select__doSelectEdge_pass0(void *user_data,
                                                   BMEdge *eed,
                                                   const float screen_co_a[2],
                                                   const float screen_co_b[2],
                                                   int index)
{
  BoxSelectUserData_ForMeshEdge *data_for_edge = static_cast<BoxSelectUserData_ForMeshEdge *>(
      user_data);
  BoxSelectUserData *data = data_for_edge->data;
  bool is_visible = true;
  if (data_for_edge->backbuf_offset) {
    uint bitmap_inedx = data_for_edge->backbuf_offset + index - 1;
    is_visible = BLI_BITMAP_TEST_BOOL(data_for_edge->esel->select_bitmap, bitmap_inedx);
  }

  const bool is_select = BM_elem_flag_test(eed, BM_ELEM_SELECT);
  const bool is_inside = (is_visible &&
                          edge_fully_inside_rect(data->rect_fl, screen_co_a, screen_co_b));
  const int sel_op_result = ED_select_op_action_deselected(data->sel_op, is_select, is_inside);
  if (sel_op_result != -1) {
    BM_edge_select_set(data->vc->em->bm, eed, sel_op_result);
    if (data->uv_selctx) {
      data->uv_selctx->edge_select_set(eed, sel_op_result);
    }

    data->is_done = true;
    data->is_changed = true;
  }
}
/**
 * Pass 1 operates on edges when partially inside.
 */
static void do_mesh_box_select__doSelectEdge_pass1(void *user_data,
                                                   BMEdge *eed,
                                                   const float screen_co_a[2],
                                                   const float screen_co_b[2],
                                                   int index)
{
  BoxSelectUserData_ForMeshEdge *data_for_edge = static_cast<BoxSelectUserData_ForMeshEdge *>(
      user_data);
  BoxSelectUserData *data = data_for_edge->data;
  bool is_visible = true;
  if (data_for_edge->backbuf_offset) {
    uint bitmap_inedx = data_for_edge->backbuf_offset + index - 1;
    is_visible = BLI_BITMAP_TEST_BOOL(data_for_edge->esel->select_bitmap, bitmap_inedx);
  }

  const bool is_select = BM_elem_flag_test(eed, BM_ELEM_SELECT);
  const bool is_inside = (is_visible && edge_inside_rect(data->rect_fl, screen_co_a, screen_co_b));
  const int sel_op_result = ED_select_op_action_deselected(data->sel_op, is_select, is_inside);
  if (sel_op_result != -1) {
    BM_edge_select_set(data->vc->em->bm, eed, sel_op_result);
    if (data->uv_selctx) {
      data->uv_selctx->edge_select_set(eed, sel_op_result);
    }

    data->is_changed = true;
  }
}
static void do_mesh_box_select__doSelectFace(void *user_data,
                                             BMFace *efa,
                                             const float screen_co[2],
                                             int /*index*/)
{
  BoxSelectUserData *data = static_cast<BoxSelectUserData *>(user_data);
  const bool is_select = BM_elem_flag_test(efa, BM_ELEM_SELECT);
  const bool is_inside = BLI_rctf_isect_pt_v(data->rect_fl, screen_co);
  const int sel_op_result = ED_select_op_action_deselected(data->sel_op, is_select, is_inside);
  if (sel_op_result != -1) {
    BM_face_select_set(data->vc->em->bm, efa, sel_op_result);
    if (data->uv_selctx) {
      data->uv_selctx->face_select_set(efa, sel_op_result);
    }

    data->is_changed = true;
  }
}
static bool do_mesh_box_select(const ViewContext *vc,
                               wmGenericUserData *wm_userdata,
                               const rcti *rect,
                               const eSelectOp sel_op)
{
  BoxSelectUserData data;
  ToolSettings *ts = vc->scene->toolsettings;

  view3d_userdata_boxselect_init(&data, vc, rect, sel_op);

  if (SEL_OP_USE_PRE_DESELECT(sel_op)) {
    if (vc->em->bm->totvertsel) {
      EDBM_flag_disable_all(vc->em, BM_ELEM_SELECT);
      data.is_changed = true;
    }
  }

  std::unique_ptr<UVSyncSelectFromMesh> uv_selctx = UVSyncSelectFromMesh::create_if_needed(
      *ts, *vc->em->bm);
  data.uv_selctx = uv_selctx.get();

  /* for non zbuf projections, don't change the GL state */
  ED_view3d_init_mats_rv3d(vc->obedit, vc->rv3d);

  GPU_matrix_set(vc->rv3d->viewmat);

  const bool use_zbuf = !XRAY_FLAG_ENABLED(vc->v3d);

  EditSelectBuf_Cache *esel = static_cast<EditSelectBuf_Cache *>(wm_userdata->data);
  if (use_zbuf) {
    if (wm_userdata->data == nullptr) {
      editselect_buf_cache_init_with_generic_userdata(wm_userdata, vc, ts->selectmode);
      esel = static_cast<EditSelectBuf_Cache *>(wm_userdata->data);
      esel->select_bitmap = DRW_select_buffer_bitmap_from_rect(
          vc->depsgraph, vc->region, vc->v3d, rect, nullptr);
    }
  }

  if (ts->selectmode & SCE_SELECT_VERTEX) {
    if (use_zbuf) {
      data.is_changed |= edbm_backbuf_check_and_select_verts(
          esel, vc->depsgraph, vc->obedit, vc->em, data.uv_selctx, sel_op);
    }
    else {
      mesh_foreachScreenVert(
          vc, do_mesh_box_select__doSelectVert, &data, V3D_PROJ_TEST_CLIP_DEFAULT);
    }
  }
  if (ts->selectmode & SCE_SELECT_EDGE) {
    /* Does both use_zbuf and non-use_zbuf versions (need screen cos for both) */
    BoxSelectUserData_ForMeshEdge cb_data{};
    cb_data.data = &data;
    cb_data.esel = use_zbuf ? esel : nullptr;
    cb_data.backbuf_offset = use_zbuf ? DRW_select_buffer_context_offset_for_object_elem(
                                            vc->depsgraph, vc->obedit, SCE_SELECT_EDGE) :
                                        0;

    const eV3DProjTest clip_flag = V3D_PROJ_TEST_CLIP_NEAR |
                                   (use_zbuf ? (eV3DProjTest)0 : V3D_PROJ_TEST_CLIP_BB);
    /* Fully inside. */
    mesh_foreachScreenEdge_clip_bb_segment(
        vc, do_mesh_box_select__doSelectEdge_pass0, &cb_data, clip_flag);
    if (data.is_done == false) {
      /* Fall back to partially inside.
       * Clip content to account for edges partially behind the view. */
      mesh_foreachScreenEdge_clip_bb_segment(vc,
                                             do_mesh_box_select__doSelectEdge_pass1,
                                             &cb_data,
                                             clip_flag | V3D_PROJ_TEST_CLIP_CONTENT_DEFAULT);
    }
  }

  if (ts->selectmode & SCE_SELECT_FACE) {
    if (use_zbuf) {
      data.is_changed |= edbm_backbuf_check_and_select_faces(
          esel, vc->depsgraph, vc->obedit, vc->em, data.uv_selctx, sel_op);
    }
    else {
      mesh_foreachScreenFace(
          vc, do_mesh_box_select__doSelectFace, &data, V3D_PROJ_TEST_CLIP_DEFAULT);
    }
  }

  if (data.is_changed) {
    EDBM_selectmode_flush(vc->em);
  }

  if (data.uv_selctx) {
    data.uv_selctx->apply();
  }

  return data.is_changed;
}

static bool do_meta_box_select(const ViewContext *vc, const rcti *rect, const eSelectOp sel_op)
{
  Object *ob = vc->obedit;
  MetaBall *mb = (MetaBall *)ob->data;
  MetaElem *ml;
  int a;
  bool changed = false;

  GPUSelectBuffer buffer;
  int hits;

  hits = view3d_gpu_select(vc, &buffer, rect, VIEW3D_SELECT_ALL, VIEW3D_SELECT_FILTER_NOP);

  if (SEL_OP_USE_PRE_DESELECT(sel_op)) {
    changed |= BKE_mball_deselect_all(mb);
  }

  int metaelem_id = 0;
  for (ml = static_cast<MetaElem *>(mb->editelems->first); ml;
       ml = ml->next, metaelem_id += 0x10000)
  {
    bool is_inside_radius = false;
    bool is_inside_stiff = false;

    for (a = 0; a < hits; a++) {
      const int select_id = buffer.storage[a].id;

      if (select_id == -1) {
        continue;
      }

      const uint hit_object = select_id & 0xFFFF;
      if (vc->obedit->runtime->select_id != hit_object) {
        continue;
      }

      if (metaelem_id != (select_id & 0xFFFF0000 & ~MBALLSEL_ANY)) {
        continue;
      }

      if (select_id & MBALLSEL_RADIUS) {
        is_inside_radius = true;
        break;
      }

      if (select_id & MBALLSEL_STIFF) {
        is_inside_stiff = true;
        break;
      }
    }
    const int flag_prev = ml->flag;
    if (is_inside_radius) {
      ml->flag |= MB_SCALE_RAD;
    }
    if (is_inside_stiff) {
      ml->flag &= ~MB_SCALE_RAD;
    }

    const bool is_select = (ml->flag & SELECT);
    const bool is_inside = is_inside_radius || is_inside_stiff;

    const int sel_op_result = ED_select_op_action_deselected(sel_op, is_select, is_inside);
    if (sel_op_result != -1) {
      SET_FLAG_FROM_TEST(ml->flag, sel_op_result, SELECT);
    }
    changed |= (flag_prev != ml->flag);
  }

  return changed;
}

static bool do_armature_box_select(const ViewContext *vc, const rcti *rect, const eSelectOp sel_op)
{
  bool changed = false;
  int a;

  GPUSelectBuffer buffer;
  int hits;

  hits = view3d_gpu_select(vc, &buffer, rect, VIEW3D_SELECT_ALL, VIEW3D_SELECT_FILTER_NOP);

  Vector<Base *> bases = BKE_view_layer_array_from_bases_in_edit_mode_unique_data(
      vc->scene, vc->view_layer, vc->v3d);

  if (SEL_OP_USE_PRE_DESELECT(sel_op)) {
    changed |= ED_armature_edit_deselect_all_visible_multi_ex(bases);
  }

  for (Base *base : bases) {
    Object *obedit = base->object;
    obedit->id.tag &= ~ID_TAG_DOIT;

    bArmature *arm = static_cast<bArmature *>(obedit->data);
    ED_armature_ebone_listbase_temp_clear(arm->edbo);
  }

  /* first we only check points inside the border */
  for (a = 0; a < hits; a++) {
    const int select_id = buffer.storage[a].id;
    if (select_id != -1) {
      if ((select_id & 0xFFFF0000) == 0) {
        continue;
      }

      EditBone *ebone;
      Base *base_edit = ED_armature_base_and_ebone_from_select_buffer(bases, select_id, &ebone);
      ebone->temp.i |= select_id & BONESEL_ANY;
      base_edit->object->id.tag |= ID_TAG_DOIT;
    }
  }

  for (Base *base : bases) {
    Object *obedit = base->object;
    if (obedit->id.tag & ID_TAG_DOIT) {
      obedit->id.tag &= ~ID_TAG_DOIT;
      changed |= ED_armature_edit_select_op_from_tagged(static_cast<bArmature *>(obedit->data),
                                                        sel_op);
    }
  }

  return changed;
}

/**
 * Compare result of 'GPU_select': 'GPUSelectResult',
 * needed for when we need to align with object draw-order.
 */
static int gpu_bone_select_buffer_cmp(const void *sel_a_p, const void *sel_b_p)
{
  /* NOTE: this is endianness-sensitive.
   * GPUSelectResult values are always expected to be little-endian. */
  uint sel_a = ((GPUSelectResult *)sel_a_p)->id;
  uint sel_b = ((GPUSelectResult *)sel_b_p)->id;

  if (sel_a < sel_b) {
    return -1;
  }
  if (sel_a > sel_b) {
    return 1;
  }
  return 0;
}

static void object_select_tag_updates(bContext &C, Scene &scene)
{
  DEG_id_tag_update(&scene.id, ID_RECALC_SELECT);
  WM_event_add_notifier(&C, NC_SCENE | ND_OB_SELECT, &scene);
}

static bool do_object_box_select(bContext *C,
                                 const ViewContext *vc,
                                 const rcti *rect,
                                 const eSelectOp sel_op)
{
  View3D *v3d = vc->v3d;

  GPUSelectBuffer buffer;
  const eV3DSelectObjectFilter select_filter = ED_view3d_select_filter_from_mode(vc->scene,
                                                                                 vc->obact);
  const int hits = view3d_gpu_select(vc, &buffer, rect, VIEW3D_SELECT_ALL, select_filter);
  BKE_view_layer_synced_ensure(vc->scene, vc->view_layer);
  LISTBASE_FOREACH (Base *, base, BKE_view_layer_object_bases_get(vc->view_layer)) {
    base->object->id.tag &= ~ID_TAG_DOIT;
  }

  bool changed = false;
  if (SEL_OP_USE_PRE_DESELECT(sel_op)) {
    changed |= object_deselect_all_visible(vc->scene, vc->view_layer, vc->v3d);
  }

  ListBase *object_bases = BKE_view_layer_object_bases_get(vc->view_layer);
  if ((hits == -1) && !SEL_OP_USE_OUTSIDE(sel_op)) {
    if (changed) {
      object_select_tag_updates(*C, *vc->scene);
      return true;
    }
  }

  blender::Map<uint32_t, Base *> base_by_object_select_id;
  LISTBASE_FOREACH (Base *, base, object_bases) {
    if (BASE_SELECTABLE(v3d, base)) {
      const uint32_t select_id = base->object->runtime->select_id;
      if ((select_id & 0x0000FFFF) != 0) {
        const uint hit_object = select_id & 0xFFFF;
        base_by_object_select_id.add(hit_object, base);
      }
    }
  }

  /* The draw order doesn't always match the order we populate the engine, see: #51695. */
  qsort(buffer.storage.data(), hits, sizeof(GPUSelectResult), gpu_bone_select_buffer_cmp);

  blender::Set<Base *> bases_inside;
  for (const GPUSelectResult *buf_iter = buffer.storage.data(), *buf_end = buf_iter + hits;
       buf_iter < buf_end;
       buf_iter++)
  {
    const uint32_t select_id = buf_iter->id;
    const uint32_t hit_object = select_id & 0xFFFF;
    if (Base *base = base_by_object_select_id.lookup_default(hit_object, nullptr)) {
      bases_inside.add(base);
    }
  }

  for (Base *base = static_cast<Base *>(object_bases->first); base && hits; base = base->next) {
    if (BASE_SELECTABLE(v3d, base)) {
      const bool is_select = base->flag & BASE_SELECTED;
      const bool is_inside = bases_inside.contains(base);
      const int sel_op_result = ED_select_op_action_deselected(sel_op, is_select, is_inside);
      if (sel_op_result != -1) {
        blender::ed::object::base_select(base,
                                         sel_op_result ? blender::ed::object::BA_SELECT :
                                                         blender::ed::object::BA_DESELECT);
        changed = true;
      }
    }
  }

  if (changed) {
    object_select_tag_updates(*C, *vc->scene);
  }
  return changed;
}

static bool do_pose_box_select(bContext *C,
                               const ViewContext *vc,
                               const rcti *rect,
                               const eSelectOp sel_op)
{
  blender::Vector<Base *> bases = do_pose_tag_select_op_prepare(vc);

  /* Selection buffer has bones potentially too. */
  GPUSelectBuffer buffer;
  const eV3DSelectObjectFilter select_filter = ED_view3d_select_filter_from_mode(vc->scene,
                                                                                 vc->obact);
  const int hits = view3d_gpu_select(vc, &buffer, rect, VIEW3D_SELECT_ALL, select_filter);
  /*
   * NOTE(@theeth): Regarding the logic use here.
   * The buffer and #ListBase have the same relative order, which makes the selection
   * very simple. Loop through both data sets at the same time, if the color
   * is the same as the object, we have a hit and can move to the next color
   * and object pair, if not, just move to the next object,
   * keeping the same color until we have a hit. */

  if (hits > 0) {
    /* no need to loop if there's no hit */

    /* The draw order doesn't always match the order we populate the engine, see: #51695. */
    qsort(buffer.storage.data(), hits, sizeof(GPUSelectResult), gpu_bone_select_buffer_cmp);

    for (const GPUSelectResult *buf_iter = buffer.storage.data(), *buf_end = buf_iter + hits;
         buf_iter < buf_end;
         buf_iter++)
    {
      bPoseChannel *pose_bone;
      Base *base = ED_armature_base_and_pchan_from_select_buffer(bases, buf_iter->id, &pose_bone);

      if (base == nullptr) {
        continue;
      }

      /* Loop over contiguous bone hits for 'base'. */
      for (; buf_iter != buf_end; buf_iter++) {
        /* should never fail */
        if (pose_bone != nullptr) {
          base->object->id.tag |= ID_TAG_DOIT;
          pose_bone->runtime.flag |= POSE_RUNTIME_IN_SELECTION_AREA;
        }

        /* Select the next bone if we're not switching bases. */
        if (buf_iter + 1 != buf_end) {
          const GPUSelectResult *col_next = buf_iter + 1;
          if ((base->object->runtime->select_id & 0x0000FFFF) != (col_next->id & 0x0000FFFF)) {
            break;
          }
          if (base->object->pose != nullptr) {
            const uint hit_bone = (col_next->id & ~BONESEL_ANY) >> 16;
            bPoseChannel *next = static_cast<bPoseChannel *>(
                BLI_findlink(&base->object->pose->chanbase, hit_bone));
            pose_bone = next;
          }
          else {
            pose_bone = nullptr;
          }
        }
      }
    }
  }

  const bool changed_multi = do_pose_tag_select_op_exec(bases, sel_op);
  if (changed_multi) {
    DEG_id_tag_update(&vc->scene->id, ID_RECALC_SELECT);
    WM_event_add_notifier(C, NC_SCENE | ND_OB_SELECT, vc->scene);
  }

  return changed_multi;
}

static bool do_grease_pencil_box_select(const ViewContext *vc,
                                        const rcti *rect,
                                        const eSelectOp sel_op)
{
  using namespace blender;
  Object *object = (vc->obedit ? vc->obedit : vc->obact);
  const Object *ob_eval = DEG_get_evaluated(vc->depsgraph, object);
  const GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object->data);
  const bke::AttrDomain selection_domain = ED_grease_pencil_selection_domain_get(
      vc->scene->toolsettings, object);

  return ed::greasepencil::selection_update(
      vc,
      sel_op,
      [&](const ed::greasepencil::MutableDrawingInfo &info,
          const IndexMask &mask,
          const StringRef attribute_name,
          IndexMaskMemory &memory) {
        bke::CurvesGeometry &curves = info.drawing.strokes_for_write();
        const bke::greasepencil::Layer &layer = grease_pencil.layer(info.layer_index);
        const bke::crazyspace::GeometryDeformation deformation =
            bke::crazyspace::get_evaluated_grease_pencil_drawing_deformation(
                ob_eval, *object, info.drawing);
        const IndexMask visible_handle_elements =
            ed::greasepencil::retrieve_visible_bezier_handle_elements(
                *object,
                info.drawing,
                info.layer_index,
                selection_domain,
                vc->v3d->overlay.handle_display,
                memory);
        const float4x4 layer_to_world = layer.to_world_space(*ob_eval);
        const float4x4 projection = ED_view3d_ob_project_mat_get_from_obmat(vc->rv3d,
                                                                            layer_to_world);

        return ed::curves::select_box_mask(*vc,
                                           curves,
                                           deformation,
                                           projection,
                                           mask,
                                           visible_handle_elements,
                                           selection_domain,
                                           attribute_name,
                                           *rect,
                                           memory);
      });
}

static wmOperatorStatus view3d_box_select_exec(bContext *C, wmOperator *op)
{
  using namespace blender;
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  rcti rect;
  bool changed_multi = false;

  wmGenericUserData wm_userdata_buf = {nullptr, nullptr, false};
  wmGenericUserData *wm_userdata = &wm_userdata_buf;

  view3d_operator_needs_gpu(C);
  BKE_object_update_select_id(CTX_data_main(C));

  /* setup view context for argument to callbacks */
  ViewContext vc = ED_view3d_viewcontext_init(C, depsgraph);

  eSelectOp sel_op = static_cast<eSelectOp>(RNA_enum_get(op->ptr, "mode"));
  WM_operator_properties_border_to_rcti(op, &rect);

  if (vc.obedit) {
    FOREACH_OBJECT_IN_MODE_BEGIN (
        vc.scene, vc.view_layer, vc.v3d, vc.obedit->type, vc.obedit->mode, ob_iter)
    {
      ED_view3d_viewcontext_init_object(&vc, ob_iter);
      bool changed = false;

      switch (vc.obedit->type) {
        case OB_MESH:
          vc.em = BKE_editmesh_from_object(vc.obedit);
          changed = do_mesh_box_select(&vc, wm_userdata, &rect, sel_op);
          if (changed) {
            DEG_id_tag_update(static_cast<ID *>(vc.obedit->data), ID_RECALC_SELECT);
            WM_event_add_notifier(C, NC_GEOM | ND_SELECT, vc.obedit->data);
          }
          break;
        case OB_CURVES_LEGACY:
        case OB_SURF:
          changed = do_nurbs_box_select(&vc, &rect, sel_op);
          if (changed) {
            DEG_id_tag_update(static_cast<ID *>(vc.obedit->data), ID_RECALC_SELECT);
            WM_event_add_notifier(C, NC_GEOM | ND_SELECT, vc.obedit->data);
          }
          break;
        case OB_MBALL:
          changed = do_meta_box_select(&vc, &rect, sel_op);
          if (changed) {
            DEG_id_tag_update(static_cast<ID *>(vc.obedit->data), ID_RECALC_SELECT);
            WM_event_add_notifier(C, NC_GEOM | ND_SELECT, vc.obedit->data);
          }
          break;
        case OB_ARMATURE:
          changed = do_armature_box_select(&vc, &rect, sel_op);
          if (changed) {
            DEG_id_tag_update(&vc.obedit->id, ID_RECALC_SELECT);
            WM_event_add_notifier(C, NC_OBJECT | ND_BONE_SELECT, vc.obedit);
            ED_outliner_select_sync_from_edit_bone_tag(C);
          }
          break;
        case OB_LATTICE:
          changed = do_lattice_box_select(&vc, &rect, sel_op);
          if (changed) {
            DEG_id_tag_update(static_cast<ID *>(vc.obedit->data), ID_RECALC_SELECT);
            WM_event_add_notifier(C, NC_GEOM | ND_SELECT, vc.obedit->data);
          }
          break;
        case OB_CURVES: {
          Curves &curves_id = *static_cast<Curves *>(vc.obedit->data);
          bke::CurvesGeometry &curves = curves_id.geometry.wrap();
          bke::crazyspace::GeometryDeformation deformation =
              bke::crazyspace::get_evaluated_curves_deformation(*vc.depsgraph, *vc.obedit);
          const bke::AttrDomain selection_domain = bke::AttrDomain(curves_id.selection_domain);
          const float4x4 projection = ED_view3d_ob_project_mat_get(vc.rv3d, vc.obedit);
          const IndexRange elements(curves.attributes().domain_size(selection_domain));
          changed = ed::curves::select_box(vc,
                                           curves,
                                           deformation,
                                           projection,
                                           elements,
                                           elements,
                                           selection_domain,
                                           rect,
                                           sel_op);
          if (changed) {
            /* Use #ID_RECALC_GEOMETRY instead of #ID_RECALC_SELECT because it is handled as a
             * generic attribute for now. */
            DEG_id_tag_update(static_cast<ID *>(vc.obedit->data), ID_RECALC_GEOMETRY);
            WM_event_add_notifier(C, NC_GEOM | ND_DATA, vc.obedit->data);
          }
          break;
        }
        case OB_POINTCLOUD: {
          PointCloud &pointcloud = *static_cast<PointCloud *>(vc.obedit->data);
          const float4x4 projection = ED_view3d_ob_project_mat_get(vc.rv3d, vc.obedit);
          changed = ed::pointcloud::select_box(pointcloud, *vc.region, projection, rect, sel_op);
          if (changed) {
            /* Use #ID_RECALC_GEOMETRY instead of #ID_RECALC_SELECT because it is handled as a
             * generic attribute for now. */
            DEG_id_tag_update(static_cast<ID *>(vc.obedit->data), ID_RECALC_GEOMETRY);
            WM_event_add_notifier(C, NC_GEOM | ND_DATA, vc.obedit->data);
          }
          break;
        }
        case OB_GREASE_PENCIL: {
          changed = do_grease_pencil_box_select(&vc, &rect, sel_op);
          break;
        }
        default:
          BLI_assert_msg(0, "box select on incorrect object type");
          break;
      }
      changed_multi |= changed;
    }
    FOREACH_OBJECT_IN_MODE_END;
  }
  else { /* No edit-mode, unified for bones and objects. */
    if (vc.obact && BKE_paint_select_face_test(vc.obact)) {
      changed_multi = do_paintface_box_select(&vc, wm_userdata, &rect, sel_op);
    }
    else if (vc.obact && BKE_paint_select_vert_test(vc.obact)) {
      changed_multi = do_paintvert_box_select(&vc, wm_userdata, &rect, sel_op);
    }
    else if (vc.obact && BKE_paint_select_grease_pencil_test(vc.obact)) {
      changed_multi = do_grease_pencil_box_select(&vc, &rect, sel_op);
    }
    else if (vc.obact && vc.obact->mode & OB_MODE_PARTICLE_EDIT) {
      changed_multi = PE_box_select(C, &rect, sel_op);
    }
    else if (vc.obact && ((vc.obact->mode & OB_MODE_POSE) ||
                          ((vc.obact->mode & OB_MODE_WEIGHT_PAINT) &&
                           BKE_object_pose_armature_get_with_wpaint_check(vc.obact))))
    {
      changed_multi = do_pose_box_select(C, &vc, &rect, sel_op);
      if (changed_multi) {
        ED_outliner_select_sync_from_pose_bone_tag(C);
      }
    }
    else { /* object mode with none active */
      changed_multi = do_object_box_select(C, &vc, &rect, sel_op);
      if (changed_multi) {
        ED_outliner_select_sync_from_object_tag(C);
      }
    }
  }

  WM_generic_user_data_free(wm_userdata);

  if (changed_multi) {
    return OPERATOR_FINISHED;
  }
  return OPERATOR_CANCELLED;
}

void VIEW3D_OT_select_box(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Box Select";
  ot->description = "Select items using box selection";
  ot->idname = "VIEW3D_OT_select_box";

  /* API callbacks. */
  ot->invoke = WM_gesture_box_invoke;
  ot->exec = view3d_box_select_exec;
  ot->modal = WM_gesture_box_modal;
  ot->poll = view3d_selectable_data;
  ot->cancel = WM_gesture_box_cancel;

  /* flags */
  ot->flag = OPTYPE_UNDO;

  /* rna */
  WM_operator_properties_gesture_box(ot);
  WM_operator_properties_select_operation(ot);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Circle Select
 * \{ */

struct CircleSelectUserData {
  const ViewContext *vc;
  bool select;
  int mval[2];
  float mval_fl[2];
  float radius;
  float radius_squared;
  eBezTriple_Flag select_flag;

  /** Only for edit-mesh selection. */
  UVSyncSelectFromMesh *uv_selctx = nullptr;

  /* runtime */
  bool is_changed;
};

static void view3d_userdata_circleselect_init(CircleSelectUserData *r_data,
                                              const ViewContext *vc,
                                              const bool select,
                                              const int mval[2],
                                              const float rad)
{
  r_data->vc = vc;
  r_data->select = select;
  copy_v2_v2_int(r_data->mval, mval);
  r_data->mval_fl[0] = mval[0];
  r_data->mval_fl[1] = mval[1];

  r_data->radius = rad;
  r_data->radius_squared = rad * rad;

  /* SELECT by default, but can be changed if needed (only few cases use and respect this). */
  r_data->select_flag = (eBezTriple_Flag)SELECT;

  /* runtime */
  r_data->is_changed = false;
}

static void mesh_circle_doSelectVert(void *user_data,
                                     BMVert *eve,
                                     const float screen_co[2],
                                     int /*index*/)
{
  CircleSelectUserData *data = static_cast<CircleSelectUserData *>(user_data);

  if (len_squared_v2v2(data->mval_fl, screen_co) <= data->radius_squared) {
    BM_vert_select_set(data->vc->em->bm, eve, data->select);
    if (data->uv_selctx) {
      data->uv_selctx->vert_select_set(eve, data->select);
    }

    data->is_changed = true;
  }
}
static void mesh_circle_doSelectEdge(void *user_data,
                                     BMEdge *eed,
                                     const float screen_co_a[2],
                                     const float screen_co_b[2],
                                     int /*index*/)
{
  CircleSelectUserData *data = static_cast<CircleSelectUserData *>(user_data);

  if (edge_inside_circle(data->mval_fl, data->radius, screen_co_a, screen_co_b)) {
    BM_edge_select_set(data->vc->em->bm, eed, data->select);
    if (data->uv_selctx) {
      data->uv_selctx->edge_select_set(eed, data->select);
    }

    data->is_changed = true;
  }
}
static void mesh_circle_doSelectFace(void *user_data,
                                     BMFace *efa,
                                     const float screen_co[2],
                                     int /*index*/)
{
  CircleSelectUserData *data = static_cast<CircleSelectUserData *>(user_data);

  if (len_squared_v2v2(data->mval_fl, screen_co) <= data->radius_squared) {
    BM_face_select_set(data->vc->em->bm, efa, data->select);
    if (data->uv_selctx) {
      data->uv_selctx->face_select_set(efa, data->select);
    }

    data->is_changed = true;
  }
}

static bool mesh_circle_select(const ViewContext *vc,
                               wmGenericUserData *wm_userdata,
                               eSelectOp sel_op,
                               const int mval[2],
                               float rad)
{
  ToolSettings *ts = vc->scene->toolsettings;
  CircleSelectUserData data;
  BLI_assert(vc->em == BKE_editmesh_from_object(vc->obedit));

  bool changed = false;
  if (SEL_OP_USE_PRE_DESELECT(sel_op)) {
    if (vc->em->bm->totvertsel) {
      EDBM_flag_disable_all(vc->em, BM_ELEM_SELECT);
      vc->em->bm->totvertsel = 0;
      vc->em->bm->totedgesel = 0;
      vc->em->bm->totfacesel = 0;
      changed = true;
    }
  }

  std::unique_ptr<UVSyncSelectFromMesh> uv_selctx = UVSyncSelectFromMesh::create_if_needed(
      *ts, *vc->em->bm);
  data.uv_selctx = uv_selctx.get();

  const bool select = (sel_op != SEL_OP_SUB);

  ED_view3d_init_mats_rv3d(vc->obedit, vc->rv3d); /* for foreach's screen/vert projection */

  view3d_userdata_circleselect_init(&data, vc, select, mval, rad);

  const bool use_zbuf = !XRAY_FLAG_ENABLED(vc->v3d);

  if (use_zbuf) {
    if (wm_userdata->data == nullptr) {
      editselect_buf_cache_init_with_generic_userdata(wm_userdata, vc, ts->selectmode);
    }
  }
  EditSelectBuf_Cache *esel = static_cast<EditSelectBuf_Cache *>(wm_userdata->data);

  if (use_zbuf) {
    if (esel->select_bitmap == nullptr) {
      esel->select_bitmap = DRW_select_buffer_bitmap_from_circle(
          vc->depsgraph, vc->region, vc->v3d, mval, int(rad + 1.0f), nullptr);
    }
  }

  if (ts->selectmode & SCE_SELECT_VERTEX) {
    if (use_zbuf) {
      if (esel->select_bitmap != nullptr) {
        changed |= edbm_backbuf_check_and_select_verts(esel,
                                                       vc->depsgraph,
                                                       vc->obedit,
                                                       vc->em,
                                                       data.uv_selctx,
                                                       select ? SEL_OP_ADD : SEL_OP_SUB);
      }
    }
    else {
      mesh_foreachScreenVert(vc, mesh_circle_doSelectVert, &data, V3D_PROJ_TEST_CLIP_DEFAULT);
    }
  }

  if (ts->selectmode & SCE_SELECT_EDGE) {
    if (use_zbuf) {
      if (esel->select_bitmap != nullptr) {
        changed |= edbm_backbuf_check_and_select_edges(esel,
                                                       vc->depsgraph,
                                                       vc->obedit,
                                                       vc->em,
                                                       data.uv_selctx,
                                                       select ? SEL_OP_ADD : SEL_OP_SUB);
      }
    }
    else {
      mesh_foreachScreenEdge_clip_bb_segment(
          vc,
          mesh_circle_doSelectEdge,
          &data,
          (V3D_PROJ_TEST_CLIP_NEAR | V3D_PROJ_TEST_CLIP_BB | V3D_PROJ_TEST_CLIP_CONTENT_DEFAULT));
    }
  }

  if (ts->selectmode & SCE_SELECT_FACE) {
    if (use_zbuf) {
      if (esel->select_bitmap != nullptr) {
        changed |= edbm_backbuf_check_and_select_faces(esel,
                                                       vc->depsgraph,
                                                       vc->obedit,
                                                       vc->em,
                                                       data.uv_selctx,
                                                       select ? SEL_OP_ADD : SEL_OP_SUB);
      }
    }
    else {
      mesh_foreachScreenFace(vc, mesh_circle_doSelectFace, &data, V3D_PROJ_TEST_CLIP_DEFAULT);
    }
  }

  changed |= data.is_changed;

  if (changed) {
    BM_mesh_select_mode_flush_ex(vc->em->bm, vc->em->selectmode, BMSelectFlushFlag::None);
  }

  if (data.uv_selctx) {
    data.uv_selctx->apply();
  }

  return changed;
}

static bool paint_facesel_circle_select(const ViewContext *vc,
                                        wmGenericUserData *wm_userdata,
                                        const eSelectOp sel_op,
                                        const int mval[2],
                                        float rad)
{
  BLI_assert(ELEM(sel_op, SEL_OP_SET, SEL_OP_ADD, SEL_OP_SUB));
  Object *ob = vc->obact;
  Mesh *mesh = static_cast<Mesh *>(ob->data);

  bool changed = false;
  if (SEL_OP_USE_PRE_DESELECT(sel_op)) {
    /* flush selection at the end */
    changed |= paintface_deselect_all_visible(vc->C, ob, SEL_DESELECT, false);
  }

  if (wm_userdata->data == nullptr) {
    editselect_buf_cache_init_with_generic_userdata(wm_userdata, vc, SCE_SELECT_FACE);
  }

  {
    EditSelectBuf_Cache *esel = static_cast<EditSelectBuf_Cache *>(wm_userdata->data);
    esel->select_bitmap = DRW_select_buffer_bitmap_from_circle(
        vc->depsgraph, vc->region, vc->v3d, mval, int(rad + 1.0f), nullptr);
    if (esel->select_bitmap != nullptr) {
      changed |= edbm_backbuf_check_and_select_faces_obmode(mesh, esel, sel_op);
      MEM_freeN(esel->select_bitmap);
      esel->select_bitmap = nullptr;
    }
  }

  if (changed) {
    paintface_flush_flags(vc->C, ob, true, false);
  }
  return changed;
}

struct CircleSelectUserData_ForMeshObjectVert {
  CircleSelectUserData circle_data;
  blender::MutableSpan<bool> select_vert;
};
static void paint_vertsel_circle_select_doSelectVert(void *user_data,
                                                     const float screen_co[2],
                                                     int index)
{
  CircleSelectUserData_ForMeshObjectVert *mesh_data =
      static_cast<CircleSelectUserData_ForMeshObjectVert *>(user_data);
  CircleSelectUserData *data = &mesh_data->circle_data;

  if (len_squared_v2v2(data->mval_fl, screen_co) <= data->radius_squared) {
    mesh_data->select_vert[index] = data->select;
    data->is_changed = true;
  }
}
static bool paint_vertsel_circle_select(const ViewContext *vc,
                                        wmGenericUserData *wm_userdata,
                                        const eSelectOp sel_op,
                                        const int mval[2],
                                        float rad)
{
  using namespace blender;
  BLI_assert(ELEM(sel_op, SEL_OP_SET, SEL_OP_ADD, SEL_OP_SUB));
  const bool use_zbuf = !XRAY_ENABLED(vc->v3d);
  Object *ob = vc->obact;
  Mesh *mesh = static_cast<Mesh *>(ob->data);
  // CircleSelectUserData data = {nullptr}; /* UNUSED. */

  bool changed = false;
  if (SEL_OP_USE_PRE_DESELECT(sel_op)) {
    /* Flush selection at the end. */
    changed |= paintvert_deselect_all_visible(ob, SEL_DESELECT, false);
  }

  const bool select = (sel_op != SEL_OP_SUB);

  if (use_zbuf) {
    if (wm_userdata->data == nullptr) {
      editselect_buf_cache_init_with_generic_userdata(wm_userdata, vc, SCE_SELECT_VERTEX);
    }
  }

  if (use_zbuf) {
    EditSelectBuf_Cache *esel = static_cast<EditSelectBuf_Cache *>(wm_userdata->data);
    esel->select_bitmap = DRW_select_buffer_bitmap_from_circle(
        vc->depsgraph, vc->region, vc->v3d, mval, int(rad + 1.0f), nullptr);
    if (esel->select_bitmap != nullptr) {
      changed |= edbm_backbuf_check_and_select_verts_obmode(mesh, esel, sel_op);
      MEM_freeN(esel->select_bitmap);
      esel->select_bitmap = nullptr;
    }
  }
  else {
    bke::MutableAttributeAccessor attributes = mesh->attributes_for_write();
    bke::SpanAttributeWriter<bool> select_vert = attributes.lookup_or_add_for_write_span<bool>(
        ".select_vert", bke::AttrDomain::Point);

    CircleSelectUserData_ForMeshObjectVert data;
    data.select_vert = select_vert.span;

    ED_view3d_init_mats_rv3d(vc->obact, vc->rv3d); /* for foreach's screen/vert projection */

    view3d_userdata_circleselect_init(&data.circle_data, vc, select, mval, rad);
    meshobject_foreachScreenVert(
        vc, paint_vertsel_circle_select_doSelectVert, &data, V3D_PROJ_TEST_CLIP_DEFAULT);
    changed |= data.circle_data.is_changed;
    select_vert.finish();
  }

  if (changed) {
    if (sel_op == SEL_OP_SUB) {
      BKE_mesh_mselect_validate(mesh);
    }
    paintvert_flush_flags(ob);
    paintvert_tag_select_update(vc->C, ob);
  }
  return changed;
}

static void nurbscurve_circle_doSelect(void *user_data,
                                       Nurb * /*nu*/,
                                       BPoint *bp,
                                       BezTriple *bezt,
                                       int beztindex,
                                       bool /*handles_visible*/,
                                       const float screen_co[2])
{
  CircleSelectUserData *data = static_cast<CircleSelectUserData *>(user_data);

  if (len_squared_v2v2(data->mval_fl, screen_co) <= data->radius_squared) {
    if (bp) {
      SET_FLAG_FROM_TEST(bp->f1, data->select, data->select_flag);
    }
    else {
      if (beztindex == 0) {
        SET_FLAG_FROM_TEST(bezt->f1, data->select, data->select_flag);
      }
      else if (beztindex == 1) {
        SET_FLAG_FROM_TEST(bezt->f2, data->select, data->select_flag);
      }
      else {
        SET_FLAG_FROM_TEST(bezt->f3, data->select, data->select_flag);
      }
    }
    data->is_changed = true;
  }
}
static bool nurbscurve_circle_select(const ViewContext *vc,
                                     const eSelectOp sel_op,
                                     const int mval[2],
                                     float rad)
{
  const bool select = (sel_op != SEL_OP_SUB);
  const bool deselect_all = (sel_op == SEL_OP_SET);
  CircleSelectUserData data;

  view3d_userdata_circleselect_init(&data, vc, select, mval, rad);

  Curve *curve = (Curve *)vc->obedit->data;
  ListBase *nurbs = BKE_curve_editNurbs_get(curve);

  /* For deselect all, items to be selected are tagged with temp flag. Clear that first. */
  if (deselect_all) {
    BKE_nurbList_flag_set(nurbs, BEZT_FLAG_TEMP_TAG, false);
    data.select_flag = BEZT_FLAG_TEMP_TAG;
  }

  ED_view3d_init_mats_rv3d(vc->obedit, vc->rv3d); /* for foreach's screen/vert projection */
  nurbs_foreachScreenVert(vc, nurbscurve_circle_doSelect, &data, V3D_PROJ_TEST_CLIP_DEFAULT);

  /* Deselect items that were not added to selection (indicated by temp flag). */
  if (deselect_all) {
    data.is_changed |= BKE_nurbList_flag_set_from_flag(nurbs, BEZT_FLAG_TEMP_TAG, SELECT);
  }

  BKE_curve_nurb_vert_active_validate(static_cast<Curve *>(vc->obedit->data));

  return data.is_changed;
}

static void latticecurve_circle_doSelect(void *user_data, BPoint *bp, const float screen_co[2])
{
  CircleSelectUserData *data = static_cast<CircleSelectUserData *>(user_data);

  if (len_squared_v2v2(data->mval_fl, screen_co) <= data->radius_squared) {
    bp->f1 = data->select ? (bp->f1 | SELECT) : (bp->f1 & ~SELECT);
    data->is_changed = true;
  }
}
static bool lattice_circle_select(const ViewContext *vc,
                                  const eSelectOp sel_op,
                                  const int mval[2],
                                  float rad)
{
  CircleSelectUserData data;
  const bool select = (sel_op != SEL_OP_SUB);

  view3d_userdata_circleselect_init(&data, vc, select, mval, rad);

  if (SEL_OP_USE_PRE_DESELECT(sel_op)) {
    data.is_changed |= ED_lattice_flags_set(vc->obedit, 0);
  }
  ED_view3d_init_mats_rv3d(vc->obedit, vc->rv3d); /* for foreach's screen/vert projection */

  lattice_foreachScreenVert(vc, latticecurve_circle_doSelect, &data, V3D_PROJ_TEST_CLIP_DEFAULT);

  return data.is_changed;
}

/**
 * \note logic is shared with the edit-bone case, see #armature_circle_doSelectJoint.
 */
static bool pchan_circle_doSelectJoint(void *user_data,
                                       bPoseChannel *pchan,
                                       const float screen_co[2])
{
  CircleSelectUserData *data = static_cast<CircleSelectUserData *>(user_data);

  if (len_squared_v2v2(data->mval_fl, screen_co) <= data->radius_squared) {
    if (data->select) {
      pchan->flag |= POSE_SELECTED;
    }
    else {
      pchan->flag &= ~POSE_SELECTED;
    }
    return true;
  }
  return false;
}
static void do_circle_select_pose__doSelectBone(void *user_data,
                                                bPoseChannel *pchan,
                                                const float screen_co_a[2],
                                                const float screen_co_b[2])
{
  CircleSelectUserData *data = static_cast<CircleSelectUserData *>(user_data);
  bArmature *arm = static_cast<bArmature *>(data->vc->obact->data);
  if (!blender::animrig::bone_is_selectable(arm, pchan)) {
    return;
  }

  bool is_point_done = false;
  int points_proj_tot = 0;

  /* Project head location to screen-space. */
  if (screen_co_a[0] != IS_CLIPPED) {
    points_proj_tot++;
    if (pchan_circle_doSelectJoint(data, pchan, screen_co_a)) {
      is_point_done = true;
    }
  }

  /* Project tail location to screen-space. */
  if (screen_co_b[0] != IS_CLIPPED) {
    points_proj_tot++;
    if (pchan_circle_doSelectJoint(data, pchan, screen_co_b)) {
      is_point_done = true;
    }
  }

  /* check if the head and/or tail is in the circle
   * - the call to check also does the selection already
   */

  /* only if the endpoints didn't get selected, deal with the middle of the bone too
   * It works nicer to only do this if the head or tail are not in the circle,
   * otherwise there is no way to circle select joints alone */
  if ((is_point_done == false) && (points_proj_tot == 2) &&
      edge_inside_circle(data->mval_fl, data->radius, screen_co_a, screen_co_b))
  {
    if (data->select) {
      pchan->flag |= POSE_SELECTED;
    }
    else {
      pchan->flag &= ~POSE_SELECTED;
    }
    data->is_changed = true;
  }

  data->is_changed |= is_point_done;
}
static bool pose_circle_select(const ViewContext *vc,
                               const eSelectOp sel_op,
                               const int mval[2],
                               float rad)
{
  BLI_assert(ELEM(sel_op, SEL_OP_SET, SEL_OP_ADD, SEL_OP_SUB));
  CircleSelectUserData data;
  const bool select = (sel_op != SEL_OP_SUB);

  view3d_userdata_circleselect_init(&data, vc, select, mval, rad);

  if (SEL_OP_USE_PRE_DESELECT(sel_op)) {
    data.is_changed |= ED_pose_deselect_all(vc->obact, SEL_DESELECT, false);
  }

  ED_view3d_init_mats_rv3d(vc->obact, vc->rv3d); /* for foreach's screen/vert projection */

  /* Treat bones as clipped segments (no joints). */
  pose_foreachScreenBone(vc,
                         do_circle_select_pose__doSelectBone,
                         &data,
                         V3D_PROJ_TEST_CLIP_DEFAULT | V3D_PROJ_TEST_CLIP_CONTENT_DEFAULT);

  if (data.is_changed) {
    ED_pose_bone_select_tag_update(vc->obact);
  }
  return data.is_changed;
}

/**
 * \note logic is shared with the pose-bone case, see #pchan_circle_doSelectJoint.
 */
static bool armature_circle_doSelectJoint(void *user_data,
                                          EditBone *ebone,
                                          const float screen_co[2],
                                          bool head)
{
  CircleSelectUserData *data = static_cast<CircleSelectUserData *>(user_data);

  if (len_squared_v2v2(data->mval_fl, screen_co) <= data->radius_squared) {
    if (head) {
      if (data->select) {
        ebone->flag |= BONE_ROOTSEL;
      }
      else {
        ebone->flag &= ~BONE_ROOTSEL;
      }
    }
    else {
      if (data->select) {
        ebone->flag |= BONE_TIPSEL;
      }
      else {
        ebone->flag &= ~BONE_TIPSEL;
      }
    }
    return true;
  }
  return false;
}
static void do_circle_select_armature__doSelectBone(void *user_data,
                                                    EditBone *ebone,
                                                    const float screen_co_a[2],
                                                    const float screen_co_b[2])
{
  CircleSelectUserData *data = static_cast<CircleSelectUserData *>(user_data);
  const bArmature *arm = static_cast<const bArmature *>(data->vc->obedit->data);
  if (!(data->select ? EBONE_SELECTABLE(arm, ebone) :
                       blender::animrig::bone_is_visible(arm, ebone)))
  {
    return;
  }

  /* When true, ignore in the next pass. */
  ebone->temp.i = false;

  bool is_point_done = false;
  bool is_edge_done = false;
  int points_proj_tot = 0;

  /* Project head location to screen-space. */
  if (screen_co_a[0] != IS_CLIPPED) {
    points_proj_tot++;
    if (armature_circle_doSelectJoint(data, ebone, screen_co_a, true)) {
      is_point_done = true;
    }
  }

  /* Project tail location to screen-space. */
  if (screen_co_b[0] != IS_CLIPPED) {
    points_proj_tot++;
    if (armature_circle_doSelectJoint(data, ebone, screen_co_b, false)) {
      is_point_done = true;
    }
  }

  /* check if the head and/or tail is in the circle
   * - the call to check also does the selection already
   */

  /* only if the endpoints didn't get selected, deal with the middle of the bone too
   * It works nicer to only do this if the head or tail are not in the circle,
   * otherwise there is no way to circle select joints alone */
  if ((is_point_done == false) && (points_proj_tot == 2) &&
      edge_inside_circle(data->mval_fl, data->radius, screen_co_a, screen_co_b))
  {
    SET_FLAG_FROM_TEST(ebone->flag, data->select, BONE_SELECTED | BONE_TIPSEL | BONE_ROOTSEL);
    is_edge_done = true;
    data->is_changed = true;
  }

  if (is_point_done || is_edge_done) {
    ebone->temp.i = true;
  }

  data->is_changed |= is_point_done;
}
static void do_circle_select_armature__doSelectBone_clip_content(void *user_data,
                                                                 EditBone *ebone,
                                                                 const float screen_co_a[2],
                                                                 const float screen_co_b[2])
{
  CircleSelectUserData *data = static_cast<CircleSelectUserData *>(user_data);
  bArmature *arm = static_cast<bArmature *>(data->vc->obedit->data);

  if (!(data->select ? EBONE_SELECTABLE(arm, ebone) :
                       blender::animrig::bone_is_visible(arm, ebone)))
  {
    return;
  }

  /* Set in the first pass, needed so circle select prioritizes joints. */
  if (ebone->temp.i != 0) {
    return;
  }

  if (edge_inside_circle(data->mval_fl, data->radius, screen_co_a, screen_co_b)) {
    SET_FLAG_FROM_TEST(ebone->flag, data->select, BONE_SELECTED | BONE_TIPSEL | BONE_ROOTSEL);
    data->is_changed = true;
  }
}
static bool armature_circle_select(const ViewContext *vc,
                                   const eSelectOp sel_op,
                                   const int mval[2],
                                   float rad)
{
  CircleSelectUserData data;
  bArmature *arm = static_cast<bArmature *>(vc->obedit->data);

  const bool select = (sel_op != SEL_OP_SUB);

  view3d_userdata_circleselect_init(&data, vc, select, mval, rad);

  if (SEL_OP_USE_PRE_DESELECT(sel_op)) {
    data.is_changed |= ED_armature_edit_deselect_all_visible(vc->obedit);
  }

  ED_view3d_init_mats_rv3d(vc->obedit, vc->rv3d);

  /* Operate on fully visible (non-clipped) points. */
  armature_foreachScreenBone(
      vc, do_circle_select_armature__doSelectBone, &data, V3D_PROJ_TEST_CLIP_DEFAULT);

  /* Operate on bones as segments clipped to the viewport bounds
   * (needed to handle bones with both points outside the view).
   * A separate pass is needed since clipped coordinates can't be used for selecting joints. */
  armature_foreachScreenBone(vc,
                             do_circle_select_armature__doSelectBone_clip_content,
                             &data,
                             V3D_PROJ_TEST_CLIP_DEFAULT | V3D_PROJ_TEST_CLIP_CONTENT_DEFAULT);

  if (data.is_changed) {
    ED_armature_edit_sync_selection(arm->edbo);
    WM_main_add_notifier(NC_OBJECT | ND_BONE_SELECT, vc->obedit);
  }
  return data.is_changed;
}

static void do_circle_select_mball__doSelectElem(void *user_data,
                                                 MetaElem *ml,
                                                 const float screen_co[2])
{
  CircleSelectUserData *data = static_cast<CircleSelectUserData *>(user_data);

  if (len_squared_v2v2(data->mval_fl, screen_co) <= data->radius_squared) {
    if (data->select) {
      ml->flag |= SELECT;
    }
    else {
      ml->flag &= ~SELECT;
    }
    data->is_changed = true;
  }
}
static bool mball_circle_select(const ViewContext *vc,
                                const eSelectOp sel_op,
                                const int mval[2],
                                float rad)
{
  CircleSelectUserData data;

  const bool select = (sel_op != SEL_OP_SUB);

  view3d_userdata_circleselect_init(&data, vc, select, mval, rad);

  if (SEL_OP_USE_PRE_DESELECT(sel_op)) {
    data.is_changed |= BKE_mball_deselect_all(static_cast<MetaBall *>(vc->obedit->data));
  }

  ED_view3d_init_mats_rv3d(vc->obedit, vc->rv3d);

  mball_foreachScreenElem(
      vc, do_circle_select_mball__doSelectElem, &data, V3D_PROJ_TEST_CLIP_DEFAULT);
  return data.is_changed;
}

static bool grease_pencil_circle_select(const ViewContext *vc,
                                        const eSelectOp sel_op,
                                        const int mval[2],
                                        const float rad)
{
  using namespace blender;
  Object *object = (vc->obedit ? vc->obedit : vc->obact);
  const Object *ob_eval = DEG_get_evaluated(vc->depsgraph, object);
  const GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object->data);
  const bke::AttrDomain selection_domain = ED_grease_pencil_selection_domain_get(
      vc->scene->toolsettings, object);

  return ed::greasepencil::selection_update(
      vc,
      sel_op,
      [&](const ed::greasepencil::MutableDrawingInfo &info,
          const IndexMask &mask,
          const StringRef attribute_name,
          IndexMaskMemory &memory) {
        bke::CurvesGeometry &curves = info.drawing.strokes_for_write();
        const bke::greasepencil::Layer &layer = grease_pencil.layer(info.layer_index);
        const bke::crazyspace::GeometryDeformation deformation =
            bke::crazyspace::get_evaluated_grease_pencil_drawing_deformation(
                ob_eval, *object, info.drawing);
        const IndexMask visible_handle_elements =
            ed::greasepencil::retrieve_visible_bezier_handle_elements(
                *object,
                info.drawing,
                info.layer_index,
                selection_domain,
                vc->v3d->overlay.handle_display,
                memory);
        const float4x4 layer_to_world = layer.to_world_space(*ob_eval);
        const float4x4 projection = ED_view3d_ob_project_mat_get_from_obmat(vc->rv3d,
                                                                            layer_to_world);

        return ed::curves::select_circle_mask(*vc,
                                              curves,
                                              deformation,
                                              projection,
                                              mask,
                                              visible_handle_elements,
                                              selection_domain,
                                              attribute_name,
                                              int2(mval),
                                              rad,
                                              memory);
      });
}

/**
 * Callbacks for circle selection in Editmode
 */
static bool obedit_circle_select(bContext *C,
                                 const ViewContext *vc,
                                 wmGenericUserData *wm_userdata,
                                 const eSelectOp sel_op,
                                 const int mval[2],
                                 float rad)
{
  using namespace blender;
  bool changed = false;
  BLI_assert(ELEM(sel_op, SEL_OP_SET, SEL_OP_ADD, SEL_OP_SUB));
  switch (vc->obedit->type) {
    case OB_MESH:
      changed = mesh_circle_select(vc, wm_userdata, sel_op, mval, rad);
      break;
    case OB_CURVES_LEGACY:
    case OB_SURF:
      changed = nurbscurve_circle_select(vc, sel_op, mval, rad);
      break;
    case OB_LATTICE:
      changed = lattice_circle_select(vc, sel_op, mval, rad);
      break;
    case OB_ARMATURE:
      changed = armature_circle_select(vc, sel_op, mval, rad);
      if (changed) {
        ED_outliner_select_sync_from_edit_bone_tag(C);
      }
      break;
    case OB_MBALL:
      changed = mball_circle_select(vc, sel_op, mval, rad);
      break;
    case OB_CURVES: {
      Curves &curves_id = *static_cast<Curves *>(vc->obedit->data);
      bke::CurvesGeometry &curves = curves_id.geometry.wrap();
      bke::crazyspace::GeometryDeformation deformation =
          bke::crazyspace::get_evaluated_curves_deformation(*vc->depsgraph, *vc->obedit);
      const bke::AttrDomain selection_domain = bke::AttrDomain(curves_id.selection_domain);
      const float4x4 projection = ED_view3d_ob_project_mat_get(vc->rv3d, vc->obedit);
      const IndexRange elements(curves.attributes().domain_size(selection_domain));
      changed = ed::curves::select_circle(*vc,
                                          curves,
                                          deformation,
                                          projection,
                                          elements,
                                          elements,
                                          selection_domain,
                                          mval,
                                          rad,
                                          sel_op);
      if (changed) {
        /* Use #ID_RECALC_GEOMETRY instead of #ID_RECALC_SELECT because it is handled as a
         * generic attribute for now. */
        DEG_id_tag_update(static_cast<ID *>(vc->obedit->data), ID_RECALC_GEOMETRY);
        WM_event_add_notifier(C, NC_GEOM | ND_DATA, vc->obedit->data);
      }
      break;
    }
    case OB_POINTCLOUD: {
      PointCloud &pointcloud = *static_cast<PointCloud *>(vc->obedit->data);
      const float4x4 projection = ED_view3d_ob_project_mat_get(vc->rv3d, vc->obedit);
      changed = ed::pointcloud::select_circle(
          pointcloud, *vc->region, projection, mval, rad, sel_op);
      if (changed) {
        /* Use #ID_RECALC_GEOMETRY instead of #ID_RECALC_SELECT because it is handled as a
         * generic attribute for now. */
        DEG_id_tag_update(static_cast<ID *>(vc->obedit->data), ID_RECALC_GEOMETRY);
        WM_event_add_notifier(C, NC_GEOM | ND_DATA, vc->obedit->data);
      }
      break;
    }
    case OB_GREASE_PENCIL:
      changed = grease_pencil_circle_select(vc, sel_op, mval, rad);
      break;

    default:
      BLI_assert(0);
      break;
  }

  if (changed) {
    DEG_id_tag_update(static_cast<ID *>(vc->obact->data), ID_RECALC_SELECT);
    WM_main_add_notifier(NC_GEOM | ND_SELECT, vc->obact->data);
  }
  return changed;
}

static bool object_circle_select(const ViewContext *vc,
                                 const eSelectOp sel_op,
                                 const int mval[2],
                                 float rad)
{
  BLI_assert(ELEM(sel_op, SEL_OP_SET, SEL_OP_ADD, SEL_OP_SUB));
  Scene *scene = vc->scene;
  ViewLayer *view_layer = vc->view_layer;
  View3D *v3d = vc->v3d;

  const float radius_squared = rad * rad;
  const float mval_fl[2] = {float(mval[0]), float(mval[1])};

  bool changed = false;
  if (SEL_OP_USE_PRE_DESELECT(sel_op)) {
    changed |= object_deselect_all_visible(vc->scene, vc->view_layer, vc->v3d);
  }
  const bool select = (sel_op != SEL_OP_SUB);
  const int select_flag = select ? BASE_SELECTED : 0;
  BKE_view_layer_synced_ensure(scene, view_layer);
  LISTBASE_FOREACH (Base *, base, BKE_view_layer_object_bases_get(view_layer)) {
    if (BASE_SELECTABLE(v3d, base) && ((base->flag & BASE_SELECTED) != select_flag)) {
      float screen_co[2];
      if (ED_view3d_project_float_global(vc->region,
                                         base->object->object_to_world().location(),
                                         screen_co,
                                         V3D_PROJ_TEST_CLIP_DEFAULT) == V3D_PROJ_RET_OK)
      {
        if (len_squared_v2v2(mval_fl, screen_co) <= radius_squared) {
          blender::ed::object::base_select(
              base, select ? blender::ed::object::BA_SELECT : blender::ed::object::BA_DESELECT);
          changed = true;
        }
      }
    }
  }

  return changed;
}

/* not a real operator, only for circle test */
static void view3d_circle_select_recalc(void *user_data)
{
  bContext *C = static_cast<bContext *>(user_data);
  Object *obedit_active = CTX_data_edit_object(C);

  if (obedit_active) {
    switch (obedit_active->type) {
      case OB_MESH: {
        ViewContext vc = em_setup_viewcontext(C);
        FOREACH_OBJECT_IN_MODE_BEGIN (
            vc.scene, vc.view_layer, vc.v3d, vc.obact->type, vc.obact->mode, ob_iter)
        {
          ED_view3d_viewcontext_init_object(&vc, ob_iter);
          BM_mesh_select_mode_flush_ex(vc.em->bm, vc.em->selectmode, BMSelectFlushFlag_All);
        }
        FOREACH_OBJECT_IN_MODE_END;
        break;
      }

      default: {
        /* TODO: investigate if this is needed for other object types. */
        CTX_data_ensure_evaluated_depsgraph(C);
        break;
      }
    }
  }
}

static wmOperatorStatus view3d_circle_select_modal(bContext *C,
                                                   wmOperator *op,
                                                   const wmEvent *event)
{
  wmOperatorStatus result = WM_gesture_circle_modal(C, op, event);
  if (result & OPERATOR_FINISHED) {
    view3d_circle_select_recalc(C);
  }
  return result;
}

static void view3d_circle_select_cancel(bContext *C, wmOperator *op)
{
  WM_gesture_circle_cancel(C, op);
  view3d_circle_select_recalc(C);
}

static wmOperatorStatus view3d_circle_select_exec(bContext *C, wmOperator *op)
{
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  const int radius = RNA_int_get(op->ptr, "radius");
  const int mval[2] = {RNA_int_get(op->ptr, "x"), RNA_int_get(op->ptr, "y")};

  /* Allow each selection type to allocate their own data that's used between executions. */
  wmGesture *gesture = static_cast<wmGesture *>(op->customdata); /* nullptr when non-modal. */
  wmGenericUserData wm_userdata_buf = {nullptr, nullptr, false};
  wmGenericUserData *wm_userdata = gesture ? &gesture->user_data : &wm_userdata_buf;

  const eSelectOp sel_op = ED_select_op_modal(
      static_cast<eSelectOp>(RNA_enum_get(op->ptr, "mode")), WM_gesture_is_modal_first(gesture));

  ViewContext vc = ED_view3d_viewcontext_init(C, depsgraph);

  Object *obact = vc.obact;
  Object *obedit = vc.obedit;

  if (obedit || BKE_paint_select_elem_test(obact) || (obact && (obact->mode & OB_MODE_POSE))) {
    view3d_operator_needs_gpu(C);
    if (obedit == nullptr) {
      BKE_object_update_select_id(CTX_data_main(C));
    }
    else {
      if (vc.obedit->type == OB_MESH) {
        vc.em = BKE_editmesh_from_object(vc.obedit);
      }
    }

    FOREACH_OBJECT_IN_MODE_BEGIN (
        vc.scene, vc.view_layer, vc.v3d, obact->type, obact->mode, ob_iter)
    {
      ED_view3d_viewcontext_init_object(&vc, ob_iter);

      obact = vc.obact;
      obedit = vc.obedit;

      if (obedit) {
        obedit_circle_select(C, &vc, wm_userdata, sel_op, mval, float(radius));
      }
      else if (BKE_paint_select_face_test(obact)) {
        paint_facesel_circle_select(&vc, wm_userdata, sel_op, mval, float(radius));
      }
      else if (BKE_paint_select_vert_test(obact)) {
        paint_vertsel_circle_select(&vc, wm_userdata, sel_op, mval, float(radius));
      }
      else if (BKE_paint_select_grease_pencil_test(obact)) {
        grease_pencil_circle_select(&vc, sel_op, mval, float(radius));
      }
      else if (obact->mode & OB_MODE_POSE) {
        pose_circle_select(&vc, sel_op, mval, float(radius));
        ED_outliner_select_sync_from_pose_bone_tag(C);
      }
      else {
        BLI_assert(0);
      }
    }
    FOREACH_OBJECT_IN_MODE_END;
  }
  else if (obact && (obact->mode & OB_MODE_PARTICLE_EDIT)) {
    if (PE_circle_select(C, wm_userdata, sel_op, mval, float(radius))) {
      return OPERATOR_FINISHED;
    }
    return OPERATOR_CANCELLED;
  }
  else if (obact && obact->mode & OB_MODE_SCULPT) {
    return OPERATOR_CANCELLED;
  }
  else if (Object *obact_pose = (obact && (obact->mode & OB_MODE_WEIGHT_PAINT)) ?
                                    BKE_object_pose_armature_get_with_wpaint_check(obact) :
                                    nullptr)
  {
    ED_view3d_viewcontext_init_object(&vc, obact_pose);
    pose_circle_select(&vc, sel_op, mval, float(radius));
    ED_outliner_select_sync_from_pose_bone_tag(C);
  }
  else {
    if (object_circle_select(&vc, sel_op, mval, float(radius))) {
      DEG_id_tag_update(&vc.scene->id, ID_RECALC_SELECT);
      WM_event_add_notifier(C, NC_SCENE | ND_OB_SELECT, vc.scene);

      ED_outliner_select_sync_from_object_tag(C);
    }
  }

  /* Otherwise this is freed by the gesture. */
  if (wm_userdata == &wm_userdata_buf) {
    WM_generic_user_data_free(wm_userdata);
  }
  else {
    EditSelectBuf_Cache *esel = static_cast<EditSelectBuf_Cache *>(wm_userdata->data);
    if (esel && esel->select_bitmap) {
      MEM_freeN(esel->select_bitmap);
      esel->select_bitmap = nullptr;
    }
  }

  return OPERATOR_FINISHED;
}

void VIEW3D_OT_select_circle(wmOperatorType *ot)
{
  ot->name = "Circle Select";
  ot->description = "Select items using circle selection";
  ot->idname = "VIEW3D_OT_select_circle";

  ot->invoke = WM_gesture_circle_invoke;
  ot->modal = view3d_circle_select_modal;
  ot->exec = view3d_circle_select_exec;
  ot->poll = view3d_selectable_data;
  ot->cancel = view3d_circle_select_cancel;
  ot->get_name = ED_select_circle_get_name;

  /* flags */
  ot->flag = OPTYPE_UNDO;

  /* properties */
  WM_operator_properties_gesture_circle(ot);
  WM_operator_properties_select_operation_simple(ot);
}

/** \} */
