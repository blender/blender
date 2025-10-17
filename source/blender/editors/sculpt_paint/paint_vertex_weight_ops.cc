/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edsculpt
 */

#include "MEM_guardedalloc.h"

#include "BLI_bitmap.h"
#include "BLI_enum_flags.hh"
#include "BLI_listbase.h"
#include "BLI_math_geom.h"
#include "BLI_math_vector.h"

#include "DNA_brush_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "BKE_attribute.hh"
#include "BKE_brush.hh"
#include "BKE_colortools.hh"
#include "BKE_context.hh"
#include "BKE_deform.hh"
#include "BKE_mesh.hh"
#include "BKE_mesh_iterators.hh"
#include "BKE_modifier.hh"
#include "BKE_object.hh"
#include "BKE_object_deform.h"
#include "BKE_paint.hh"
#include "BKE_report.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_query.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "ED_armature.hh"
#include "ED_mesh.hh"
#include "ED_screen.hh"
#include "ED_view3d.hh"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "paint_intern.hh" /* own include */

/* -------------------------------------------------------------------- */
/** \name Store Previous Weights
 *
 * Use to avoid feedback loop w/ mirrored edits.
 * \{ */

struct WPaintPrev {
  /* previous vertex weights */
  MDeformVert *wpaint_prev;
  /* allocation size of prev buffers */
  int tot;
};

static void wpaint_prev_init(WPaintPrev *wpp)
{
  wpp->wpaint_prev = nullptr;
  wpp->tot = 0;
}

static void wpaint_prev_create(WPaintPrev *wpp, MDeformVert *dverts, int dcount)
{
  wpaint_prev_init(wpp);

  if (dverts && dcount) {
    wpp->wpaint_prev = MEM_malloc_arrayN<MDeformVert>(dcount, __func__);
    wpp->tot = dcount;
    BKE_defvert_array_copy(wpp->wpaint_prev, dverts, dcount);
  }
}

static void wpaint_prev_destroy(WPaintPrev *wpp)
{
  if (wpp->wpaint_prev) {
    BKE_defvert_array_free(wpp->wpaint_prev, wpp->tot);
  }
  wpp->wpaint_prev = nullptr;
  wpp->tot = 0;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Weight from Bones Operator
 * \{ */

static bool weight_from_bones_poll(bContext *C)
{
  Object *ob = CTX_data_active_object(C);

  return (ob && (ob->mode & OB_MODE_WEIGHT_PAINT) && BKE_modifiers_is_deformed_by_armature(ob));
}

static wmOperatorStatus weight_from_bones_exec(bContext *C, wmOperator *op)
{
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Scene *scene = CTX_data_scene(C);
  Object *ob = CTX_data_active_object(C);
  Object *armob = BKE_modifiers_is_deformed_by_armature(ob);
  Mesh *mesh = static_cast<Mesh *>(ob->data);
  int type = RNA_enum_get(op->ptr, "type");

  ED_object_vgroup_calc_from_armature(
      op->reports, depsgraph, scene, ob, armob, type, (mesh->symmetry & ME_SYMMETRY_X));

  DEG_id_tag_update(&mesh->id, 0);
  DEG_relations_tag_update(CTX_data_main(C));
  WM_event_add_notifier(C, NC_GEOM | ND_DATA, mesh);

  return OPERATOR_FINISHED;
}

void PAINT_OT_weight_from_bones(wmOperatorType *ot)
{
  static const EnumPropertyItem type_items[] = {
      {ARM_GROUPS_AUTO, "AUTOMATIC", 0, "Automatic", "Automatic weights from bones"},
      {ARM_GROUPS_ENVELOPE,
       "ENVELOPES",
       0,
       "From Envelopes",
       "Weights from envelopes with user defined radius"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  /* identifiers */
  ot->name = "Weight from Bones";
  ot->idname = "PAINT_OT_weight_from_bones";
  ot->description =
      ("Set the weights of the groups matching the attached armature's selected bones, "
       "using the distance between the vertices and the bones");

  /* API callbacks. */
  ot->exec = weight_from_bones_exec;
  ot->invoke = WM_menu_invoke;
  ot->poll = weight_from_bones_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  ot->prop = RNA_def_enum(
      ot->srna, "type", type_items, 0, "Type", "Method to use for assigning weights");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Sample Weight Operator
 * \{ */

/**
 * Sets wp->weight to the closest weight value to vertex.
 *
 * \note we can't sample front-buffer, weight colors are interpolated too unpredictable.
 */
static wmOperatorStatus weight_sample_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Mesh *mesh;
  bool changed = false;

  ViewContext vc = ED_view3d_viewcontext_init(C, depsgraph);
  mesh = BKE_mesh_from_object(vc.obact);
  const MDeformVert *dvert = mesh->deform_verts().data();

  if (mesh && dvert && vc.v3d && vc.rv3d && (mesh->vertex_group_active_index != 0)) {
    const bool use_vert_sel = (mesh->editflag & ME_EDIT_PAINT_VERT_SEL) != 0;
    int v_idx_best = -1;
    uint index;

    view3d_operator_needs_gpu(C);
    ED_view3d_init_mats_rv3d(vc.obact, vc.rv3d);

    if (use_vert_sel) {
      if (ED_mesh_pick_vert(
              C, vc.obact, event->mval, ED_MESH_PICK_DEFAULT_VERT_DIST, true, &index))
      {
        v_idx_best = index;
      }
    }
    else {
      if (ED_mesh_pick_face_vert(C, vc.obact, event->mval, ED_MESH_PICK_DEFAULT_FACE_DIST, &index))
      {
        v_idx_best = index;
      }
      else if (ED_mesh_pick_face(C, vc.obact, event->mval, ED_MESH_PICK_DEFAULT_FACE_DIST, &index))
      {
        /* This relies on knowing the internal workings of #ED_mesh_pick_face_vert() */
        BKE_report(
            op->reports, RPT_WARNING, "The modifier used does not support deformed locations");
      }
    }

    if (v_idx_best != -1) { /* should always be valid */
      ToolSettings *ts = vc.scene->toolsettings;
      Brush *brush = BKE_paint_brush(&ts->wpaint->paint);
      const int vgroup_active = mesh->vertex_group_active_index - 1;
      float vgroup_weight = BKE_defvert_find_weight(&dvert[v_idx_best], vgroup_active);
      const int defbase_tot = BLI_listbase_count(&mesh->vertex_group_names);
      bool use_lock_relative = ts->wpaint_lock_relative;
      bool *defbase_locked = nullptr, *defbase_unlocked = nullptr;

      if (use_lock_relative) {
        defbase_locked = BKE_object_defgroup_lock_flags_get(vc.obact, defbase_tot);
        defbase_unlocked = BKE_object_defgroup_validmap_get(vc.obact, defbase_tot);

        use_lock_relative = BKE_object_defgroup_check_lock_relative(
            defbase_locked, defbase_unlocked, vgroup_active);
      }

      /* use combined weight in multipaint mode,
       * since that's what is displayed to the user in the colors */
      if (ts->multipaint) {
        int defbase_tot_sel;
        bool *defbase_sel = BKE_object_defgroup_selected_get(
            vc.obact, defbase_tot, &defbase_tot_sel);

        if (defbase_tot_sel > 1) {
          if (ME_USING_MIRROR_X_VERTEX_GROUPS(mesh)) {
            BKE_object_defgroup_mirror_selection(
                vc.obact, defbase_tot, defbase_sel, defbase_sel, &defbase_tot_sel);
          }

          use_lock_relative = use_lock_relative &&
                              BKE_object_defgroup_check_lock_relative_multi(
                                  defbase_tot, defbase_locked, defbase_sel, defbase_tot_sel);

          bool is_normalized = ts->auto_normalize || use_lock_relative;
          vgroup_weight = BKE_defvert_multipaint_collective_weight(
              &dvert[v_idx_best], defbase_tot, defbase_sel, defbase_tot_sel, is_normalized);
        }

        MEM_freeN(defbase_sel);
      }

      if (use_lock_relative) {
        BKE_object_defgroup_split_locked_validmap(
            defbase_tot, defbase_locked, defbase_unlocked, defbase_locked, defbase_unlocked);

        vgroup_weight = BKE_defvert_lock_relative_weight(
            vgroup_weight, &dvert[v_idx_best], defbase_tot, defbase_locked, defbase_unlocked);
      }

      MEM_SAFE_FREE(defbase_locked);
      MEM_SAFE_FREE(defbase_unlocked);

      CLAMP(vgroup_weight, 0.0f, 1.0f);
      BKE_brush_weight_set(&ts->wpaint->paint, brush, vgroup_weight);
      changed = true;
    }
  }

  if (changed) {
    /* not really correct since the brush didn't change, but redraws the toolbar */
    WM_main_add_notifier(NC_BRUSH | NA_EDITED, nullptr); /* ts->wpaint->paint.brush */

    return OPERATOR_FINISHED;
  }
  return OPERATOR_CANCELLED;
}

void PAINT_OT_weight_sample(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Weight Paint Sample Weight";
  ot->idname = "PAINT_OT_weight_sample";
  ot->description = "Use the mouse to sample a weight in the 3D view";

  /* API callbacks. */
  ot->invoke = weight_sample_invoke;
  ot->poll = weight_paint_mode_poll;

  /* flags */
  ot->flag = OPTYPE_UNDO | OPTYPE_DEPENDS_ON_CURSOR;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Weight Paint Sample Group Operator
 * \{ */

/**
 * Samples cursor location, and gives menu with vertex groups to activate.
 * This function fills in used vertex-groups.
 */
static bool weight_paint_sample_mark_groups(const MDeformVert *dvert,
                                            blender::MutableSpan<bool> groups)
{
  bool found = false;
  int i = dvert->totweight;
  MDeformWeight *dw;
  for (dw = dvert->dw; i > 0; dw++, i--) {
    if (UNLIKELY(dw->def_nr >= groups.size())) {
      continue;
    }
    groups[dw->def_nr] = true;
    found = true;
  }
  return found;
}

static wmOperatorStatus weight_sample_group_invoke(bContext *C,
                                                   wmOperator *op,
                                                   const wmEvent *event)
{
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  ViewContext vc = ED_view3d_viewcontext_init(C, depsgraph);
  BLI_assert(vc.v3d && vc.rv3d); /* Ensured by poll. */

  Mesh *mesh = BKE_mesh_from_object(vc.obact);
  const MDeformVert *dverts = mesh->deform_verts().data();
  if (BLI_listbase_is_empty(&mesh->vertex_group_names) || (dverts == nullptr)) {
    BKE_report(op->reports, RPT_WARNING, "No vertex group data");
    return OPERATOR_CANCELLED;
  }

  const bool use_vert_sel = (mesh->editflag & ME_EDIT_PAINT_VERT_SEL) != 0;
  blender::Array<bool> groups(BLI_listbase_count(&mesh->vertex_group_names), false);

  bool found = false;

  view3d_operator_needs_gpu(C);
  ED_view3d_init_mats_rv3d(vc.obact, vc.rv3d);

  if (use_vert_sel) {
    /* Extract from the vertex. */
    uint index;
    if (ED_mesh_pick_vert(C, vc.obact, event->mval, ED_MESH_PICK_DEFAULT_VERT_DIST, true, &index))
    {
      const MDeformVert *dvert = &dverts[index];
      found |= weight_paint_sample_mark_groups(dvert, groups);
    }
  }
  else {
    /* Extract from the face. */
    const blender::OffsetIndices faces = mesh->faces();
    const blender::Span<int> corner_verts = mesh->corner_verts();
    uint index;
    if (ED_mesh_pick_face(C, vc.obact, event->mval, ED_MESH_PICK_DEFAULT_FACE_DIST, &index)) {
      for (const int vert : corner_verts.slice(faces[index])) {
        found |= weight_paint_sample_mark_groups(&dverts[vert], groups);
      }
    }
  }

  if (found == false) {
    BKE_report(op->reports, RPT_WARNING, "No vertex groups found");
    return OPERATOR_CANCELLED;
  }

  uiPopupMenu *pup = UI_popup_menu_begin(
      C, WM_operatortype_name(op->type, op->ptr).c_str(), ICON_NONE);
  uiLayout *layout = UI_popup_menu_layout(pup);
  wmOperatorType *ot = WM_operatortype_find("OBJECT_OT_vertex_group_set_active", false);
  blender::wm::OpCallContext opcontext = blender::wm::OpCallContext::ExecDefault;
  layout->operator_context_set(opcontext);
  int i = 0;
  LISTBASE_FOREACH_INDEX (bDeformGroup *, dg, &mesh->vertex_group_names, i) {
    if (groups[i] == false) {
      continue;
    }
    PointerRNA op_ptr = layout->op(
        ot, dg->name, ICON_NONE, blender::wm::OpCallContext::ExecDefault, UI_ITEM_NONE);
    RNA_property_enum_set(&op_ptr, ot->prop, i);
  }
  UI_popup_menu_end(C, pup);

  return OPERATOR_INTERFACE;
}

void PAINT_OT_weight_sample_group(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Weight Paint Sample Group";
  ot->idname = "PAINT_OT_weight_sample_group";
  ot->description = "Select one of the vertex groups available under current mouse position";

  /* API callbacks. */
  ot->invoke = weight_sample_group_invoke;
  ot->poll = weight_paint_mode_region_view3d_poll;

  /* flags */
  ot->flag = OPTYPE_UNDO | OPTYPE_DEPENDS_ON_CURSOR;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Weight Set Operator
 * \{ */

/* fills in the selected faces with the current weight and vertex group */
static bool weight_paint_set(Object *ob, float paintweight)
{
  using namespace blender;
  Mesh *mesh = static_cast<Mesh *>(ob->data);
  MDeformWeight *dw, *dw_prev;
  int vgroup_active, vgroup_mirror = -1;
  const bool topology = (mesh->editflag & ME_EDIT_MIRROR_TOPO) != 0;

  /* mutually exclusive, could be made into a */
  const short paint_selmode = ME_EDIT_PAINT_SEL_MODE(mesh);

  const blender::OffsetIndices faces = mesh->faces();
  const blender::Span<int> corner_verts = mesh->corner_verts();
  MDeformVert *dvert = mesh->deform_verts_for_write().data();

  if (mesh->faces_num == 0 || dvert == nullptr) {
    return false;
  }

  vgroup_active = BKE_object_defgroup_active_index_get(ob) - 1;

  /* if mirror painting, find the other group */
  if (ME_USING_MIRROR_X_VERTEX_GROUPS(mesh)) {
    vgroup_mirror = ED_wpaint_mirror_vgroup_ensure(ob, vgroup_active);
  }

  WPaintPrev wpp;
  wpaint_prev_create(&wpp, dvert, mesh->verts_num);

  const bke::AttributeAccessor attributes = mesh->attributes();
  const VArraySpan select_vert = *attributes.lookup<bool>(".select_vert", bke::AttrDomain::Point);
  const VArraySpan select_poly = *attributes.lookup<bool>(".select_poly", bke::AttrDomain::Face);

  for (const int i : faces.index_range()) {
    if ((paint_selmode == SCE_SELECT_FACE) && !(!select_poly.is_empty() && select_poly[i])) {
      continue;
    }

    for (const int vert : corner_verts.slice(faces[i])) {
      if (!dvert[vert].flag) {
        if ((paint_selmode == SCE_SELECT_VERTEX) &&
            !(!select_vert.is_empty() && select_vert[vert]))
        {
          continue;
        }

        dw = BKE_defvert_ensure_index(&dvert[vert], vgroup_active);
        if (dw) {
          dw_prev = BKE_defvert_ensure_index(wpp.wpaint_prev + vert, vgroup_active);
          dw_prev->weight = dw->weight; /* set the undo weight */
          dw->weight = paintweight;

          if (mesh->symmetry & ME_SYMMETRY_X) {
            /* x mirror painting */
            int j = mesh_get_x_mirror_vert(ob, nullptr, vert, topology);
            if (j >= 0) {
              /* copy, not paint again */
              if (vgroup_mirror != -1) {
                dw = BKE_defvert_ensure_index(dvert + j, vgroup_mirror);
                dw_prev = BKE_defvert_ensure_index(wpp.wpaint_prev + j, vgroup_mirror);
              }
              else {
                dw = BKE_defvert_ensure_index(dvert + j, vgroup_active);
                dw_prev = BKE_defvert_ensure_index(wpp.wpaint_prev + j, vgroup_active);
              }
              dw_prev->weight = dw->weight; /* set the undo weight */
              dw->weight = paintweight;
            }
          }
        }
        dvert[vert].flag = 1;
      }
    }
  }

  {
    MDeformVert *dv = dvert;
    for (int index = mesh->verts_num; index != 0; index--, dv++) {
      dv->flag = 0;
    }
  }

  wpaint_prev_destroy(&wpp);

  DEG_id_tag_update(&mesh->id, 0);

  return true;
}

static wmOperatorStatus weight_paint_set_exec(bContext *C, wmOperator *op)
{
  Object *obact = CTX_data_active_object(C);
  ToolSettings *ts = CTX_data_tool_settings(C);
  Brush *brush = BKE_paint_brush(&ts->wpaint->paint);
  float vgroup_weight = BKE_brush_weight_get(&ts->wpaint->paint, brush);

  if (ED_wpaint_ensure_data(C, op->reports, WPAINT_ENSURE_MIRROR, nullptr) == false) {
    return OPERATOR_CANCELLED;
  }

  if (weight_paint_set(obact, vgroup_weight)) {
    ED_region_tag_redraw(CTX_wm_region(C)); /* XXX: should redraw all 3D views. */
    return OPERATOR_FINISHED;
  }
  return OPERATOR_CANCELLED;
}

void PAINT_OT_weight_set(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Set Weight";
  ot->idname = "PAINT_OT_weight_set";
  ot->description = "Fill the active vertex group with the current paint weight";

  /* API callbacks. */
  ot->exec = weight_paint_set_exec;
  ot->poll = weight_paint_mode_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Interactive Weight Gradient Operator
 * \{ */

/* *** VGroups Gradient *** */
struct WPGradient_vertStore {
  enum Flag {
    VGRAD_STORE_NOP = 0,
    VGRAD_STORE_DW_EXIST = (1 << 0),
    VGRAD_STORE_IS_MODIFIED = (1 << 1)
  };
  float sco[2];
  float weight_orig;
  Flag flag;
};
ENUM_OPERATORS(WPGradient_vertStore::Flag);

struct WPGradient_vertStoreBase {
  WPaintPrev wpp;
  WPGradient_vertStore elem[0];
};

struct WPGradient_userData {
  ARegion *region;
  Scene *scene;
  Mesh *mesh;
  MDeformVert *dvert;
  blender::VArraySpan<bool> select_vert;
  blender::VArray<bool> hide_vert;
  Brush *brush;
  const float *sco_start; /* [2] */
  const float *sco_end;   /* [2] */
  float sco_line_div;     /* store (1.0f / len_v2v2(sco_start, sco_end)) */
  int def_nr;
  bool is_init;
  WPGradient_vertStoreBase *vert_cache;
  /* only for init */
  BLI_bitmap *vert_visit;

  /* options */
  bool use_select;
  bool use_vgroup_restrict;
  short type;
  float weightpaint;
};

static void gradientVert_update(WPGradient_userData *grad_data, int index)
{
  WPGradient_vertStore *vs = &grad_data->vert_cache->elem[index];

  /* Optionally restrict to assigned vertices only. */
  if (grad_data->use_vgroup_restrict &&
      ((vs->flag & WPGradient_vertStore::VGRAD_STORE_DW_EXIST) == 0))
  {
    /* In this case the vertex will never have been touched. */
    BLI_assert((vs->flag & WPGradient_vertStore::VGRAD_STORE_IS_MODIFIED) == 0);
    return;
  }

  float alpha;
  if (grad_data->type == WPAINT_GRADIENT_TYPE_LINEAR) {
    alpha = line_point_factor_v2(vs->sco, grad_data->sco_start, grad_data->sco_end);
  }
  else {
    BLI_assert(grad_data->type == WPAINT_GRADIENT_TYPE_RADIAL);
    alpha = len_v2v2(grad_data->sco_start, vs->sco) * grad_data->sco_line_div;
  }

  /* adjust weight */
  alpha = BKE_brush_curve_strength_clamped(grad_data->brush, std::max(0.0f, alpha), 1.0f);

  if (alpha != 0.0f) {
    MDeformVert *dv = &grad_data->dvert[index];
    MDeformWeight *dw = BKE_defvert_ensure_index(dv, grad_data->def_nr);
    // dw->weight = alpha; // testing
    int tool = grad_data->brush->blend;
    float testw;

    /* init if we just added */
    testw = ED_wpaint_blend_tool(
        tool, vs->weight_orig, grad_data->weightpaint, alpha * grad_data->brush->alpha);
    CLAMP(testw, 0.0f, 1.0f);
    dw->weight = testw;
    vs->flag |= WPGradient_vertStore::VGRAD_STORE_IS_MODIFIED;
  }
  else {
    MDeformVert *dv = &grad_data->dvert[index];
    if (vs->flag & WPGradient_vertStore::VGRAD_STORE_DW_EXIST) {
      /* normally we nullptr check, but in this case we know it exists */
      MDeformWeight *dw = BKE_defvert_find_index(dv, grad_data->def_nr);
      dw->weight = vs->weight_orig;
    }
    else {
      /* wasn't originally existing, remove */
      MDeformWeight *dw = BKE_defvert_find_index(dv, grad_data->def_nr);
      if (dw) {
        BKE_defvert_remove_group(dv, dw);
      }
    }
    vs->flag &= ~WPGradient_vertStore::VGRAD_STORE_IS_MODIFIED;
  }
}

static void gradientVertUpdate__mapFunc(void *user_data,
                                        int index,
                                        const float /*co*/[3],
                                        const float /*no*/[3])
{
  WPGradient_userData *grad_data = static_cast<WPGradient_userData *>(user_data);
  WPGradient_vertStore *vs = &grad_data->vert_cache->elem[index];

  if (vs->sco[0] == FLT_MAX) {
    return;
  }

  gradientVert_update(grad_data, index);
}

static void gradientVertInit__mapFunc(void *user_data,
                                      int index,
                                      const float co[3],
                                      const float /*no*/[3])
{
  WPGradient_userData *grad_data = static_cast<WPGradient_userData *>(user_data);
  WPGradient_vertStore *vs = &grad_data->vert_cache->elem[index];

  if (grad_data->hide_vert[index] ||
      (grad_data->use_select &&
       (!grad_data->select_vert.is_empty() && !grad_data->select_vert[index])))
  {
    copy_v2_fl(vs->sco, FLT_MAX);
    return;
  }

  /* run first pass only,
   * the screen coords of the verts need to be cached because
   * updating the mesh may move them about (entering feedback loop) */
  if (BLI_BITMAP_TEST(grad_data->vert_visit, index)) {
    /* Do not copy FLT_MAX here, for generative modifiers we are getting here
     * multiple times with the same orig index. */
    return;
  }

  if (ED_view3d_project_float_object(
          grad_data->region, co, vs->sco, V3D_PROJ_TEST_CLIP_BB | V3D_PROJ_TEST_CLIP_NEAR) !=
      V3D_PROJ_RET_OK)
  {
    copy_v2_fl(vs->sco, FLT_MAX);
    return;
  }

  MDeformVert *dv = &grad_data->dvert[index];
  const MDeformWeight *dw = BKE_defvert_find_index(dv, grad_data->def_nr);
  if (dw) {
    vs->weight_orig = dw->weight;
    vs->flag = WPGradient_vertStore::VGRAD_STORE_DW_EXIST;
  }
  else {
    vs->weight_orig = 0.0f;
    vs->flag = WPGradient_vertStore::VGRAD_STORE_NOP;
  }
  BLI_BITMAP_ENABLE(grad_data->vert_visit, index);
  gradientVert_update(grad_data, index);
}

static wmOperatorStatus paint_weight_gradient_modal(bContext *C,
                                                    wmOperator *op,
                                                    const wmEvent *event)
{
  wmGesture *gesture = static_cast<wmGesture *>(op->customdata);
  WPGradient_vertStoreBase *vert_cache = static_cast<WPGradient_vertStoreBase *>(
      gesture->user_data.data);
  Object *ob = CTX_data_active_object(C);
  wmOperatorStatus ret;

  if (BKE_object_defgroup_active_is_locked(ob)) {
    BKE_report(op->reports, RPT_WARNING, "Active group is locked, aborting");
    ret = OPERATOR_CANCELLED;
  }
  else {
    ret = WM_gesture_straightline_modal(C, op, event);
  }

  if (ret & OPERATOR_RUNNING_MODAL) {
    if (event->type == LEFTMOUSE && event->val == KM_RELEASE) { /* XXX, hardcoded */
      /* generally crap! redo! */
      WM_gesture_straightline_cancel(C, op);
      ret &= ~OPERATOR_RUNNING_MODAL;
      ret |= OPERATOR_FINISHED;
    }
  }

  if (ret & OPERATOR_CANCELLED) {
    if (vert_cache != nullptr) {
      Mesh *mesh = static_cast<Mesh *>(ob->data);
      if (vert_cache->wpp.wpaint_prev) {
        MDeformVert *dvert = mesh->deform_verts_for_write().data();
        BKE_defvert_array_free_elems(dvert, mesh->verts_num);
        BKE_defvert_array_copy(dvert, vert_cache->wpp.wpaint_prev, mesh->verts_num);
        wpaint_prev_destroy(&vert_cache->wpp);
      }
      MEM_freeN(vert_cache);
    }

    DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);
  }
  else if (ret & OPERATOR_FINISHED) {
    wpaint_prev_destroy(&vert_cache->wpp);
    MEM_freeN(vert_cache);
  }

  return ret;
}

static wmOperatorStatus paint_weight_gradient_exec(bContext *C, wmOperator *op)
{
  using namespace blender;
  wmGesture *gesture = static_cast<wmGesture *>(op->customdata);
  WPGradient_vertStoreBase *vert_cache;
  ARegion *region = CTX_wm_region(C);
  Scene *scene = CTX_data_scene(C);
  Object *ob = CTX_data_active_object(C);
  Mesh *mesh = static_cast<Mesh *>(ob->data);
  MDeformVert *dverts = mesh->deform_verts_for_write().data();
  int x_start = RNA_int_get(op->ptr, "xstart");
  int y_start = RNA_int_get(op->ptr, "ystart");
  int x_end = RNA_int_get(op->ptr, "xend");
  int y_end = RNA_int_get(op->ptr, "yend");
  const float sco_start[2] = {float(x_start), float(y_start)};
  const float sco_end[2] = {float(x_end), float(y_end)};
  const bool is_interactive = (gesture != nullptr);

  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);

  WPGradient_userData data = {nullptr};

  if (is_interactive) {
    if (gesture->user_data.data == nullptr) {
      gesture->user_data.data = MEM_mallocN(sizeof(WPGradient_vertStoreBase) +
                                                (sizeof(WPGradient_vertStore) * mesh->verts_num),
                                            __func__);
      gesture->user_data.use_free = false;
      data.is_init = true;

      wpaint_prev_create(
          &((WPGradient_vertStoreBase *)gesture->user_data.data)->wpp, dverts, mesh->verts_num);

      /* On initialization only, convert face -> vert sel. */
      if (mesh->editflag & ME_EDIT_PAINT_FACE_SEL) {
        bke::mesh_select_face_flush(*mesh);
      }
    }

    vert_cache = static_cast<WPGradient_vertStoreBase *>(gesture->user_data.data);
  }
  else {
    if (ED_wpaint_ensure_data(C, op->reports, eWPaintFlag(0), nullptr) == false) {
      return OPERATOR_CANCELLED;
    }

    data.is_init = true;
    vert_cache = static_cast<WPGradient_vertStoreBase *>(MEM_mallocN(
        sizeof(WPGradient_vertStoreBase) + (sizeof(WPGradient_vertStore) * mesh->verts_num),
        __func__));
  }

  const blender::bke::AttributeAccessor attributes = mesh->attributes();

  data.region = region;
  data.scene = scene;
  data.mesh = mesh;
  data.dvert = dverts;
  data.select_vert = *attributes.lookup<bool>(".select_vert", bke::AttrDomain::Point);
  data.hide_vert = *attributes.lookup_or_default<bool>(
      ".hide_vert", bke::AttrDomain::Point, false);
  data.sco_start = sco_start;
  data.sco_end = sco_end;
  data.sco_line_div = 1.0f / len_v2v2(sco_start, sco_end);
  data.def_nr = BKE_object_defgroup_active_index_get(ob) - 1;
  data.use_select = (mesh->editflag & (ME_EDIT_PAINT_FACE_SEL | ME_EDIT_PAINT_VERT_SEL)) != 0;
  data.vert_cache = vert_cache;
  data.vert_visit = nullptr;
  data.type = RNA_enum_get(op->ptr, "type");

  {
    ToolSettings *ts = CTX_data_tool_settings(C);
    VPaint *wp = ts->wpaint;
    Brush *brush = BKE_paint_brush(&wp->paint);

    BKE_curvemapping_init(brush->curve_distance_falloff);

    data.brush = brush;
    data.weightpaint = BKE_brush_weight_get(&wp->paint, brush);
    data.use_vgroup_restrict = (ts->wpaint->flag & VP_FLAG_VGROUP_RESTRICT) != 0;
  }

  ED_view3d_init_mats_rv3d(ob, static_cast<RegionView3D *>(region->regiondata));

  const Object *ob_eval = DEG_get_evaluated(depsgraph, ob);
  const Mesh *mesh_eval = BKE_object_get_evaluated_mesh(ob_eval);
  if (data.is_init) {
    data.vert_visit = BLI_BITMAP_NEW(mesh->verts_num, __func__);

    BKE_mesh_foreach_mapped_vert(mesh_eval, gradientVertInit__mapFunc, &data, MESH_FOREACH_NOP);

    MEM_freeN(data.vert_visit);
    data.vert_visit = nullptr;
  }
  else {
    BKE_mesh_foreach_mapped_vert(mesh_eval, gradientVertUpdate__mapFunc, &data, MESH_FOREACH_NOP);
  }

  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);

  if (is_interactive == false) {
    MEM_freeN(vert_cache);
  }

  if (scene->toolsettings->auto_normalize) {
    const int vgroup_num = BLI_listbase_count(&mesh->vertex_group_names);
    bool *lock_flags = BKE_object_defgroup_lock_flags_get(ob, vgroup_num);
    if (!lock_flags) {
      lock_flags = MEM_malloc_arrayN<bool>(vgroup_num, "lock_flags");
      std::memset(lock_flags, 0, vgroup_num); /* Clear to false. */
      lock_flags[data.def_nr] = true;
    }
    bool *vgroup_validmap = BKE_object_defgroup_validmap_get(ob, vgroup_num);
    if (vgroup_validmap != nullptr) {
      MDeformVert *dvert = dverts;
      Span<bool> subset_flags_span = Span(vgroup_validmap, vgroup_num);
      Span<bool> lock_flags_span = Span(lock_flags, vgroup_num);

      for (int i = 0; i < mesh->verts_num; i++) {
        if ((data.vert_cache->elem[i].flag & WPGradient_vertStore::VGRAD_STORE_IS_MODIFIED) != 0) {
          BKE_defvert_normalize_lock_map(dvert[i], subset_flags_span, lock_flags_span);
        }
      }
      MEM_SAFE_FREE(lock_flags);
      MEM_freeN(vgroup_validmap);
    }
  }

  return OPERATOR_FINISHED;
}

static wmOperatorStatus paint_weight_gradient_invoke(bContext *C,
                                                     wmOperator *op,
                                                     const wmEvent *event)
{
  wmOperatorStatus ret;

  if (ED_wpaint_ensure_data(C, op->reports, eWPaintFlag(0), nullptr) == false) {
    return OPERATOR_CANCELLED;
  }

  ret = WM_gesture_straightline_invoke(C, op, event);
  if (ret & OPERATOR_RUNNING_MODAL) {
    ARegion *region = CTX_wm_region(C);
    if (region->regiontype == RGN_TYPE_WINDOW) {
      /* TODO: hard-coded, extend `WM_gesture_straightline_*`. */
      if (event->type == LEFTMOUSE && event->val == KM_PRESS) {
        wmGesture *gesture = static_cast<wmGesture *>(op->customdata);
        gesture->is_active = true;
      }
    }
  }
  return ret;
}

void PAINT_OT_weight_gradient(wmOperatorType *ot)
{
  /* defined in DNA_space_types.h */
  static const EnumPropertyItem gradient_types[] = {
      {WPAINT_GRADIENT_TYPE_LINEAR, "LINEAR", 0, "Linear", ""},
      {WPAINT_GRADIENT_TYPE_RADIAL, "RADIAL", 0, "Radial", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Weight Gradient";
  ot->idname = "PAINT_OT_weight_gradient";
  ot->description = "Draw a line to apply a weight gradient to selected vertices";

  /* API callbacks. */
  ot->invoke = paint_weight_gradient_invoke;
  ot->modal = paint_weight_gradient_modal;
  ot->exec = paint_weight_gradient_exec;
  ot->poll = weight_paint_poll_ignore_tool;
  ot->cancel = WM_gesture_straightline_cancel;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_DEPENDS_ON_CURSOR;

  prop = RNA_def_enum(ot->srna, "type", gradient_types, 0, "Type", "");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  WM_operator_properties_gesture_straightline(ot, WM_CURSOR_EDIT);
}

/** \} */
