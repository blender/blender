/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spview3d
 */

#include "MEM_guardedalloc.h"

#include "BLI_linklist.h"
#include "BLI_listbase.h"
#include "BLI_math_matrix.h"
#include "BLI_math_rotation.h"
#include "BLI_math_vector.h"
#include "BLI_rect.h"

#include "BKE_action.hh"
#include "BKE_context.hh"
#ifdef WITH_XR_OPENXR
#  include "BKE_idprop.hh"
#endif
#include "BKE_global.hh"
#include "BKE_layer.hh"
#include "BKE_lib_id.hh"
#include "BKE_main.hh"
#include "BKE_modifier.hh"
#include "BKE_object.hh"
#include "BKE_report.hh"
#include "BKE_scene.hh"

#include "DEG_depsgraph_query.hh"

#include "UI_resources.hh"

#include "GPU_matrix.hh"
#include "GPU_select.hh"
#include "GPU_state.hh"

#include "WM_api.hh"

#include "ED_info.hh"
#include "ED_object.hh"
#include "ED_screen.hh"

#include "DRW_engine.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "view3d_intern.hh" /* own include */
#include "view3d_navigate.hh"

#include "DNA_camera_types.h"

/* -------------------------------------------------------------------- */
/** \name Camera to View Operator
 * \{ */

static wmOperatorStatus view3d_camera_to_view_exec(bContext *C, wmOperator * /*op*/)
{
  const Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  View3D *v3d;
  ARegion *region;
  RegionView3D *rv3d;

  ObjectTfmProtectedChannels obtfm;

  ED_view3d_context_user_region(C, &v3d, &region);
  rv3d = static_cast<RegionView3D *>(region->regiondata);

  ED_view3d_smooth_view_force_finish(C, v3d, region);

  ED_view3d_lastview_store(rv3d);

  BKE_object_tfm_protected_backup(v3d->camera, &obtfm);

  ED_view3d_to_object(depsgraph, v3d->camera, rv3d->ofs, rv3d->viewquat, rv3d->dist);

  BKE_object_tfm_protected_restore(v3d->camera, &obtfm, v3d->camera->protectflag);

  DEG_id_tag_update(&v3d->camera->id, ID_RECALC_TRANSFORM);
  rv3d->persp = RV3D_CAMOB;

  WM_event_add_notifier(C, NC_OBJECT | ND_TRANSFORM, v3d->camera);

  return OPERATOR_FINISHED;
}

static bool view3d_camera_to_view_poll(bContext *C)
{
  View3D *v3d;
  ARegion *region;

  if (ED_view3d_context_user_region(C, &v3d, &region)) {
    RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);
    if (v3d && v3d->camera && BKE_id_is_editable(CTX_data_main(C), &v3d->camera->id)) {
      if (rv3d && (RV3D_LOCK_FLAGS(rv3d) & RV3D_LOCK_ANY_TRANSFORM) == 0) {
        if (rv3d->persp != RV3D_CAMOB) {
          return true;
        }
      }
    }
  }

  return false;
}

void VIEW3D_OT_camera_to_view(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Align Camera to View";
  ot->description = "Set camera view to active view";
  ot->idname = "VIEW3D_OT_camera_to_view";

  /* API callbacks. */
  ot->exec = view3d_camera_to_view_exec;
  ot->poll = view3d_camera_to_view_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Camera Fit Frame to Selected Operator
 * \{ */

/**
 * Unlike #VIEW3D_OT_view_selected this is for framing a render and not
 * meant to take into account vertex/bone selection for eg.
 */
static wmOperatorStatus view3d_camera_to_view_selected_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Scene *scene = CTX_data_scene(C);
  View3D *v3d = CTX_wm_view3d(C); /* can be nullptr */
  Object *camera_ob = v3d ? v3d->camera : scene->camera;

  if (camera_ob == nullptr) {
    BKE_report(op->reports, RPT_ERROR, "No active camera");
    return OPERATOR_CANCELLED;
  }

  if (ED_view3d_camera_to_view_selected(bmain, depsgraph, scene, camera_ob)) {
    WM_event_add_notifier(C, NC_OBJECT | ND_TRANSFORM, camera_ob);
    return OPERATOR_FINISHED;
  }
  return OPERATOR_CANCELLED;
}

void VIEW3D_OT_camera_to_view_selected(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Camera Fit Frame to Selected";
  ot->description = "Move the camera so selected objects are framed";
  ot->idname = "VIEW3D_OT_camera_to_view_selected";

  /* API callbacks. */
  ot->exec = view3d_camera_to_view_selected_exec;
  ot->poll = ED_operator_scene_editable;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Object as Camera Operator
 * \{ */

static void sync_viewport_camera_smoothview(bContext *C,
                                            View3D *v3d,
                                            Object *ob,
                                            const int smooth_viewtx)
{
  Main *bmain = CTX_data_main(C);
  LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
    LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
      LISTBASE_FOREACH (SpaceLink *, space_link, &area->spacedata) {
        if (space_link->spacetype == SPACE_VIEW3D) {
          View3D *other_v3d = reinterpret_cast<View3D *>(space_link);
          if (other_v3d == v3d) {
            continue;
          }
          if (other_v3d->camera == ob) {
            continue;
          }
          /* Checking the other view is needed to prevent local cameras being modified. */
          if (v3d->scenelock && other_v3d->scenelock) {
            ListBase *lb = (space_link == area->spacedata.first) ? &area->regionbase :
                                                                   &space_link->regionbase;
            LISTBASE_FOREACH (ARegion *, other_region, lb) {
              if (other_region->regiontype == RGN_TYPE_WINDOW) {
                if (other_region->regiondata) {
                  RegionView3D *other_rv3d = static_cast<RegionView3D *>(other_region->regiondata);
                  if (other_rv3d->persp == RV3D_CAMOB) {
                    Object *other_camera_old = other_v3d->camera;
                    other_v3d->camera = ob;

                    V3D_SmoothParams sview_params = {};
                    sview_params.camera_old = other_camera_old;
                    sview_params.camera = other_v3d->camera;
                    sview_params.ofs = other_rv3d->ofs;
                    sview_params.quat = other_rv3d->viewquat;
                    sview_params.dist = &other_rv3d->dist;
                    sview_params.lens = &other_v3d->lens;
                    /* No undo because this switches cameras. */
                    sview_params.undo_str = nullptr;

                    ED_view3d_lastview_store(other_rv3d);
                    ED_view3d_smooth_view(
                        C, other_v3d, other_region, smooth_viewtx, &sview_params);
                  }
                  else {
                    other_v3d->camera = ob;
                  }
                }
              }
            }
          }
        }
      }
    }
  }
}

static wmOperatorStatus view3d_setobjectascamera_exec(bContext *C, wmOperator *op)
{
  View3D *v3d;
  ARegion *region;
  RegionView3D *rv3d;

  Scene *scene = CTX_data_scene(C);
  Object *ob = CTX_data_active_object(C);

  const int smooth_viewtx = WM_operator_smooth_viewtx_get(op);

  /* no nullptr check is needed, poll checks */
  ED_view3d_context_user_region(C, &v3d, &region);
  rv3d = static_cast<RegionView3D *>(region->regiondata);

  ED_view3d_smooth_view_force_finish(C, v3d, region);

  if (ob) {
    Object *camera_old = (rv3d->persp == RV3D_CAMOB) ? V3D_CAMERA_SCENE(scene, v3d) : nullptr;
    rv3d->persp = RV3D_CAMOB;
    v3d->camera = ob;
    if (v3d->scenelock && scene->camera != ob) {
      scene->camera = ob;
      DEG_id_tag_update(&scene->id, ID_RECALC_SYNC_TO_EVAL);
      DEG_relations_tag_update(CTX_data_main(C));
    }

    /* unlikely but looks like a glitch when set to the same */
    if (camera_old != ob) {
      V3D_SmoothParams sview_params = {};
      sview_params.camera_old = camera_old;
      sview_params.camera = v3d->camera;
      sview_params.ofs = rv3d->ofs;
      sview_params.quat = rv3d->viewquat;
      sview_params.dist = &rv3d->dist;
      sview_params.lens = &v3d->lens;
      /* No undo because this switches cameras. */
      sview_params.undo_str = nullptr;

      ED_view3d_lastview_store(rv3d);
      ED_view3d_smooth_view(C, v3d, region, smooth_viewtx, &sview_params);
    }

    if (v3d->scenelock) {
      sync_viewport_camera_smoothview(C, v3d, ob, smooth_viewtx);
      WM_event_add_notifier(C, NC_SCENE, scene);
    }
    WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, scene);
  }

  return OPERATOR_FINISHED;
}

bool ED_operator_rv3d_user_region_poll(bContext *C)
{
  View3D *v3d_dummy;
  ARegion *region_dummy;

  return ED_view3d_context_user_region(C, &v3d_dummy, &region_dummy);
}

void VIEW3D_OT_object_as_camera(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Set Active Object as Camera";
  ot->description = "Set the active object as the active camera for this view or scene";
  ot->idname = "VIEW3D_OT_object_as_camera";

  /* API callbacks. */
  ot->exec = view3d_setobjectascamera_exec;
  ot->poll = ED_operator_rv3d_user_region_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Window and View Matrix Calculation
 * \{ */

void view3d_winmatrix_set(const Depsgraph *depsgraph,
                          ARegion *region,
                          const View3D *v3d,
                          const rcti *rect)
{
  RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);
  rctf full_viewplane;
  float clipsta, clipend;
  bool is_ortho;

  is_ortho = ED_view3d_viewplane_get(depsgraph,
                                     v3d,
                                     rv3d,
                                     region->winx,
                                     region->winy,
                                     &full_viewplane,
                                     &clipsta,
                                     &clipend,
                                     nullptr);
  rv3d->is_persp = !is_ortho;

#if 0
  printf("%s: %d %d %f %f %f %f %f %f\n",
         __func__,
         winx,
         winy,
         full_viewplane.xmin,
         full_viewplane.ymin,
         full_viewplane.xmax,
         full_viewplane.ymax,
         clipsta,
         clipend);
#endif

  /* Note the code here was tweaked to avoid an apparent compiler bug in clang 13 (see #91680). */
  rctf viewplane;
  if (rect) {
    /* Smaller viewplane subset for selection picking. */
    viewplane.xmin = full_viewplane.xmin +
                     (BLI_rctf_size_x(&full_viewplane) * (rect->xmin / float(region->winx)));
    viewplane.ymin = full_viewplane.ymin +
                     (BLI_rctf_size_y(&full_viewplane) * (rect->ymin / float(region->winy)));
    viewplane.xmax = full_viewplane.xmin +
                     (BLI_rctf_size_x(&full_viewplane) * (rect->xmax / float(region->winx)));
    viewplane.ymax = full_viewplane.ymin +
                     (BLI_rctf_size_y(&full_viewplane) * (rect->ymax / float(region->winy)));
  }
  else {
    viewplane = full_viewplane;
  }

  if (is_ortho) {
    GPU_matrix_ortho_set(
        viewplane.xmin, viewplane.xmax, viewplane.ymin, viewplane.ymax, clipsta, clipend);
  }
  else {
    GPU_matrix_frustum_set(
        viewplane.xmin, viewplane.xmax, viewplane.ymin, viewplane.ymax, clipsta, clipend);
  }

  /* update matrix in 3d view region */
  GPU_matrix_projection_get(rv3d->winmat);
}

static void obmat_to_viewmat(RegionView3D *rv3d, Object *ob)
{
  float bmat[4][4];

  rv3d->view = RV3D_VIEW_USER; /* don't show the grid */

  normalize_m4_m4(bmat, ob->object_to_world().ptr());
  invert_m4_m4(rv3d->viewmat, bmat);

  /* view quat calculation, needed for add object */
  mat4_normalized_to_quat(rv3d->viewquat, rv3d->viewmat);
}

void view3d_viewmatrix_set(const Depsgraph *depsgraph,
                           const Scene *scene,
                           const View3D *v3d,
                           RegionView3D *rv3d,
                           const float rect_scale[2])
{
  if (rv3d->persp == RV3D_CAMOB) { /* obs/camera */
    if (v3d->camera) {
      Object *ob_camera_eval = DEG_get_evaluated(depsgraph, v3d->camera);
      obmat_to_viewmat(rv3d, ob_camera_eval);
    }
    else {
      quat_to_mat4(rv3d->viewmat, rv3d->viewquat);
      rv3d->viewmat[3][2] -= rv3d->dist;
    }
  }
  else {
    bool use_lock_ofs = false;

    /* should be moved to better initialize later on XXX */
    if (RV3D_LOCK_FLAGS(rv3d) & RV3D_LOCK_ROTATION) {
      ED_view3d_lock(rv3d);
    }

    quat_to_mat4(rv3d->viewmat, rv3d->viewquat);
    if (rv3d->persp == RV3D_PERSP) {
      rv3d->viewmat[3][2] -= rv3d->dist;
    }
    if (v3d->ob_center) {
      Object *ob_eval = DEG_get_evaluated(depsgraph, v3d->ob_center);
      float vec[3];

      copy_v3_v3(vec, ob_eval->object_to_world().location());
      if (ob_eval->type == OB_ARMATURE && v3d->ob_center_bone[0]) {
        bPoseChannel *pchan = BKE_pose_channel_find_name(ob_eval->pose, v3d->ob_center_bone);
        if (pchan) {
          copy_v3_v3(vec, pchan->pose_mat[3]);
          mul_m4_v3(ob_eval->object_to_world().ptr(), vec);
        }
      }
      translate_m4(rv3d->viewmat, -vec[0], -vec[1], -vec[2]);
      use_lock_ofs = true;
    }
    else if (v3d->ob_center_cursor) {
      float vec[3];
      copy_v3_v3(vec, scene->cursor.location);
      translate_m4(rv3d->viewmat, -vec[0], -vec[1], -vec[2]);
      use_lock_ofs = true;
    }
    else {
      translate_m4(rv3d->viewmat, rv3d->ofs[0], rv3d->ofs[1], rv3d->ofs[2]);
    }

    /* lock offset */
    if (use_lock_ofs) {
      float persmat[4][4], persinv[4][4];
      float vec[3];

      /* we could calculate the real persmat/persinv here
       * but it would be unreliable so better to later */
      mul_m4_m4m4(persmat, rv3d->winmat, rv3d->viewmat);
      invert_m4_m4(persinv, persmat);

      mul_v2_v2fl(vec, rv3d->ofs_lock, rv3d->is_persp ? rv3d->dist : 1.0f);
      vec[2] = 0.0f;

      if (rect_scale) {
        /* Since `RegionView3D.winmat` has been calculated and this function doesn't take the
         * #ARegion we don't know about the region size.
         * Use `rect_scale` when drawing a sub-region to apply 2D offset,
         * scaled by the difference between the sub-region and the region size.
         */
        vec[0] /= rect_scale[0];
        vec[1] /= rect_scale[1];
      }

      mul_mat3_m4_v3(persinv, vec);
      translate_m4(rv3d->viewmat, vec[0], vec[1], vec[2]);
    }
    /* end lock offset */
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name GPU Select Utilities
 * \{ */

void view3d_gpu_select_cache_begin()
{
  GPU_select_cache_begin();
}

void view3d_gpu_select_cache_end()
{
  GPU_select_cache_end();
}

struct DrawSelectLoopUserData {
  uint pass;
  uint hits;
  GPUSelectBuffer *buffer;
  const rcti *rect;
  GPUSelectMode gpu_select_mode;
};

static bool drw_select_loop_pass(eDRWSelectStage stage, void *user_data)
{
  bool continue_pass = false;
  DrawSelectLoopUserData *data = static_cast<DrawSelectLoopUserData *>(user_data);
  if (stage == DRW_SELECT_PASS_PRE) {
    GPU_select_begin_next(data->buffer, data->rect, data->gpu_select_mode, data->hits);
    /* always run POST after PRE. */
    continue_pass = true;
  }
  else if (stage == DRW_SELECT_PASS_POST) {
    int hits = GPU_select_end();
    if (data->pass == 0) {
      /* quirk of GPU_select_end, only take hits value from first call. */
      data->hits = hits;
    }
    data->pass += 1;
  }
  else {
    BLI_assert(0);
  }
  return continue_pass;
}

eV3DSelectObjectFilter ED_view3d_select_filter_from_mode(const Scene *scene, const Object *obact)
{
  if (scene->toolsettings->object_flag & SCE_OBJECT_MODE_LOCK) {
    if (obact && (obact->mode & OB_MODE_ALL_WEIGHT_PAINT) &&
        BKE_object_pose_armature_get((Object *)obact))
    {
      return VIEW3D_SELECT_FILTER_WPAINT_POSE_MODE_LOCK;
    }
    return VIEW3D_SELECT_FILTER_OBJECT_MODE_LOCK;
  }
  return VIEW3D_SELECT_FILTER_NOP;
}

/** Implement #VIEW3D_SELECT_FILTER_OBJECT_MODE_LOCK. */
static bool drw_select_filter_object_mode_lock(Object *ob, void *user_data)
{
  const Object *obact = static_cast<const Object *>(user_data);
  return BKE_object_is_mode_compat(ob, eObjectMode(obact->mode));
}

/**
 * Implement #VIEW3D_SELECT_FILTER_WPAINT_POSE_MODE_LOCK for special case when
 * we want to select pose bones (this doesn't switch modes).
 */
static bool drw_select_filter_object_mode_lock_for_weight_paint(Object *ob, void *user_data)
{
  LinkNode *ob_pose_list = static_cast<LinkNode *>(user_data);
  return ob_pose_list && (BLI_linklist_index(ob_pose_list, DEG_get_original(ob)) != -1);
}

int view3d_gpu_select_ex(const ViewContext *vc,
                         GPUSelectBuffer *buffer,
                         const rcti *input,
                         eV3DSelectMode select_mode,
                         eV3DSelectObjectFilter select_filter,
                         const bool do_material_slot_selection)
{
  bThemeState theme_state;
  const wmWindowManager *wm = CTX_wm_manager(vc->C);
  Depsgraph *depsgraph = vc->depsgraph;
  Scene *scene = vc->scene;
  View3D *v3d = vc->v3d;
  ARegion *region = vc->region;
  rcti rect;
  int hits = 0;
  BKE_view_layer_synced_ensure(scene, vc->view_layer);
  const bool use_obedit_skip = (BKE_view_layer_edit_object_get(vc->view_layer) != nullptr) &&
                               (vc->obedit == nullptr);
  const bool use_nearest = select_mode == VIEW3D_SELECT_PICK_NEAREST;
  bool draw_surface = true;

  GPUSelectMode gpu_select_mode = GPU_SELECT_INVALID;

  /* case not a box select */
  if (input->xmin == input->xmax) {
    const int xy[2] = {input->xmin, input->ymin};
    /* seems to be default value for bones only now */
    BLI_rcti_init_pt_radius(&rect, xy, 12);
  }
  else {
    rect = *input;
  }

  if (select_mode == VIEW3D_SELECT_PICK_NEAREST) {
    gpu_select_mode = GPU_SELECT_PICK_NEAREST;
  }
  else if (select_mode == VIEW3D_SELECT_PICK_ALL) {
    gpu_select_mode = GPU_SELECT_PICK_ALL;
  }
  else {
    gpu_select_mode = GPU_SELECT_ALL;
  }

  /* Important to use `vc->obact`, not `BKE_view_layer_active_object_get(vc->view_layer)` below,
   * so it will be nullptr when hidden. */
  struct {
    DRW_ObjectFilterFn fn;
    void *user_data;
  } object_filter = {nullptr, nullptr};

  /* Re-use cache (rect must be smaller than the cached)
   * other context is assumed to be unchanged */
  if (GPU_select_is_cached()) {
    GPU_select_begin_next(buffer, &rect, gpu_select_mode, 0);
    GPU_select_cache_load_id();
    hits = GPU_select_end();
    return hits;
  }

  switch (select_filter) {
    case VIEW3D_SELECT_FILTER_OBJECT_MODE_LOCK: {
      Object *obact = vc->obact;
      if (obact && obact->mode != OB_MODE_OBJECT) {
        object_filter.fn = drw_select_filter_object_mode_lock;
        object_filter.user_data = obact;
      }
      break;
    }
    case VIEW3D_SELECT_FILTER_WPAINT_POSE_MODE_LOCK: {
      Object *obact = vc->obact;
      BLI_assert(obact && (obact->mode & OB_MODE_ALL_WEIGHT_PAINT));
      /* While this uses `alloca` in a loop (which we typically avoid),
       * the number of items is nearly always 1, maybe 2..3 in rare cases. */
      LinkNode *ob_pose_list = nullptr;
      VirtualModifierData virtual_modifier_data;
      ModifierData *md = BKE_modifiers_get_virtual_modifierlist(obact, &virtual_modifier_data);
      for (; md; md = md->next) {
        if (md->type == eModifierType_Armature) {
          ArmatureModifierData *amd = reinterpret_cast<ArmatureModifierData *>(md);
          if (amd->object && (amd->object->mode & OB_MODE_POSE)) {
            BLI_linklist_prepend_alloca(&ob_pose_list, amd->object);
          }
        }
        else if (md->type == eModifierType_GreasePencilArmature) {
          GreasePencilArmatureModifierData *amd =
              reinterpret_cast<GreasePencilArmatureModifierData *>(md);
          if (amd->object && (amd->object->mode & OB_MODE_POSE)) {
            BLI_linklist_prepend_alloca(&ob_pose_list, amd->object);
          }
        }
      }
      object_filter.fn = drw_select_filter_object_mode_lock_for_weight_paint;
      object_filter.user_data = ob_pose_list;
      break;
    }
    case VIEW3D_SELECT_FILTER_NOP:
      break;
  }

  /* Tools may request depth outside of regular drawing code. */
  UI_Theme_Store(&theme_state);
  UI_SetTheme(SPACE_VIEW3D, RGN_TYPE_WINDOW);

  /* All of the queries need to be perform on the drawing context. */
  DRW_gpu_context_enable();

  G.f |= G_FLAG_PICKSEL;

  /* Important we use the 'viewmat' and don't re-calculate since
   * the object & bone view locking takes 'rect' into account, see: #51629. */
  ED_view3d_draw_setup_view(
      wm, vc->win, depsgraph, scene, region, v3d, vc->rv3d->viewmat, nullptr, &rect);

  if (!XRAY_ACTIVE(v3d)) {
    GPU_depth_test(GPU_DEPTH_LESS_EQUAL);
  }

  /* If in X-ray mode, we select the wires in priority. */
  if (XRAY_ACTIVE(v3d) && use_nearest) {
    /* We need to call "GPU_select_*" API's inside DRW_draw_select_loop
     * because the GPU context created & destroyed inside this function. */
    DrawSelectLoopUserData drw_select_loop_user_data = {};
    drw_select_loop_user_data.pass = 0;
    drw_select_loop_user_data.hits = 0;
    drw_select_loop_user_data.buffer = buffer;
    drw_select_loop_user_data.rect = &rect;
    drw_select_loop_user_data.gpu_select_mode = gpu_select_mode;

    draw_surface = false;
    DRW_draw_select_loop(depsgraph,
                         region,
                         v3d,
                         use_obedit_skip,
                         draw_surface,
                         use_nearest,
                         do_material_slot_selection,
                         &rect,
                         drw_select_loop_pass,
                         &drw_select_loop_user_data,
                         object_filter.fn,
                         object_filter.user_data);
    hits = drw_select_loop_user_data.hits;
    /* FIX: This cleanup the state before doing another selection pass.
     * (see #56695) */
    GPU_select_cache_end();
  }

  if (hits == 0) {
    /* We need to call "GPU_select_*" API's inside DRW_draw_select_loop
     * because the GPU context created & destroyed inside this function. */
    DrawSelectLoopUserData drw_select_loop_user_data = {};
    drw_select_loop_user_data.pass = 0;
    drw_select_loop_user_data.hits = 0;
    drw_select_loop_user_data.buffer = buffer;
    drw_select_loop_user_data.rect = &rect;
    drw_select_loop_user_data.gpu_select_mode = gpu_select_mode;

    /* If are not in wireframe mode, we need to use the mesh surfaces to check for hits */
    draw_surface = (v3d->shading.type > OB_WIRE) || !XRAY_ENABLED(v3d);
    DRW_draw_select_loop(depsgraph,
                         region,
                         v3d,
                         use_obedit_skip,
                         draw_surface,
                         use_nearest,
                         do_material_slot_selection,
                         &rect,
                         drw_select_loop_pass,
                         &drw_select_loop_user_data,
                         object_filter.fn,
                         object_filter.user_data);
    hits = drw_select_loop_user_data.hits;
  }

  G.f &= ~G_FLAG_PICKSEL;
  ED_view3d_draw_setup_view(
      wm, vc->win, depsgraph, scene, region, v3d, vc->rv3d->viewmat, nullptr, nullptr);

  if (!XRAY_ACTIVE(v3d)) {
    GPU_depth_test(GPU_DEPTH_NONE);
  }

  DRW_gpu_context_disable();

  UI_Theme_Restore(&theme_state);

  return hits;
}

int view3d_gpu_select(const ViewContext *vc,
                      GPUSelectBuffer *buffer,
                      const rcti *input,
                      eV3DSelectMode select_mode,
                      eV3DSelectObjectFilter select_filter)
{
  return view3d_gpu_select_ex(vc, buffer, input, select_mode, select_filter, false);
}

int view3d_gpu_select_with_id_filter(const ViewContext *vc,
                                     GPUSelectBuffer *buffer,
                                     const rcti *input,
                                     eV3DSelectMode select_mode,
                                     eV3DSelectObjectFilter select_filter,
                                     uint select_id)
{
  const int64_t start = buffer->storage.size();
  int hits = view3d_gpu_select(vc, buffer, input, select_mode, select_filter);

  /* Selection sometimes uses -1 for an invalid selection ID, remove these as they
   * interfere with detection of actual number of hits in the selection. */
  if (hits > 0) {
    hits = GPU_select_buffer_remove_by_id(buffer->storage.as_mutable_span().slice(start, hits),
                                          select_id);

    /* Trim buffer to the exact size in case selections were removed. */
    buffer->storage.resize(start + hits);
  }
  return hits;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Local View Operators
 * \{ */

static uint free_localview_bit(Main *bmain)
{
  ushort local_view_bits = 0;

  /* Sometimes we lose a local-view: when an area is closed.
   * Check all areas: which local-views are in use? */
  LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
    LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
      SpaceLink *sl = static_cast<SpaceLink *>(area->spacedata.first);
      for (; sl; sl = sl->next) {
        if (sl->spacetype == SPACE_VIEW3D) {
          View3D *v3d = reinterpret_cast<View3D *>(sl);
          if (v3d->localvd) {
            local_view_bits |= v3d->local_view_uid;
          }
        }
      }
    }
  }

  for (int i = 0; i < 16; i++) {
    if ((local_view_bits & (1 << i)) == 0) {
      return (1 << i);
    }
  }

  return 0;
}

static bool view3d_localview_init(const Depsgraph *depsgraph,
                                  wmWindowManager *wm,
                                  wmWindow *win,
                                  Main *bmain,
                                  const Scene *scene,
                                  ViewLayer *view_layer,
                                  ScrArea *area,
                                  const bool frame_selected,
                                  const int smooth_viewtx,
                                  ReportList *reports)
{
  View3D *v3d = static_cast<View3D *>(area->spacedata.first);
  blender::float3 min, max, box;
  float size = 0.0f;
  uint local_view_bit;
  bool changed = false;

  if (v3d->localvd) {
    return changed;
  }

  INIT_MINMAX(min, max);

  local_view_bit = free_localview_bit(bmain);

  if (local_view_bit == 0) {
    /* TODO(dfelinto): We can kick one of the other 3D views out of local view
     * specially if it is not being used. */
    BKE_report(reports, RPT_ERROR, "No more than 16 local views");
    changed = false;
  }
  else {
    BKE_view_layer_synced_ensure(scene, view_layer);
    Object *obedit = BKE_view_layer_edit_object_get(view_layer);
    if (obedit) {
      BKE_view_layer_synced_ensure(scene, view_layer);
      LISTBASE_FOREACH (Base *, base, BKE_view_layer_object_bases_get(view_layer)) {
        base->local_view_bits &= ~local_view_bit;
      }
      FOREACH_BASE_IN_EDIT_MODE_BEGIN (scene, view_layer, v3d, base_iter) {
        Object *ob_eval = DEG_get_evaluated(depsgraph, base_iter->object);
        BKE_object_minmax(ob_eval ? ob_eval : base_iter->object, min, max);
        base_iter->local_view_bits |= local_view_bit;
        changed = true;
      }
      FOREACH_BASE_IN_EDIT_MODE_END;
    }
    else {
      BKE_view_layer_synced_ensure(scene, view_layer);
      LISTBASE_FOREACH (Base *, base, BKE_view_layer_object_bases_get(view_layer)) {
        if (BASE_SELECTED(v3d, base)) {
          Object *ob_eval = DEG_get_evaluated(depsgraph, base->object);
          BKE_object_minmax(ob_eval ? ob_eval : base->object, min, max);
          base->local_view_bits |= local_view_bit;
          changed = true;
        }
        else {
          base->local_view_bits &= ~local_view_bit;
        }
      }
    }

    sub_v3_v3v3(box, max, min);
    size = max_fff(box[0], box[1], box[2]);
  }

  if (changed == false) {
    return false;
  }

  /* Apply any running smooth-view values before reading from the viewport. */
  LISTBASE_FOREACH (ARegion *, region, &area->regionbase) {
    if (region->regiontype == RGN_TYPE_WINDOW) {
      RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);
      if (rv3d->sms) {
        ED_view3d_smooth_view_force_finish_no_camera_lock(depsgraph, wm, win, scene, v3d, region);
      }
    }
  }

  v3d->localvd = MEM_mallocN<View3D>("localview");
  *v3d->localvd = blender::dna::shallow_copy(*v3d);
  v3d->local_view_uid = local_view_bit;

  LISTBASE_FOREACH (ARegion *, region, &area->regionbase) {
    if (region->regiontype == RGN_TYPE_WINDOW) {
      RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);
      bool ok_dist = true;

      /* New view values. */
      Object *camera_old = nullptr;
      float dist_new, ofs_new[3];

      rv3d->localvd = MEM_mallocN<RegionView3D>("localview region");
      memcpy(rv3d->localvd, rv3d, sizeof(RegionView3D));

      if (frame_selected) {
        float mid[3];
        mid_v3_v3v3(mid, min, max);
        negate_v3_v3(ofs_new, mid);

        if (rv3d->persp == RV3D_CAMOB) {
          camera_old = v3d->camera;
          const Camera &camera = *static_cast<Camera *>(camera_old->data);
          rv3d->persp = (camera.type == CAM_ORTHO) ? RV3D_ORTHO : RV3D_PERSP;
        }

        if (rv3d->persp == RV3D_ORTHO) {
          if (size < 0.0001f) {
            ok_dist = false;
          }
        }

        if (ok_dist) {
          dist_new = ED_view3d_radius_to_dist(
              v3d, region, depsgraph, rv3d->persp, true, (size / 2) * VIEW3D_MARGIN);

          if (rv3d->persp == RV3D_PERSP) {
            /* Don't zoom closer than the near clipping plane. */
            const float dist_min = ED_view3d_dist_soft_min_get(v3d, true);
            CLAMP_MIN(dist_new, dist_min);
          }
        }

        V3D_SmoothParams sview_params = {};
        sview_params.camera_old = camera_old;
        sview_params.ofs = ofs_new;
        sview_params.quat = rv3d->viewquat;
        sview_params.dist = ok_dist ? &dist_new : nullptr;
        sview_params.lens = &v3d->lens;
        /* No undo because this doesn't move the camera. */
        sview_params.undo_str = nullptr;

        ED_view3d_smooth_view_ex(
            depsgraph, wm, win, area, v3d, region, smooth_viewtx, &sview_params);
      }
    }
  }

  return changed;
}

static bool view3d_localview_exit(const Depsgraph *depsgraph,
                                  wmWindowManager *wm,
                                  wmWindow *win,
                                  const Scene *scene,
                                  ViewLayer *view_layer,
                                  ScrArea *area,
                                  const bool frame_selected,
                                  const int smooth_viewtx)
{
  View3D *v3d = static_cast<View3D *>(area->spacedata.first);
  bool changed = false;

  if (v3d->localvd == nullptr) {
    return changed;
  }
  BKE_view_layer_synced_ensure(scene, view_layer);
  LISTBASE_FOREACH (Base *, base, BKE_view_layer_object_bases_get(view_layer)) {
    if (base->local_view_bits & v3d->local_view_uid) {
      base->local_view_bits &= ~v3d->local_view_uid;
    }
  }

  /* Apply any running smooth-view values before reading from the viewport. */
  LISTBASE_FOREACH (ARegion *, region, &area->regionbase) {
    if (region->regiontype == RGN_TYPE_WINDOW) {
      RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);
      if (rv3d->localvd == nullptr) {
        continue;
      }
      if (rv3d->sms) {
        ED_view3d_smooth_view_force_finish_no_camera_lock(depsgraph, wm, win, scene, v3d, region);
      }
    }
  }

  Object *camera_old = v3d->camera;
  Object *camera_new = v3d->localvd->camera;

  v3d->local_view_uid = 0;
  v3d->camera = v3d->localvd->camera;

  MEM_freeN(v3d->localvd);
  v3d->localvd = nullptr;
  ED_view3d_local_stats_free(v3d);

  LISTBASE_FOREACH (ARegion *, region, &area->regionbase) {
    if (region->regiontype == RGN_TYPE_WINDOW) {
      RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);

      if (rv3d->localvd == nullptr) {
        continue;
      }

      if (frame_selected && depsgraph) {
        Object *camera_old_rv3d, *camera_new_rv3d;

        camera_old_rv3d = (rv3d->persp == RV3D_CAMOB) ? camera_old : nullptr;
        camera_new_rv3d = (rv3d->localvd->persp == RV3D_CAMOB) ? camera_new : nullptr;

        rv3d->view = rv3d->localvd->view;
        rv3d->view_axis_roll = rv3d->localvd->view_axis_roll;
        rv3d->persp = rv3d->localvd->persp;
        rv3d->camzoom = rv3d->localvd->camzoom;

        V3D_SmoothParams sview_params = {};
        sview_params.camera_old = camera_old_rv3d;
        sview_params.camera = camera_new_rv3d;
        sview_params.ofs = rv3d->localvd->ofs;
        sview_params.quat = rv3d->localvd->viewquat;
        sview_params.dist = &rv3d->localvd->dist;
        /* No undo because this doesn't move the camera. */
        sview_params.undo_str = nullptr;

        ED_view3d_smooth_view_ex(
            depsgraph, wm, win, area, v3d, region, smooth_viewtx, &sview_params);
      }

      MEM_freeN(rv3d->localvd);
      rv3d->localvd = nullptr;
      changed = true;
    }
  }
  return changed;
}

bool ED_localview_exit_if_empty(const Depsgraph *depsgraph,
                                Scene *scene,
                                ViewLayer *view_layer,
                                wmWindowManager *wm,
                                wmWindow *win,
                                View3D *v3d,
                                ScrArea *area,
                                const bool frame_selected,
                                const int smooth_viewtx)
{
  if (v3d->localvd == nullptr) {
    return false;
  }

  v3d->localvd->runtime.flag &= ~V3D_RUNTIME_LOCAL_MAYBE_EMPTY;

  BKE_view_layer_synced_ensure(scene, view_layer);
  LISTBASE_FOREACH (Base *, base, BKE_view_layer_object_bases_get(view_layer)) {
    if (base->local_view_bits & v3d->local_view_uid) {
      return false;
    }
  }

  return view3d_localview_exit(
      depsgraph, wm, win, scene, view_layer, area, frame_selected, smooth_viewtx);
}

static wmOperatorStatus localview_exec(bContext *C, wmOperator *op)
{
  const Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  const int smooth_viewtx = WM_operator_smooth_viewtx_get(op);
  wmWindowManager *wm = CTX_wm_manager(C);
  wmWindow *win = CTX_wm_window(C);
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  ScrArea *area = CTX_wm_area(C);
  View3D *v3d = CTX_wm_view3d(C);
  bool frame_selected = RNA_boolean_get(op->ptr, "frame_selected");
  bool changed;

  if (v3d->localvd) {
    changed = view3d_localview_exit(
        depsgraph, wm, win, scene, view_layer, area, frame_selected, smooth_viewtx);
  }
  else {
    changed = view3d_localview_init(depsgraph,
                                    wm,
                                    win,
                                    bmain,
                                    scene,
                                    view_layer,
                                    area,
                                    frame_selected,
                                    smooth_viewtx,
                                    op->reports);
  }

  if (changed) {
    DEG_id_type_tag(bmain, ID_OB);
    ED_area_tag_redraw(area);

    /* Unselected objects become selected when exiting. */
    if (v3d->localvd == nullptr) {
      DEG_id_tag_update(&scene->id, ID_RECALC_SELECT);
      WM_event_add_notifier(C, NC_SCENE | ND_OB_SELECT, scene);
    }
    else {
      DEG_id_tag_update(&scene->id, ID_RECALC_BASE_FLAGS);
    }

    return OPERATOR_FINISHED;
  }
  return OPERATOR_CANCELLED;
}

void VIEW3D_OT_localview(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Local View";
  ot->description = "Toggle display of selected object(s) separately and centered in view";
  ot->idname = "VIEW3D_OT_localview";

  /* API callbacks. */
  ot->exec = localview_exec;
  /* Use undo because local-view changes object layer bit-flags. */
  ot->flag = OPTYPE_UNDO;

  ot->poll = ED_operator_view3d_active;

  RNA_def_boolean(ot->srna,
                  "frame_selected",
                  true,
                  "Frame Selected",
                  "Move the view to frame the selected objects");
}

static wmOperatorStatus localview_remove_from_exec(bContext *C, wmOperator *op)
{
  View3D *v3d = CTX_wm_view3d(C);
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  bool changed = false;
  BKE_view_layer_synced_ensure(scene, view_layer);
  LISTBASE_FOREACH (Base *, base, BKE_view_layer_object_bases_get(view_layer)) {
    if (BASE_SELECTED(v3d, base)) {
      base->local_view_bits &= ~v3d->local_view_uid;
      blender::ed::object::base_select(base, blender::ed::object::BA_DESELECT);

      if (base == view_layer->basact) {
        view_layer->basact = nullptr;
      }
      changed = true;
    }
  }

  /* If some object was removed from the local view, exit the local view if it is now empty. */
  if (changed) {
    ED_localview_exit_if_empty(CTX_data_ensure_evaluated_depsgraph(C),
                               scene,
                               view_layer,
                               CTX_wm_manager(C),
                               CTX_wm_window(C),
                               v3d,
                               CTX_wm_area(C),
                               true,
                               WM_operator_smooth_viewtx_get(op));
  }

  if (changed) {
    DEG_tag_on_visible_update(bmain, false);
    DEG_id_tag_update(&scene->id, ID_RECALC_SELECT);
    WM_event_add_notifier(C, NC_SCENE | ND_OB_SELECT, scene);
    WM_event_add_notifier(C, NC_SCENE | ND_OB_ACTIVE, scene);
    return OPERATOR_FINISHED;
  }

  BKE_report(op->reports, RPT_ERROR, "No object selected");
  return OPERATOR_CANCELLED;
}

static bool localview_remove_from_poll(bContext *C)
{
  if (CTX_data_edit_object(C) != nullptr) {
    return false;
  }

  View3D *v3d = CTX_wm_view3d(C);
  return v3d && v3d->localvd;
}

void VIEW3D_OT_localview_remove_from(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Remove from Local View";
  ot->description = "Move selected objects out of local view";
  ot->idname = "VIEW3D_OT_localview_remove_from";

  /* API callbacks. */
  ot->exec = localview_remove_from_exec;
  ot->poll = localview_remove_from_poll;
  ot->flag = OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Local Collections
 * \{ */

static uint free_localcollection_bit(const Main *bmain,
                                     ushort local_collections_uid,
                                     bool *r_reset)
{
  ushort local_view_bits = 0;

  /* Check all areas: which local-views are in use? */
  LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
    LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
      LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
        if (sl->spacetype == SPACE_VIEW3D) {
          View3D *v3d = reinterpret_cast<View3D *>(sl);
          if (v3d->flag & V3D_LOCAL_COLLECTIONS) {
            local_view_bits |= v3d->local_collections_uid;
          }
        }
      }
    }
  }

  /* First try to keep the old uuid. */
  if (local_collections_uid && ((local_collections_uid & local_view_bits) == 0)) {
    return local_collections_uid;
  }

  /* Otherwise get the first free available. */
  for (int i = 0; i < 16; i++) {
    if ((local_view_bits & (1 << i)) == 0) {
      *r_reset = true;
      return (1 << i);
    }
  }

  return 0;
}

static void local_collections_reset_uuid(LayerCollection *layer_collection,
                                         const ushort local_view_bit)
{
  if (layer_collection->flag & LAYER_COLLECTION_HIDE) {
    layer_collection->local_collections_bits &= ~local_view_bit;
  }
  else {
    layer_collection->local_collections_bits |= local_view_bit;
  }

  LISTBASE_FOREACH (LayerCollection *, child, &layer_collection->layer_collections) {
    local_collections_reset_uuid(child, local_view_bit);
  }
}

static void view3d_local_collections_reset(const Main *bmain, const uint local_view_bit)
{
  LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
    LISTBASE_FOREACH (ViewLayer *, view_layer, &scene->view_layers) {
      LISTBASE_FOREACH (LayerCollection *, layer_collection, &view_layer->layer_collections) {
        local_collections_reset_uuid(layer_collection, local_view_bit);
      }
    }
  }
}

bool ED_view3d_local_collections_set(const Main *bmain, View3D *v3d)
{
  if ((v3d->flag & V3D_LOCAL_COLLECTIONS) == 0) {
    return true;
  }

  bool reset = false;
  v3d->flag &= ~V3D_LOCAL_COLLECTIONS;
  uint local_view_bit = free_localcollection_bit(bmain, v3d->local_collections_uid, &reset);

  if (local_view_bit == 0) {
    return false;
  }

  v3d->local_collections_uid = local_view_bit;
  v3d->flag |= V3D_LOCAL_COLLECTIONS;

  if (reset) {
    view3d_local_collections_reset(bmain, local_view_bit);
  }

  return true;
}

void ED_view3d_local_collections_reset(const bContext *C, const bool reset_all)
{
  Main *bmain = CTX_data_main(C);
  uint local_view_bit = ~0;
  bool do_reset = false;

  /* Reset only the ones that are not in use. */
  LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
    LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
      LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
        if (sl->spacetype == SPACE_VIEW3D) {
          View3D *v3d = reinterpret_cast<View3D *>(sl);
          if (v3d->local_collections_uid) {
            if (v3d->flag & V3D_LOCAL_COLLECTIONS) {
              local_view_bit &= ~v3d->local_collections_uid;
            }
            else {
              do_reset = true;
            }
          }
        }
      }
    }
  }

  if (do_reset) {
    view3d_local_collections_reset(bmain, local_view_bit);
  }
  else if (reset_all && (do_reset || (local_view_bit != ~0))) {
    view3d_local_collections_reset(bmain, ~0);
    View3D v3d = {};
    v3d.local_collections_uid = ~0;
    BKE_layer_collection_local_sync(CTX_data_scene(C), CTX_data_view_layer(C), &v3d);
    DEG_id_tag_update(&CTX_data_scene(C)->id, ID_RECALC_BASE_FLAGS);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name XR Functionality
 * \{ */

#ifdef WITH_XR_OPENXR

static void view3d_xr_mirror_begin(RegionView3D *rv3d)
{
  /* If there is no session yet, changes below should not be applied! */
  BLI_assert(WM_xr_session_exists(&((wmWindowManager *)G_MAIN->wm.first)->xr));

  rv3d->runtime_viewlock |= RV3D_LOCK_ANY_TRANSFORM;
  /* Force perspective view. This isn't reset but that's not really an issue. */
  rv3d->persp = RV3D_PERSP;
}

static void view3d_xr_mirror_end(RegionView3D *rv3d)
{
  rv3d->runtime_viewlock &= ~RV3D_LOCK_ANY_TRANSFORM;
}

void ED_view3d_xr_mirror_update(const ScrArea *area, const View3D *v3d, const bool enable)
{
  ARegion *region_rv3d;

  BLI_assert(v3d->spacetype == SPACE_VIEW3D);

  if (ED_view3d_area_user_region(area, v3d, &region_rv3d)) {
    if (enable) {
      view3d_xr_mirror_begin(static_cast<RegionView3D *>(region_rv3d->regiondata));
    }
    else {
      view3d_xr_mirror_end(static_cast<RegionView3D *>(region_rv3d->regiondata));
    }
  }
}

void ED_view3d_xr_shading_update(wmWindowManager *wm, const View3D *v3d, const Scene *scene)
{
  if (v3d->runtime.flag & V3D_RUNTIME_XR_SESSION_ROOT) {
    View3DShading *xr_shading = &wm->xr.session_settings.shading;
    /* Flags that shouldn't be overridden by the 3D View shading. */
    int flag_copy = 0;
    if (v3d->shading.type != OB_SOLID) {
      /* Don't set V3D_SHADING_WORLD_ORIENTATION for solid shading since it results in distorted
       * lighting when the view matrix has a scale factor. */
      flag_copy |= V3D_SHADING_WORLD_ORIENTATION;
    }

    BLI_assert(WM_xr_session_exists(&wm->xr));

    if (v3d->shading.type == OB_RENDER) {
      if (!(BKE_scene_uses_blender_workbench(scene) || BKE_scene_uses_blender_eevee(scene))) {
        /* Keep old shading while using Cycles or another engine, they are typically not usable in
         * VR. */
        return;
      }
    }

    if (xr_shading->prop) {
      IDP_FreeProperty(xr_shading->prop);
      xr_shading->prop = nullptr;
    }

    /* Copy shading from View3D to VR view. */
    const int old_xr_shading_flag = xr_shading->flag;
    *xr_shading = v3d->shading;
    xr_shading->flag = (xr_shading->flag & ~flag_copy) | (old_xr_shading_flag & flag_copy);
    if (v3d->shading.prop) {
      xr_shading->prop = IDP_CopyProperty(xr_shading->prop);
    }
  }
}

bool ED_view3d_is_region_xr_mirror_active(const wmWindowManager *wm,
                                          const View3D *v3d,
                                          const ARegion *region)
{
  return (v3d->flag & V3D_XR_SESSION_MIRROR) &&
         /* The free region (e.g. the camera region in quad-view) is always
          * the last in the list base. We don't want any other to be affected. */
         !region->next &&  //
         WM_xr_session_is_ready(&wm->xr);
}

#endif

/** \} */
