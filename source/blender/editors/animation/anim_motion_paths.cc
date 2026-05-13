/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "MEM_guardedalloc.h"

#include <cstdlib>

#include "BLI_bounds.hh"
#include "BLI_listbase.h"
#include "BLI_listbase_wrapper.hh"
#include "BLI_math_matrix.h"
#include "BLI_math_matrix.hh"
#include "BLI_math_vector.h"
#include "BLI_string.h"

#include "DNA_anim_types.h"
#include "DNA_armature_types.h"
#include "DNA_scene_types.h"

#include "BKE_action.hh"
#include "BKE_anim_data.hh"
#include "BKE_main.hh"
#include "BKE_scene.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_build.hh"
#include "DEG_depsgraph_query.hh"

#include "GPU_batch.hh"
#include "GPU_vertex_buffer.hh"

#include "ED_anim_api.hh"
#include "ED_keyframes_keylist.hh"

#include "ANIM_action.hh"
#include "ANIM_action_legacy.hh"
#include "ANIM_animdata.hh"
#include "ANIM_bone_collections.hh"

#include "CLG_log.h"

namespace blender {

static CLG_LogRef LOG = {"anim.motion_paths"};

/* Motion path needing to be baked (mpt). */
struct MPathTarget {
  bMotionPath *mpath; /* Motion path in question. */

  AnimKeylist *keylist; /* Temp, to know where the keyframes are. */

  /* Original (Source Objects) */
  Object *ob;          /* Source Object */
  bPoseChannel *pchan; /* Source pose-channel (if applicable). */
};

/* ........ */

Depsgraph *animviz_depsgraph_build(Main *bmain,
                                   Scene *scene,
                                   ViewLayer *view_layer,
                                   const Span<MPathTarget *> targets)
{
  /* Allocate dependency graph. */
  Depsgraph *depsgraph = DEG_graph_new(bmain, scene, view_layer, DAG_EVAL_VIEWPORT);

  /* Make a flat array of IDs for the DEG API. */
  Array<ID *> ids(targets.size());
  int current_id_index = 0;
  for (const MPathTarget *mpt : targets) {
    ids[current_id_index++] = &mpt->ob->id;
  }

  /* Build graph from all requested IDs. */
  DEG_graph_build_from_ids(depsgraph, ids);

  return depsgraph;
}

void animviz_build_motionpath_targets(Object *ob, Vector<MPathTarget *> &r_targets)
{
  /* TODO: it would be nice in future to be able to update objects dependent on these bones too? */

  MPathTarget *mpt;

  /* Object itself first. */
  if ((ob->avs.recalc & ANIMVIZ_RECALC_PATHS) && (ob->mpath)) {
    /* New target for object. */
    mpt = MEM_new_zeroed<MPathTarget>("MPathTarget Ob");
    mpt->mpath = ob->mpath;
    mpt->ob = ob;

    r_targets.append(mpt);
  }

  /* Bones. */
  if ((ob->pose) && (ob->pose->avs.recalc & ANIMVIZ_RECALC_PATHS)) {
    bArmature *arm = id_cast<bArmature *>(ob->data);
    for (bPoseChannel &pchan : ob->pose->chanbase) {
      if (!pchan.mpath) {
        continue;
      }
      Bone *bone = pchan.bone_get(*ob);
      if (!bone || !ANIM_bone_in_visible_collection(arm, bone)) {
        continue;
      }
      /* New target for bone. */
      mpt = MEM_new_zeroed<MPathTarget>("MPathTarget PoseBone");
      mpt->mpath = pchan.mpath;
      mpt->ob = ob;
      mpt->pchan = &pchan;
      r_targets.append(mpt);
    }
  }
}

void animviz_free_motionpath_targets(Vector<MPathTarget *> &targets)
{
  for (MPathTarget *mpt : targets) {
    MEM_delete(mpt);
  }
  targets.clear_and_shrink();
}

/* ........ */

/* Perform baking for the targets on the current frame. */
static void motionpaths_calc_bake_targets(const Span<MPathTarget *> targets,
                                          const int cframe,
                                          Depsgraph *depsgraph,
                                          Object *camera)
{
  /* For each target, check if it can be baked on the current frame. */
  for (const MPathTarget *mpt : targets) {
    bMotionPath *mpath = mpt->mpath;

    /* Current frame must be within the range the cache works for.
     * - is inclusive of the first frame, but not the last otherwise we get buffer overruns.
     */
    if ((cframe < mpath->start_frame) || (cframe >= mpath->end_frame)) {
      continue;
    }

    /* Get the relevant cache vert to write to. */
    bMotionPathVert *mpv = mpath->points + (cframe - mpath->start_frame);

    Object *ob_eval = DEG_get_evaluated(depsgraph, mpt->ob);

    /* Lookup evaluated pose channel, here because the depsgraph
     * evaluation can change them so they are not cached in mpt. */
    bPoseChannel *pchan_eval = nullptr;
    if (mpt->pchan) {
      pchan_eval = BKE_pose_channel_find_name(ob_eval->pose, mpt->pchan->name);
    }

    /* Pose-channel or object path baking? */
    if (pchan_eval) {
      /* Heads or tails. */
      if (mpath->flag & MOTIONPATH_FLAG_BHEAD) {
        copy_v3_v3(mpv->co, pchan_eval->pose_head);
      }
      else {
        copy_v3_v3(mpv->co, pchan_eval->pose_tail);
      }

      /* Result must be in world-space. */
      mul_m4_v3(ob_eval->object_to_world().ptr(), mpv->co);
    }
    else {
      /* World-space object location. */
      copy_v3_v3(mpv->co, ob_eval->object_to_world().location());
    }

    if (mpath->flag & MOTIONPATH_FLAG_BAKE_CAMERA && camera) {
      Object *cam_eval = DEG_get_evaluated(depsgraph, camera);
      /* Convert point to camera space. */
      float3 co_camera_space = math::transform_point(cam_eval->world_to_object(), float3(mpv->co));
      copy_v3_v3(mpv->co, co_camera_space);
    }

    float mframe = float(cframe);

    /* Tag if it's a keyframe. */
    if (ED_keylist_find_exact(mpt->keylist, mframe)) {
      mpv->flag |= MOTIONPATH_VERT_KEY;
    }
    else {
      mpv->flag &= ~MOTIONPATH_VERT_KEY;
    }

    /* Incremental update on evaluated object if possible, for fast updating
     * while dragging in transform. */
    bMotionPath *mpath_eval = nullptr;
    if (mpt->pchan) {
      mpath_eval = (pchan_eval) ? pchan_eval->mpath : nullptr;
    }
    else {
      mpath_eval = ob_eval->mpath;
    }

    if (mpath_eval && mpath_eval->length == mpath->length) {
      bMotionPathVert *mpv_eval = mpath_eval->points + (cframe - mpath_eval->start_frame);
      *mpv_eval = *mpv;

      GPU_VERTBUF_DISCARD_SAFE(mpath_eval->points_vbo);
      GPU_BATCH_DISCARD_SAFE(mpath_eval->batch_line);
      GPU_BATCH_DISCARD_SAFE(mpath_eval->batch_points);
    }
  }
}

/* Get pointer to animviz settings for the given target. */
static bAnimVizSettings *animviz_target_settings_get(const MPathTarget *mpt)
{
  if (mpt->pchan != nullptr) {
    return &mpt->ob->pose->avs;
  }
  return &mpt->ob->avs;
}

/* Returns the combined range of all `MPathTarget` start and end frames. */
static Bounds<int> motionpath_get_global_framerange(const Span<MPathTarget *> targets)
{
  Bounds<int> frame_range = {INT_MAX, INT_MIN};
  for (const MPathTarget *mpt : targets) {
    frame_range.min = min_ii(frame_range.min, mpt->mpath->start_frame);
    frame_range.max = max_ii(frame_range.max, mpt->mpath->end_frame);
  }
  return frame_range;
}

static int motionpath_get_prev_keyframe(MPathTarget *mpt,
                                        AnimKeylist *keylist,
                                        const int current_frame)
{
  /* TODO(jbakker): Remove complexity, key-lists are ordered. */

  if (current_frame <= mpt->mpath->start_frame) {
    return mpt->mpath->start_frame;
  }

  float current_frame_float = current_frame;
  const ActKeyColumn *ak = ED_keylist_find_prev(keylist, current_frame_float);
  if (ak == nullptr) {
    return mpt->mpath->start_frame;
  }

  return ak->cfra;
}

static int motionpath_get_prev_prev_keyframe(MPathTarget *mpt,
                                             AnimKeylist *keylist,
                                             const int current_frame)
{
  int frame = motionpath_get_prev_keyframe(mpt, keylist, current_frame);
  return motionpath_get_prev_keyframe(mpt, keylist, frame);
}

static int motionpath_get_next_keyframe(MPathTarget *mpt,
                                        AnimKeylist *keylist,
                                        const int current_frame)
{
  if (current_frame >= mpt->mpath->end_frame) {
    return mpt->mpath->end_frame;
  }

  float current_frame_float = current_frame;
  const ActKeyColumn *ak = ED_keylist_find_next(keylist, current_frame_float);
  if (ak == nullptr) {
    return mpt->mpath->end_frame;
  }

  return ak->cfra;
}

static int motionpath_get_next_next_keyframe(MPathTarget *mpt,
                                             AnimKeylist *keylist,
                                             const int current_frame)
{
  int frame = motionpath_get_next_keyframe(mpt, keylist, current_frame);
  return motionpath_get_next_keyframe(mpt, keylist, frame);
}

static bool motionpath_check_can_use_keyframe_range(MPathTarget * /*mpt*/,
                                                    AnimData *adt,
                                                    const Span<FCurve *> fcurves)
{
  if (adt == nullptr || fcurves.is_empty()) {
    return false;
  }
  /* NOTE: We might needed to do a full frame range update if there is a specific setup of NLA
   * or drivers or modifiers on the f-curves. */
  return true;
}

static Bounds<int> motionpath_calculate_update_range(MPathTarget *mpt,
                                                     AnimData *adt,
                                                     const Span<FCurve *> fcurves,
                                                     const int current_frame)
{
  /* If the current frame is outside of the configured motion path range we ignore update of this
   * motion path by using invalid frame range where start frame is above the end frame. */
  if (current_frame < mpt->mpath->start_frame || current_frame > mpt->mpath->end_frame) {
    return {INT_MAX, INT_MIN};
  }

  /* Similar to the case when there is only a single keyframe: need to update en entire range to
   * a constant value. */
  if (!motionpath_check_can_use_keyframe_range(mpt, adt, fcurves)) {
    return {mpt->mpath->start_frame, mpt->mpath->end_frame};
  }

  Bounds<int> frame_range = {INT_MAX, INT_MIN};
  /* NOTE: Iterate over individual f-curves, and check their keyframes individually and pick a
   * widest range from them. This is because it's possible to have more narrow keyframe on a
   * channel which wasn't edited.
   * Could be optimized further by storing some flags about which channels has been modified so
   * we ignore all others (which can potentially make an update range unnecessary wide). */
  for (FCurve *fcu : fcurves) {
    AnimKeylist *keylist = ED_keylist_create();
    fcurve_to_keylist(adt, fcu, keylist, 0, {-FLT_MAX, FLT_MAX}, true);
    ED_keylist_prepare_for_direct_access(keylist);

    int fcu_sfra = motionpath_get_prev_prev_keyframe(mpt, keylist, current_frame);
    int fcu_efra = motionpath_get_next_next_keyframe(mpt, keylist, current_frame);

    /* Extend range further, since acceleration compensation propagates even further away. */
    if (fcu->auto_smoothing != FCURVE_SMOOTH_NONE) {
      fcu_sfra = motionpath_get_prev_prev_keyframe(mpt, keylist, fcu_sfra);
      fcu_efra = motionpath_get_next_next_keyframe(mpt, keylist, fcu_efra + 1);
    }

    if (fcu_sfra <= fcu_efra) {
      frame_range.min = min_ii(frame_range.min, fcu_sfra);
      frame_range.max = max_ii(frame_range.max, fcu_efra + 1);
    }

    ED_keylist_free(keylist);
  }
  return frame_range;
}

static void motionpath_free_free_tree_data(MutableSpan<MPathTarget *> targets)
{
  for (MPathTarget *mpt : targets) {
    ED_keylist_free(mpt->keylist);
  }
}

void animviz_motionpath_compute_range(Object *ob, Scene *scene)
{
  bAnimVizSettings *avs = ob->mode == OB_MODE_POSE ? &ob->pose->avs : &ob->avs;

  if (avs->path_range == MOTIONPATH_RANGE_MANUAL) {
    /* Don't touch manually-determined ranges. */
    return;
  }

  const bool has_action = ob->adt && ob->adt->action;
  if (avs->path_range == MOTIONPATH_RANGE_SCENE || !has_action ||
      !animrig::legacy::assigned_action_has_keyframes(ob->adt))
  {
    /* Default to the scene (preview) range if there is no animation data to
     * find selected keys in. */
    avs->path_sf = scene->playback_start();
    avs->path_ef = scene->playback_end();
    return;
  }

  AnimKeylist *keylist = ED_keylist_create();
  for (FCurve *fcu : animrig::fcurves_for_assigned_action(ob->adt)) {
    fcurve_to_keylist(ob->adt, fcu, keylist, 0, {-FLT_MAX, FLT_MAX}, true);
  }

  Bounds<float> frame_range;
  switch (avs->path_range) {
    case MOTIONPATH_RANGE_KEYS_SELECTED:
      if (ED_keylist_selected_keys_frame_range(keylist, &frame_range)) {
        break;
      }
      ATTR_FALLTHROUGH; /* Fall through if there were no selected keys found. */
    case MOTIONPATH_RANGE_KEYS_ALL:
      ED_keylist_all_keys_frame_range(keylist, &frame_range);
      break;
    case MOTIONPATH_RANGE_MANUAL:
    case MOTIONPATH_RANGE_SCENE:
      BLI_assert_msg(false, "This should not happen, function should have exited earlier.");
  };

  avs->path_sf = frame_range.min;
  avs->path_ef = frame_range.max;

  ED_keylist_free(keylist);
}

static void build_keylist_for_target(MPathTarget &target, AnimKeylist &keylist)
{
  /* For object level motion paths this is a nullptr in which case the filtering is ignored. */
  bPoseChannel *pose_bone = target.pchan;
  for (FCurve *fcu : animrig::fcurves_for_assigned_action(target.ob->adt)) {
    if (pose_bone &&
        !animrig::fcurve_matches_collection_path(*fcu, "pose.bones[", pose_bone->name))
    {
      continue;
    }
    /* When only updating a subset of the motion path we could pass a range here to improve
     * performance. */
    fcurve_to_keylist(target.ob->adt, fcu, &keylist, 0, {-FLT_MAX, FLT_MAX}, true);
  }
}

void animviz_calc_motionpaths(Depsgraph *depsgraph,
                              Main *bmain,
                              Scene *scene,
                              MutableSpan<MPathTarget *> targets,
                              eAnimvizCalcRange range)
{
  using namespace blender::animrig;

  if (targets.is_empty()) {
    return;
  }

  const int cfra = scene->r.cfra;
  /* The frame range to calculate. Inclusive/Exclusive. */
  Bounds<int> frame_range = {INT_MAX, INT_MIN};
  switch (range) {
    case ANIMVIZ_CALC_RANGE_CURRENT_FRAME:
      frame_range = motionpath_get_global_framerange(targets);
      if (frame_range.is_empty()) {
        return;
      }
      if (!frame_range.contains(cfra)) {
        return;
      }
      frame_range = {cfra, cfra + 1};
      break;
    case ANIMVIZ_CALC_RANGE_CHANGED:
      /* Nothing to do here, will be handled later when iterating through the targets. */
      break;
    case ANIMVIZ_CALC_RANGE_FULL:
      frame_range = motionpath_get_global_framerange(targets);
      if (frame_range.is_empty()) {
        return;
      }
      break;
  }

  /* Get copies of objects/bones to get the calculated results from
   * (for copy-on-evaluation), so that we actually get some results.
   */

  /* TODO: Create a copy of background depsgraph that only contain these entities,
   * and only evaluates them.
   *
   * For until that is done we force dependency graph to not be active, so we don't lose unkeyed
   * changes during updating the motion path.
   * This still doesn't include unkeyed changes to the path itself, but allows to have updates in
   * an environment when auto-keying and pose paste is used. */

  const bool is_active_depsgraph = DEG_is_active(depsgraph);
  if (is_active_depsgraph) {
    DEG_make_inactive(depsgraph);
  }

  for (MPathTarget *mpt : targets) {
    AnimData *adt = BKE_animdata_from_id(&mpt->ob->id);

    /* Build list of all keyframes in active action for object or pchan. */
    mpt->keylist = ED_keylist_create();

    Vector<FCurve *> fcurves;
    if (adt && adt->action) {
      /* Get pointer to animviz settings for each target. */
      bAnimVizSettings *avs = animviz_target_settings_get(mpt);

      /* For bones it is likely that all FCurves belong to a group named after the bone. Only
       * checking FCurves of a given group can improve performance when building the keylist. */
      if ((mpt->pchan) && (avs->path_viewflag & MOTIONPATH_VIEW_KFACT) == 0) {
        Action &action = adt->action->wrap();
        bActionGroup *agrp = nullptr;
        Channelbag *cbag = channelbag_for_action_slot(action, adt->slot_handle);
        agrp = cbag ? cbag->channel_group_find(mpt->pchan->name) : nullptr;

        if (agrp) {
          fcurves = listbase_to_vector<FCurve>(agrp->channels);
          action_group_to_keylist(adt, agrp, mpt->keylist, 0, {-FLT_MAX, FLT_MAX});
        }
      }
      else {
        build_keylist_for_target(*mpt, *mpt->keylist);
      }
    }
    ED_keylist_prepare_for_direct_access(mpt->keylist);

    if (range == ANIMVIZ_CALC_RANGE_CHANGED) {
      const Bounds<int> target_bounds = motionpath_calculate_update_range(mpt, adt, fcurves, cfra);
      if (!target_bounds.is_empty()) {
        frame_range.min = min_ii(frame_range.min, target_bounds.min);
        frame_range.max = max_ii(frame_range.max, target_bounds.max);
      }
    }
  }

  if (frame_range.is_empty()) {
    motionpath_free_free_tree_data(targets);
    return;
  }

  /* Calculate path over requested range. */
  CLOG_INFO(&LOG,
            "Calculating MotionPaths between frames %d - %d (%d frames)",
            frame_range.min,
            frame_range.max,
            frame_range.max - frame_range.min + 1);

  for (int frame = frame_range.min; frame < frame_range.max; frame++) {
    if (range == ANIMVIZ_CALC_RANGE_CURRENT_FRAME) {
      /* For current frame, only update tagged. */
      BLI_assert(frame == scene->r.cfra);
      BKE_scene_graph_update_tagged(depsgraph, bmain);
    }
    else {
      /* Update relevant data for new frame. */
      DEG_evaluate_on_framechange(depsgraph, frame);
    }

    /* Perform baking for targets. */
    motionpaths_calc_bake_targets(targets, frame, depsgraph, scene->camera);
  }

  if (is_active_depsgraph) {
    DEG_make_active(depsgraph);
  }

  /* Clear recalc flags from targets. */
  for (MPathTarget *mpt : targets) {
    bMotionPath *mpath = mpt->mpath;

    /* Get pointer to animviz settings for each target. */
    bAnimVizSettings *avs = animviz_target_settings_get(mpt);

    /* Clear the flag requesting recalculation of targets. */
    avs->recalc &= ~ANIMVIZ_RECALC_PATHS;

    /* Clean temp data. */
    ED_keylist_free(mpt->keylist);

    /* Free previous batches to force update. */
    GPU_VERTBUF_DISCARD_SAFE(mpath->points_vbo);
    GPU_BATCH_DISCARD_SAFE(mpath->batch_line);
    GPU_BATCH_DISCARD_SAFE(mpath->batch_points);
  }
}

}  // namespace blender
