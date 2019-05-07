/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/** \file
 * \ingroup edsculpt
 */

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_bitmap.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"
#include "IMB_colormanagement.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_particle_types.h"
#include "DNA_brush_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "BKE_brush.h"
#include "BKE_context.h"
#include "BKE_deform.h"
#include "BKE_mesh.h"
#include "BKE_mesh_iterators.h"
#include "BKE_mesh_mapping.h"
#include "BKE_mesh_runtime.h"
#include "BKE_modifier.h"
#include "BKE_object_deform.h"
#include "BKE_paint.h"
#include "BKE_report.h"
#include "BKE_colortools.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_armature.h"
#include "ED_mesh.h"
#include "ED_screen.h"
#include "ED_view3d.h"

#include "paint_intern.h" /* own include */

/* -------------------------------------------------------------------- */
/** \name Store Previous Weights
 *
 * Use to avoid feedback loop w/ mirrored edits.
 * \{ */

struct WPaintPrev {
  /* previous vertex weights */
  struct MDeformVert *wpaint_prev;
  /* allocation size of prev buffers */
  int tot;
};

static void wpaint_prev_init(struct WPaintPrev *wpp)
{
  wpp->wpaint_prev = NULL;
  wpp->tot = 0;
}

static void wpaint_prev_create(struct WPaintPrev *wpp, MDeformVert *dverts, int dcount)
{
  wpaint_prev_init(wpp);

  if (dverts && dcount) {
    wpp->wpaint_prev = MEM_mallocN(sizeof(MDeformVert) * dcount, "wpaint prev");
    wpp->tot = dcount;
    BKE_defvert_array_copy(wpp->wpaint_prev, dverts, dcount);
  }
}

static void wpaint_prev_destroy(struct WPaintPrev *wpp)
{
  if (wpp->wpaint_prev) {
    BKE_defvert_array_free(wpp->wpaint_prev, wpp->tot);
  }
  wpp->wpaint_prev = NULL;
  wpp->tot = 0;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Weight from Bones Operator
 * \{ */

static bool weight_from_bones_poll(bContext *C)
{
  Object *ob = CTX_data_active_object(C);

  return (ob && (ob->mode & OB_MODE_WEIGHT_PAINT) && modifiers_isDeformedByArmature(ob));
}

static int weight_from_bones_exec(bContext *C, wmOperator *op)
{
  Depsgraph *depsgraph = CTX_data_depsgraph(C);
  Scene *scene = CTX_data_scene(C);
  Object *ob = CTX_data_active_object(C);
  Object *armob = modifiers_isDeformedByArmature(ob);
  Mesh *me = ob->data;
  int type = RNA_enum_get(op->ptr, "type");

  ED_object_vgroup_calc_from_armature(
      op->reports, depsgraph, scene, ob, armob, type, (me->editflag & ME_EDIT_MIRROR_X));

  DEG_id_tag_update(&me->id, 0);
  DEG_relations_tag_update(CTX_data_main(C));
  WM_event_add_notifier(C, NC_GEOM | ND_DATA, me);

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
      {0, NULL, 0, NULL, NULL},
  };

  /* identifiers */
  ot->name = "Weight from Bones";
  ot->idname = "PAINT_OT_weight_from_bones";
  ot->description =
      ("Set the weights of the groups matching the attached armature's selected bones, "
       "using the distance between the vertices and the bones");

  /* api callbacks */
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

/* sets wp->weight to the closest weight value to vertex */
/* note: we cant sample frontbuf, weight colors are interpolated too unpredictable */
static int weight_sample_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  ViewContext vc;
  Mesh *me;
  bool changed = false;

  ED_view3d_viewcontext_init(C, &vc);
  me = BKE_mesh_from_object(vc.obact);

  if (me && me->dvert && vc.v3d && vc.rv3d && (vc.obact->actdef != 0)) {
    const bool use_vert_sel = (me->editflag & ME_EDIT_PAINT_VERT_SEL) != 0;
    int v_idx_best = -1;
    uint index;

    view3d_operator_needs_opengl(C);
    ED_view3d_init_mats_rv3d(vc.obact, vc.rv3d);

    if (use_vert_sel) {
      if (ED_mesh_pick_vert(
              C, vc.obact, event->mval, ED_MESH_PICK_DEFAULT_VERT_DIST, true, &index)) {
        v_idx_best = index;
      }
    }
    else {
      if (ED_mesh_pick_face_vert(
              C, vc.obact, event->mval, ED_MESH_PICK_DEFAULT_FACE_DIST, &index)) {
        v_idx_best = index;
      }
      else if (ED_mesh_pick_face(
                   C, vc.obact, event->mval, ED_MESH_PICK_DEFAULT_FACE_DIST, &index)) {
        /* this relies on knowning the internal worksings of ED_mesh_pick_face_vert() */
        BKE_report(
            op->reports, RPT_WARNING, "The modifier used does not support deformed locations");
      }
    }

    if (v_idx_best != -1) { /* should always be valid */
      ToolSettings *ts = vc.scene->toolsettings;
      Brush *brush = BKE_paint_brush(&ts->wpaint->paint);
      const int vgroup_active = vc.obact->actdef - 1;
      float vgroup_weight = defvert_find_weight(&me->dvert[v_idx_best], vgroup_active);

      /* use combined weight in multipaint mode,
       * since that's what is displayed to the user in the colors */
      if (ts->multipaint) {
        int defbase_tot_sel;
        const int defbase_tot = BLI_listbase_count(&vc.obact->defbase);
        bool *defbase_sel = BKE_object_defgroup_selected_get(
            vc.obact, defbase_tot, &defbase_tot_sel);

        if (defbase_tot_sel > 1) {
          if (me->editflag & ME_EDIT_MIRROR_X) {
            BKE_object_defgroup_mirror_selection(
                vc.obact, defbase_tot, defbase_sel, defbase_sel, &defbase_tot_sel);
          }

          vgroup_weight = BKE_defvert_multipaint_collective_weight(&me->dvert[v_idx_best],
                                                                   defbase_tot,
                                                                   defbase_sel,
                                                                   defbase_tot_sel,
                                                                   ts->auto_normalize);

          /* If auto-normalize is enabled, but weights are not normalized,
           * the value can exceed 1. */
          CLAMP(vgroup_weight, 0.0f, 1.0f);
        }

        MEM_freeN(defbase_sel);
      }

      BKE_brush_weight_set(vc.scene, brush, vgroup_weight);
      changed = true;
    }
  }

  if (changed) {
    /* not really correct since the brush didn't change, but redraws the toolbar */
    WM_main_add_notifier(NC_BRUSH | NA_EDITED, NULL); /* ts->wpaint->paint.brush */

    return OPERATOR_FINISHED;
  }
  else {
    return OPERATOR_CANCELLED;
  }
}

void PAINT_OT_weight_sample(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Weight Paint Sample Weight";
  ot->idname = "PAINT_OT_weight_sample";
  ot->description = "Use the mouse to sample a weight in the 3D view";

  /* api callbacks */
  ot->invoke = weight_sample_invoke;
  ot->poll = weight_paint_mode_poll;

  /* flags */
  ot->flag = OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Weight Paint Sample Group Operator
 * \{ */

/* samples cursor location, and gives menu with vertex groups to activate */
static bool weight_paint_sample_enum_itemf__helper(const MDeformVert *dvert,
                                                   const int defbase_tot,
                                                   int *groups)
{
  /* this func fills in used vgroup's */
  bool found = false;
  int i = dvert->totweight;
  MDeformWeight *dw;
  for (dw = dvert->dw; i > 0; dw++, i--) {
    if (dw->def_nr < defbase_tot) {
      groups[dw->def_nr] = true;
      found = true;
    }
  }
  return found;
}
static const EnumPropertyItem *weight_paint_sample_enum_itemf(bContext *C,
                                                              PointerRNA *UNUSED(ptr),
                                                              PropertyRNA *UNUSED(prop),
                                                              bool *r_free)
{
  if (C) {
    wmWindow *win = CTX_wm_window(C);
    if (win && win->eventstate) {
      ViewContext vc;
      Mesh *me;

      ED_view3d_viewcontext_init(C, &vc);
      me = BKE_mesh_from_object(vc.obact);

      if (me && me->dvert && vc.v3d && vc.rv3d && vc.obact->defbase.first) {
        const int defbase_tot = BLI_listbase_count(&vc.obact->defbase);
        const bool use_vert_sel = (me->editflag & ME_EDIT_PAINT_VERT_SEL) != 0;
        int *groups = MEM_callocN(defbase_tot * sizeof(int), "groups");
        bool found = false;
        uint index;

        const int mval[2] = {
            win->eventstate->x - vc.ar->winrct.xmin,
            win->eventstate->y - vc.ar->winrct.ymin,
        };

        view3d_operator_needs_opengl(C);
        ED_view3d_init_mats_rv3d(vc.obact, vc.rv3d);

        if (use_vert_sel) {
          if (ED_mesh_pick_vert(C, vc.obact, mval, ED_MESH_PICK_DEFAULT_VERT_DIST, true, &index)) {
            MDeformVert *dvert = &me->dvert[index];
            found |= weight_paint_sample_enum_itemf__helper(dvert, defbase_tot, groups);
          }
        }
        else {
          if (ED_mesh_pick_face(C, vc.obact, mval, ED_MESH_PICK_DEFAULT_FACE_DIST, &index)) {
            const MPoly *mp = &me->mpoly[index];
            uint fidx = mp->totloop - 1;

            do {
              MDeformVert *dvert = &me->dvert[me->mloop[mp->loopstart + fidx].v];
              found |= weight_paint_sample_enum_itemf__helper(dvert, defbase_tot, groups);
            } while (fidx--);
          }
        }

        if (found == false) {
          MEM_freeN(groups);
        }
        else {
          EnumPropertyItem *item = NULL, item_tmp = {0};
          int totitem = 0;
          int i = 0;
          bDeformGroup *dg;
          for (dg = vc.obact->defbase.first; dg && i < defbase_tot; i++, dg = dg->next) {
            if (groups[i]) {
              item_tmp.identifier = item_tmp.name = dg->name;
              item_tmp.value = i;
              RNA_enum_item_add(&item, &totitem, &item_tmp);
            }
          }

          RNA_enum_item_end(&item, &totitem);
          *r_free = true;

          MEM_freeN(groups);
          return item;
        }
      }
    }
  }

  return DummyRNA_NULL_items;
}

static int weight_sample_group_exec(bContext *C, wmOperator *op)
{
  int type = RNA_enum_get(op->ptr, "group");
  ViewContext vc;
  ED_view3d_viewcontext_init(C, &vc);

  BLI_assert(type + 1 >= 0);
  vc.obact->actdef = type + 1;

  DEG_id_tag_update(&vc.obact->id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, vc.obact);
  return OPERATOR_FINISHED;
}

/* TODO, we could make this a menu into OBJECT_OT_vertex_group_set_active
 * rather than its own operator */
void PAINT_OT_weight_sample_group(wmOperatorType *ot)
{
  PropertyRNA *prop = NULL;

  /* identifiers */
  ot->name = "Weight Paint Sample Group";
  ot->idname = "PAINT_OT_weight_sample_group";
  ot->description = "Select one of the vertex groups available under current mouse position";

  /* api callbacks */
  ot->exec = weight_sample_group_exec;
  ot->invoke = WM_menu_invoke;
  ot->poll = weight_paint_mode_poll;

  /* flags */
  ot->flag = OPTYPE_UNDO;

  /* keyingset to use (dynamic enum) */
  prop = RNA_def_enum(
      ot->srna, "group", DummyRNA_DEFAULT_items, 0, "Keying Set", "The Keying Set to use");
  RNA_def_enum_funcs(prop, weight_paint_sample_enum_itemf);
  RNA_def_property_flag(prop, PROP_ENUM_NO_TRANSLATE);
  ot->prop = prop;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Weight Set Operator
 * \{ */

/* fills in the selected faces with the current weight and vertex group */
static bool weight_paint_set(Object *ob, float paintweight)
{
  Mesh *me = ob->data;
  const MPoly *mp;
  MDeformWeight *dw, *dw_prev;
  int vgroup_active, vgroup_mirror = -1;
  uint index;
  const bool topology = (me->editflag & ME_EDIT_MIRROR_TOPO) != 0;

  /* mutually exclusive, could be made into a */
  const short paint_selmode = ME_EDIT_PAINT_SEL_MODE(me);

  if (me->totpoly == 0 || me->dvert == NULL || !me->mpoly) {
    return false;
  }

  vgroup_active = ob->actdef - 1;

  /* if mirror painting, find the other group */
  if (me->editflag & ME_EDIT_MIRROR_X) {
    vgroup_mirror = ED_wpaint_mirror_vgroup_ensure(ob, vgroup_active);
  }

  struct WPaintPrev wpp;
  wpaint_prev_create(&wpp, me->dvert, me->totvert);

  for (index = 0, mp = me->mpoly; index < me->totpoly; index++, mp++) {
    uint fidx = mp->totloop - 1;

    if ((paint_selmode == SCE_SELECT_FACE) && !(mp->flag & ME_FACE_SEL)) {
      continue;
    }

    do {
      uint vidx = me->mloop[mp->loopstart + fidx].v;

      if (!me->dvert[vidx].flag) {
        if ((paint_selmode == SCE_SELECT_VERTEX) && !(me->mvert[vidx].flag & SELECT)) {
          continue;
        }

        dw = defvert_verify_index(&me->dvert[vidx], vgroup_active);
        if (dw) {
          dw_prev = defvert_verify_index(wpp.wpaint_prev + vidx, vgroup_active);
          dw_prev->weight = dw->weight; /* set the undo weight */
          dw->weight = paintweight;

          if (me->editflag & ME_EDIT_MIRROR_X) { /* x mirror painting */
            int j = mesh_get_x_mirror_vert(ob, NULL, vidx, topology);
            if (j >= 0) {
              /* copy, not paint again */
              if (vgroup_mirror != -1) {
                dw = defvert_verify_index(me->dvert + j, vgroup_mirror);
                dw_prev = defvert_verify_index(wpp.wpaint_prev + j, vgroup_mirror);
              }
              else {
                dw = defvert_verify_index(me->dvert + j, vgroup_active);
                dw_prev = defvert_verify_index(wpp.wpaint_prev + j, vgroup_active);
              }
              dw_prev->weight = dw->weight; /* set the undo weight */
              dw->weight = paintweight;
            }
          }
        }
        me->dvert[vidx].flag = 1;
      }

    } while (fidx--);
  }

  {
    MDeformVert *dv = me->dvert;
    for (index = me->totvert; index != 0; index--, dv++) {
      dv->flag = 0;
    }
  }

  wpaint_prev_destroy(&wpp);

  DEG_id_tag_update(&me->id, 0);

  return true;
}

static int weight_paint_set_exec(bContext *C, wmOperator *op)
{
  struct Scene *scene = CTX_data_scene(C);
  Object *obact = CTX_data_active_object(C);
  ToolSettings *ts = CTX_data_tool_settings(C);
  Brush *brush = BKE_paint_brush(&ts->wpaint->paint);
  float vgroup_weight = BKE_brush_weight_get(scene, brush);

  if (ED_wpaint_ensure_data(C, op->reports, WPAINT_ENSURE_MIRROR, NULL) == false) {
    return OPERATOR_CANCELLED;
  }

  if (weight_paint_set(obact, vgroup_weight)) {
    ED_region_tag_redraw(CTX_wm_region(C)); /* XXX - should redraw all 3D views */
    return OPERATOR_FINISHED;
  }
  else {
    return OPERATOR_CANCELLED;
  }
}

void PAINT_OT_weight_set(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Set Weight";
  ot->idname = "PAINT_OT_weight_set";
  ot->description = "Fill the active vertex group with the current paint weight";

  /* api callbacks */
  ot->exec = weight_paint_set_exec;
  ot->poll = mask_paint_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Interactive Weight Gradient Operator
 * \{ */

/* *** VGroups Gradient *** */
typedef struct WPGradient_vertStore {
  float sco[2];
  float weight_orig;
  enum {
    VGRAD_STORE_NOP = 0,
    VGRAD_STORE_DW_EXIST = (1 << 0),
  } flag;
} WPGradient_vertStore;

typedef struct WPGradient_vertStoreBase {
  struct WPaintPrev wpp;
  WPGradient_vertStore elem[0];
} WPGradient_vertStoreBase;

typedef struct WPGradient_userData {
  struct ARegion *ar;
  Scene *scene;
  Mesh *me;
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
  short use_select;
  short type;
  float weightpaint;
} WPGradient_userData;

static void gradientVert_update(WPGradient_userData *grad_data, int index)
{
  Mesh *me = grad_data->me;
  WPGradient_vertStore *vs = &grad_data->vert_cache->elem[index];
  float alpha;

  if (grad_data->type == WPAINT_GRADIENT_TYPE_LINEAR) {
    alpha = line_point_factor_v2(vs->sco, grad_data->sco_start, grad_data->sco_end);
  }
  else {
    BLI_assert(grad_data->type == WPAINT_GRADIENT_TYPE_RADIAL);
    alpha = len_v2v2(grad_data->sco_start, vs->sco) * grad_data->sco_line_div;
  }
  /* no need to clamp 'alpha' yet */

  /* adjust weight */
  alpha = BKE_brush_curve_strength_clamped(grad_data->brush, alpha, 1.0f);

  if (alpha != 0.0f) {
    MDeformVert *dv = &me->dvert[index];
    MDeformWeight *dw = defvert_verify_index(dv, grad_data->def_nr);
    // dw->weight = alpha; // testing
    int tool = grad_data->brush->blend;
    float testw;

    /* init if we just added */
    testw = ED_wpaint_blend_tool(
        tool, vs->weight_orig, grad_data->weightpaint, alpha * grad_data->brush->alpha);
    CLAMP(testw, 0.0f, 1.0f);
    dw->weight = testw;
  }
  else {
    MDeformVert *dv = &me->dvert[index];
    if (vs->flag & VGRAD_STORE_DW_EXIST) {
      /* normally we NULL check, but in this case we know it exists */
      MDeformWeight *dw = defvert_find_index(dv, grad_data->def_nr);
      dw->weight = vs->weight_orig;
    }
    else {
      /* wasn't originally existing, remove */
      MDeformWeight *dw = defvert_find_index(dv, grad_data->def_nr);
      if (dw) {
        defvert_remove_group(dv, dw);
      }
    }
  }
}

static void gradientVertUpdate__mapFunc(void *userData,
                                        int index,
                                        const float UNUSED(co[3]),
                                        const float UNUSED(no_f[3]),
                                        const short UNUSED(no_s[3]))
{
  WPGradient_userData *grad_data = userData;
  WPGradient_vertStore *vs = &grad_data->vert_cache->elem[index];

  if (vs->sco[0] == FLT_MAX) {
    return;
  }

  gradientVert_update(grad_data, index);
}

static void gradientVertInit__mapFunc(void *userData,
                                      int index,
                                      const float co[3],
                                      const float UNUSED(no_f[3]),
                                      const short UNUSED(no_s[3]))
{
  WPGradient_userData *grad_data = userData;
  Mesh *me = grad_data->me;
  WPGradient_vertStore *vs = &grad_data->vert_cache->elem[index];

  if (grad_data->use_select && !(me->mvert[index].flag & SELECT)) {
    copy_v2_fl(vs->sco, FLT_MAX);
    return;
  }

  /* run first pass only,
   * the screen coords of the verts need to be cached because
   * updating the mesh may move them about (entering feedback loop) */
  if (BLI_BITMAP_TEST(grad_data->vert_visit, index)) {
    copy_v2_fl(vs->sco, FLT_MAX);
    return;
  }

  if (ED_view3d_project_float_object(
          grad_data->ar, co, vs->sco, V3D_PROJ_TEST_CLIP_BB | V3D_PROJ_TEST_CLIP_NEAR) !=
      V3D_PROJ_RET_OK) {
    return;
  }

  MDeformVert *dv = &me->dvert[index];
  const MDeformWeight *dw = defvert_find_index(dv, grad_data->def_nr);
  if (dw) {
    vs->weight_orig = dw->weight;
    vs->flag = VGRAD_STORE_DW_EXIST;
  }
  else {
    vs->weight_orig = 0.0f;
    vs->flag = VGRAD_STORE_NOP;
  }
  BLI_BITMAP_ENABLE(grad_data->vert_visit, index);
  gradientVert_update(grad_data, index);
}

static int paint_weight_gradient_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  wmGesture *gesture = op->customdata;
  WPGradient_vertStoreBase *vert_cache = gesture->userdata;
  int ret = WM_gesture_straightline_modal(C, op, event);

  if (ret & OPERATOR_RUNNING_MODAL) {
    if (event->type == LEFTMOUSE && event->val == KM_RELEASE) { /* XXX, hardcoded */
      /* generally crap! redo! */
      WM_gesture_straightline_cancel(C, op);
      ret &= ~OPERATOR_RUNNING_MODAL;
      ret |= OPERATOR_FINISHED;
    }
  }

  if (ret & OPERATOR_CANCELLED) {
    Object *ob = CTX_data_active_object(C);
    if (vert_cache != NULL) {
      Mesh *me = ob->data;
      if (vert_cache->wpp.wpaint_prev) {
        BKE_defvert_array_free_elems(me->dvert, me->totvert);
        BKE_defvert_array_copy(me->dvert, vert_cache->wpp.wpaint_prev, me->totvert);
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

static int paint_weight_gradient_exec(bContext *C, wmOperator *op)
{
  wmGesture *gesture = op->customdata;
  WPGradient_vertStoreBase *vert_cache;
  struct ARegion *ar = CTX_wm_region(C);
  Scene *scene = CTX_data_scene(C);
  Object *ob = CTX_data_active_object(C);
  Mesh *me = ob->data;
  int x_start = RNA_int_get(op->ptr, "xstart");
  int y_start = RNA_int_get(op->ptr, "ystart");
  int x_end = RNA_int_get(op->ptr, "xend");
  int y_end = RNA_int_get(op->ptr, "yend");
  float sco_start[2] = {x_start, y_start};
  float sco_end[2] = {x_end, y_end};
  const bool is_interactive = (gesture != NULL);

  Depsgraph *depsgraph = CTX_data_depsgraph(C);

  WPGradient_userData data = {NULL};

  if (is_interactive) {
    if (gesture->userdata == NULL) {
      gesture->userdata = MEM_mallocN(sizeof(WPGradient_vertStoreBase) +
                                          (sizeof(WPGradient_vertStore) * me->totvert),
                                      __func__);
      gesture->userdata_free = false;
      data.is_init = true;

      wpaint_prev_create(
          &((WPGradient_vertStoreBase *)gesture->userdata)->wpp, me->dvert, me->totvert);

      /* on init only, convert face -> vert sel  */
      if (me->editflag & ME_EDIT_PAINT_FACE_SEL) {
        BKE_mesh_flush_select_from_polys(me);
      }
    }

    vert_cache = gesture->userdata;
  }
  else {
    if (ED_wpaint_ensure_data(C, op->reports, 0, NULL) == false) {
      return OPERATOR_CANCELLED;
    }

    data.is_init = true;
    vert_cache = MEM_mallocN(
        sizeof(WPGradient_vertStoreBase) + (sizeof(WPGradient_vertStore) * me->totvert), __func__);
  }

  data.ar = ar;
  data.scene = scene;
  data.me = ob->data;
  data.sco_start = sco_start;
  data.sco_end = sco_end;
  data.sco_line_div = 1.0f / len_v2v2(sco_start, sco_end);
  data.def_nr = ob->actdef - 1;
  data.use_select = (me->editflag & (ME_EDIT_PAINT_FACE_SEL | ME_EDIT_PAINT_VERT_SEL));
  data.vert_cache = vert_cache;
  data.vert_visit = NULL;
  data.type = RNA_enum_get(op->ptr, "type");

  {
    ToolSettings *ts = CTX_data_tool_settings(C);
    VPaint *wp = ts->wpaint;
    struct Brush *brush = BKE_paint_brush(&wp->paint);

    curvemapping_initialize(brush->curve);

    data.brush = brush;
    data.weightpaint = BKE_brush_weight_get(scene, brush);
  }

  ED_view3d_init_mats_rv3d(ob, ar->regiondata);

  Scene *scene_eval = DEG_get_evaluated_scene(depsgraph);
  Object *ob_eval = DEG_get_evaluated_object(depsgraph, ob);

  CustomData_MeshMasks cddata_masks = scene->customdata_mask;
  cddata_masks.vmask |= CD_MASK_ORIGINDEX;
  cddata_masks.emask |= CD_MASK_ORIGINDEX;
  cddata_masks.pmask |= CD_MASK_ORIGINDEX;
  Mesh *me_eval = mesh_get_eval_final(depsgraph, scene_eval, ob_eval, &cddata_masks);
  if (data.is_init) {
    data.vert_visit = BLI_BITMAP_NEW(me->totvert, __func__);

    BKE_mesh_foreach_mapped_vert(me_eval, gradientVertInit__mapFunc, &data, MESH_FOREACH_NOP);

    MEM_freeN(data.vert_visit);
    data.vert_visit = NULL;
  }
  else {
    BKE_mesh_foreach_mapped_vert(me_eval, gradientVertUpdate__mapFunc, &data, MESH_FOREACH_NOP);
  }

  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);

  if (is_interactive == false) {
    MEM_freeN(vert_cache);
  }

  return OPERATOR_FINISHED;
}

static int paint_weight_gradient_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  int ret;

  if (ED_wpaint_ensure_data(C, op->reports, 0, NULL) == false) {
    return OPERATOR_CANCELLED;
  }

  ret = WM_gesture_straightline_invoke(C, op, event);
  if (ret & OPERATOR_RUNNING_MODAL) {
    struct ARegion *ar = CTX_wm_region(C);
    if (ar->regiontype == RGN_TYPE_WINDOW) {
      /* TODO, hardcoded, extend WM_gesture_straightline_ */
      if (event->type == LEFTMOUSE && event->val == KM_PRESS) {
        wmGesture *gesture = op->customdata;
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
      {0, NULL, 0, NULL, NULL},
  };

  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Weight Gradient";
  ot->idname = "PAINT_OT_weight_gradient";
  ot->description = "Draw a line to apply a weight gradient to selected vertices";

  /* api callbacks */
  ot->invoke = paint_weight_gradient_invoke;
  ot->modal = paint_weight_gradient_modal;
  ot->exec = paint_weight_gradient_exec;
  ot->poll = weight_paint_poll_ignore_tool;
  ot->cancel = WM_gesture_straightline_cancel;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_USE_EVAL_DATA;

  prop = RNA_def_enum(ot->srna, "type", gradient_types, 0, "Type", "");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  WM_operator_properties_gesture_straightline(ot, CURSOR_EDIT);
}

/** \} */
