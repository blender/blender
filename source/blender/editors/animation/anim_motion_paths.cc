/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "MEM_guardedalloc.h"

#include <cstdlib>

#include "BLI_bounds.hh"
#include "BLI_listbase.hh"
#include "BLI_listbase_wrapper.hh"
#include "BLI_math_matrix.hh"
#include "BLI_math_matrix_c.hh"
#include "BLI_math_vector_c.hh"
#include "BLI_string.hh"

#include "DNA_anim_types.h"
#include "DNA_armature_types.h"
#include "DNA_scene_types.h"

#include "BKE_action.hh"
#include "BKE_anim_data.hh"
#include "BKE_camera.h"
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

/* ........ */

namespace ed::motionpath {

void tag_for_recalc(bMotionPath &motion_path)
{
  if (!motion_path.points) {
    return;
  }
  for (int i = 0; i < motion_path.length; i++) {
    motion_path.points[i].flag &= ~MOTIONPATH_VERT_EVALUATED;
  }
}

}  // namespace ed::motionpath

Depsgraph *animviz_depsgraph_build(Main *bmain,
                                   Scene *scene,
                                   ViewLayer *view_layer,
                                   const Span<MPathTarget> targets)
{
  /* Allocate dependency graph. */
  Depsgraph *depsgraph = DEG_graph_new(bmain, scene, view_layer, DAG_EVAL_VIEWPORT);

  /* Make a flat array of IDs for the DEG API. */
  Array<ID *> ids(targets.size());
  int current_id_index = 0;
  for (const MPathTarget &mpt : targets) {
    ids[current_id_index++] = &mpt.ob->id;
  }

  /* Build graph from all requested IDs. */
  DEG_graph_build_from_ids(depsgraph, ids);

  return depsgraph;
}

void animviz_build_motionpath_targets(Object *ob, Vector<MPathTarget> &r_targets)
{
  /* TODO: it would be nice in future to be able to update objects dependent on these bones too? */

  /* Object itself first. */
  if ((ob->avs.recalc & ANIMVIZ_RECALC_PATHS) && (ob->mpath)) {
    /* New target for object. */
    MPathTarget mpt;
    mpt.mpath = ob->mpath;
    mpt.ob = ob;

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
      MPathTarget mpt;
      mpt.mpath = pchan.mpath;
      mpt.ob = ob;
      mpt.pchan = &pchan;
      r_targets.append(mpt);
    }
  }
}

/* ........ */

/* Converts the given point into NDC space. */
static float3 transform_mpath_point_to_camera(Depsgraph &depsgraph,
                                              Object &camera,
                                              const float3 point)
{
  Object *cam_eval = DEG_get_evaluated(&depsgraph, &camera);
  /* Aka projection matrix. */
  float4x4 window_matrix;
  Scene *scene = DEG_get_input_scene(&depsgraph);
  BKE_camera_multiview_window_matrix(&scene->r, cam_eval, nullptr, window_matrix.ptr());
  /* World to Object is the view matrix. */
  float4x4 perspective_matrix = window_matrix * cam_eval->world_to_object();
  const float4 co_clip_space = perspective_matrix * float4(point.x, point.y, point.z, 1.0);
  /* Storing the verts in NDC space which contains lens effects like sensor offset. See
   * `overlay_motion_path.hh/motion_path_sync`. Negative w values are behind the camera, thus
   * can't be correctly projected into the scene. Using abs(w) is consistent with
   * `project_point` in shader code. */
  const float3 co_ndc_space = float3(co_clip_space) /
                              math::max(math::abs(co_clip_space.w), 0.0001f);
  return co_ndc_space;
}

/* Perform baking for the targets on the current frame. Returns true if data was modified. */
static bool motionpaths_calc_bake_target(const MPathTarget &mpt,
                                         const int cframe,
                                         Depsgraph *depsgraph,
                                         Object *camera)
{
  /* For each target, check if it can be baked on the current frame. */
  bMotionPath *mpath = mpt.mpath;

  /* Current frame must be within the range the cache works for.
   * - is inclusive of the first frame, but not the last otherwise we get buffer overruns.
   */
  if ((cframe < mpath->start_frame) || (cframe >= mpath->end_frame)) {
    return false;
  }

  /* Get the relevant cache vert to write to. */
  bMotionPathVert &mpv = mpath->points[cframe - mpath->start_frame];
  float3 calculated_point;
  float3 previous_point;
  copy_v3_v3(previous_point, mpv.co);

  Object *ob_eval = DEG_get_evaluated(depsgraph, mpt.ob);

  /* Lookup evaluated pose channel, here because the depsgraph
   * evaluation can change them so they are not cached in mpt. */
  bPoseChannel *pchan_eval = nullptr;
  if (mpt.pchan) {
    pchan_eval = BKE_pose_channel_find_name(ob_eval->pose, mpt.pchan->name);
  }

  /* Pose-channel or object path baking? */
  if (pchan_eval) {
    /* Heads or tails. */
    if (mpath->flag & MOTIONPATH_FLAG_BHEAD) {
      copy_v3_v3(calculated_point, pchan_eval->pose_head);
    }
    else {
      copy_v3_v3(calculated_point, pchan_eval->pose_tail);
    }

    /* Result must be in world-space. */
    mul_m4_v3(ob_eval->object_to_world().ptr(), calculated_point);
  }
  else {
    /* World-space object location. */
    copy_v3_v3(calculated_point, ob_eval->object_to_world().location());
  }

  if (mpath->flag & MOTIONPATH_FLAG_BAKE_CAMERA && camera) {
    calculated_point = transform_mpath_point_to_camera(*depsgraph, *camera, calculated_point);
  }

  copy_v3_v3(mpv.co, calculated_point);

  /* Tag if it's a keyframe. */
  if (ED_keylist_find_exact(mpt.keylist, cframe)) {
    mpv.flag |= MOTIONPATH_VERT_KEY;
  }
  else {
    mpv.flag &= ~MOTIONPATH_VERT_KEY;
  }

  /* Incremental update on evaluated object if possible, for fast updating
   * while dragging in transform. */
  bMotionPath *mpath_eval = nullptr;
  if (mpt.pchan) {
    mpath_eval = (pchan_eval) ? pchan_eval->mpath : nullptr;
  }
  else {
    mpath_eval = ob_eval->mpath;
  }

  if (mpath_eval && mpath_eval->length == mpath->length) {
    bMotionPathVert &mpv_eval = mpath_eval->points[cframe - mpath_eval->start_frame];
    mpv_eval = mpv;

    GPU_VERTBUF_DISCARD_SAFE(mpath_eval->points_vbo);
    GPU_BATCH_DISCARD_SAFE(mpath_eval->batch_line);
    GPU_BATCH_DISCARD_SAFE(mpath_eval->batch_points);
  }

  const bool was_already_evaluated = mpv.flag & MOTIONPATH_VERT_EVALUATED;
  mpv.flag |= MOTIONPATH_VERT_EVALUATED;
  /* This does a floating point equality comparison. While that is usually a bad idea, the code
   * that arrives at those numbers is deterministic. So the result will be *identical* as long as
   * the input values are the same. Since we care about equality of the input values bitwise
   * equality is the only correct metric here. */
  const bool has_changed = previous_point != calculated_point;
  /* If the data was not evaluated before, by definition it changed even if the values are the
   * same. */
  return has_changed || !was_already_evaluated;
}

/* Get pointer to animviz settings for the given target. */
static bAnimVizSettings *animviz_target_settings_get(const MPathTarget &mpt)
{
  if (mpt.pchan != nullptr) {
    return &mpt.ob->pose->avs;
  }
  return &mpt.ob->avs;
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
                              Scene *scene,
                              MutableSpan<MPathTarget> targets,
                              const int modified_frame)
{
  using namespace blender::animrig;
  BLI_assert_msg(!DEG_is_active(depsgraph),
                 "Motion path calculation should always happen with a minimal depsgraph.");

  if (targets.is_empty()) {
    return;
  }

  for (MPathTarget &mpt : targets) {
    AnimData *adt = BKE_animdata_from_id(&mpt.ob->id);

    /* Build list of all keyframes in active action for object or pchan. */
    mpt.keylist = ED_keylist_create();

    if (adt && adt->action) {
      /* Get pointer to animviz settings for each target. */
      bAnimVizSettings *avs = animviz_target_settings_get(mpt);

      /* For bones it is likely that all FCurves belong to a group named after the bone. Only
       * checking FCurves of a given group can improve performance when building the keylist. */
      if ((mpt.pchan) && (avs->path_viewflag & MOTIONPATH_VIEW_KFACT) == 0) {
        Action &action = adt->action->wrap();
        bActionGroup *agrp = nullptr;
        Channelbag *cbag = channelbag_for_action_slot(action, adt->slot_handle);
        agrp = cbag ? cbag->channel_group_find(mpt.pchan->name) : nullptr;

        if (agrp) {
          action_group_to_keylist(adt, agrp, mpt.keylist, 0, {-FLT_MAX, FLT_MAX});
        }
      }
      else {
        build_keylist_for_target(mpt, *mpt.keylist);
      }
    }
    ED_keylist_prepare_for_direct_access(mpt.keylist);
  }

  Bounds<int> evaluated_range = {INT_MAX, INT_MIN};

  /* We need this extra loop for the edge case when the ranges of the motion paths don't overlap.
   * We need to touch at least one frame of each motion path to ensure it has the
   * `MOTIONPATH_VERT_EVALUATED` flag. In practice this will almost always be the case and this
   * loop will trigger the `continue` immediately below. */
  for (MPathTarget &mpt : targets) {
    /* We can safely skip the target if either the start or end frame of it's range was already
     * visited. That is because if we had visited it, and it would need recalculation,
     * `motionpaths_calc_bake_target` would return true meaning the inner loop would continue to
     * run. */
    if (evaluated_range.contains(mpt.mpath->start_frame) ||
        evaluated_range.contains(mpt.mpath->end_frame))
    {
      continue;
    }

    const int start_frame = clamp_i(modified_frame, mpt.mpath->start_frame, mpt.mpath->end_frame);
    int frame = start_frame;

    bool finished_left = false;
    bool finished_right = false;
    /* Counts how many times the result of `motionpaths_calc_bake_target` hasn't changed existing
     * data. */
    int stable_result_counter = 0;
    /* At least 2 frames need to return a result different from the currently buffered values. This
     * is because FCURVE_SMOOTH can affect the interpolation beyond a key, but on said key it will
     * be on the exact value of the key. */
    constexpr int stable_result_threshold = 2;

    while (!finished_left || !finished_right) {
      /* Update relevant data for new frame. */
      DEG_evaluate_on_framechange(depsgraph, frame);

      /* Perform baking for targets. */
      bool any_modified = false;
      for (const MPathTarget &target : targets) {
        any_modified |= motionpaths_calc_bake_target(target, frame, depsgraph, scene->camera);
      }
      if (frame == start_frame) {
        evaluated_range = {frame, frame};
        frame--;
        continue;
      }

      if (any_modified) {
        stable_result_counter = 0;
      }
      else {
        stable_result_counter++;
      }

      if (frame < start_frame) {
        finished_left |= stable_result_counter >= stable_result_threshold;
        evaluated_range.min = frame;
      }
      else {
        finished_right |= stable_result_counter >= stable_result_threshold;
        evaluated_range.max = frame;
      }

      if (!finished_left) {
        frame = evaluated_range.min - 1;
      }
      else {
        frame = evaluated_range.max + 1;
      }
    }
  }

  /* Clear recalc flags from targets. */
  for (MPathTarget &mpt : targets) {
    bMotionPath *mpath = mpt.mpath;

    /* Get pointer to animviz settings for each target. */
    bAnimVizSettings *avs = animviz_target_settings_get(mpt);

    /* Clear the flag requesting recalculation of targets. */
    avs->recalc &= ~ANIMVIZ_RECALC_PATHS;

    /* Clean temp data. */
    ED_keylist_free(mpt.keylist);

    /* Free previous batches to force update. */
    GPU_VERTBUF_DISCARD_SAFE(mpath->points_vbo);
    GPU_BATCH_DISCARD_SAFE(mpath->batch_line);
    GPU_BATCH_DISCARD_SAFE(mpath->batch_points);
  }
}

}  // namespace blender
