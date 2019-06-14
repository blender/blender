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
 * \ingroup bke
 */

#include "MEM_guardedalloc.h"

#include <stdlib.h>

#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_dlrbTree.h"

#include "DNA_anim_types.h"
#include "DNA_armature_types.h"
#include "DNA_scene_types.h"

#include "BKE_animsys.h"
#include "BKE_action.h"
#include "BKE_main.h"
#include "BKE_scene.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "GPU_batch.h"
#include "GPU_vertex_buffer.h"

#include "ED_anim_api.h"
#include "ED_keyframes_draw.h"

#include "CLG_log.h"

static CLG_LogRef LOG = {"ed.anim.motion_paths"};

/* Motion path needing to be baked (mpt) */
typedef struct MPathTarget {
  struct MPathTarget *next, *prev;

  bMotionPath *mpath; /* motion path in question */

  DLRBT_Tree keys; /* temp, to know where the keyframes are */

  /* Original (Source Objects) */
  Object *ob;          /* source object */
  bPoseChannel *pchan; /* source posechannel (if applicable) */

  /* "Evaluated" Copies (these come from the background COW copie
   * that provide all the coordinates we want to save off)
   */
  Object *ob_eval; /* evaluated object */
} MPathTarget;

/* ........ */

/* get list of motion paths to be baked for the given object
 * - assumes the given list is ready to be used
 */
/* TODO: it would be nice in future to be able to update objects dependent on these bones too? */
void animviz_get_object_motionpaths(Object *ob, ListBase *targets)
{
  MPathTarget *mpt;

  /* object itself first */
  if ((ob->avs.recalc & ANIMVIZ_RECALC_PATHS) && (ob->mpath)) {
    /* new target for object */
    mpt = MEM_callocN(sizeof(MPathTarget), "MPathTarget Ob");
    BLI_addtail(targets, mpt);

    mpt->mpath = ob->mpath;
    mpt->ob = ob;
  }

  /* bones */
  if ((ob->pose) && (ob->pose->avs.recalc & ANIMVIZ_RECALC_PATHS)) {
    bArmature *arm = ob->data;
    bPoseChannel *pchan;

    for (pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
      if ((pchan->bone) && (arm->layer & pchan->bone->layer) && (pchan->mpath)) {
        /* new target for bone */
        mpt = MEM_callocN(sizeof(MPathTarget), "MPathTarget PoseBone");
        BLI_addtail(targets, mpt);

        mpt->mpath = pchan->mpath;
        mpt->ob = ob;
        mpt->pchan = pchan;
      }
    }
  }
}

/* ........ */

/* update scene for current frame */
static void motionpaths_calc_update_scene(Main *bmain, struct Depsgraph *depsgraph)
{
  /* Do all updates
   *  - if this is too slow, resort to using a more efficient way
   *    that doesn't force complete update, but for now, this is the
   *    most accurate way!
   *
   * TODO(segey): Bring back partial updates, which became impossible
   * with the new depsgraph due to unsorted nature of bases.
   *
   * TODO(sergey): Use evaluation context dedicated to motion paths.
   */
  BKE_scene_graph_update_for_newframe(depsgraph, bmain);
}

/* ........ */

/* perform baking for the targets on the current frame */
static void motionpaths_calc_bake_targets(ListBase *targets, int cframe)
{
  MPathTarget *mpt;

  /* for each target, check if it can be baked on the current frame */
  for (mpt = targets->first; mpt; mpt = mpt->next) {
    bMotionPath *mpath = mpt->mpath;

    /* current frame must be within the range the cache works for
     * - is inclusive of the first frame, but not the last otherwise we get buffer overruns
     */
    if ((cframe < mpath->start_frame) || (cframe >= mpath->end_frame)) {
      continue;
    }

    /* get the relevant cache vert to write to */
    bMotionPathVert *mpv = mpath->points + (cframe - mpath->start_frame);

    Object *ob_eval = mpt->ob_eval;

    /* Lookup evaluated pose channel, here because the depsgraph
     * evaluation can change them so they are not cached in mpt. */
    bPoseChannel *pchan_eval = NULL;
    if (mpt->pchan) {
      pchan_eval = BKE_pose_channel_find_name(ob_eval->pose, mpt->pchan->name);
    }

    /* pose-channel or object path baking? */
    if (pchan_eval) {
      /* heads or tails */
      if (mpath->flag & MOTIONPATH_FLAG_BHEAD) {
        copy_v3_v3(mpv->co, pchan_eval->pose_head);
      }
      else {
        copy_v3_v3(mpv->co, pchan_eval->pose_tail);
      }

      /* result must be in worldspace */
      mul_m4_v3(ob_eval->obmat, mpv->co);
    }
    else {
      /* worldspace object location */
      copy_v3_v3(mpv->co, ob_eval->obmat[3]);
    }

    float mframe = (float)(cframe);

    /* Tag if it's a keyframe */
    if (BLI_dlrbTree_search_exact(&mpt->keys, compare_ak_cfraPtr, &mframe)) {
      mpv->flag |= MOTIONPATH_VERT_KEY;
    }
    else {
      mpv->flag &= ~MOTIONPATH_VERT_KEY;
    }

    /* Incremental update on evaluated object if possible, for fast updating
     * while dragging in transform. */
    bMotionPath *mpath_eval = NULL;
    if (mpt->pchan) {
      mpath_eval = (pchan_eval) ? pchan_eval->mpath : NULL;
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

/* Perform baking of the given object's and/or its bones' transforms to motion paths
 * - scene: current scene
 * - ob: object whose flagged motionpaths should get calculated
 * - recalc: whether we need to
 */
/* TODO: include reports pointer? */
void animviz_calc_motionpaths(Depsgraph *depsgraph,
                              Main *bmain,
                              Scene *scene,
                              ListBase *targets,
                              bool restore,
                              bool current_frame_only)
{
  /* sanity check */
  if (ELEM(NULL, targets, targets->first)) {
    return;
  }

  /* Compute frame range to bake within.
   * TODO: this method could be improved...
   * 1) max range for standard baking
   * 2) minimum range for recalc baking (i.e. between keyframes, but how?) */
  int sfra = INT_MAX;
  int efra = INT_MIN;

  for (MPathTarget *mpt = targets->first; mpt; mpt = mpt->next) {
    /* try to increase area to do (only as much as needed) */
    sfra = MIN2(sfra, mpt->mpath->start_frame);
    efra = MAX2(efra, mpt->mpath->end_frame);
  }

  if (efra <= sfra) {
    return;
  }

  /* Limit frame range if we are updating just the current frame. */
  /* set frame values */
  int cfra = CFRA;
  if (current_frame_only) {
    if (cfra < sfra || cfra > efra) {
      return;
    }
    sfra = efra = cfra;
  }

  /* get copies of objects/bones to get the calculated results from
   * (for copy-on-write evaluation), so that we actually get some results
   */

  /* TODO: Create a copy of background depsgraph that only contain these entities,
   * and only evaluates them.
   *
   * For until that is done we force dependency graph to not be active, so we don't loose unkeyed
   * changes during updating the motion path.
   * This still doesn't include unkeyed changes to the path itself, but allows to have updates in
   * an environment when auto-keying and pose paste is used. */

  const bool is_active_depsgraph = DEG_is_active(depsgraph);
  if (is_active_depsgraph) {
    DEG_make_inactive(depsgraph);
  }

  for (MPathTarget *mpt = targets->first; mpt; mpt = mpt->next) {
    mpt->ob_eval = DEG_get_evaluated_object(depsgraph, mpt->ob);

    AnimData *adt = BKE_animdata_from_id(&mpt->ob_eval->id);

    /* build list of all keyframes in active action for object or pchan */
    BLI_dlrbTree_init(&mpt->keys);

    if (adt) {
      bAnimVizSettings *avs;

      /* get pointer to animviz settings for each target */
      if (mpt->pchan) {
        avs = &mpt->ob->pose->avs;
      }
      else {
        avs = &mpt->ob->avs;
      }

      /* it is assumed that keyframes for bones are all grouped in a single group
       * unless an option is set to always use the whole action
       */
      if ((mpt->pchan) && (avs->path_viewflag & MOTIONPATH_VIEW_KFACT) == 0) {
        bActionGroup *agrp = BKE_action_group_find_name(adt->action, mpt->pchan->name);

        if (agrp) {
          agroup_to_keylist(adt, agrp, &mpt->keys, 0);
        }
      }
      else {
        action_to_keylist(adt, adt->action, &mpt->keys, 0);
      }
    }
  }

  /* calculate path over requested range */
  CLOG_INFO(&LOG,
            1,
            "Calculating MotionPaths between frames %d - %d (%d frames)",
            sfra,
            efra,
            efra - sfra + 1);
  for (CFRA = sfra; CFRA <= efra; CFRA++) {
    if (current_frame_only) {
      /* For current frame, only update tagged. */
      BKE_scene_graph_update_tagged(depsgraph, bmain);
    }
    else {
      /* Update relevant data for new frame. */
      motionpaths_calc_update_scene(bmain, depsgraph);
    }

    /* perform baking for targets */
    motionpaths_calc_bake_targets(targets, CFRA);
  }

  /* reset original environment */
  /* NOTE: We don't always need to reevaluate the main scene, as the depsgraph
   * may be a temporary one that works on a subset of the data. We always have
   * to resoture the current frame though. */
  CFRA = cfra;
  if (!current_frame_only && restore) {
    motionpaths_calc_update_scene(bmain, depsgraph);
  }

  if (is_active_depsgraph) {
    DEG_make_active(depsgraph);
  }

  /* clear recalc flags from targets */
  for (MPathTarget *mpt = targets->first; mpt; mpt = mpt->next) {
    bAnimVizSettings *avs;
    bMotionPath *mpath = mpt->mpath;

    /* get pointer to animviz settings for each target */
    if (mpt->pchan) {
      avs = &mpt->ob->pose->avs;
    }
    else {
      avs = &mpt->ob->avs;
    }

    /* clear the flag requesting recalculation of targets */
    avs->recalc &= ~ANIMVIZ_RECALC_PATHS;

    /* Clean temp data */
    BLI_dlrbTree_free(&mpt->keys);

    /* Free previous batches to force update. */
    GPU_VERTBUF_DISCARD_SAFE(mpath->points_vbo);
    GPU_BATCH_DISCARD_SAFE(mpath->batch_line);
    GPU_BATCH_DISCARD_SAFE(mpath->batch_points);
  }
}
