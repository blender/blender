/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup animrig
 */

#include "BKE_animsys.h"
#include "BKE_context.hh"
#include "BKE_fcurve.h"
#include "BKE_layer.hh"
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
#include "ANIM_rna.hh"

#include "WM_api.hh"
#include "WM_types.hh"

namespace blender::animrig {

static eInsertKeyFlags get_autokey_flags(Scene *scene)
{
  eInsertKeyFlags flag = INSERTKEY_NOFLAGS;

  /* Visual keying. */
  if (is_keying_flag(scene, KEYING_FLAG_VISUALKEY)) {
    flag |= INSERTKEY_MATRIX;
  }

  /* Only needed. */
  if (is_keying_flag(scene, AUTOKEY_FLAG_INSERTNEEDED)) {
    flag |= INSERTKEY_NEEDED;
  }

  /* Keyframing mode - only replace existing keyframes. */
  if (is_autokey_mode(scene, AUTOKEY_MODE_EDITKEYS)) {
    flag |= INSERTKEY_REPLACE;
  }

  /* Cycle-aware keyframe insertion - preserve cycle period and flow. */
  if (is_keying_flag(scene, KEYING_FLAG_CYCLEAWARE)) {
    flag |= INSERTKEY_CYCLE_AWARE;
  }

  return flag;
}

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

void autokeyframe_object(bContext *C, Scene *scene, Object *ob, Span<std::string> rna_paths)
{
  ID *id = &ob->id;
  if (!autokeyframe_cfra_can_key(scene, id)) {
    return;
  }

  ReportList *reports = CTX_wm_reports(C);
  KeyingSet *active_ks = ANIM_scene_get_active_keyingset(scene);
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  const AnimationEvalContext anim_eval_context = BKE_animsys_eval_context_construct(
      depsgraph, BKE_scene_frame_get(scene));

  /* Get flags used for inserting keyframes. */
  const eInsertKeyFlags flag = get_autokey_flags(scene);

  /* Add data-source override for the object. */
  blender::Vector<PointerRNA> sources;
  ANIM_relative_keyingset_add_source(sources, id);

  if (is_keying_flag(scene, AUTOKEY_FLAG_ONLYKEYINGSET) && (active_ks)) {
    /* Only insert into active keyingset
     * NOTE: we assume here that the active Keying Set
     * does not need to have its iterator overridden.
     */
    ANIM_apply_keyingset(
        C, &sources, active_ks, MODIFYKEY_MODE_INSERT, anim_eval_context.eval_time);
    return;
  }

  if (is_keying_flag(scene, AUTOKEY_FLAG_INSERTAVAILABLE)) {
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
    return;
  }

  const float scene_frame = BKE_scene_frame_get(scene);
  Main *bmain = CTX_data_main(C);

  for (PointerRNA ptr : sources) {
    insert_key_rna(&ptr,
                   rna_paths,
                   scene_frame,
                   flag,
                   eBezTriple_KeyframeType(scene->toolsettings->keyframe_type),
                   bmain,
                   reports,
                   anim_eval_context);
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

void autokeyframe_pose_channel(bContext *C,
                               Scene *scene,
                               Object *ob,
                               bPoseChannel *pose_channel,
                               Span<std::string> rna_paths,
                               short targetless_ik)
{
  Main *bmain = CTX_data_main(C);
  ID *id = &ob->id;
  AnimData *adt = ob->adt;
  bAction *act = (adt) ? adt->action : nullptr;

  if (!blender::animrig::autokeyframe_cfra_can_key(scene, id)) {
    return;
  }

  ReportList *reports = CTX_wm_reports(C);
  ToolSettings *ts = scene->toolsettings;
  KeyingSet *active_ks = ANIM_scene_get_active_keyingset(scene);
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  const float scene_frame = BKE_scene_frame_get(scene);
  const AnimationEvalContext anim_eval_context = BKE_animsys_eval_context_construct(depsgraph,
                                                                                    scene_frame);

  /* flag is initialized from UserPref keyframing settings
   * - special exception for targetless IK - INSERTKEY_MATRIX keyframes should get
   *   visual keyframes even if flag not set, as it's not that useful otherwise
   *   (for quick animation recording)
   */
  eInsertKeyFlags flag = get_autokey_flags(scene);

  if (targetless_ik) {
    flag |= INSERTKEY_MATRIX;
  }

  blender::Vector<PointerRNA> sources;
  /* Add data-source override for the camera object. */
  ANIM_relative_keyingset_add_source(sources, id, &RNA_PoseBone, pose_channel);

  /* only insert into active keyingset? */
  if (blender::animrig::is_keying_flag(scene, AUTOKEY_FLAG_ONLYKEYINGSET) && (active_ks)) {
    /* Run the active Keying Set on the current data-source. */
    ANIM_apply_keyingset(
        C, &sources, active_ks, MODIFYKEY_MODE_INSERT, anim_eval_context.eval_time);
    return;
  }

  /* only insert into available channels? */
  if (blender::animrig::is_keying_flag(scene, AUTOKEY_FLAG_INSERTAVAILABLE)) {
    if (!act) {
      return;
    }
    LISTBASE_FOREACH (FCurve *, fcu, &act->curves) {
      /* only insert keyframes for this F-Curve if it affects the current bone */
      char pchan_name[sizeof(pose_channel->name)];
      if (!BLI_str_quoted_substr(fcu->rna_path, "bones[", pchan_name, sizeof(pchan_name))) {
        continue;
      }

      /* only if bone name matches too...
       * NOTE: this will do constraints too, but those are ok to do here too?
       */
      if (STREQ(pchan_name, pose_channel->name)) {
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
    return;
  }

  for (PointerRNA &ptr : sources) {
    insert_key_rna(&ptr,
                   rna_paths,
                   scene_frame,
                   flag,
                   eBezTriple_KeyframeType(scene->toolsettings->keyframe_type),
                   bmain,
                   reports,
                   anim_eval_context);
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

  if (driven) {
    return false;
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
  else {
    ID *id = ptr->owner_id;
    Main *bmain = CTX_data_main(C);

    /* TODO: this should probably respect the keyingset only option for anim */
    if (autokeyframe_cfra_can_key(scene, id)) {
      ReportList *reports = CTX_wm_reports(C);
      ToolSettings *ts = scene->toolsettings;
      const eInsertKeyFlags flag = get_autokey_flags(scene);
      const std::optional<std::string> path = RNA_path_from_ID_to_property(ptr, prop);

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
                                fcu ? fcu->rna_path : (path ? path->c_str() : nullptr),
                                rnaindex,
                                &anim_eval_context,
                                eBezTriple_KeyframeType(ts->keyframe_type),
                                flag) != 0;

      WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, nullptr);
    }
  }
  return changed;
}

}  // namespace blender::animrig
