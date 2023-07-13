/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spview3d
 *
 * The purpose of View3DCameraControl is to allow editing \a rv3d manipulation
 * (mainly \a ofs and \a viewquat) for the purpose of view navigation
 * without having to worry about positioning the camera, its parent...
 * or other details.
 * Typical view-control usage:
 *
 * - Acquire a view-control (#ED_view3d_cameracontrol_acquire).
 * - Modify `rv3d->ofs`, `rv3d->viewquat`.
 * - Update the view data (#ED_view3d_cameracontrol_acquire) -
 *   within a loop which draws the viewport.
 * - Finish and release the view-control (#ED_view3d_cameracontrol_release),
 *   either keeping the current view or restoring the initial view.
 *
 * Notes:
 *
 * - when acquiring `rv3d->dist` is set to zero
 *   (so `rv3d->ofs` is always the view-point)
 * - updating can optionally keyframe the camera object.
 */

#include "DNA_camera_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "BKE_object.h"

#include "DEG_depsgraph.h"

#include "view3d_intern.h" /* own include */

struct View3DCameraControl {

  /* -------------------------------------------------------------------- */
  /* Context (assign these to vars before use) */
  Scene *ctx_scene;
  View3D *ctx_v3d;
  RegionView3D *ctx_rv3d;

  /* -------------------------------------------------------------------- */
  /* internal vars */

  /* for parenting calculation */
  float view_mat_prev[4][4];

  /* -------------------------------------------------------------------- */
  /* optional capabilities */

  bool use_parent_root;

  /* -------------------------------------------------------------------- */
  /* initial values */

  /* root most parent */
  Object *root_parent;

  /* backup values */
  float dist_backup;
  /* backup the views distance since we use a zero dist for fly mode */
  float ofs_backup[3];
  /* backup the views offset in case the user cancels flying in non camera mode */

  /* backup the views quat in case the user cancels flying in non camera mode. */
  float rot_backup[4];
  /* remember if we're ortho or not, only used for restoring the view if it was a ortho view */
  char persp_backup;

  /* are we flying an ortho camera in perspective view,
   * which was originally in ortho view?
   * could probably figure it out but better be explicit */
  bool is_ortho_cam;

  /* backup the objects transform */
  void *obtfm;
};

BLI_INLINE Object *view3d_cameracontrol_object(const View3DCameraControl *vctrl)
{
  return vctrl->root_parent ? vctrl->root_parent : vctrl->ctx_v3d->camera;
}

Object *ED_view3d_cameracontrol_object_get(View3DCameraControl *vctrl)
{
  RegionView3D *rv3d = vctrl->ctx_rv3d;

  if (rv3d->persp == RV3D_CAMOB) {
    return view3d_cameracontrol_object(vctrl);
  }

  return nullptr;
}

View3DCameraControl *ED_view3d_cameracontrol_acquire(Depsgraph *depsgraph,
                                                     Scene *scene,
                                                     View3D *v3d,
                                                     RegionView3D *rv3d)
{
  View3DCameraControl *vctrl;

  vctrl = static_cast<View3DCameraControl *>(MEM_callocN(sizeof(View3DCameraControl), __func__));

  /* Store context */
  vctrl->ctx_scene = scene;
  vctrl->ctx_v3d = v3d;
  vctrl->ctx_rv3d = rv3d;

  vctrl->use_parent_root = v3d->camera != nullptr &&
                           v3d->camera->transflag & OB_TRANSFORM_ADJUST_ROOT_PARENT_FOR_VIEW_LOCK;

  vctrl->persp_backup = rv3d->persp;
  vctrl->dist_backup = rv3d->dist;

  /* check for flying ortho camera - which we can't support well
   * we _could_ also check for an ortho camera but this is easier */
  if ((rv3d->persp == RV3D_CAMOB) && (rv3d->is_persp == false)) {
    ((Camera *)v3d->camera->data)->type = CAM_PERSP;
    vctrl->is_ortho_cam = true;
  }

  if (rv3d->persp == RV3D_CAMOB) {
    Object *ob_back;
    if (vctrl->use_parent_root && (vctrl->root_parent = v3d->camera->parent)) {
      while (vctrl->root_parent->parent) {
        vctrl->root_parent = vctrl->root_parent->parent;
      }
      ob_back = vctrl->root_parent;
    }
    else {
      ob_back = v3d->camera;
    }

    /* store the original camera loc and rot */
    vctrl->obtfm = BKE_object_tfm_backup(ob_back);

    BKE_object_where_is_calc(depsgraph, scene, v3d->camera);
    negate_v3_v3(rv3d->ofs, v3d->camera->object_to_world[3]);

    rv3d->dist = 0.0;
  }
  else {
    /* perspective or ortho */
    if (rv3d->persp == RV3D_ORTHO) {
      /* if ortho projection, make perspective */
      rv3d->persp = RV3D_PERSP;
    }

    copy_qt_qt(vctrl->rot_backup, rv3d->viewquat);
    copy_v3_v3(vctrl->ofs_backup, rv3d->ofs);

    /* The dist defines a vector that is in front of the offset
     * to rotate the view about.
     * this is no good for fly mode because we
     * want to rotate about the viewers center.
     * but to correct the dist removal we must
     * alter offset so the view doesn't jump. */

    ED_view3d_distance_set(rv3d, 0.0f);
    /* Done with correcting for the dist */
  }

  ED_view3d_to_m4(vctrl->view_mat_prev, rv3d->ofs, rv3d->viewquat, rv3d->dist);

  return vctrl;
}

/**
 * A version of #BKE_object_apply_mat4 that respects #Object.protectflag,
 * applying the locking back to the view to avoid the view.
 * This is needed so the view doesn't get out of sync with the object,
 * causing visible jittering when in fly/walk mode for e.g.
 *
 * \note This could be exposed as an API option, as we might not want the view
 * to be constrained by the thing it's controlling.
 */
static bool object_apply_mat4_with_protect(
    Object *ob,
    const float obmat[4][4],
    const bool use_parent, /* Only use when applying lock. */
    RegionView3D *rv3d,
    const float view_mat[4][4])
{
  const bool use_protect = (ob->protectflag != 0);
  bool view_changed = false;

  ObjectTfmProtectedChannels obtfm;
  if (use_protect) {
    BKE_object_tfm_protected_backup(ob, &obtfm);
  }

  BKE_object_apply_mat4(ob, obmat, true, use_parent);

  if (use_protect) {
    float obmat_noprotect[4][4], obmat_protect[4][4];

    BKE_object_to_mat4(ob, obmat_noprotect);
    BKE_object_tfm_protected_restore(ob, &obtfm, ob->protectflag);
    BKE_object_to_mat4(ob, obmat_protect);

    if (!equals_m4m4(obmat_noprotect, obmat_protect)) {
      /* Apply the lock protection back to the view, without this the view
       * keeps moving, ignoring the object locking, causing jittering in some cases. */
      float diff_mat[4][4];
      float view_mat_protect[4][4];
      float obmat_noprotect_inv[4][4];
      invert_m4_m4(obmat_noprotect_inv, obmat_noprotect);
      mul_m4_m4m4(diff_mat, obmat_protect, obmat_noprotect_inv);

      mul_m4_m4m4(view_mat_protect, diff_mat, view_mat);
      ED_view3d_from_m4(view_mat_protect, rv3d->ofs, rv3d->viewquat, &rv3d->dist);
      view_changed = true;
    }
  }
  return view_changed;
}

void ED_view3d_cameracontrol_update(View3DCameraControl *vctrl, /* args for keyframing */
                                    const bool use_autokey,
                                    struct bContext *C,
                                    const bool do_rotate,
                                    const bool do_translate)
{
  /* We are in camera view so apply the view offset and rotation to the view matrix
   * and set the camera to the view. */

  Scene *scene = vctrl->ctx_scene;
  View3D *v3d = vctrl->ctx_v3d;
  RegionView3D *rv3d = vctrl->ctx_rv3d;

  ID *id_key;

  float view_mat[4][4];
  ED_view3d_to_m4(view_mat, rv3d->ofs, rv3d->viewquat, rv3d->dist);

  /* transform the parent or the camera? */
  if (vctrl->root_parent) {
    Object *ob_update;

    float prev_view_imat[4][4];
    float diff_mat[4][4];
    float parent_mat[4][4];

    invert_m4_m4(prev_view_imat, vctrl->view_mat_prev);
    mul_m4_m4m4(diff_mat, view_mat, prev_view_imat);
    mul_m4_m4m4(parent_mat, diff_mat, vctrl->root_parent->object_to_world);

    if (object_apply_mat4_with_protect(vctrl->root_parent, parent_mat, false, rv3d, view_mat)) {
      /* Calculate again since the view locking changes the matrix. */
      ED_view3d_to_m4(view_mat, rv3d->ofs, rv3d->viewquat, rv3d->dist);
    }

    ob_update = v3d->camera->parent;
    while (ob_update) {
      DEG_id_tag_update(&ob_update->id, ID_RECALC_TRANSFORM);
      ob_update = ob_update->parent;
    }

    copy_m4_m4(vctrl->view_mat_prev, view_mat);

    id_key = &vctrl->root_parent->id;
  }
  else {
    float scale_mat[4][4];
    float scale_back[3];

    /* even though we handle the scale matrix, this still changes over time */
    copy_v3_v3(scale_back, v3d->camera->scale);

    size_to_mat4(scale_mat, v3d->camera->scale);
    mul_m4_m4m4(view_mat, view_mat, scale_mat);

    object_apply_mat4_with_protect(v3d->camera, view_mat, true, rv3d, view_mat);

    DEG_id_tag_update(&v3d->camera->id, ID_RECALC_TRANSFORM);

    copy_v3_v3(v3d->camera->scale, scale_back);

    id_key = &v3d->camera->id;
  }

  /* record the motion */
  if (use_autokey) {
    ED_view3d_camera_autokey(scene, id_key, C, do_rotate, do_translate);
  }
}

void ED_view3d_cameracontrol_release(View3DCameraControl *vctrl, const bool restore)
{
  View3D *v3d = vctrl->ctx_v3d;
  RegionView3D *rv3d = vctrl->ctx_rv3d;

  if (restore) {
    /* Revert to original view? */
    if (vctrl->persp_backup == RV3D_CAMOB) { /* a camera view */
      Object *ob_back = view3d_cameracontrol_object(vctrl);

      /* store the original camera loc and rot */
      BKE_object_tfm_restore(ob_back, vctrl->obtfm);

      DEG_id_tag_update(&ob_back->id, ID_RECALC_TRANSFORM);
    }
    else {
      /* Non Camera we need to reset the view back
       * to the original location because the user canceled. */
      copy_qt_qt(rv3d->viewquat, vctrl->rot_backup);
      rv3d->persp = vctrl->persp_backup;
    }
    /* always, is set to zero otherwise */
    copy_v3_v3(rv3d->ofs, vctrl->ofs_backup);
    rv3d->dist = vctrl->dist_backup;
  }
  else if (vctrl->persp_backup == RV3D_CAMOB) { /* camera */
    DEG_id_tag_update((ID *)view3d_cameracontrol_object(vctrl), ID_RECALC_TRANSFORM);

    /* always, is set to zero otherwise */
    copy_v3_v3(rv3d->ofs, vctrl->ofs_backup);
    rv3d->dist = vctrl->dist_backup;
  }
  else { /* not camera */
    /* Apply the fly mode view */
    /* restore the dist */
    ED_view3d_distance_set(rv3d, vctrl->dist_backup);
    /* Done with correcting for the dist */
  }

  if (vctrl->is_ortho_cam) {
    ((Camera *)v3d->camera->data)->type = CAM_ORTHO;
  }

  if (vctrl->obtfm) {
    MEM_freeN(vctrl->obtfm);
  }

  MEM_freeN(vctrl);
}
