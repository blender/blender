/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "MEM_guardedalloc.h"

#include <cstdlib>

#include "BLI_listbase.h"
#include "BLI_math_matrix.h"
#include "BLI_math_matrix.hh"

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
#include "ANIM_bone_collections.hh"

#include "CLG_log.h"

static CLG_LogRef LOG = {"ed.anim.motion_paths"};

/* Motion path needing to be baked (mpt). */
struct MPathTarget {
  bMotionPath *mpath; /* Motion path in question. */

  AnimKeylist *keylist; /* Temp, to know where the keyframes are. */

  /* Original (Source Objects) */
  Object *ob;          /* Source Object */
  bPoseChannel *pchan; /* Source pose-channel (if applicable). */

  /* "Evaluated" Copies (these come from the background evaluated copy
   * that provide all the coordinates we want to save off). */
  Object *ob_eval; /* Evaluated Object. */
};

/* ........ */

/* Update scene for current frame. */
static void motionpaths_calc_update_scene(Depsgraph *depsgraph)
{
  BKE_scene_graph_update_for_newframe(depsgraph);
}

Depsgraph *animviz_depsgraph_build(Main *bmain,
                                   Scene *scene,
                                   ViewLayer *view_layer,
                                   blender::Span<MPathTarget *> targets)
{
  /* Allocate dependency graph. */
  Depsgraph *depsgraph = DEG_graph_new(bmain, scene, view_layer, DAG_EVAL_VIEWPORT);

  /* Make a flat array of IDs for the DEG API. */
  blender::Array<ID *> ids(targets.size());
  int current_id_index = 0;
  for (const MPathTarget *mpt : targets) {
    ids[current_id_index++] = &mpt->ob->id;
  }

  /* Build graph from all requested IDs. */
  DEG_graph_build_from_ids(depsgraph, ids);

  /* Update once so we can access pointers of evaluated animation data. */
  motionpaths_calc_update_scene(depsgraph);
  return depsgraph;
}

void animviz_build_motionpath_targets(Object *ob, blender::Vector<MPathTarget *> &r_targets)
{
  /* TODO: it would be nice in future to be able to update objects dependent on these bones too? */

  MPathTarget *mpt;

  /* Object itself first. */
  if ((ob->avs.recalc & ANIMVIZ_RECALC_PATHS) && (ob->mpath)) {
    /* New target for object. */
    mpt = static_cast<MPathTarget *>(MEM_callocN(sizeof(MPathTarget), "MPathTarget Ob"));
    mpt->mpath = ob->mpath;
    mpt->ob = ob;

    r_targets.append(mpt);
  }

  /* Bones. */
  if ((ob->pose) && (ob->pose->avs.recalc & ANIMVIZ_RECALC_PATHS)) {
    bArmature *arm = static_cast<bArmature *>(ob->data);
    LISTBASE_FOREACH (bPoseChannel *, pchan, &ob->pose->chanbase) {
      if ((pchan->bone) && ANIM_bonecoll_is_visible_pchan(arm, pchan) && (pchan->mpath)) {
        /* New target for bone. */
        mpt = static_cast<MPathTarget *>(MEM_callocN(sizeof(MPathTarget), "MPathTarget PoseBone"));
        mpt->mpath = pchan->mpath;
        mpt->ob = ob;
        mpt->pchan = pchan;
        r_targets.append(mpt);
      }
    }
  }
}

void animviz_free_motionpath_targets(blender::Vector<MPathTarget *> &targets)
{
  for (MPathTarget *mpt : targets) {
    MEM_freeN(mpt);
  }
  targets.clear_and_shrink();
}

/* ........ */

/* Perform baking for the targets on the current frame. */
static void motionpaths_calc_bake_targets(blender::Span<MPathTarget *> targets,
                                          int cframe,
                                          Depsgraph *depsgraph,
                                          Object *camera)
{
  using namespace blender;
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

    Object *ob_eval = mpt->ob_eval;

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
      Object *cam_eval = DEG_get_evaluated_object(depsgraph, camera);
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

static void motionpath_get_global_framerange(blender::Span<MPathTarget *> targets,
                                             int *r_sfra,
                                             int *r_efra)
{
  *r_sfra = INT_MAX;
  *r_efra = INT_MIN;
  for (const MPathTarget *mpt : targets) {
    *r_sfra = min_ii(*r_sfra, mpt->mpath->start_frame);
    *r_efra = max_ii(*r_efra, mpt->mpath->end_frame);
  }
}

/* TODO(jbakker): Remove complexity, keylists are ordered. */
static int motionpath_get_prev_keyframe(MPathTarget *mpt, AnimKeylist *keylist, int current_frame)
{
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
                                             int current_frame)
{
  int frame = motionpath_get_prev_keyframe(mpt, keylist, current_frame);
  return motionpath_get_prev_keyframe(mpt, keylist, frame);
}

static int motionpath_get_next_keyframe(MPathTarget *mpt, AnimKeylist *keylist, int current_frame)
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
                                             int current_frame)
{
  int frame = motionpath_get_next_keyframe(mpt, keylist, current_frame);
  return motionpath_get_next_keyframe(mpt, keylist, frame);
}

static bool motionpath_check_can_use_keyframe_range(MPathTarget * /*mpt*/,
                                                    AnimData *adt,
                                                    ListBase *fcurve_list)
{
  if (adt == nullptr || fcurve_list == nullptr) {
    return false;
  }
  /* NOTE: We might needed to do a full frame range update if there is a specific setup of NLA
   * or drivers or modifiers on the f-curves. */
  return true;
}

static void motionpath_calculate_update_range(MPathTarget *mpt,
                                              AnimData *adt,
                                              ListBase *fcurve_list,
                                              int current_frame,
                                              int *r_sfra,
                                              int *r_efra)
{
  *r_sfra = INT_MAX;
  *r_efra = INT_MIN;

  /* If the current frame is outside of the configured motion path range we ignore update of this
   * motion path by using invalid frame range where start frame is above the end frame. */
  if (current_frame < mpt->mpath->start_frame || current_frame > mpt->mpath->end_frame) {
    return;
  }

  /* Similar to the case when there is only a single keyframe: need to update en entire range to
   * a constant value. */
  if (!motionpath_check_can_use_keyframe_range(mpt, adt, fcurve_list)) {
    *r_sfra = mpt->mpath->start_frame;
    *r_efra = mpt->mpath->end_frame;
    return;
  }

  /* NOTE: Iterate over individual f-curves, and check their keyframes individually and pick a
   * widest range from them. This is because it's possible to have more narrow keyframe on a
   * channel which wasn't edited.
   * Could be optimized further by storing some flags about which channels has been modified so
   * we ignore all others (which can potentially make an update range unnecessary wide). */
  for (FCurve *fcu = static_cast<FCurve *>(fcurve_list->first); fcu != nullptr; fcu = fcu->next) {
    AnimKeylist *keylist = ED_keylist_create();
    fcurve_to_keylist(adt, fcu, keylist, 0, {-FLT_MAX, FLT_MAX}, true);
    ED_keylist_prepare_for_direct_access(keylist);

    int fcu_sfra = motionpath_get_prev_prev_keyframe(mpt, keylist, current_frame);
    int fcu_efra = motionpath_get_next_next_keyframe(mpt, keylist, current_frame);

    /* Extend range further, since acceleration compensation propagates even further away. */
    if (fcu->auto_smoothing != FCURVE_SMOOTH_NONE) {
      fcu_sfra = motionpath_get_prev_prev_keyframe(mpt, keylist, fcu_sfra);
      fcu_efra = motionpath_get_next_next_keyframe(mpt, keylist, fcu_efra);
    }

    if (fcu_sfra <= fcu_efra) {
      *r_sfra = min_ii(*r_sfra, fcu_sfra);
      *r_efra = max_ii(*r_efra, fcu_efra);
    }

    ED_keylist_free(keylist);
  }
}

static void motionpath_free_free_tree_data(blender::MutableSpan<MPathTarget *> targets)
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
      !blender::animrig::legacy::assigned_action_has_keyframes(ob->adt))
  {
    /* Default to the scene (preview) range if there is no animation data to
     * find selected keys in. */
    avs->path_sf = PSFRA;
    avs->path_ef = PEFRA;
    return;
  }

  AnimKeylist *keylist = ED_keylist_create();
  for (FCurve *fcu : blender::animrig::legacy::fcurves_for_assigned_action(ob->adt)) {
    fcurve_to_keylist(ob->adt, fcu, keylist, 0, {-FLT_MAX, FLT_MAX}, true);
  }

  blender::Bounds<float> frame_range;
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

void animviz_calc_motionpaths(Depsgraph *depsgraph,
                              Main *bmain,
                              Scene *scene,
                              blender::MutableSpan<MPathTarget *> targets,
                              eAnimvizCalcRange range,
                              bool restore)
{
  /* TODO: include reports pointer? */
  using namespace blender::animrig;

  if (targets.is_empty()) {
    return;
  }

  const int cfra = scene->r.cfra;
  int sfra = INT_MAX, efra = INT_MIN;
  switch (range) {
    case ANIMVIZ_CALC_RANGE_CURRENT_FRAME:
      motionpath_get_global_framerange(targets, &sfra, &efra);
      if (sfra > efra) {
        return;
      }
      if (cfra < sfra || cfra > efra) {
        return;
      }
      sfra = efra = cfra;
      break;
    case ANIMVIZ_CALC_RANGE_CHANGED:
      /* Nothing to do here, will be handled later when iterating through the targets. */
      break;
    case ANIMVIZ_CALC_RANGE_FULL:
      motionpath_get_global_framerange(targets, &sfra, &efra);
      if (sfra > efra) {
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
    mpt->ob_eval = DEG_get_evaluated_object(depsgraph, mpt->ob);

    AnimData *adt = BKE_animdata_from_id(&mpt->ob_eval->id);

    /* Build list of all keyframes in active action for object or pchan. */
    mpt->keylist = ED_keylist_create();

    ListBase *fcurve_list = nullptr;
    if (adt && adt->action) {
      /* Get pointer to animviz settings for each target. */
      bAnimVizSettings *avs = animviz_target_settings_get(mpt);

      /* It is assumed that keyframes for bones are all grouped in a single group
       * unless an option is set to always use the whole action.
       */
      if ((mpt->pchan) && (avs->path_viewflag & MOTIONPATH_VIEW_KFACT) == 0) {
        Action &action = adt->action->wrap();
        bActionGroup *agrp = nullptr;
        if (action.is_action_layered()) {
          Channelbag *cbag = channelbag_for_action_slot(action, adt->slot_handle);
          agrp = cbag ? cbag->channel_group_find(mpt->pchan->name) : nullptr;
        }
        else {
          agrp = BKE_action_group_find_name(adt->action, mpt->pchan->name);
        }

        if (agrp) {
          fcurve_list = &agrp->channels;
          action_group_to_keylist(adt, agrp, mpt->keylist, 0, {-FLT_MAX, FLT_MAX});
        }
      }
      else {
        fcurve_list = &adt->action->curves;
        action_to_keylist(adt, adt->action, mpt->keylist, 0, {-FLT_MAX, FLT_MAX});
      }
    }
    ED_keylist_prepare_for_direct_access(mpt->keylist);

    if (range == ANIMVIZ_CALC_RANGE_CHANGED) {
      int mpt_sfra, mpt_efra;
      motionpath_calculate_update_range(mpt, adt, fcurve_list, cfra, &mpt_sfra, &mpt_efra);
      if (mpt_sfra <= mpt_efra) {
        sfra = min_ii(sfra, mpt_sfra);
        efra = max_ii(efra, mpt_efra);
      }
    }
  }

  if (sfra > efra) {
    motionpath_free_free_tree_data(targets);
    return;
  }

  /* Calculate path over requested range. */
  CLOG_INFO(&LOG,
            1,
            "Calculating MotionPaths between frames %d - %d (%d frames)",
            sfra,
            efra,
            efra - sfra + 1);
  for (scene->r.cfra = sfra; scene->r.cfra <= efra; scene->r.cfra++) {
    if (range == ANIMVIZ_CALC_RANGE_CURRENT_FRAME) {
      /* For current frame, only update tagged. */
      BKE_scene_graph_update_tagged(depsgraph, bmain);
    }
    else {
      /* Update relevant data for new frame. */
      motionpaths_calc_update_scene(depsgraph);
    }

    /* Perform baking for targets. */
    motionpaths_calc_bake_targets(targets, scene->r.cfra, depsgraph, scene->camera);
  }

  /* Reset original environment. */
  /* NOTE: We don't always need to reevaluate the main scene, as the depsgraph
   * may be a temporary one that works on a subset of the data.
   * We always have to restore the current frame though. */
  scene->r.cfra = cfra;
  if (range != ANIMVIZ_CALC_RANGE_CURRENT_FRAME && restore) {
    motionpaths_calc_update_scene(depsgraph);
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
