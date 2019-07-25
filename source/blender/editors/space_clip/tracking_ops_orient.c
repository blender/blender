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
 *
 * The Original Code is Copyright (C) 2016 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup spclip
 */

#include "MEM_guardedalloc.h"

#include "DNA_constraint_types.h"
#include "DNA_object_types.h" /* SELECT */
#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "BLI_utildefines.h"
#include "BLI_math.h"

#include "BKE_context.h"
#include "BKE_constraint.h"
#include "BKE_tracking.h"
#include "BKE_layer.h"
#include "BKE_object.h"
#include "BKE_report.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_clip.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "clip_intern.h"

/********************** set origin operator *********************/

static Object *get_camera_with_movieclip(Scene *scene, MovieClip *clip)
{
  Object *camera = scene->camera;

  if (camera != NULL && BKE_object_movieclip_get(scene, camera, false) == clip) {
    return camera;
  }

  FOREACH_SCENE_OBJECT_BEGIN (scene, ob) {
    if (ob->type == OB_CAMERA) {
      if (BKE_object_movieclip_get(scene, ob, false) == clip) {
        camera = ob;
        break;
      }
    }
  }
  FOREACH_SCENE_OBJECT_END;

  return camera;
}

static Object *get_orientation_object(bContext *C)
{
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  SpaceClip *sc = CTX_wm_space_clip(C);
  MovieClip *clip = ED_space_clip_get_clip(sc);
  MovieTracking *tracking = &clip->tracking;
  MovieTrackingObject *tracking_object = BKE_tracking_object_get_active(tracking);
  Object *object = NULL;

  if (tracking_object->flag & TRACKING_OBJECT_CAMERA) {
    object = get_camera_with_movieclip(scene, clip);
  }
  else {
    object = OBACT(view_layer);
  }

  if (object != NULL && object->parent != NULL) {
    object = object->parent;
  }

  return object;
}

static bool set_orientation_poll(bContext *C)
{
  SpaceClip *sc = CTX_wm_space_clip(C);
  if (sc != NULL) {
    ViewLayer *view_layer = CTX_data_view_layer(C);
    MovieClip *clip = ED_space_clip_get_clip(sc);
    if (clip != NULL) {
      MovieTracking *tracking = &clip->tracking;
      MovieTrackingObject *tracking_object = BKE_tracking_object_get_active(tracking);
      if (tracking_object->flag & TRACKING_OBJECT_CAMERA) {
        return true;
      }
      else {
        return OBACT(view_layer) != NULL;
      }
    }
  }
  return false;
}

static int count_selected_bundles(bContext *C)
{
  SpaceClip *sc = CTX_wm_space_clip(C);
  MovieClip *clip = ED_space_clip_get_clip(sc);
  ListBase *tracksbase = BKE_tracking_get_active_tracks(&clip->tracking);
  int tot = 0;
  for (MovieTrackingTrack *track = tracksbase->first; track != NULL; track = track->next) {
    if (TRACK_VIEW_SELECTED(sc, track) && (track->flag & TRACK_HAS_BUNDLE)) {
      tot++;
    }
  }
  return tot;
}

static void object_solver_inverted_matrix(Scene *scene, Object *ob, float invmat[4][4])
{
  bool found = false;
  for (bConstraint *con = ob->constraints.first; con != NULL; con = con->next) {
    const bConstraintTypeInfo *cti = BKE_constraint_typeinfo_get(con);
    if (cti == NULL) {
      continue;
    }
    if (cti->type == CONSTRAINT_TYPE_OBJECTSOLVER) {
      bObjectSolverConstraint *data = (bObjectSolverConstraint *)con->data;
      if (!found) {
        Object *cam = data->camera ? data->camera : scene->camera;
        BKE_object_where_is_calc_mat4(cam, invmat);
      }
      mul_m4_m4m4(invmat, invmat, data->invmat);
      found = true;
    }
  }
  if (found) {
    invert_m4(invmat);
  }
  else {
    unit_m4(invmat);
  }
}

static Object *object_solver_camera(Scene *scene, Object *ob)
{
  for (bConstraint *con = ob->constraints.first; con != NULL; con = con->next) {
    const bConstraintTypeInfo *cti = BKE_constraint_typeinfo_get(con);
    if (cti == NULL) {
      continue;
    }
    if (cti->type == CONSTRAINT_TYPE_OBJECTSOLVER) {
      bObjectSolverConstraint *data = (bObjectSolverConstraint *)con->data;
      return (data->camera != NULL) ? data->camera : scene->camera;
    }
  }
  return NULL;
}

static int set_origin_exec(bContext *C, wmOperator *op)
{
  SpaceClip *sc = CTX_wm_space_clip(C);
  MovieClip *clip = ED_space_clip_get_clip(sc);
  MovieTracking *tracking = &clip->tracking;
  Scene *scene = CTX_data_scene(C);
  Object *camera = get_camera_with_movieclip(scene, clip);
  int selected_count = count_selected_bundles(C);

  if (selected_count == 0) {
    BKE_report(op->reports,
               RPT_ERROR,
               "At least one track with bundle should be selected to "
               "define origin position");

    return OPERATOR_CANCELLED;
  }

  Object *object = get_orientation_object(C);
  if (object == NULL) {
    BKE_report(op->reports, RPT_ERROR, "No object to apply orientation on");
    return OPERATOR_CANCELLED;
  }

  MovieTrackingObject *tracking_object = BKE_tracking_object_get_active(tracking);
  ListBase *tracksbase = BKE_tracking_object_get_tracks(tracking, tracking_object);

  float median[3] = {0.0f, 0.0f, 0.0f};
  zero_v3(median);
  for (MovieTrackingTrack *track = tracksbase->first; track != NULL; track = track->next) {
    if (TRACK_VIEW_SELECTED(sc, track) && (track->flag & TRACK_HAS_BUNDLE)) {
      add_v3_v3(median, track->bundle_pos);
    }
  }
  mul_v3_fl(median, 1.0f / selected_count);

  float mat[4][4], vec[3];
  BKE_tracking_get_camera_object_matrix(scene, camera, mat);
  mul_v3_m4v3(vec, mat, median);

  if (tracking_object->flag & TRACKING_OBJECT_CAMERA) {
    sub_v3_v3(object->loc, vec);
  }
  else {
    object_solver_inverted_matrix(scene, object, mat);
    mul_v3_m4v3(vec, mat, vec);
    copy_v3_v3(object->loc, vec);
  }

  DEG_id_tag_update(&clip->id, 0);
  DEG_id_tag_update(&object->id, ID_RECALC_TRANSFORM);

  WM_event_add_notifier(C, NC_MOVIECLIP | NA_EVALUATED, clip);
  WM_event_add_notifier(C, NC_OBJECT | ND_TRANSFORM, NULL);

  return OPERATOR_FINISHED;
}

void CLIP_OT_set_origin(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Set Origin";
  ot->description =
      "Set active marker as origin by moving camera (or its parent if present) in 3D space";
  ot->idname = "CLIP_OT_set_origin";

  /* api callbacks */
  ot->exec = set_origin_exec;
  ot->poll = set_orientation_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  RNA_def_boolean(
      ot->srna, "use_median", 0, "Use Median", "Set origin to median point of selected bundles");
}

/********************** set floor operator *********************/

static void set_axis(Scene *scene,
                     Object *ob,
                     MovieClip *clip,
                     MovieTrackingObject *tracking_object,
                     MovieTrackingTrack *track,
                     char axis)
{
  Object *camera = get_camera_with_movieclip(scene, clip);
  const bool is_camera = (tracking_object->flag & TRACKING_OBJECT_CAMERA) != 0;
  bool flip = false;
  float mat[4][4], vec[3], obmat[4][4], dvec[3];

  BKE_object_to_mat4(ob, obmat);

  BKE_tracking_get_camera_object_matrix(scene, camera, mat);
  mul_v3_m4v3(vec, mat, track->bundle_pos);
  copy_v3_v3(dvec, vec);

  if (!is_camera) {
    float imat[4][4];

    object_solver_inverted_matrix(scene, ob, imat);
    mul_v3_m4v3(vec, imat, vec);

    invert_m4_m4(imat, obmat);
    mul_v3_m4v3(dvec, imat, vec);

    sub_v3_v3(vec, obmat[3]);
  }

  if (len_squared_v2(vec) < (1e-3f * 1e-3f)) {
    return;
  }

  unit_m4(mat);

  if (axis == 'X') {
    if (fabsf(dvec[1]) < 1e-3f) {
      flip = true;

      mat[0][0] = -1.0f;
      mat[0][1] = 0.0f;
      mat[0][2] = 0.0f;
      mat[1][0] = 0.0f;
      mat[1][1] = -1.0f;
      mat[1][2] = 0.0f;
      mat[2][0] = 0.0f;
      mat[2][1] = 0.0f;
      mat[2][2] = 1.0f;
    }
    else {
      copy_v3_v3(mat[0], vec);

      if (is_camera || fabsf(vec[2]) < 1e-3f) {
        mat[0][2] = 0.0f;
        mat[2][0] = 0.0f;
        mat[2][1] = 0.0f;
        mat[2][2] = 1.0f;
        cross_v3_v3v3(mat[1], mat[2], mat[0]);
      }
      else {
        vec[2] = 0.0f;

        cross_v3_v3v3(mat[1], mat[0], vec);
        cross_v3_v3v3(mat[2], mat[0], mat[1]);
      }
    }
  }
  else {
    if (fabsf(dvec[0]) < 1e-3f) {
      flip = true;

      mat[0][0] = -1.0f;
      mat[0][1] = 0.0f;
      mat[0][2] = 0.0f;
      mat[1][0] = 0.0f;
      mat[1][1] = -1.0f;
      mat[1][2] = 0.0f;
      mat[2][0] = 0.0f;
      mat[2][1] = 0.0f;
      mat[2][2] = 1.0f;
    }
    else {
      copy_v3_v3(mat[1], vec);

      if (is_camera || fabsf(vec[2]) < 1e-3f) {
        mat[1][2] = 0.0f;
        mat[2][0] = 0.0f;
        mat[2][1] = 0.0f;
        mat[2][2] = 1.0f;
        cross_v3_v3v3(mat[0], mat[1], mat[2]);
      }
      else {
        vec[2] = 0.0f;

        cross_v3_v3v3(mat[0], vec, mat[1]);
        cross_v3_v3v3(mat[2], mat[0], mat[1]);
      }
    }
  }

  normalize_v3(mat[0]);
  normalize_v3(mat[1]);
  normalize_v3(mat[2]);

  if (is_camera) {
    invert_m4(mat);

    mul_m4_m4m4(mat, mat, obmat);
  }
  else {
    if (!flip) {
      float lmat[4][4], ilmat[4][4], rmat[3][3];

      BKE_object_rot_to_mat3(ob, rmat, true);
      invert_m3(rmat);
      mul_m4_m4m3(mat, mat, rmat);

      unit_m4(lmat);
      copy_v3_v3(lmat[3], obmat[3]);
      invert_m4_m4(ilmat, lmat);

      mul_m4_series(mat, lmat, mat, ilmat, obmat);
    }
    else {
      mul_m4_m4m4(mat, obmat, mat);
    }
  }

  BKE_object_apply_mat4(ob, mat, 0, 0);
}

static int set_plane_exec(bContext *C, wmOperator *op)
{
  SpaceClip *sc = CTX_wm_space_clip(C);
  MovieClip *clip = ED_space_clip_get_clip(sc);
  Scene *scene = CTX_data_scene(C);
  MovieTracking *tracking = &clip->tracking;
  MovieTrackingObject *tracking_object;
  MovieTrackingTrack *track, *axis_track = NULL, *act_track;
  ListBase *tracksbase;
  Object *object;
  Object *camera = get_camera_with_movieclip(scene, clip);
  int tot = 0;
  float vec[3][3], mat[4][4], obmat[4][4], newmat[4][4], orig[3] = {0.0f, 0.0f, 0.0f};
  int plane = RNA_enum_get(op->ptr, "plane");
  float rot[4][4] = {
      {0.0f, 0.0f, -1.0f, 0.0f},
      {0.0f, 1.0f, 0.0f, 0.0f},
      {1.0f, 0.0f, 0.0f, 0.0f},
      {0.0f, 0.0f, 0.0f, 1.0f},
  }; /* 90 degrees Y-axis rotation matrix */

  if (count_selected_bundles(C) != 3) {
    BKE_report(op->reports, RPT_ERROR, "Three tracks with bundles are needed to orient the floor");

    return OPERATOR_CANCELLED;
  }

  tracking_object = BKE_tracking_object_get_active(tracking);
  tracksbase = BKE_tracking_object_get_tracks(tracking, tracking_object);
  act_track = BKE_tracking_track_get_active(tracking);

  object = get_orientation_object(C);
  if (object == NULL) {
    BKE_report(op->reports, RPT_ERROR, "No object to apply orientation on");
    return OPERATOR_CANCELLED;
  }

  BKE_tracking_get_camera_object_matrix(scene, camera, mat);

  /* Get 3 bundles to use as reference. */
  track = tracksbase->first;
  while (track && tot < 3) {
    if (track->flag & TRACK_HAS_BUNDLE && TRACK_VIEW_SELECTED(sc, track)) {
      mul_v3_m4v3(vec[tot], mat, track->bundle_pos);
      if (tot == 0 || track == act_track) {
        copy_v3_v3(orig, vec[tot]);
      }
      else {
        axis_track = track;
      }
      tot++;
    }
    track = track->next;
  }

  sub_v3_v3(vec[1], vec[0]);
  sub_v3_v3(vec[2], vec[0]);

  /* Construct ortho-normal basis. */
  unit_m4(mat);
  if (plane == 0) { /* floor */
    cross_v3_v3v3(mat[0], vec[1], vec[2]);
    copy_v3_v3(mat[1], vec[1]);
    cross_v3_v3v3(mat[2], mat[0], mat[1]);
  }
  else if (plane == 1) { /* wall */
    cross_v3_v3v3(mat[2], vec[1], vec[2]);
    copy_v3_v3(mat[1], vec[1]);
    cross_v3_v3v3(mat[0], mat[1], mat[2]);
  }

  normalize_v3(mat[0]);
  normalize_v3(mat[1]);
  normalize_v3(mat[2]);

  /* Move to origin point. */
  mat[3][0] = orig[0];
  mat[3][1] = orig[1];
  mat[3][2] = orig[2];

  if (tracking_object->flag & TRACKING_OBJECT_CAMERA) {
    invert_m4(mat);

    BKE_object_to_mat4(object, obmat);
    mul_m4_m4m4(mat, mat, obmat);
    mul_m4_m4m4(newmat, rot, mat);
    BKE_object_apply_mat4(object, newmat, 0, 0);

    /* Make camera have positive z-coordinate. */
    if (object->loc[2] < 0) {
      invert_m4(rot);
      mul_m4_m4m4(newmat, rot, mat);
      BKE_object_apply_mat4(object, newmat, 0, 0);
    }
  }
  else {
    BKE_object_apply_mat4(object, mat, 0, 0);
  }

  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Scene *scene_eval = DEG_get_evaluated_scene(depsgraph);
  Object *object_eval = DEG_get_evaluated_object(depsgraph, object);
  BKE_object_transform_copy(object_eval, object);
  BKE_object_where_is_calc(depsgraph, scene_eval, object_eval);
  BKE_object_transform_copy(object, object_eval);

  set_axis(scene, object, clip, tracking_object, axis_track, 'X');

  DEG_id_tag_update(&clip->id, 0);
  DEG_id_tag_update(&object->id, ID_RECALC_TRANSFORM);

  WM_event_add_notifier(C, NC_MOVIECLIP | NA_EVALUATED, clip);
  WM_event_add_notifier(C, NC_OBJECT | ND_TRANSFORM, NULL);

  return OPERATOR_FINISHED;
}

void CLIP_OT_set_plane(wmOperatorType *ot)
{
  static const EnumPropertyItem plane_items[] = {
      {0, "FLOOR", 0, "Floor", "Set floor plane"},
      {1, "WALL", 0, "Wall", "Set wall plane"},
      {0, NULL, 0, NULL, NULL},
  };

  /* identifiers */
  ot->name = "Set Plane";
  ot->description =
      "Set plane based on 3 selected bundles by moving camera "
      "(or its parent if present) in 3D space";
  ot->idname = "CLIP_OT_set_plane";

  /* api callbacks */
  ot->exec = set_plane_exec;
  ot->poll = set_orientation_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  RNA_def_enum(ot->srna, "plane", plane_items, 0, "Plane", "Plane to be used for orientation");
}

/********************** set axis operator *********************/

static int set_axis_exec(bContext *C, wmOperator *op)
{
  SpaceClip *sc = CTX_wm_space_clip(C);
  MovieClip *clip = ED_space_clip_get_clip(sc);
  MovieTracking *tracking = &clip->tracking;
  MovieTrackingObject *tracking_object = BKE_tracking_object_get_active(tracking);
  Scene *scene = CTX_data_scene(C);
  Object *object;
  int axis = RNA_enum_get(op->ptr, "axis");

  if (count_selected_bundles(C) != 1) {
    BKE_report(
        op->reports, RPT_ERROR, "Single track with bundle should be selected to define axis");
    return OPERATOR_CANCELLED;
  }

  object = get_orientation_object(C);
  if (object == NULL) {
    BKE_report(op->reports, RPT_ERROR, "No object to apply orientation on");
    return OPERATOR_CANCELLED;
  }

  ListBase *tracksbase = BKE_tracking_object_get_tracks(tracking, tracking_object);
  MovieTrackingTrack *track = tracksbase->first;
  while (track) {
    if (TRACK_VIEW_SELECTED(sc, track) && (track->flag & TRACK_HAS_BUNDLE)) {
      break;
    }
    track = track->next;
  }

  set_axis(scene, object, clip, tracking_object, track, axis == 0 ? 'X' : 'Y');

  DEG_id_tag_update(&clip->id, 0);
  DEG_id_tag_update(&object->id, ID_RECALC_TRANSFORM);

  WM_event_add_notifier(C, NC_MOVIECLIP | NA_EVALUATED, clip);
  WM_event_add_notifier(C, NC_OBJECT | ND_TRANSFORM, NULL);

  return OPERATOR_FINISHED;
}

void CLIP_OT_set_axis(wmOperatorType *ot)
{
  static const EnumPropertyItem axis_actions[] = {
      {0, "X", 0, "X", "Align bundle align X axis"},
      {1, "Y", 0, "Y", "Align bundle align Y axis"},
      {0, NULL, 0, NULL, NULL},
  };

  /* identifiers */
  ot->name = "Set Axis";
  ot->description =
      "Set direction of scene axis rotating camera "
      "(or its parent if present) and assume selected track "
      "lies on real axis, joining it with the origin";
  ot->idname = "CLIP_OT_set_axis";

  /* api callbacks */
  ot->exec = set_axis_exec;
  ot->poll = set_orientation_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  RNA_def_enum(ot->srna, "axis", axis_actions, 0, "Axis", "Axis to use to align bundle along");
}

/********************** set scale operator *********************/

static int do_set_scale(bContext *C, wmOperator *op, bool scale_solution, bool apply_scale)
{
  SpaceClip *sc = CTX_wm_space_clip(C);
  MovieClip *clip = ED_space_clip_get_clip(sc);
  MovieTracking *tracking = &clip->tracking;
  MovieTrackingObject *tracking_object = BKE_tracking_object_get_active(tracking);
  MovieTrackingTrack *track;
  Scene *scene = CTX_data_scene(C);
  Object *object = NULL;
  Object *camera = get_camera_with_movieclip(scene, clip);
  ListBase *tracksbase = BKE_tracking_get_active_tracks(tracking);
  int tot = 0;
  float vec[2][3], mat[4][4], scale;
  float dist = RNA_float_get(op->ptr, "distance");

  if (count_selected_bundles(C) != 2) {
    BKE_report(op->reports, RPT_ERROR, "Two tracks with bundles should be selected to set scale");
    return OPERATOR_CANCELLED;
  }

  if (!scale_solution && !apply_scale) {
    object = get_orientation_object(C);
    if (object == NULL) {
      BKE_report(op->reports, RPT_ERROR, "No object to apply orientation on");
      return OPERATOR_CANCELLED;
    }
  }

  BKE_tracking_get_camera_object_matrix(scene, camera, mat);

  track = tracksbase->first;
  while (track) {
    if (TRACK_VIEW_SELECTED(sc, track)) {
      mul_v3_m4v3(vec[tot], mat, track->bundle_pos);
      tot++;
    }
    track = track->next;
  }

  sub_v3_v3(vec[0], vec[1]);

  if (len_v3(vec[0]) > 1e-5f) {
    scale = dist / len_v3(vec[0]);
    if (apply_scale) {
      /* Apply scale on reconstructed scene itself. */
      MovieTrackingReconstruction *reconstruction = BKE_tracking_get_active_reconstruction(
          tracking);
      MovieReconstructedCamera *reconstructed_cameras;
      int i;

      for (track = tracksbase->first; track; track = track->next) {
        mul_v3_fl(track->bundle_pos, scale);
      }

      reconstructed_cameras = reconstruction->cameras;
      for (i = 0; i < reconstruction->camnr; i++) {
        mul_v3_fl(reconstructed_cameras[i].mat[3], scale);
      }

      WM_event_add_notifier(C, NC_MOVIECLIP | NA_EVALUATED, clip);
      WM_event_add_notifier(C, NC_OBJECT | ND_TRANSFORM, NULL);
    }
    else {
      if (tracking_object->flag & TRACKING_OBJECT_CAMERA) {
        mul_v3_fl(object->scale, scale);
        mul_v3_fl(object->loc, scale);
      }
      else if (!scale_solution) {
        Object *solver_camera = object_solver_camera(scene, object);

        object->scale[0] = object->scale[1] = object->scale[2] = 1.0f / scale;

        if (solver_camera) {
          object->scale[0] /= solver_camera->scale[0];
          object->scale[1] /= solver_camera->scale[1];
          object->scale[2] /= solver_camera->scale[2];
        }
      }
      else {
        tracking_object->scale = scale;
      }

      DEG_id_tag_update(&clip->id, 0);

      if (object) {
        DEG_id_tag_update(&object->id, ID_RECALC_TRANSFORM);
      }

      WM_event_add_notifier(C, NC_MOVIECLIP | NA_EVALUATED, clip);
      WM_event_add_notifier(C, NC_OBJECT | ND_TRANSFORM, NULL);
    }
  }

  return OPERATOR_FINISHED;
}

static int set_scale_exec(bContext *C, wmOperator *op)
{
  return do_set_scale(C, op, false, false);
}

static int set_scale_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
  SpaceClip *sc = CTX_wm_space_clip(C);
  MovieClip *clip = ED_space_clip_get_clip(sc);

  if (!RNA_struct_property_is_set(op->ptr, "distance")) {
    RNA_float_set(op->ptr, "distance", clip->tracking.settings.dist);
  }

  return set_scale_exec(C, op);
}

void CLIP_OT_set_scale(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Set Scale";
  ot->description = "Set scale of scene by scaling camera (or its parent if present)";
  ot->idname = "CLIP_OT_set_scale";

  /* api callbacks */
  ot->exec = set_scale_exec;
  ot->invoke = set_scale_invoke;
  ot->poll = set_orientation_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  RNA_def_float(ot->srna,
                "distance",
                0.0f,
                -FLT_MAX,
                FLT_MAX,
                "Distance",
                "Distance between selected tracks",
                -100.0f,
                100.0f);
}

/********************** set solution scale operator *********************/

static bool set_solution_scale_poll(bContext *C)
{
  SpaceClip *sc = CTX_wm_space_clip(C);
  if (sc != NULL) {
    MovieClip *clip = ED_space_clip_get_clip(sc);
    if (clip != NULL) {
      MovieTracking *tracking = &clip->tracking;
      MovieTrackingObject *tracking_object = BKE_tracking_object_get_active(tracking);
      return (tracking_object->flag & TRACKING_OBJECT_CAMERA) == 0;
    }
  }
  return false;
}

static int set_solution_scale_exec(bContext *C, wmOperator *op)
{
  return do_set_scale(C, op, true, false);
}

static int set_solution_scale_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
  SpaceClip *sc = CTX_wm_space_clip(C);
  MovieClip *clip = ED_space_clip_get_clip(sc);

  if (!RNA_struct_property_is_set(op->ptr, "distance")) {
    RNA_float_set(op->ptr, "distance", clip->tracking.settings.object_distance);
  }

  return set_solution_scale_exec(C, op);
}

void CLIP_OT_set_solution_scale(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Set Solution Scale";
  ot->description =
      "Set object solution scale using distance between "
      "two selected tracks";
  ot->idname = "CLIP_OT_set_solution_scale";

  /* api callbacks */
  ot->exec = set_solution_scale_exec;
  ot->invoke = set_solution_scale_invoke;
  ot->poll = set_solution_scale_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  RNA_def_float(ot->srna,
                "distance",
                0.0f,
                -FLT_MAX,
                FLT_MAX,
                "Distance",
                "Distance between selected tracks",
                -100.0f,
                100.0f);
}

/********************** apply solution scale operator *********************/

static bool apply_solution_scale_poll(bContext *C)
{
  SpaceClip *sc = CTX_wm_space_clip(C);
  if (sc != NULL) {
    MovieClip *clip = ED_space_clip_get_clip(sc);
    if (clip != NULL) {
      MovieTracking *tracking = &clip->tracking;
      MovieTrackingObject *tracking_object = BKE_tracking_object_get_active(tracking);
      return (tracking_object->flag & TRACKING_OBJECT_CAMERA) != 0;
    }
  }
  return 0;
}

static int apply_solution_scale_exec(bContext *C, wmOperator *op)
{
  return do_set_scale(C, op, false, true);
}

static int apply_solution_scale_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
  SpaceClip *sc = CTX_wm_space_clip(C);
  MovieClip *clip = ED_space_clip_get_clip(sc);
  if (!RNA_struct_property_is_set(op->ptr, "distance")) {
    RNA_float_set(op->ptr, "distance", clip->tracking.settings.dist);
  }
  return apply_solution_scale_exec(C, op);
}

void CLIP_OT_apply_solution_scale(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Apply Solution Scale";
  ot->description =
      "Apply scale on solution itself to make distance between "
      "selected tracks equals to desired";
  ot->idname = "CLIP_OT_apply_solution_scale";

  /* api callbacks */
  ot->exec = apply_solution_scale_exec;
  ot->invoke = apply_solution_scale_invoke;
  ot->poll = apply_solution_scale_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  RNA_def_float(ot->srna,
                "distance",
                0.0f,
                -FLT_MAX,
                FLT_MAX,
                "Distance",
                "Distance between selected tracks",
                -100.0f,
                100.0f);
}
