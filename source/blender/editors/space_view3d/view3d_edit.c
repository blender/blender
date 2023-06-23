/* SPDX-FileCopyrightText: 2008 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spview3d
 *
 * 3D view manipulation/operators.
 */

#include "DNA_armature_types.h"
#include "DNA_camera_types.h"
#include "DNA_world_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"

#include "BKE_action.h"
#include "BKE_armature.h"
#include "BKE_camera.h"
#include "BKE_lib_id.h"
#include "BKE_main.h"
#include "BKE_object.h"
#include "BKE_report.h"
#include "BKE_scene.h"
#include "BKE_screen.h"

#include "DEG_depsgraph_query.h"

#include "WM_api.h"
#include "WM_message.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "ED_screen.h"
#include "ED_transform.h"
#include "ED_transform_snap_object_context.h"

#include "view3d_intern.h" /* own include */

/* test for unlocked camera view in quad view */
static bool view3d_camera_user_poll(bContext *C)
{
  View3D *v3d;
  ARegion *region;

  if (ED_view3d_context_user_region(C, &v3d, &region)) {
    RegionView3D *rv3d = region->regiondata;
    if ((rv3d->persp == RV3D_CAMOB) && !(RV3D_LOCK_FLAGS(rv3d) & RV3D_LOCK_ANY_TRANSFORM)) {
      return 1;
    }
  }

  return 0;
}

static bool view3d_lock_poll(bContext *C)
{
  View3D *v3d = CTX_wm_view3d(C);
  if (v3d) {
    RegionView3D *rv3d = CTX_wm_region_view3d(C);
    if (rv3d) {
      return ED_view3d_offset_lock_check(v3d, rv3d);
    }
  }
  return false;
}

/* -------------------------------------------------------------------- */
/** \name View Lock Clear Operator
 * \{ */

static int view_lock_clear_exec(bContext *C, wmOperator *UNUSED(op))
{
  View3D *v3d = CTX_wm_view3d(C);

  if (v3d) {
    ED_view3d_lock_clear(v3d);

    WM_event_add_notifier(C, NC_SPACE | ND_SPACE_VIEW3D, v3d);

    return OPERATOR_FINISHED;
  }

  return OPERATOR_CANCELLED;
}

void VIEW3D_OT_view_lock_clear(wmOperatorType *ot)
{

  /* identifiers */
  ot->name = "View Lock Clear";
  ot->description = "Clear all view locking";
  ot->idname = "VIEW3D_OT_view_lock_clear";

  /* api callbacks */
  ot->exec = view_lock_clear_exec;
  ot->poll = ED_operator_region_view3d_active;

  /* flags */
  ot->flag = 0;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name View Lock to Active Operator
 * \{ */

static int view_lock_to_active_exec(bContext *C, wmOperator *UNUSED(op))
{
  View3D *v3d = CTX_wm_view3d(C);
  Object *obact = CTX_data_active_object(C);

  if (v3d) {
    ED_view3d_lock_clear(v3d);

    v3d->ob_center = obact; /* can be NULL */

    if (obact && obact->type == OB_ARMATURE) {
      if (obact->mode & OB_MODE_POSE) {
        Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
        Object *obact_eval = DEG_get_evaluated_object(depsgraph, obact);
        bPoseChannel *pcham_act = BKE_pose_channel_active_if_layer_visible(obact_eval);
        if (pcham_act) {
          STRNCPY(v3d->ob_center_bone, pcham_act->name);
        }
      }
      else {
        EditBone *ebone_act = ((bArmature *)obact->data)->act_edbone;
        if (ebone_act) {
          STRNCPY(v3d->ob_center_bone, ebone_act->name);
        }
      }
    }

    WM_event_add_notifier(C, NC_SPACE | ND_SPACE_VIEW3D, v3d);

    return OPERATOR_FINISHED;
  }

  return OPERATOR_CANCELLED;
}

void VIEW3D_OT_view_lock_to_active(wmOperatorType *ot)
{

  /* identifiers */
  ot->name = "View Lock to Active";
  ot->description = "Lock the view to the active object/bone";
  ot->idname = "VIEW3D_OT_view_lock_to_active";

  /* api callbacks */
  ot->exec = view_lock_to_active_exec;
  ot->poll = ED_operator_region_view3d_active;

  /* flags */
  ot->flag = 0;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Frame Camera Bounds Operator
 * \{ */

static int view3d_center_camera_exec(bContext *C, wmOperator *UNUSED(op))
{
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Scene *scene = CTX_data_scene(C);
  float xfac, yfac;
  float size[2];

  View3D *v3d;
  ARegion *region;
  RegionView3D *rv3d;

  /* no NULL check is needed, poll checks */
  ED_view3d_context_user_region(C, &v3d, &region);
  rv3d = region->regiondata;

  rv3d->camdx = rv3d->camdy = 0.0f;

  ED_view3d_calc_camera_border_size(scene, depsgraph, region, v3d, rv3d, size);

  /* 4px is just a little room from the edge of the area */
  xfac = (float)region->winx / (float)(size[0] + 4);
  yfac = (float)region->winy / (float)(size[1] + 4);

  rv3d->camzoom = BKE_screen_view3d_zoom_from_fac(min_ff(xfac, yfac));
  CLAMP(rv3d->camzoom, RV3D_CAMZOOM_MIN, RV3D_CAMZOOM_MAX);

  WM_event_add_notifier(C, NC_SPACE | ND_SPACE_VIEW3D, v3d);

  return OPERATOR_FINISHED;
}

void VIEW3D_OT_view_center_camera(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Frame Camera Bounds";
  ot->description = "Center the camera view, resizing the view to fit its bounds";
  ot->idname = "VIEW3D_OT_view_center_camera";

  /* api callbacks */
  ot->exec = view3d_center_camera_exec;
  ot->poll = view3d_camera_user_poll;

  /* flags */
  ot->flag = 0;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name View Lock Center Operator
 * \{ */

static int view3d_center_lock_exec(bContext *C, wmOperator *UNUSED(op))
{
  RegionView3D *rv3d = CTX_wm_region_view3d(C);

  zero_v2(rv3d->ofs_lock);

  WM_event_add_notifier(C, NC_SPACE | ND_SPACE_VIEW3D, CTX_wm_view3d(C));

  return OPERATOR_FINISHED;
}

void VIEW3D_OT_view_center_lock(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "View Lock Center";
  ot->description = "Center the view lock offset";
  ot->idname = "VIEW3D_OT_view_center_lock";

  /* api callbacks */
  ot->exec = view3d_center_lock_exec;
  ot->poll = view3d_lock_poll;

  /* flags */
  ot->flag = 0;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Set Render Border Operator
 * \{ */

static int render_border_exec(bContext *C, wmOperator *op)
{
  View3D *v3d = CTX_wm_view3d(C);
  ARegion *region = CTX_wm_region(C);
  RegionView3D *rv3d = ED_view3d_context_rv3d(C);

  Scene *scene = CTX_data_scene(C);

  rcti rect;
  rctf vb, border;

  /* get box select values using rna */
  WM_operator_properties_border_to_rcti(op, &rect);

  /* calculate range */

  if (rv3d->persp == RV3D_CAMOB) {
    Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
    ED_view3d_calc_camera_border(scene, depsgraph, region, v3d, rv3d, &vb, false);
  }
  else {
    vb.xmin = 0;
    vb.ymin = 0;
    vb.xmax = region->winx;
    vb.ymax = region->winy;
  }

  border.xmin = ((float)rect.xmin - vb.xmin) / BLI_rctf_size_x(&vb);
  border.ymin = ((float)rect.ymin - vb.ymin) / BLI_rctf_size_y(&vb);
  border.xmax = ((float)rect.xmax - vb.xmin) / BLI_rctf_size_x(&vb);
  border.ymax = ((float)rect.ymax - vb.ymin) / BLI_rctf_size_y(&vb);

  /* actually set border */
  CLAMP(border.xmin, 0.0f, 1.0f);
  CLAMP(border.ymin, 0.0f, 1.0f);
  CLAMP(border.xmax, 0.0f, 1.0f);
  CLAMP(border.ymax, 0.0f, 1.0f);

  if (rv3d->persp == RV3D_CAMOB) {
    scene->r.border = border;

    WM_event_add_notifier(C, NC_SCENE | ND_RENDER_OPTIONS, NULL);
  }
  else {
    v3d->render_border = border;

    WM_event_add_notifier(C, NC_SPACE | ND_SPACE_VIEW3D, NULL);
  }

  /* drawing a border outside the camera view switches off border rendering */
  if (border.xmin == border.xmax || border.ymin == border.ymax) {
    if (rv3d->persp == RV3D_CAMOB) {
      scene->r.mode &= ~R_BORDER;
    }
    else {
      v3d->flag2 &= ~V3D_RENDER_BORDER;
    }
  }
  else {
    if (rv3d->persp == RV3D_CAMOB) {
      scene->r.mode |= R_BORDER;
    }
    else {
      v3d->flag2 |= V3D_RENDER_BORDER;
    }
  }

  if (rv3d->persp == RV3D_CAMOB) {
    DEG_id_tag_update(&scene->id, ID_RECALC_COPY_ON_WRITE);
  }
  return OPERATOR_FINISHED;
}

void VIEW3D_OT_render_border(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Set Render Region";
  ot->description = "Set the boundaries of the border render and enable border render";
  ot->idname = "VIEW3D_OT_render_border";

  /* api callbacks */
  ot->invoke = WM_gesture_box_invoke;
  ot->exec = render_border_exec;
  ot->modal = WM_gesture_box_modal;
  ot->cancel = WM_gesture_box_cancel;

  ot->poll = ED_operator_view3d_active;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  WM_operator_properties_border(ot);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Clear Render Border Operator
 * \{ */

static int clear_render_border_exec(bContext *C, wmOperator *UNUSED(op))
{
  View3D *v3d = CTX_wm_view3d(C);
  RegionView3D *rv3d = ED_view3d_context_rv3d(C);

  Scene *scene = CTX_data_scene(C);
  rctf *border = NULL;

  if (rv3d->persp == RV3D_CAMOB) {
    scene->r.mode &= ~R_BORDER;
    border = &scene->r.border;

    WM_event_add_notifier(C, NC_SCENE | ND_RENDER_OPTIONS, NULL);
  }
  else {
    v3d->flag2 &= ~V3D_RENDER_BORDER;
    border = &v3d->render_border;

    WM_event_add_notifier(C, NC_SPACE | ND_SPACE_VIEW3D, NULL);
  }

  border->xmin = 0.0f;
  border->ymin = 0.0f;
  border->xmax = 1.0f;
  border->ymax = 1.0f;

  if (rv3d->persp == RV3D_CAMOB) {
    DEG_id_tag_update(&scene->id, ID_RECALC_COPY_ON_WRITE);
  }
  return OPERATOR_FINISHED;
}

void VIEW3D_OT_clear_render_border(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Clear Render Region";
  ot->description = "Clear the boundaries of the border render and disable border render";
  ot->idname = "VIEW3D_OT_clear_render_border";

  /* api callbacks */
  ot->exec = clear_render_border_exec;
  ot->poll = ED_operator_view3d_active;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Set Camera Zoom 1:1 Operator
 *
 * Sets the view to 1:1 camera/render-pixel.
 * \{ */

static void view3d_set_1_to_1_viewborder(Scene *scene,
                                         Depsgraph *depsgraph,
                                         ARegion *region,
                                         View3D *v3d)
{
  RegionView3D *rv3d = region->regiondata;
  float size[2];

  int im_width, im_height;
  BKE_render_resolution(&scene->r, false, &im_width, &im_height);

  ED_view3d_calc_camera_border_size(scene, depsgraph, region, v3d, rv3d, size);

  rv3d->camzoom = BKE_screen_view3d_zoom_from_fac((float)im_width / size[0]);
  CLAMP(rv3d->camzoom, RV3D_CAMZOOM_MIN, RV3D_CAMZOOM_MAX);
}

static int view3d_zoom_1_to_1_camera_exec(bContext *C, wmOperator *UNUSED(op))
{
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Scene *scene = CTX_data_scene(C);

  View3D *v3d;
  ARegion *region;

  /* no NULL check is needed, poll checks */
  ED_view3d_context_user_region(C, &v3d, &region);

  view3d_set_1_to_1_viewborder(scene, depsgraph, region, v3d);

  WM_event_add_notifier(C, NC_SPACE | ND_SPACE_VIEW3D, v3d);

  return OPERATOR_FINISHED;
}

void VIEW3D_OT_zoom_camera_1_to_1(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Zoom Camera 1:1";
  ot->description = "Match the camera to 1:1 to the render output";
  ot->idname = "VIEW3D_OT_zoom_camera_1_to_1";

  /* api callbacks */
  ot->exec = view3d_zoom_1_to_1_camera_exec;
  ot->poll = view3d_camera_user_poll;

  /* flags */
  ot->flag = 0;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name View Toggle Perspective/Orthographic Operator
 * \{ */

static int viewpersportho_exec(bContext *C, wmOperator *UNUSED(op))
{
  View3D *v3d_dummy;
  ARegion *region;
  RegionView3D *rv3d;

  /* no NULL check is needed, poll checks */
  ED_view3d_context_user_region(C, &v3d_dummy, &region);
  rv3d = region->regiondata;

  /* Could add a separate lock flag for locking persp. */
  if ((RV3D_LOCK_FLAGS(rv3d) & RV3D_LOCK_ANY_TRANSFORM) == 0) {
    if (rv3d->persp != RV3D_ORTHO) {
      rv3d->persp = RV3D_ORTHO;
    }
    else {
      rv3d->persp = RV3D_PERSP;
    }
    ED_region_tag_redraw(region);
  }

  return OPERATOR_FINISHED;
}

void VIEW3D_OT_view_persportho(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "View Perspective/Orthographic";
  ot->description = "Switch the current view from perspective/orthographic projection";
  ot->idname = "VIEW3D_OT_view_persportho";

  /* api callbacks */
  ot->exec = viewpersportho_exec;
  ot->poll = ED_operator_rv3d_user_region_poll;

  /* flags */
  ot->flag = 0;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name View Navigate Operator
 *
 * Wraps walk/fly modes.
 * \{ */

static int view3d_navigate_invoke(bContext *C, wmOperator *UNUSED(op), const wmEvent *event)
{
  eViewNavigation_Method mode = U.navigation_mode;

  switch (mode) {
    case VIEW_NAVIGATION_FLY:
      WM_operator_name_call(C, "VIEW3D_OT_fly", WM_OP_INVOKE_DEFAULT, NULL, event);
      break;
    case VIEW_NAVIGATION_WALK:
    default:
      WM_operator_name_call(C, "VIEW3D_OT_walk", WM_OP_INVOKE_DEFAULT, NULL, event);
      break;
  }

  return OPERATOR_FINISHED;
}

void VIEW3D_OT_navigate(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "View Navigation (Walk/Fly)";
  ot->description =
      "Interactively navigate around the scene (uses the mode (walk/fly) preference)";
  ot->idname = "VIEW3D_OT_navigate";

  /* api callbacks */
  ot->invoke = view3d_navigate_invoke;
  ot->poll = ED_operator_view3d_active;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Background Image Add Operator
 * \{ */

static Camera *background_image_camera_from_context(bContext *C)
{
  /* Needed to support drag-and-drop & camera buttons context. */
  View3D *v3d = CTX_wm_view3d(C);
  if (v3d != NULL) {
    if (v3d->camera && v3d->camera->data && v3d->camera->type == OB_CAMERA) {
      return v3d->camera->data;
    }
    return NULL;
  }

  return CTX_data_pointer_get_type(C, "camera", &RNA_Camera).data;
}

static int background_image_add_exec(bContext *C, wmOperator *UNUSED(op))
{
  Camera *cam = background_image_camera_from_context(C);
  BKE_camera_background_image_new(cam);

  return OPERATOR_FINISHED;
}

static int background_image_add_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
  Camera *cam = background_image_camera_from_context(C);
  Image *ima;
  CameraBGImage *bgpic;

  ima = (Image *)WM_operator_drop_load_path(C, op, ID_IM);
  /* may be NULL, continue anyway */

  bgpic = BKE_camera_background_image_new(cam);
  bgpic->ima = ima;

  cam->flag |= CAM_SHOW_BG_IMAGE;

  WM_event_add_notifier(C, NC_CAMERA | ND_DRAW_RENDER_VIEWPORT, cam);
  DEG_id_tag_update(&cam->id, ID_RECALC_COPY_ON_WRITE);

  return OPERATOR_FINISHED;
}

static bool background_image_add_poll(bContext *C)
{
  return background_image_camera_from_context(C) != NULL;
}

void VIEW3D_OT_background_image_add(wmOperatorType *ot)
{
  /* identifiers */
  /* NOTE: having key shortcut here is bad practice,
   * but for now keep because this displays when dragging an image over the 3D viewport */
  ot->name = "Add Background Image";
  ot->description = "Add a new background image";
  ot->idname = "VIEW3D_OT_background_image_add";

  /* api callbacks */
  ot->invoke = background_image_add_invoke;
  ot->exec = background_image_add_exec;
  ot->poll = background_image_add_poll;

  /* flags */
  ot->flag = OPTYPE_UNDO;

  /* properties */
  WM_operator_properties_id_lookup(ot, true);
  WM_operator_properties_filesel(ot,
                                 FILE_TYPE_FOLDER | FILE_TYPE_IMAGE | FILE_TYPE_MOVIE,
                                 FILE_SPECIAL,
                                 FILE_OPENFILE,
                                 WM_FILESEL_FILEPATH | WM_FILESEL_RELPATH,
                                 FILE_DEFAULTDISPLAY,
                                 FILE_SORT_DEFAULT);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Background Image Remove Operator
 * \{ */

static int background_image_remove_exec(bContext *C, wmOperator *op)
{
  Camera *cam = CTX_data_pointer_get_type(C, "camera", &RNA_Camera).data;
  const int index = RNA_int_get(op->ptr, "index");
  CameraBGImage *bgpic_rem = BLI_findlink(&cam->bg_images, index);

  if (bgpic_rem) {
    if (ID_IS_OVERRIDE_LIBRARY(cam) &&
        (bgpic_rem->flag & CAM_BGIMG_FLAG_OVERRIDE_LIBRARY_LOCAL) == 0) {
      BKE_reportf(op->reports,
                  RPT_WARNING,
                  "Cannot remove background image %d from camera '%s', as it is from the linked "
                  "reference data",
                  index,
                  cam->id.name + 2);
      return OPERATOR_CANCELLED;
    }

    id_us_min((ID *)bgpic_rem->ima);
    id_us_min((ID *)bgpic_rem->clip);

    BKE_camera_background_image_remove(cam, bgpic_rem);

    WM_event_add_notifier(C, NC_CAMERA | ND_DRAW_RENDER_VIEWPORT, cam);
    DEG_id_tag_update(&cam->id, ID_RECALC_COPY_ON_WRITE);

    return OPERATOR_FINISHED;
  }
  return OPERATOR_CANCELLED;
}

void VIEW3D_OT_background_image_remove(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Remove Background Image";
  ot->description = "Remove a background image from the 3D view";
  ot->idname = "VIEW3D_OT_background_image_remove";

  /* api callbacks */
  ot->exec = background_image_remove_exec;
  ot->poll = ED_operator_camera_poll;

  /* flags */
  ot->flag = 0;

  /* properties */
  RNA_def_int(
      ot->srna, "index", 0, 0, INT_MAX, "Index", "Background image index to remove", 0, INT_MAX);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Drop World Operator
 * \{ */

static int drop_world_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);

  World *world = (World *)WM_operator_properties_id_lookup_from_name_or_session_uuid(
      bmain, op->ptr, ID_WO);
  if (world == NULL) {
    return OPERATOR_CANCELLED;
  }

  id_us_min((ID *)scene->world);
  id_us_plus(&world->id);
  scene->world = world;

  DEG_id_tag_update(&scene->id, ID_RECALC_COPY_ON_WRITE);
  DEG_relations_tag_update(bmain);

  WM_event_add_notifier(C, NC_SCENE | ND_WORLD, scene);

  return OPERATOR_FINISHED;
}

static bool drop_world_poll(bContext *C)
{
  return ED_operator_scene_editable(C);
}

void VIEW3D_OT_drop_world(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Drop World";
  ot->description = "Drop a world into the scene";
  ot->idname = "VIEW3D_OT_drop_world";

  /* api callbacks */
  ot->exec = drop_world_exec;
  ot->poll = drop_world_poll;

  /* flags */
  ot->flag = OPTYPE_UNDO | OPTYPE_INTERNAL;

  /* properties */
  WM_operator_properties_id_lookup(ot, true);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name View Clipping Planes Operator
 *
 * Draw border or toggle off.
 * \{ */

static void calc_local_clipping(float clip_local[6][4],
                                const BoundBox *clipbb,
                                const float mat[4][4])
{
  BoundBox clipbb_local;
  float imat[4][4];

  invert_m4_m4(imat, mat);

  for (int i = 0; i < 8; i++) {
    mul_v3_m4v3(clipbb_local.vec[i], imat, clipbb->vec[i]);
  }

  ED_view3d_clipping_calc_from_boundbox(clip_local, &clipbb_local, is_negative_m4(mat));
}

void ED_view3d_clipping_local(RegionView3D *rv3d, const float mat[4][4])
{
  if (rv3d->rflag & RV3D_CLIPPING) {
    calc_local_clipping(rv3d->clip_local, rv3d->clipbb, mat);
  }
}

static int view3d_clipping_exec(bContext *C, wmOperator *op)
{
  ARegion *region = CTX_wm_region(C);
  RegionView3D *rv3d = CTX_wm_region_view3d(C);
  rcti rect;

  WM_operator_properties_border_to_rcti(op, &rect);

  rv3d->rflag |= RV3D_CLIPPING;
  rv3d->clipbb = MEM_callocN(sizeof(BoundBox), "clipbb");

  /* NULL object because we don't want it in object space */
  ED_view3d_clipping_calc(rv3d->clipbb, rv3d->clip, region, NULL, &rect);

  return OPERATOR_FINISHED;
}

static int view3d_clipping_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  RegionView3D *rv3d = CTX_wm_region_view3d(C);
  ARegion *region = CTX_wm_region(C);

  if (rv3d->rflag & RV3D_CLIPPING) {
    rv3d->rflag &= ~RV3D_CLIPPING;
    ED_region_tag_redraw(region);
    MEM_SAFE_FREE(rv3d->clipbb);
    return OPERATOR_FINISHED;
  }
  return WM_gesture_box_invoke(C, op, event);
}

void VIEW3D_OT_clip_border(wmOperatorType *ot)
{

  /* identifiers */
  ot->name = "Clipping Region";
  ot->description = "Set the view clipping region";
  ot->idname = "VIEW3D_OT_clip_border";

  /* api callbacks */
  ot->invoke = view3d_clipping_invoke;
  ot->exec = view3d_clipping_exec;
  ot->modal = WM_gesture_box_modal;
  ot->cancel = WM_gesture_box_cancel;

  ot->poll = ED_operator_region_view3d_active;

  /* flags */
  ot->flag = 0;

  /* properties */
  WM_operator_properties_border(ot);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Set Cursor Operator
 * \{ */

/* cursor position in vec, result in vec, mval in region coords */
void ED_view3d_cursor3d_position(bContext *C,
                                 const int mval[2],
                                 const bool use_depth,
                                 float cursor_co[3])
{
  ARegion *region = CTX_wm_region(C);
  View3D *v3d = CTX_wm_view3d(C);
  RegionView3D *rv3d = region->regiondata;
  bool flip;
  bool depth_used = false;

  /* normally the caller should ensure this,
   * but this is called from areas that aren't already dealing with the viewport */
  if (rv3d == NULL) {
    return;
  }

  ED_view3d_calc_zfac_ex(rv3d, cursor_co, &flip);

  /* Reset the depth based on the view offset (we _know_ the offset is in front of us). */
  if (flip) {
    negate_v3_v3(cursor_co, rv3d->ofs);
    /* re initialize, no need to check flip again */
    ED_view3d_calc_zfac(rv3d, cursor_co);
  }

  if (use_depth) { /* maybe this should be accessed some other way */
    Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);

    view3d_operator_needs_opengl(C);
    if (ED_view3d_autodist(depsgraph, region, v3d, mval, cursor_co, true, NULL)) {
      depth_used = true;
    }
  }

  if (depth_used == false) {
    float depth_pt[3];
    copy_v3_v3(depth_pt, cursor_co);
    ED_view3d_win_to_3d_int(v3d, region, depth_pt, mval, cursor_co);
  }
}

void ED_view3d_cursor3d_position_rotation(bContext *C,
                                          const int mval[2],
                                          const bool use_depth,
                                          enum eV3DCursorOrient orientation,
                                          float cursor_co[3],
                                          float cursor_quat[4])
{
  Scene *scene = CTX_data_scene(C);
  View3D *v3d = CTX_wm_view3d(C);
  ARegion *region = CTX_wm_region(C);
  RegionView3D *rv3d = region->regiondata;

  /* XXX, caller should check. */
  if (rv3d == NULL) {
    return;
  }

  ED_view3d_cursor3d_position(C, mval, use_depth, cursor_co);

  if (orientation == V3D_CURSOR_ORIENT_NONE) {
    /* pass */
  }
  else if (orientation == V3D_CURSOR_ORIENT_VIEW) {
    copy_qt_qt(cursor_quat, rv3d->viewquat);
    cursor_quat[0] *= -1.0f;
  }
  else if (orientation == V3D_CURSOR_ORIENT_XFORM) {
    float mat[3][3];
    ED_transform_calc_orientation_from_type(C, mat);
    mat3_to_quat(cursor_quat, mat);
  }
  else if (orientation == V3D_CURSOR_ORIENT_GEOM) {
    copy_qt_qt(cursor_quat, rv3d->viewquat);
    cursor_quat[0] *= -1.0f;

    const float mval_fl[2] = {UNPACK2(mval)};
    float ray_no[3];
    float ray_co[3];

    SnapObjectContext *snap_context = ED_transform_snap_object_context_create(scene, 0);

    float obmat[4][4];
    Object *ob_dummy = NULL;
    float dist_px = 0;
    if (ED_transform_snap_object_project_view3d_ex(snap_context,
                                                   CTX_data_ensure_evaluated_depsgraph(C),
                                                   region,
                                                   v3d,
                                                   SCE_SNAP_TO_FACE,
                                                   &(const struct SnapObjectParams){
                                                       .snap_target_select = SCE_SNAP_TARGET_ALL,
                                                       .edit_mode_type = SNAP_GEOM_FINAL,
                                                       .use_occlusion_test = true,
                                                   },
                                                   NULL,
                                                   mval_fl,
                                                   NULL,
                                                   &dist_px,
                                                   ray_co,
                                                   ray_no,
                                                   NULL,
                                                   &ob_dummy,
                                                   obmat,
                                                   NULL) != 0)
    {
      if (use_depth) {
        copy_v3_v3(cursor_co, ray_co);
      }

      /* Math normal (Z). */
      {
        float tquat[4];
        float z_src[3] = {0, 0, 1};
        mul_qt_v3(cursor_quat, z_src);
        rotation_between_vecs_to_quat(tquat, z_src, ray_no);
        mul_qt_qtqt(cursor_quat, tquat, cursor_quat);
      }

      /* Match object matrix (X). */
      {
        const float ortho_axis_dot[3] = {
            dot_v3v3(ray_no, obmat[0]),
            dot_v3v3(ray_no, obmat[1]),
            dot_v3v3(ray_no, obmat[2]),
        };
        const int ortho_axis = axis_dominant_v3_ortho_single(ortho_axis_dot);

        float tquat_best[4];
        float angle_best = -1.0f;

        float tan_dst[3];
        project_plane_v3_v3v3(tan_dst, obmat[ortho_axis], ray_no);
        normalize_v3(tan_dst);

        /* As the tangent is arbitrary from the users point of view,
         * make the cursor 'roll' on the shortest angle.
         * otherwise this can cause noticeable 'flipping', see #72419. */
        for (int axis = 0; axis < 2; axis++) {
          float tan_src[3] = {0, 0, 0};
          tan_src[axis] = 1.0f;
          mul_qt_v3(cursor_quat, tan_src);

          for (int axis_sign = 0; axis_sign < 2; axis_sign++) {
            float tquat_test[4];
            rotation_between_vecs_to_quat(tquat_test, tan_src, tan_dst);
            const float angle_test = angle_normalized_qt(tquat_test);
            if (angle_test < angle_best || angle_best == -1.0f) {
              angle_best = angle_test;
              copy_qt_qt(tquat_best, tquat_test);
            }
            negate_v3(tan_src);
          }
        }
        mul_qt_qtqt(cursor_quat, tquat_best, cursor_quat);
      }
    }
    ED_transform_snap_object_context_destroy(snap_context);
  }
}

void ED_view3d_cursor3d_update(bContext *C,
                               const int mval[2],
                               const bool use_depth,
                               enum eV3DCursorOrient orientation)
{
  Scene *scene = CTX_data_scene(C);
  View3D *v3d = CTX_wm_view3d(C);
  ARegion *region = CTX_wm_region(C);
  RegionView3D *rv3d = region->regiondata;

  View3DCursor *cursor_curr = &scene->cursor;
  View3DCursor cursor_prev = *cursor_curr;

  {
    float quat[4], quat_prev[4];
    BKE_scene_cursor_rot_to_quat(cursor_curr, quat);
    copy_qt_qt(quat_prev, quat);
    ED_view3d_cursor3d_position_rotation(
        C, mval, use_depth, orientation, cursor_curr->location, quat);

    if (!equals_v4v4(quat_prev, quat)) {
      if ((cursor_curr->rotation_mode == ROT_MODE_AXISANGLE) && RV3D_VIEW_IS_AXIS(rv3d->view)) {
        float tmat[3][3], cmat[3][3];
        quat_to_mat3(tmat, quat);
        negate_v3_v3(cursor_curr->rotation_axis, tmat[2]);
        axis_angle_to_mat3(cmat, cursor_curr->rotation_axis, 0.0f);
        cursor_curr->rotation_angle = angle_signed_on_axis_v3v3_v3(
            cmat[0], tmat[0], cursor_curr->rotation_axis);
      }
      else {
        BKE_scene_cursor_quat_to_rot(cursor_curr, quat, true);
      }
    }
  }

  /* offset the cursor lock to avoid jumping to new offset */
  if (v3d->ob_center_cursor) {
    if (U.uiflag & USER_LOCK_CURSOR_ADJUST) {

      float co_2d_curr[2], co_2d_prev[2];

      if ((ED_view3d_project_float_global(
               region, cursor_prev.location, co_2d_prev, V3D_PROJ_TEST_NOP) == V3D_PROJ_RET_OK) &&
          (ED_view3d_project_float_global(
               region, cursor_curr->location, co_2d_curr, V3D_PROJ_TEST_NOP) == V3D_PROJ_RET_OK))
      {
        rv3d->ofs_lock[0] += (co_2d_curr[0] - co_2d_prev[0]) / (region->winx * 0.5f);
        rv3d->ofs_lock[1] += (co_2d_curr[1] - co_2d_prev[1]) / (region->winy * 0.5f);
      }
    }
    else {
      /* Cursor may be outside of the view,
       * prevent it getting 'lost', see: #40353 & #45301 */
      zero_v2(rv3d->ofs_lock);
    }
  }

  if (v3d->localvd) {
    WM_event_add_notifier(C, NC_SPACE | ND_SPACE_VIEW3D, v3d);
  }
  else {
    WM_event_add_notifier(C, NC_SCENE | NA_EDITED, scene);
  }

  {
    struct wmMsgBus *mbus = CTX_wm_message_bus(C);
    wmMsgParams_RNA msg_key_params = {{0}};
    RNA_pointer_create(&scene->id, &RNA_View3DCursor, &scene->cursor, &msg_key_params.ptr);
    WM_msg_publish_rna_params(mbus, &msg_key_params);
  }

  DEG_id_tag_update(&scene->id, ID_RECALC_COPY_ON_WRITE);
}

static int view3d_cursor3d_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  bool use_depth = (U.uiflag & USER_DEPTH_CURSOR);
  {
    PropertyRNA *prop = RNA_struct_find_property(op->ptr, "use_depth");
    if (RNA_property_is_set(op->ptr, prop)) {
      use_depth = RNA_property_boolean_get(op->ptr, prop);
    }
    else {
      RNA_property_boolean_set(op->ptr, prop, use_depth);
    }
  }
  const enum eV3DCursorOrient orientation = RNA_enum_get(op->ptr, "orientation");
  ED_view3d_cursor3d_update(C, event->mval, use_depth, orientation);

  /* Use pass-through to allow click-drag to transform the cursor. */
  return OPERATOR_FINISHED | OPERATOR_PASS_THROUGH;
}

void VIEW3D_OT_cursor3d(wmOperatorType *ot)
{

  /* identifiers */
  ot->name = "Set 3D Cursor";
  ot->description = "Set the location of the 3D cursor";
  ot->idname = "VIEW3D_OT_cursor3d";

  /* api callbacks */
  ot->invoke = view3d_cursor3d_invoke;

  ot->poll = ED_operator_region_view3d_active;

  /* flags */
  //  ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;

  PropertyRNA *prop;
  static const EnumPropertyItem orientation_items[] = {
      {V3D_CURSOR_ORIENT_NONE, "NONE", 0, "None", "Leave orientation unchanged"},
      {V3D_CURSOR_ORIENT_VIEW, "VIEW", 0, "View", "Orient to the viewport"},
      {V3D_CURSOR_ORIENT_XFORM,
       "XFORM",
       0,
       "Transform",
       "Orient to the current transform setting"},
      {V3D_CURSOR_ORIENT_GEOM, "GEOM", 0, "Geometry", "Match the surface normal"},
      {0, NULL, 0, NULL, NULL},
  };

  prop = RNA_def_boolean(
      ot->srna, "use_depth", true, "Surface Project", "Project onto the surface");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  prop = RNA_def_enum(ot->srna,
                      "orientation",
                      orientation_items,
                      V3D_CURSOR_ORIENT_VIEW,
                      "Orientation",
                      "Preset viewpoint to use");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Toggle Shading Operator
 * \{ */

static const EnumPropertyItem prop_shading_type_items[] = {
    {OB_WIRE, "WIREFRAME", 0, "Wireframe", "Toggle wireframe shading"},
    {OB_SOLID, "SOLID", 0, "Solid", "Toggle solid shading"},
    {OB_MATERIAL, "MATERIAL", 0, "Material Preview", "Toggle material preview shading"},
    {OB_RENDER, "RENDERED", 0, "Rendered", "Toggle rendered shading"},
    {0, NULL, 0, NULL, NULL},
};

static int toggle_shading_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  View3D *v3d = CTX_wm_view3d(C);
  ScrArea *area = CTX_wm_area(C);
  int type = RNA_enum_get(op->ptr, "type");

  if (type == OB_SOLID) {
    if (v3d->shading.type != type) {
      v3d->shading.type = type;
    }
    else if (v3d->shading.type == OB_WIRE) {
      v3d->shading.type = OB_SOLID;
    }
    else {
      v3d->shading.type = OB_WIRE;
    }
  }
  else {
    char *prev_type = ((type == OB_WIRE) ? &v3d->shading.prev_type_wire : &v3d->shading.prev_type);
    if (v3d->shading.type == type) {
      if (*prev_type == type || !ELEM(*prev_type, OB_WIRE, OB_SOLID, OB_MATERIAL, OB_RENDER)) {
        *prev_type = OB_SOLID;
      }
      v3d->shading.type = *prev_type;
    }
    else {
      *prev_type = v3d->shading.type;
      v3d->shading.type = type;
    }
  }

  ED_view3d_shade_update(bmain, v3d, area);
  WM_event_add_notifier(C, NC_SPACE | ND_SPACE_VIEW3D | NS_VIEW3D_SHADING, v3d);

  return OPERATOR_FINISHED;
}

void VIEW3D_OT_toggle_shading(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Toggle Shading Type";
  ot->description = "Toggle shading type in 3D viewport";
  ot->idname = "VIEW3D_OT_toggle_shading";

  /* api callbacks */
  ot->exec = toggle_shading_exec;
  ot->poll = ED_operator_view3d_active;

  prop = RNA_def_enum(
      ot->srna, "type", prop_shading_type_items, 0, "Type", "Shading type to toggle");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Toggle XRay
 * \{ */

static int toggle_xray_exec(bContext *C, wmOperator *op)
{
  View3D *v3d = CTX_wm_view3d(C);
  ScrArea *area = CTX_wm_area(C);
  Object *obact = CTX_data_active_object(C);

  if (obact && ((obact->mode & OB_MODE_POSE) ||
                ((obact->mode & OB_MODE_WEIGHT_PAINT) && BKE_object_pose_armature_get(obact))))
  {
    v3d->overlay.flag ^= V3D_OVERLAY_BONE_SELECT;
  }
  else {
    const bool xray_active = ((obact && (obact->mode & OB_MODE_EDIT)) ||
                              ELEM(v3d->shading.type, OB_WIRE, OB_SOLID));

    if (v3d->shading.type == OB_WIRE) {
      v3d->shading.flag ^= V3D_SHADING_XRAY_WIREFRAME;
    }
    else {
      v3d->shading.flag ^= V3D_SHADING_XRAY;
    }
    if (!xray_active) {
      BKE_report(op->reports, RPT_INFO, "X-Ray not available in current mode");
    }
  }

  ED_area_tag_redraw(area);
  WM_event_add_notifier(C, NC_SPACE | ND_SPACE_VIEW3D | NS_VIEW3D_SHADING, v3d);

  return OPERATOR_FINISHED;
}

void VIEW3D_OT_toggle_xray(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Toggle X-Ray";
  ot->idname = "VIEW3D_OT_toggle_xray";
  ot->description = "Transparent scene display. Allow selecting through items";

  /* api callbacks */
  ot->exec = toggle_xray_exec;
  ot->poll = ED_operator_view3d_active;
}

/** \} */
