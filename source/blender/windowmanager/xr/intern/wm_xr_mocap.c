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
 * \ingroup wm
 *
 * \name Window-Manager XR Motion Capture
 *
 * API for XR motion capture objects.
 */

#include "BKE_context.h"
#include "BKE_layer.h"

#include "BLI_listbase.h"
#include "BLI_math.h"

#include "DEG_depsgraph.h"

#include "DNA_object_types.h"

#include "ED_keyframing.h"
#include "ED_object.h"
#include "ED_screen.h"
#include "ED_transform.h"

#include "GHOST_C-api.h"

#include "MEM_guardedalloc.h"

#include "WM_api.h"

#include "wm_xr_intern.h"

/* -------------------------------------------------------------------- */
/** \name XR Motion Capture Objects
 *
 * List of XR motion capture objects. Stored in session settings and can be written to files.
 * \{ */

XrMotionCaptureObject *WM_xr_mocap_object_new(XrSessionSettings *settings, Object *ob)
{
  XrMotionCaptureObject *mocap_ob = WM_xr_mocap_object_find(settings, ob);

  if (!mocap_ob) {
    mocap_ob = MEM_callocN(sizeof(XrMotionCaptureObject), __func__);
    mocap_ob->ob = ob;
    BLI_addtail(&settings->mocap_objects, mocap_ob);
  }

  return mocap_ob;
}

void WM_xr_mocap_object_ensure_unique(XrSessionSettings *settings, XrMotionCaptureObject *mocap_ob)
{
  LISTBASE_FOREACH (XrMotionCaptureObject *, mocap_ob_other, &settings->mocap_objects) {
    if ((mocap_ob_other != mocap_ob) && (mocap_ob_other->ob == mocap_ob->ob)) {
      mocap_ob->ob = NULL;
      return;
    }
  }
}

void WM_xr_mocap_object_remove(XrSessionSettings *settings, XrMotionCaptureObject *mocap_ob)
{
  int idx = BLI_findindex(&settings->mocap_objects, mocap_ob);

  if (idx != -1) {
    BLI_freelinkN(&settings->mocap_objects, mocap_ob);

    if (idx <= settings->sel_mocap_object) {
      if (--settings->sel_mocap_object < 0) {
        settings->sel_mocap_object = 0;
      }
    }
  }
}

XrMotionCaptureObject *WM_xr_mocap_object_find(const XrSessionSettings *settings, const Object *ob)
{
  return ob ? BLI_findptr(&settings->mocap_objects, ob, offsetof(XrMotionCaptureObject, ob)) :
              NULL;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Motion Capture Runtime
 *
 * Runtime functions for motion capture object poses and autokeying.
 * \{ */

static void wm_xr_mocap_object_pose_get(const XrMotionCaptureObject *mocap_ob, GHOST_XrPose *pose)
{
  BLI_assert(mocap_ob->ob);
  copy_v3_v3(pose->position, mocap_ob->ob->loc);
  eul_to_quat(pose->orientation_quat, mocap_ob->ob->rot);
}

static void wm_xr_mocap_object_pose_set(const GHOST_XrPose *pose,
                                        XrMotionCaptureObject *mocap_ob,
                                        bool apply_offset)
{
  BLI_assert(mocap_ob->ob);

  if (apply_offset) {
    /* Convert offsets to pose (device) space. */
    float loc_ofs[3], rot_ofs[4];
    copy_v3_v3(loc_ofs, mocap_ob->location_offset);
    mul_qt_v3(pose->orientation_quat, loc_ofs);

    eul_to_quat(rot_ofs, mocap_ob->rotation_offset);
    normalize_qt(rot_ofs);
    invert_qt_normalized(rot_ofs);
    mul_qt_qtqt(rot_ofs, pose->orientation_quat, rot_ofs);
    normalize_qt(rot_ofs);

    add_v3_v3v3(mocap_ob->ob->loc, pose->position, loc_ofs);
    quat_to_eul(mocap_ob->ob->rot, rot_ofs);
  }
  else {
    copy_v3_v3(mocap_ob->ob->loc, pose->position);
    quat_to_eul(mocap_ob->ob->rot, pose->orientation_quat);
  }

  DEG_id_tag_update(&mocap_ob->ob->id, ID_RECALC_TRANSFORM);
}

void wm_xr_mocap_orig_poses_store(const XrSessionSettings *settings, wmXrSessionState *state)
{
  ListBase *mocap_orig_poses = &state->mocap_orig_poses;

  LISTBASE_FOREACH (XrMotionCaptureObject *, mocap_ob, &settings->mocap_objects) {
    wmXrMotionCapturePose *mocap_pose = MEM_callocN(sizeof(wmXrMotionCapturePose), __func__);
    mocap_pose->ob = mocap_ob->ob;
    if (mocap_ob->ob) {
      wm_xr_mocap_object_pose_get(mocap_ob, &mocap_pose->pose);
    }
    BLI_addtail(mocap_orig_poses, mocap_pose);
  }
}

void wm_xr_mocap_orig_poses_restore(const wmXrSessionState *state, XrSessionSettings *settings)
{
  ListBase *mocap_objects = &settings->mocap_objects;

  LISTBASE_FOREACH (wmXrMotionCapturePose *, mocap_pose, &state->mocap_orig_poses) {
    XrMotionCaptureObject *mocap_ob = BLI_findptr(
        mocap_objects, mocap_pose->ob, offsetof(XrMotionCaptureObject, ob));
    if (mocap_ob && mocap_ob->ob) {
      wm_xr_mocap_object_pose_set(&mocap_pose->pose, mocap_ob, false);
    }
  }
}

void wm_xr_mocap_object_autokey(
    bContext *C, Scene *scene, ViewLayer *view_layer, wmWindow *win, Object *ob, bool notify)
{
  /* Poll functions in keyingsets_utils.py require an active window and object. */
  wmWindow *win_prev = win ? CTX_wm_window(C) : NULL;
  if (win) {
    CTX_wm_window_set(C, win);
  }

  Object *obact = CTX_data_active_object(C);
  if (!obact) {
    Base *base = BKE_view_layer_base_find(view_layer, ob);
    if (base) {
      ED_object_base_select(base, BA_SELECT);
      ED_object_base_activate(C, base);
    }
  }

  bScreen *screen = CTX_wm_screen(C);
  if (screen && screen->animtimer && (IS_AUTOKEY_FLAG(scene, INSERTAVAIL) == 0) &&
      ((scene->toolsettings->autokey_flag & ANIMRECORD_FLAG_WITHNLA) != 0)) {
    ED_transform_animrecord_check_state(scene, screen->animtimer, ob);
  }

  ED_transform_autokeyframe_object(C, scene, view_layer, ob, TFM_TRANSLATION);
  if (IS_AUTOKEY_FLAG(scene, INSERTNEEDED)) {
    ED_transform_autokeyframe_object(C, scene, view_layer, ob, TFM_ROTATION);
  }

  if (ED_transform_motionpath_need_update_object(scene, ob)) {
    ListBase lb = {NULL, NULL};
    BLI_addtail(&lb, BLI_genericNodeN(ob));

    ED_objects_recalculate_paths(C, scene, OBJECT_PATH_CALC_RANGE_CURRENT_FRAME, &lb);

    BLI_freelistN(&lb);
  }

  if (notify) {
    WM_event_add_notifier(C, NC_OBJECT | ND_KEYS, NULL);
    WM_main_add_notifier(NC_ANIMATION | ND_KEYFRAME | NA_EDITED, NULL);
  }

  if (win) {
    CTX_wm_window_set(C, win_prev);
  }
}

void wm_xr_mocap_objects_update(const char *user_path,
                                const GHOST_XrPose *pose,
                                bContext *C,
                                XrSessionSettings *settings,
                                Scene *scene,
                                ViewLayer *view_layer,
                                wmWindow *win,
                                bScreen *screen_anim,
                                bool *notify)
{
  LISTBASE_FOREACH (XrMotionCaptureObject *, mocap_ob, &settings->mocap_objects) {
    if (mocap_ob->ob && ((mocap_ob->flag & XR_MOCAP_OBJECT_ENABLE) != 0) &&
        STREQ(mocap_ob->user_path, user_path)) {
      wm_xr_mocap_object_pose_set(pose, mocap_ob, true);

      if (((mocap_ob->flag & XR_MOCAP_OBJECT_AUTOKEY) != 0) && screen_anim &&
          autokeyframe_cfra_can_key(scene, &mocap_ob->ob->id)) {
        wm_xr_mocap_object_autokey(C, scene, view_layer, win, mocap_ob->ob, *notify);
        *notify = false;
      }
    }
  }
}

bool WM_xr_session_state_mocap_pose_get(const wmXrData *xr, XrMotionCaptureObject *mocap_ob)
{
  if (!WM_xr_session_exists(xr)) {
    return false;
  }

  if (mocap_ob->ob) {
    wmXrMotionCapturePose *mocap_pose = BLI_findptr(&xr->runtime->session_state.mocap_orig_poses,
                                                    mocap_ob->ob,
                                                    offsetof(wmXrMotionCapturePose, ob));
    if (!mocap_pose) {
      return false;
    }
    wm_xr_mocap_object_pose_set(&mocap_pose->pose, mocap_ob, false);
  }

  return true;
}

void WM_xr_session_state_mocap_pose_set(wmXrData *xr, const XrMotionCaptureObject *mocap_ob)
{
  if (WM_xr_session_exists(xr) && mocap_ob->ob) {
    wmXrMotionCapturePose *mocap_pose = BLI_findptr(&xr->runtime->session_state.mocap_orig_poses,
                                                    mocap_ob->ob,
                                                    offsetof(wmXrMotionCapturePose, ob));
    if (mocap_pose) {
      wm_xr_mocap_object_pose_get(mocap_ob, &mocap_pose->pose);
    }
  }
}

/** \} */
