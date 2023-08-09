/* SPDX-FileCopyrightText: 2007 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edmesh
 */

#include "DNA_object_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_math_vector.h"
#include "BLI_string.h"

#include "BLT_translation.h"

#include "DNA_mesh_types.h"

#include "BKE_context.h"
#include "BKE_editmesh.h"
#include "BKE_layer.h"
#include "BKE_modifier.h"
#include "BKE_report.h"
#include "BKE_unit.h"

#include "UI_interface.hh"

#include "ED_mesh.hh"
#include "ED_numinput.hh"
#include "ED_screen.hh"
#include "ED_space_api.hh"
#include "ED_view3d.hh"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "WM_api.hh"
#include "WM_types.hh"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "mesh_intern.h" /* own include */

#define SUBD_SMOOTH_MAX 4.0f
#define SUBD_CUTS_MAX 500

/* ringsel operator */

struct MeshCoordsCache {
  bool is_init, is_alloc;
  const float (*coords)[3];
};

/* struct for properties used while drawing */
struct RingSelOpData {
  ARegion *region;   /* region that ringsel was activated in */
  void *draw_handle; /* for drawing preview loop */

  EditMesh_PreSelEdgeRing *presel_edgering;

  ViewContext vc;

  Depsgraph *depsgraph;

  Base **bases;
  uint bases_len;

  MeshCoordsCache *geom_cache;

  /* These values switch objects based on the object under the cursor. */
  uint base_index;
  Object *ob;
  BMEditMesh *em;
  BMEdge *eed;

  NumInput num;

  bool extend;
  bool do_cut;

  float cuts; /* cuts as float so smooth mouse pan works in small increments */
  float smoothness;
};

/* modal loop selection drawing callback */
static void ringsel_draw(const bContext * /*C*/, ARegion * /*region*/, void *arg)
{
  RingSelOpData *lcd = static_cast<RingSelOpData *>(arg);
  EDBM_preselect_edgering_draw(lcd->presel_edgering, lcd->ob->object_to_world);
}

static void edgering_select(RingSelOpData *lcd)
{
  if (!lcd->eed) {
    return;
  }

  if (!lcd->extend) {
    for (uint base_index = 0; base_index < lcd->bases_len; base_index++) {
      Object *ob_iter = lcd->bases[base_index]->object;
      BMEditMesh *em = BKE_editmesh_from_object(ob_iter);
      EDBM_flag_disable_all(em, BM_ELEM_SELECT);
      DEG_id_tag_update(static_cast<ID *>(ob_iter->data), ID_RECALC_SELECT);
      WM_main_add_notifier(NC_GEOM | ND_SELECT, ob_iter->data);
    }
  }

  BMEditMesh *em = lcd->em;
  BMEdge *eed_start = lcd->eed;
  BMWalker walker;
  BMEdge *eed;
  BMW_init(&walker,
           em->bm,
           BMW_EDGERING,
           BMW_MASK_NOP,
           BMW_MASK_NOP,
           BMW_MASK_NOP,
           BMW_FLAG_TEST_HIDDEN,
           BMW_NIL_LAY);

  for (eed = static_cast<BMEdge *>(BMW_begin(&walker, eed_start)); eed;
       eed = static_cast<BMEdge *>(BMW_step(&walker)))
  {
    BM_edge_select_set(em->bm, eed, true);
  }
  BMW_end(&walker);
}

static void ringsel_find_edge(RingSelOpData *lcd, const int previewlines)
{
  if (lcd->eed) {
    MeshCoordsCache *gcache = &lcd->geom_cache[lcd->base_index];
    if (gcache->is_init == false) {
      Scene *scene_eval = (Scene *)DEG_get_evaluated_id(lcd->vc.depsgraph, &lcd->vc.scene->id);
      Object *ob_eval = DEG_get_evaluated_object(lcd->vc.depsgraph, lcd->ob);
      BMEditMesh *em_eval = BKE_editmesh_from_object(ob_eval);
      gcache->coords = BKE_editmesh_vert_coords_when_deformed(
          lcd->vc.depsgraph, em_eval, scene_eval, ob_eval, nullptr, &gcache->is_alloc);
      gcache->is_init = true;
    }

    EDBM_preselect_edgering_update_from_edge(
        lcd->presel_edgering, lcd->em->bm, lcd->eed, previewlines, gcache->coords);
  }
  else {
    EDBM_preselect_edgering_clear(lcd->presel_edgering);
  }
}

static void ringsel_finish(bContext *C, wmOperator *op)
{
  RingSelOpData *lcd = static_cast<RingSelOpData *>(op->customdata);
  const int cuts = RNA_int_get(op->ptr, "number_cuts");
  const float smoothness = RNA_float_get(op->ptr, "smoothness");
  const int smooth_falloff = RNA_enum_get(op->ptr, "falloff");
#ifdef BMW_EDGERING_NGON
  const bool use_only_quads = false;
#else
  const bool use_only_quads = false;
#endif

  if (lcd->eed) {
    BMEditMesh *em = lcd->em;
    BMVert *v_eed_orig[2] = {lcd->eed->v1, lcd->eed->v2};

    edgering_select(lcd);

    if (lcd->do_cut) {
      const bool is_macro = (op->opm != nullptr);
      /* a single edge (rare, but better support) */
      const bool is_edge_wire = BM_edge_is_wire(lcd->eed);
      const bool is_single = is_edge_wire || !BM_edge_is_any_face_len_test(lcd->eed, 4);
      const int seltype = is_edge_wire ? SUBDIV_SELECT_INNER :
                          is_single    ? SUBDIV_SELECT_NONE :
                                         SUBDIV_SELECT_LOOPCUT;

      /* Enable grid-fill, so that intersecting loop-cut works as one would expect.
       * Note though that it will break edge-slide in this specific case.
       * See #31939. */
      BM_mesh_esubdivide(em->bm,
                         BM_ELEM_SELECT,
                         smoothness,
                         smooth_falloff,
                         true,
                         0.0f,
                         0.0f,
                         cuts,
                         seltype,
                         SUBD_CORNER_PATH,
                         0,
                         true,
                         use_only_quads,
                         0);

      /* When used in a macro the tessellation will be recalculated anyway,
       * this is needed here because modifiers depend on updated tessellation, see #45920 */
      EDBMUpdate_Params params{};
      params.calc_looptri = true;
      params.calc_normals = false;
      params.is_destructive = true;
      EDBM_update(static_cast<Mesh *>(lcd->ob->data), &params);

      if (is_single) {
        /* de-select endpoints */
        BM_vert_select_set(em->bm, v_eed_orig[0], false);
        BM_vert_select_set(em->bm, v_eed_orig[1], false);

        EDBM_selectmode_flush_ex(lcd->em, SCE_SELECT_VERTEX);
      }
      /* We can't slide multiple edges in vertex select mode, force edge select mode. Do this for
       * all meshes in multi-object editmode so their selectmode is in sync for following
       * operators. */
      else if (is_macro && (cuts > 1) && (em->selectmode & SCE_SELECT_VERTEX)) {
        EDBM_selectmode_disable_multi(C, SCE_SELECT_VERTEX, SCE_SELECT_EDGE);
      }
      /* Force edge slide to edge select mode in face select mode. Do this for all meshes in
       * multi-object editmode so their selectmode is in sync for following operators. */
      else if (EDBM_selectmode_disable_multi(C, SCE_SELECT_FACE, SCE_SELECT_EDGE)) {
        /* pass, the change will flush selection */
      }
      else {
        /* else flush explicitly */
        EDBM_selectmode_flush(lcd->em);
      }
    }
    else {
      /* XXX Is this piece of code ever used now? Simple loop select is now
       *     in editmesh_select.cc (around line 1000)... */
      /* sets as active, useful for other tools */
      if (em->selectmode & SCE_SELECT_VERTEX) {
        /* low priority TODO: get vertex close to mouse. */
        BM_select_history_store(em->bm, lcd->eed->v1);
      }
      if (em->selectmode & SCE_SELECT_EDGE) {
        BM_select_history_store(em->bm, lcd->eed);
      }

      EDBM_selectmode_flush(lcd->em);
      DEG_id_tag_update(static_cast<ID *>(lcd->ob->data), ID_RECALC_SELECT);
      WM_event_add_notifier(C, NC_GEOM | ND_SELECT, lcd->ob->data);
    }
  }
}

/* called when modal loop selection is done... */
static void ringsel_exit(bContext * /*C*/, wmOperator *op)
{
  RingSelOpData *lcd = static_cast<RingSelOpData *>(op->customdata);

  /* deactivate the extra drawing stuff in 3D-View */
  ED_region_draw_cb_exit(lcd->region->type, lcd->draw_handle);

  EDBM_preselect_edgering_destroy(lcd->presel_edgering);

  for (uint i = 0; i < lcd->bases_len; i++) {
    MeshCoordsCache *gcache = &lcd->geom_cache[i];
    if (gcache->is_alloc) {
      MEM_freeN((void *)gcache->coords);
    }
  }
  MEM_freeN(lcd->geom_cache);

  MEM_freeN(lcd->bases);

  ED_region_tag_redraw(lcd->region);

  /* free the custom data */
  MEM_freeN(lcd);
  op->customdata = nullptr;
}

/* called when modal loop selection gets set up... */
static int ringsel_init(bContext *C, wmOperator *op, bool do_cut)
{
  RingSelOpData *lcd;
  Scene *scene = CTX_data_scene(C);

  /* alloc new customdata */
  lcd = static_cast<RingSelOpData *>(
      op->customdata = MEM_callocN(sizeof(RingSelOpData), "ringsel Modal Op Data"));

  em_setup_viewcontext(C, &lcd->vc);

  lcd->depsgraph = CTX_data_ensure_evaluated_depsgraph(C);

  /* assign the drawing handle for drawing preview line... */
  lcd->region = CTX_wm_region(C);
  lcd->draw_handle = ED_region_draw_cb_activate(
      lcd->region->type, ringsel_draw, lcd, REGION_DRAW_POST_VIEW);
  lcd->presel_edgering = EDBM_preselect_edgering_create();
  /* Initialize once the cursor is over a mesh. */
  lcd->ob = nullptr;
  lcd->em = nullptr;
  lcd->extend = do_cut ? false : RNA_boolean_get(op->ptr, "extend");
  lcd->do_cut = do_cut;
  lcd->cuts = RNA_int_get(op->ptr, "number_cuts");
  lcd->smoothness = RNA_float_get(op->ptr, "smoothness");

  initNumInput(&lcd->num);
  lcd->num.idx_max = 1;
  lcd->num.val_flag[0] |= NUM_NO_NEGATIVE | NUM_NO_FRACTION;
  /* No specific flags for smoothness. */
  lcd->num.unit_sys = scene->unit.system;
  lcd->num.unit_type[0] = B_UNIT_NONE;
  lcd->num.unit_type[1] = B_UNIT_NONE;

  ED_region_tag_redraw(lcd->region);

  return 1;
}

static void ringcut_cancel(bContext *C, wmOperator *op)
{
  /* this is just a wrapper around exit() */
  ringsel_exit(C, op);
}

static void loopcut_update_edge(RingSelOpData *lcd,
                                uint base_index,
                                BMEdge *e,
                                const int previewlines)
{
  if (e != lcd->eed) {
    lcd->eed = e;
    lcd->ob = lcd->vc.obedit;
    lcd->base_index = base_index;
    lcd->em = lcd->vc.em;
    ringsel_find_edge(lcd, previewlines);
  }
  else if (e == nullptr) {
    lcd->ob = nullptr;
    lcd->em = nullptr;
    lcd->base_index = UINT_MAX;
  }
}

static void loopcut_mouse_move(RingSelOpData *lcd, const int previewlines)
{
  struct {
    Object *ob;
    BMEdge *eed;
    float dist;
    int base_index;
  } best{};
  best.dist = ED_view3d_select_dist_px();

  uint base_index;
  BMEdge *eed_test = EDBM_edge_find_nearest_ex(&lcd->vc,
                                               &best.dist,
                                               nullptr,
                                               false,
                                               false,
                                               nullptr,
                                               lcd->bases,
                                               lcd->bases_len,
                                               &base_index);

  if (eed_test) {
    best.ob = lcd->bases[base_index]->object;
    best.eed = eed_test;
    best.base_index = base_index;
  }

  if (best.eed) {
    ED_view3d_viewcontext_init_object(&lcd->vc, best.ob);
  }

  loopcut_update_edge(lcd, best.base_index, best.eed, previewlines);
}

/* called by both init() and exec() */
static int loopcut_init(bContext *C, wmOperator *op, const wmEvent *event)
{
  const bool is_interactive = (event != nullptr);

  /* Use for redo - intentionally wrap int to uint. */
  struct {
    uint base_index;
    uint e_index;
  } exec_data{};
  exec_data.base_index = uint(RNA_int_get(op->ptr, "object_index"));
  exec_data.e_index = uint(RNA_int_get(op->ptr, "edge_index"));

  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);

  uint bases_len;
  Base **bases = BKE_view_layer_array_from_bases_in_edit_mode(
      scene, view_layer, CTX_wm_view3d(C), &bases_len);

  if (is_interactive) {
    for (uint base_index = 0; base_index < bases_len; base_index++) {
      Object *ob_iter = bases[base_index]->object;
      if (BKE_modifiers_is_deformed_by_lattice(ob_iter) ||
          BKE_modifiers_is_deformed_by_armature(ob_iter))
      {
        BKE_report(
            op->reports, RPT_WARNING, "Loop cut does not work well on deformed edit mesh display");
        break;
      }
    }
  }

  view3d_operator_needs_opengl(C);

  /* for re-execution, check edge index is in range before we setup ringsel */
  bool ok = true;
  if (is_interactive == false) {
    if (exec_data.base_index >= bases_len) {
      ok = false;
    }
    else {
      Object *ob_iter = bases[exec_data.base_index]->object;
      BMEditMesh *em = BKE_editmesh_from_object(ob_iter);
      if (exec_data.e_index >= em->bm->totedge) {
        ok = false;
      }
    }
  }

  if (!ok || !ringsel_init(C, op, true)) {
    MEM_freeN(bases);
    return OPERATOR_CANCELLED;
  }

  /* add a modal handler for this operator - handles loop selection */
  if (is_interactive) {
    op->flag |= OP_IS_MODAL_CURSOR_REGION;
    WM_event_add_modal_handler(C, op);
  }

  RingSelOpData *lcd = static_cast<RingSelOpData *>(op->customdata);

  lcd->bases = bases;
  lcd->bases_len = bases_len;
  lcd->geom_cache = static_cast<MeshCoordsCache *>(
      MEM_callocN(sizeof(*lcd->geom_cache) * bases_len, __func__));

  if (is_interactive) {
    copy_v2_v2_int(lcd->vc.mval, event->mval);
    loopcut_mouse_move(lcd, is_interactive ? 1 : 0);
  }
  else {

    Object *ob_iter = bases[exec_data.base_index]->object;
    ED_view3d_viewcontext_init_object(&lcd->vc, ob_iter);

    BMEdge *e;
    BM_mesh_elem_table_ensure(lcd->vc.em->bm, BM_EDGE);
    e = BM_edge_at_index(lcd->vc.em->bm, exec_data.e_index);
    loopcut_update_edge(lcd, exec_data.base_index, e, 0);
  }

#ifdef USE_LOOPSLIDE_HACK
  /* for use in macro so we can restore, HACK */
  {
    ToolSettings *settings = scene->toolsettings;
    const bool mesh_select_mode[3] = {
        (settings->selectmode & SCE_SELECT_VERTEX) != 0,
        (settings->selectmode & SCE_SELECT_EDGE) != 0,
        (settings->selectmode & SCE_SELECT_FACE) != 0,
    };

    RNA_boolean_set_array(op->ptr, "mesh_select_mode_init", mesh_select_mode);
  }
#endif

  if (is_interactive) {
    ED_workspace_status_text(
        C,
        TIP_("Select a ring to be cut, use mouse-wheel or page-up/down for number of cuts, "
             "hold Alt for smooth"));
    return OPERATOR_RUNNING_MODAL;
  }

  ringsel_finish(C, op);
  ringsel_exit(C, op);
  return OPERATOR_FINISHED;
}

static int ringcut_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  /* When accessed as a tool, get the active edge from the pre-selection gizmo. */
  {
    ARegion *region = CTX_wm_region(C);
    wmGizmoMap *gzmap = region->gizmo_map;
    wmGizmoGroup *gzgroup = gzmap ? WM_gizmomap_group_find(gzmap,
                                                           "VIEW3D_GGT_mesh_preselect_edgering") :
                                    nullptr;
    if ((gzgroup != nullptr) && gzgroup->gizmos.first) {
      wmGizmo *gz = static_cast<wmGizmo *>(gzgroup->gizmos.first);
      const int object_index = RNA_int_get(gz->ptr, "object_index");
      const int edge_index = RNA_int_get(gz->ptr, "edge_index");

      if (object_index != -1 && edge_index != -1) {
        RNA_int_set(op->ptr, "object_index", object_index);
        RNA_int_set(op->ptr, "edge_index", edge_index);
        return loopcut_init(C, op, nullptr);
      }
      return OPERATOR_CANCELLED;
    }
  }

  return loopcut_init(C, op, event);
}

static int loopcut_exec(bContext *C, wmOperator *op)
{
  return loopcut_init(C, op, nullptr);
}

static int loopcut_finish(RingSelOpData *lcd, bContext *C, wmOperator *op)
{
  /* finish */
  ED_region_tag_redraw(lcd->region);
  ED_workspace_status_text(C, nullptr);

  if (lcd->eed) {
    /* set for redo */
    BM_mesh_elem_index_ensure(lcd->em->bm, BM_EDGE);
    RNA_int_set(op->ptr, "object_index", lcd->base_index);
    RNA_int_set(op->ptr, "edge_index", BM_elem_index_get(lcd->eed));

    /* execute */
    ringsel_finish(C, op);
    ringsel_exit(C, op);
  }
  else {
    ringcut_cancel(C, op);
    return OPERATOR_CANCELLED;
  }

  return OPERATOR_FINISHED;
}

static int loopcut_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  if (event->type == NDOF_MOTION) {
    return OPERATOR_PASS_THROUGH;
  }

  RingSelOpData *lcd = static_cast<RingSelOpData *>(op->customdata);
  float cuts = lcd->cuts;
  float smoothness = lcd->smoothness;
  bool show_cuts = false;
  const bool has_numinput = hasNumInput(&lcd->num);

  em_setup_viewcontext(C, &lcd->vc);
  lcd->region = lcd->vc.region;

  view3d_operator_needs_opengl(C);

  /* using the keyboard to input the number of cuts */
  /* Modal numinput active, try to handle numeric inputs first... */
  if (event->val == KM_PRESS && has_numinput && handleNumInput(C, &lcd->num, event)) {
    float values[2] = {cuts, smoothness};
    applyNumInput(&lcd->num, values);
    cuts = values[0];
    smoothness = values[1];
  }
  else {
    bool handled = false;
    switch (event->type) {
      case EVT_RETKEY:
      case EVT_PADENTER:
      case LEFTMOUSE: /* confirm */ /* XXX hardcoded */
        if (event->val == KM_PRESS) {
          return loopcut_finish(lcd, C, op);
        }

        ED_region_tag_redraw(lcd->region);
        handled = true;
        break;
      case RIGHTMOUSE: /* abort */ /* XXX hardcoded */
        ED_region_tag_redraw(lcd->region);
        ringsel_exit(C, op);
        ED_workspace_status_text(C, nullptr);

        return OPERATOR_CANCELLED;
      case EVT_ESCKEY:
        if (event->val == KM_RELEASE) {
          /* cancel */
          ED_region_tag_redraw(lcd->region);
          ED_workspace_status_text(C, nullptr);

          ringcut_cancel(C, op);
          return OPERATOR_CANCELLED;
        }

        ED_region_tag_redraw(lcd->region);
        handled = true;
        break;
      case MOUSEPAN:
        if ((event->modifier & KM_ALT) == 0) {
          cuts += 0.02f * (event->xy[1] - event->prev_xy[1]);
          if (cuts < 1 && lcd->cuts >= 1) {
            cuts = 1;
          }
        }
        else {
          smoothness += 0.002f * (event->xy[1] - event->prev_xy[1]);
        }
        handled = true;
        break;
      case EVT_PADPLUSKEY:
      case EVT_PAGEUPKEY:
      case WHEELUPMOUSE: /* change number of cuts */
        if (event->val == KM_RELEASE) {
          break;
        }
        if ((event->modifier & KM_ALT) == 0) {
          cuts += 1;
        }
        else {
          smoothness += 0.05f;
        }
        handled = true;
        break;
      case EVT_PADMINUS:
      case EVT_PAGEDOWNKEY:
      case WHEELDOWNMOUSE: /* change number of cuts */
        if (event->val == KM_RELEASE) {
          break;
        }
        if ((event->modifier & KM_ALT) == 0) {
          cuts = max_ff(cuts - 1, 1);
        }
        else {
          smoothness -= 0.05f;
        }
        handled = true;
        break;
      case MOUSEMOVE: {
/* mouse moved somewhere to select another loop */

/* This is normally disabled for all modal operators.
 * This is an exception since mouse movement doesn't relate to numeric input.
 *
 * If numeric input changes we'll need to add this back see: D2973 */
#if 0
        if (!has_numinput)
#endif
        {
          lcd->vc.mval[0] = event->mval[0];
          lcd->vc.mval[1] = event->mval[1];
          loopcut_mouse_move(lcd, int(lcd->cuts));

          ED_region_tag_redraw(lcd->region);
          handled = true;
        }
        break;
      }
    }

    /* Modal numinput inactive, try to handle numeric inputs last... */
    if (!handled && event->val == KM_PRESS && handleNumInput(C, &lcd->num, event)) {
      float values[2] = {cuts, smoothness};
      applyNumInput(&lcd->num, values);
      cuts = values[0];
      smoothness = values[1];
    }
  }

  if (cuts != lcd->cuts) {
    /* allow zero so you can backspace and type in a value
     * otherwise 1 as minimum would make more sense */
    lcd->cuts = clamp_f(cuts, 0, SUBD_CUTS_MAX);
    RNA_int_set(op->ptr, "number_cuts", int(lcd->cuts));
    ringsel_find_edge(lcd, int(lcd->cuts));
    show_cuts = true;
    ED_region_tag_redraw(lcd->region);
  }

  if (smoothness != lcd->smoothness) {
    lcd->smoothness = clamp_f(smoothness, -SUBD_SMOOTH_MAX, SUBD_SMOOTH_MAX);
    RNA_float_set(op->ptr, "smoothness", lcd->smoothness);
    show_cuts = true;
    ED_region_tag_redraw(lcd->region);
  }

  if (show_cuts) {
    Scene *sce = CTX_data_scene(C);
    char buf[UI_MAX_DRAW_STR];
    char str_rep[NUM_STR_REP_LEN * 2];
    if (hasNumInput(&lcd->num)) {
      outputNumInput(&lcd->num, str_rep, &sce->unit);
    }
    else {
      BLI_snprintf(str_rep, NUM_STR_REP_LEN, "%d", int(lcd->cuts));
      BLI_snprintf(str_rep + NUM_STR_REP_LEN, NUM_STR_REP_LEN, "%.2f", smoothness);
    }
    SNPRINTF(
        buf, TIP_("Number of Cuts: %s, Smooth: %s (Alt)"), str_rep, str_rep + NUM_STR_REP_LEN);
    ED_workspace_status_text(C, buf);
  }

  /* keep going until the user confirms */
  return OPERATOR_RUNNING_MODAL;
}

/* for bmesh this tool is in bmesh_select.c */
#if 0

void MESH_OT_edgering_select(wmOperatorType *ot)
{
  /* description */
  ot->name = "Edge Ring Select";
  ot->idname = "MESH_OT_edgering_select";
  ot->description = "Select an edge ring";

  /* callbacks */
  ot->invoke = ringsel_invoke;
  ot->poll = ED_operator_editmesh_region_view3d;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_boolean(ot->srna, "extend", 0, "Extend", "Extend the selection");
}

#endif

void MESH_OT_loopcut(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* description */
  ot->name = "Loop Cut";
  ot->idname = "MESH_OT_loopcut";
  ot->description = "Add a new loop between existing loops";

  /* callbacks */
  ot->invoke = ringcut_invoke;
  ot->exec = loopcut_exec;
  ot->modal = loopcut_modal;
  ot->cancel = ringcut_cancel;
  ot->poll = ED_operator_editmesh_region_view3d;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_BLOCKING;

  /* properties */
  prop = RNA_def_int(ot->srna, "number_cuts", 1, 1, 1000000, "Number of Cuts", "", 1, 100);
  /* avoid re-using last var because it can cause
   * _very_ high poly meshes and annoy users (or worse crash) */
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  prop = RNA_def_float(ot->srna,
                       "smoothness",
                       0.0f,
                       -1e3f,
                       1e3f,
                       "Smoothness",
                       "Smoothness factor",
                       -SUBD_SMOOTH_MAX,
                       SUBD_SMOOTH_MAX);
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  WM_operatortype_props_advanced_begin(ot);

  prop = RNA_def_property(ot->srna, "falloff", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, rna_enum_proportional_falloff_curve_only_items);
  RNA_def_property_enum_default(prop, PROP_INVSQUARE);
  RNA_def_property_ui_text(prop, "Falloff", "Falloff type of the feather");
  RNA_def_property_translation_context(prop,
                                       BLT_I18NCONTEXT_ID_CURVE_LEGACY); /* Abusing id_curve :/ */

  /* For redo only. */
  prop = RNA_def_int(ot->srna, "object_index", -1, -1, INT_MAX, "Object Index", "", 0, INT_MAX);
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_MESH);
  RNA_def_property_flag(prop, PROP_HIDDEN);
  prop = RNA_def_int(ot->srna, "edge_index", -1, -1, INT_MAX, "Edge Index", "", 0, INT_MAX);
  RNA_def_property_flag(prop, PROP_HIDDEN);

#ifdef USE_LOOPSLIDE_HACK
  prop = RNA_def_boolean_array(ot->srna, "mesh_select_mode_init", 3, nullptr, "", "");
  RNA_def_property_flag(prop, PROP_HIDDEN);
#endif
}
