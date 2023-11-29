/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup animrig
 */

#include "BKE_animsys.h"
#include "BKE_context.hh"
#include "BKE_fcurve.h"
#include "BKE_layer.h"
#include "BKE_object.hh"
#include "BKE_scene.h"

#include "BLI_listbase.h"
#include "BLI_string.h"

#include "DNA_scene_types.h"

#include "RNA_path.hh"
#include "RNA_prototypes.h"

#include "ED_keyframing.hh"
#include "ED_scene.hh"
#include "ED_transform.hh"

#include "ANIM_keyframing.hh"

#include "WM_api.hh"
#include "WM_types.hh"

namespace blender::animrig {

bool is_autokey_on(const Scene *scene)
{
  if (scene) {
    return scene->toolsettings->autokey_mode & AUTOKEY_ON;
  }
  return U.autokey_mode & AUTOKEY_ON;
}

bool is_autokey_mode(const Scene *scene, const eAutokey_Mode mode)
{
  if (scene) {
    return scene->toolsettings->autokey_mode == mode;
  }
  return U.autokey_mode == mode;
}

bool is_autokey_flag(const Scene *scene, const eKeyInsert_Flag flag)
{
  if (scene) {
    return (scene->toolsettings->autokey_flag & flag) || (U.autokey_flag & flag);
  }
  return U.autokey_flag & flag;
}

bool autokeyframe_cfra_can_key(const Scene *scene, ID *id)
{
  /* Only filter if auto-key mode requires this. */
  if (!is_autokey_on(scene)) {
    return false;
  }

  if (is_autokey_mode(scene, AUTOKEY_MODE_EDITKEYS)) {
    /* Replace Mode:
     * For whole block, only key if there's a keyframe on that frame already
     * This is a valid assumption when we're blocking + tweaking
     */
    const float cfra = BKE_scene_frame_get(scene);
    return id_frame_has_keyframe(id, cfra);
  }

  /* Normal Mode (or treat as being normal mode):
   *
   * Just in case the flags aren't set properly (i.e. only on/off is set, without a mode)
   * let's set the "normal" flag too, so that it will all be sane everywhere...
   */
  scene->toolsettings->autokey_mode = AUTOKEY_MODE_NORMAL;

  return true;
}

void autokeyframe_object(
    bContext *C, Scene *scene, ViewLayer *view_layer, Object *ob, const eTfmMode tmode)
{
  /* TODO: this should probably be done per channel instead. */
  ID *id = &ob->id;
  if (!autokeyframe_cfra_can_key(scene, id)) {
    return;
  }

  ReportList *reports = CTX_wm_reports(C);
  KeyingSet *active_ks = ANIM_scene_get_active_keyingset(scene);
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  const AnimationEvalContext anim_eval_context = BKE_animsys_eval_context_construct(
      depsgraph, BKE_scene_frame_get(scene));
  eInsertKeyFlags flag = eInsertKeyFlags(0);

  /* Get flags used for inserting keyframes. */
  flag = ANIM_get_keyframing_flags(scene, true);

  /* Add data-source override for the object. */
  blender::Vector<PointerRNA> sources;
  ANIM_relative_keyingset_add_source(sources, id);

  if (is_autokey_flag(scene, AUTOKEY_FLAG_ONLYKEYINGSET) && (active_ks)) {
    /* Only insert into active keyingset
     * NOTE: we assume here that the active Keying Set
     * does not need to have its iterator overridden.
     */
    ANIM_apply_keyingset(
        C, &sources, active_ks, MODIFYKEY_MODE_INSERT, anim_eval_context.eval_time);
  }

  else if (is_autokey_flag(scene, AUTOKEY_FLAG_INSERTAVAILABLE)) {
    /* Only key on available channels. */
    AnimData *adt = ob->adt;
    ToolSettings *ts = scene->toolsettings;
    Main *bmain = CTX_data_main(C);

    if (adt && adt->action) {
      LISTBASE_FOREACH (FCurve *, fcu, &adt->action->curves) {
        insert_keyframe(bmain,
                        reports,
                        id,
                        adt->action,
                        (fcu->grp ? fcu->grp->name : nullptr),
                        fcu->rna_path,
                        fcu->array_index,
                        &anim_eval_context,
                        eBezTriple_KeyframeType(ts->keyframe_type),
                        flag);
      }
    }
  }

  else if (is_autokey_flag(scene, AUTOKEY_FLAG_INSERTNEEDED)) {
    bool do_loc = false, do_rot = false, do_scale = false;

    /* Filter the conditions when this happens (assume that curarea->spacetype==SPACE_VIE3D). */
    if (tmode == TFM_TRANSLATION) {
      do_loc = true;
    }
    else if (ELEM(tmode, TFM_ROTATION, TFM_TRACKBALL)) {
      if (scene->toolsettings->transform_pivot_point == V3D_AROUND_ACTIVE) {
        BKE_view_layer_synced_ensure(scene, view_layer);
        if (ob != BKE_view_layer_active_object_get(view_layer)) {
          do_loc = true;
        }
      }
      else if (scene->toolsettings->transform_pivot_point == V3D_AROUND_CURSOR) {
        do_loc = true;
      }

      if ((scene->toolsettings->transform_flag & SCE_XFORM_AXIS_ALIGN) == 0) {
        do_rot = true;
      }
    }
    else if (tmode == TFM_RESIZE) {
      if (scene->toolsettings->transform_pivot_point == V3D_AROUND_ACTIVE) {
        BKE_view_layer_synced_ensure(scene, view_layer);
        if (ob != BKE_view_layer_active_object_get(view_layer)) {
          do_loc = true;
        }
      }
      else if (scene->toolsettings->transform_pivot_point == V3D_AROUND_CURSOR) {
        do_loc = true;
      }

      if ((scene->toolsettings->transform_flag & SCE_XFORM_AXIS_ALIGN) == 0) {
        do_scale = true;
      }
    }

    if (do_loc) {
      KeyingSet *ks = ANIM_builtin_keyingset_get_named(ANIM_KS_LOCATION_ID);
      ANIM_apply_keyingset(C, &sources, ks, MODIFYKEY_MODE_INSERT, anim_eval_context.eval_time);
    }
    if (do_rot) {
      KeyingSet *ks = ANIM_builtin_keyingset_get_named(ANIM_KS_ROTATION_ID);
      ANIM_apply_keyingset(C, &sources, ks, MODIFYKEY_MODE_INSERT, anim_eval_context.eval_time);
    }
    if (do_scale) {
      KeyingSet *ks = ANIM_builtin_keyingset_get_named(ANIM_KS_SCALING_ID);
      ANIM_apply_keyingset(C, &sources, ks, MODIFYKEY_MODE_INSERT, anim_eval_context.eval_time);
    }
  }

  /* Insert keyframe in all (transform) channels. */
  else {
    KeyingSet *ks = ANIM_builtin_keyingset_get_named(ANIM_KS_LOC_ROT_SCALE_ID);
    ANIM_apply_keyingset(C, &sources, ks, MODIFYKEY_MODE_INSERT, anim_eval_context.eval_time);
  }
}

bool autokeyframe_object(bContext *C, Scene *scene, Object *ob, KeyingSet *ks)
{
  if (!autokeyframe_cfra_can_key(scene, &ob->id)) {
    return false;
  }

  /* Now insert the key-frame(s) using the Keying Set:
   * 1) Add data-source override for the Object.
   * 2) Insert key-frames.
   * 3) Free the extra info.
   */
  blender::Vector<PointerRNA> sources;
  ANIM_relative_keyingset_add_source(sources, &ob->id);
  ANIM_apply_keyingset(C, &sources, ks, MODIFYKEY_MODE_INSERT, BKE_scene_frame_get(scene));

  return true;
}

bool autokeyframe_pchan(bContext *C, Scene *scene, Object *ob, bPoseChannel *pchan, KeyingSet *ks)
{
  if (!autokeyframe_cfra_can_key(scene, &ob->id)) {
    return false;
  }

  /* Now insert the keyframe(s) using the Keying Set:
   * 1) Add data-source override for the pose-channel.
   * 2) Insert key-frames.
   * 3) Free the extra info.
   */
  blender::Vector<PointerRNA> sources;
  ANIM_relative_keyingset_add_source(sources, &ob->id, &RNA_PoseBone, pchan);
  ANIM_apply_keyingset(C, &sources, ks, MODIFYKEY_MODE_INSERT, BKE_scene_frame_get(scene));

  return true;
}

void autokeyframe_pose(bContext *C, Scene *scene, Object *ob, int tmode, short targetless_ik)
{
  Main *bmain = CTX_data_main(C);
  ID *id = &ob->id;
  AnimData *adt = ob->adt;
  bAction *act = (adt) ? adt->action : nullptr;
  bPose *pose = ob->pose;

  if (!blender::animrig::autokeyframe_cfra_can_key(scene, id)) {
    return;
  }

  ReportList *reports = CTX_wm_reports(C);
  ToolSettings *ts = scene->toolsettings;
  KeyingSet *active_ks = ANIM_scene_get_active_keyingset(scene);
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  const AnimationEvalContext anim_eval_context = BKE_animsys_eval_context_construct(
      depsgraph, BKE_scene_frame_get(scene));
  eInsertKeyFlags flag = eInsertKeyFlags(0);

  /* flag is initialized from UserPref keyframing settings
   * - special exception for targetless IK - INSERTKEY_MATRIX keyframes should get
   *   visual keyframes even if flag not set, as it's not that useful otherwise
   *   (for quick animation recording)
   */
  flag = ANIM_get_keyframing_flags(scene, true);

  if (targetless_ik) {
    flag |= INSERTKEY_MATRIX;
  }

  LISTBASE_FOREACH (bPoseChannel *, pchan, &pose->chanbase) {
    if ((pchan->bone->flag & BONE_TRANSFORM) == 0 &&
        !((pose->flag & POSE_MIRROR_EDIT) && (pchan->bone->flag & BONE_TRANSFORM_MIRROR)))
    {
      continue;
    }

    blender::Vector<PointerRNA> sources;
    /* Add data-source override for the camera object. */
    ANIM_relative_keyingset_add_source(sources, id, &RNA_PoseBone, pchan);

    /* only insert into active keyingset? */
    if (blender::animrig::is_autokey_flag(scene, AUTOKEY_FLAG_ONLYKEYINGSET) && (active_ks)) {
      /* Run the active Keying Set on the current data-source. */
      ANIM_apply_keyingset(
          C, &sources, active_ks, MODIFYKEY_MODE_INSERT, anim_eval_context.eval_time);
    }
    /* only insert into available channels? */
    else if (blender::animrig::is_autokey_flag(scene, AUTOKEY_FLAG_INSERTAVAILABLE)) {
      if (act) {
        LISTBASE_FOREACH (FCurve *, fcu, &act->curves) {
          /* only insert keyframes for this F-Curve if it affects the current bone */
          char pchan_name[sizeof(pchan->name)];
          if (!BLI_str_quoted_substr(fcu->rna_path, "bones[", pchan_name, sizeof(pchan_name))) {
            continue;
          }

          /* only if bone name matches too...
           * NOTE: this will do constraints too, but those are ok to do here too?
           */
          if (STREQ(pchan_name, pchan->name)) {
            blender::animrig::insert_keyframe(bmain,
                                              reports,
                                              id,
                                              act,
                                              ((fcu->grp) ? (fcu->grp->name) : (nullptr)),
                                              fcu->rna_path,
                                              fcu->array_index,
                                              &anim_eval_context,
                                              eBezTriple_KeyframeType(ts->keyframe_type),
                                              flag);
          }
        }
      }
    }
    /* only insert keyframe if needed? */
    else if (blender::animrig::is_autokey_flag(scene, AUTOKEY_FLAG_INSERTNEEDED)) {
      bool do_loc = false, do_rot = false, do_scale = false;

      /* Filter the conditions when this happens
       * (assume that 'curarea->spacetype == SPACE_VIEW3D'). */
      if (tmode == TFM_TRANSLATION) {
        if (targetless_ik) {
          do_rot = true;
        }
        else {
          do_loc = true;
        }
      }
      else if (ELEM(tmode, TFM_ROTATION, TFM_TRACKBALL)) {
        if (ELEM(scene->toolsettings->transform_pivot_point, V3D_AROUND_CURSOR, V3D_AROUND_ACTIVE))
        {
          do_loc = true;
        }

        if ((scene->toolsettings->transform_flag & SCE_XFORM_AXIS_ALIGN) == 0) {
          do_rot = true;
        }
      }
      else if (tmode == TFM_RESIZE) {
        if (ELEM(scene->toolsettings->transform_pivot_point, V3D_AROUND_CURSOR, V3D_AROUND_ACTIVE))
        {
          do_loc = true;
        }

        if ((scene->toolsettings->transform_flag & SCE_XFORM_AXIS_ALIGN) == 0) {
          do_scale = true;
        }
      }

      if (do_loc) {
        KeyingSet *ks = ANIM_builtin_keyingset_get_named(ANIM_KS_LOCATION_ID);
        ANIM_apply_keyingset(C, &sources, ks, MODIFYKEY_MODE_INSERT, anim_eval_context.eval_time);
      }
      if (do_rot) {
        KeyingSet *ks = ANIM_builtin_keyingset_get_named(ANIM_KS_ROTATION_ID);
        ANIM_apply_keyingset(C, &sources, ks, MODIFYKEY_MODE_INSERT, anim_eval_context.eval_time);
      }
      if (do_scale) {
        KeyingSet *ks = ANIM_builtin_keyingset_get_named(ANIM_KS_SCALING_ID);
        ANIM_apply_keyingset(C, &sources, ks, MODIFYKEY_MODE_INSERT, anim_eval_context.eval_time);
      }
    }
    /* insert keyframe in all (transform) channels */
    else {
      KeyingSet *ks = ANIM_builtin_keyingset_get_named(ANIM_KS_LOC_ROT_SCALE_ID);
      ANIM_apply_keyingset(C, &sources, ks, MODIFYKEY_MODE_INSERT, anim_eval_context.eval_time);
    }
  }
}

bool autokeyframe_property(bContext *C,
                           Scene *scene,
                           PointerRNA *ptr,
                           PropertyRNA *prop,
                           const int rnaindex,
                           const float cfra,
                           const bool only_if_property_keyed)
{

  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  const AnimationEvalContext anim_eval_context = BKE_animsys_eval_context_construct(depsgraph,
                                                                                    cfra);
  bAction *action;
  bool driven;
  bool special;
  bool changed = false;

  /* For entire array buttons we check the first component, it's not perfect
   * but works well enough in typical cases. */
  const int rnaindex_check = (rnaindex == -1) ? 0 : rnaindex;
  FCurve *fcu = BKE_fcurve_find_by_rna_context_ui(
      C, ptr, prop, rnaindex_check, nullptr, &action, &driven, &special);

  /* Only early out when we actually want an existing F-curve already
   * (e.g. auto-keyframing from buttons). */
  if (fcu == nullptr && (driven || special || only_if_property_keyed)) {
    return changed;
  }

  if (special) {
    /* NLA Strip property. */
    if (is_autokey_on(scene)) {
      ReportList *reports = CTX_wm_reports(C);
      ToolSettings *ts = scene->toolsettings;

      changed = insert_keyframe_direct(reports,
                                       *ptr,
                                       prop,
                                       fcu,
                                       &anim_eval_context,
                                       eBezTriple_KeyframeType(ts->keyframe_type),
                                       nullptr,
                                       eInsertKeyFlags(0));
      WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, nullptr);
    }
  }
  else if (driven) {
    /* Driver - Try to insert keyframe using the driver's input as the frame,
     * making it easier to set up corrective drivers.
     */
    if (is_autokey_on(scene)) {
      ReportList *reports = CTX_wm_reports(C);
      ToolSettings *ts = scene->toolsettings;

      changed = insert_keyframe_direct(reports,
                                       *ptr,
                                       prop,
                                       fcu,
                                       &anim_eval_context,
                                       eBezTriple_KeyframeType(ts->keyframe_type),
                                       nullptr,
                                       INSERTKEY_DRIVER);
      WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, nullptr);
    }
  }
  else {
    ID *id = ptr->owner_id;
    Main *bmain = CTX_data_main(C);

    /* TODO: this should probably respect the keyingset only option for anim */
    if (autokeyframe_cfra_can_key(scene, id)) {
      ReportList *reports = CTX_wm_reports(C);
      ToolSettings *ts = scene->toolsettings;
      const eInsertKeyFlags flag = ANIM_get_keyframing_flags(scene, true);
      char *path = RNA_path_from_ID_to_property(ptr, prop);

      if (only_if_property_keyed) {
        /* NOTE: We use rnaindex instead of fcu->array_index,
         *       because a button may control all items of an array at once.
         *       E.g., color wheels (see #42567). */
        BLI_assert((fcu->array_index == rnaindex) || (rnaindex == -1));
      }
      changed = insert_keyframe(bmain,
                                reports,
                                id,
                                action,
                                (fcu && fcu->grp) ? fcu->grp->name : nullptr,
                                fcu ? fcu->rna_path : path,
                                rnaindex,
                                &anim_eval_context,
                                eBezTriple_KeyframeType(ts->keyframe_type),
                                flag) != 0;
      if (path) {
        MEM_freeN(path);
      }
      WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, nullptr);
    }
  }
  return changed;
}

}  // namespace blender::animrig
