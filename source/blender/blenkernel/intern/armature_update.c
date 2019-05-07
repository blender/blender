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
 * The Original Code is Copyright (C) 2015 Blender Foundation.
 * All rights reserved.
 *
 * Defines and code for core node types
 */

/** \file
 * \ingroup bke
 */

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_listbase.h"
#include "BLI_math.h"

#include "DNA_armature_types.h"
#include "DNA_constraint_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_action.h"
#include "BKE_anim.h"
#include "BKE_armature.h"
#include "BKE_curve.h"
#include "BKE_displist.h"
#include "BKE_fcurve.h"
#include "BKE_object.h"
#include "BKE_scene.h"

#include "BIK_api.h"

#include "DEG_depsgraph.h"

/* ********************** SPLINE IK SOLVER ******************* */

/* Temporary evaluation tree data used for Spline IK */
typedef struct tSplineIK_Tree {
  struct tSplineIK_Tree *next, *prev;

  int type; /* type of IK that this serves (CONSTRAINT_TYPE_KINEMATIC or ..._SPLINEIK) */

  short chainlen;  /* number of bones in the chain */
  float totlength; /* total length of bones in the chain */

  const float *points;  /* parametric positions for the joints along the curve */
  bPoseChannel **chain; /* chain of bones to affect using Spline IK (ordered from the tip) */

  bPoseChannel *root; /* bone that is the root node of the chain */

  bConstraint *con;            /* constraint for this chain */
  bSplineIKConstraint *ikData; /* constraint settings for this chain */
} tSplineIK_Tree;

/* ----------- */

/* Tag the bones in the chain formed by the given bone for IK */
static void splineik_init_tree_from_pchan(Scene *UNUSED(scene),
                                          Object *UNUSED(ob),
                                          bPoseChannel *pchan_tip)
{
  bPoseChannel *pchan, *pchanRoot = NULL;
  bPoseChannel *pchanChain[255];
  bConstraint *con = NULL;
  bSplineIKConstraint *ikData = NULL;
  float boneLengths[255];
  float totLength = 0.0f;
  int segcount = 0;

  /* find the SplineIK constraint */
  for (con = pchan_tip->constraints.first; con; con = con->next) {
    if (con->type == CONSTRAINT_TYPE_SPLINEIK) {
      ikData = con->data;

      /* target can only be curve */
      if ((ikData->tar == NULL) || (ikData->tar->type != OB_CURVE)) {
        continue;
      }
      /* skip if disabled */
      if ((con->enforce == 0.0f) || (con->flag & (CONSTRAINT_DISABLE | CONSTRAINT_OFF))) {
        continue;
      }

      /* otherwise, constraint is ok... */
      break;
    }
  }
  if (con == NULL) {
    return;
  }

  /* find the root bone and the chain of bones from the root to the tip
   * NOTE: this assumes that the bones are connected, but that may not be true... */
  for (pchan = pchan_tip; pchan && (segcount < ikData->chainlen);
       pchan = pchan->parent, segcount++) {
    /* store this segment in the chain */
    pchanChain[segcount] = pchan;

    /* if performing rebinding, calculate the length of the bone */
    boneLengths[segcount] = pchan->bone->length;
    totLength += boneLengths[segcount];
  }

  if (segcount == 0) {
    return;
  }
  else {
    pchanRoot = pchanChain[segcount - 1];
  }

  /* perform binding step if required */
  if ((ikData->flag & CONSTRAINT_SPLINEIK_BOUND) == 0) {
    float segmentLen = (1.0f / (float)segcount);
    int i;

    /* setup new empty array for the points list */
    if (ikData->points) {
      MEM_freeN(ikData->points);
    }
    ikData->numpoints = ikData->chainlen + 1;
    ikData->points = MEM_mallocN(sizeof(float) * ikData->numpoints, "Spline IK Binding");

    /* bind 'tip' of chain (i.e. first joint = tip of bone with the Spline IK Constraint) */
    ikData->points[0] = 1.0f;

    /* perform binding of the joints to parametric positions along the curve based
     * proportion of the total length that each bone occupies
     */
    for (i = 0; i < segcount; i++) {
      /* 'head' joints, traveling towards the root of the chain
       * - 2 methods; the one chosen depends on whether we've got usable lengths
       */
      if ((ikData->flag & CONSTRAINT_SPLINEIK_EVENSPLITS) || (totLength == 0.0f)) {
        /* 1) equi-spaced joints */
        ikData->points[i + 1] = ikData->points[i] - segmentLen;
      }
      else {
        /* 2) to find this point on the curve, we take a step from the previous joint
         *    a distance given by the proportion that this bone takes
         */
        ikData->points[i + 1] = ikData->points[i] - (boneLengths[i] / totLength);
      }
    }

    /* spline has now been bound */
    ikData->flag |= CONSTRAINT_SPLINEIK_BOUND;
  }

  /* disallow negative values (happens with float precision) */
  CLAMP_MIN(ikData->points[segcount], 0.0f);

  /* make a new Spline-IK chain, and store it in the IK chains */
  /* TODO: we should check if there is already an IK chain on this,
   * since that would take precedence... */
  {
    /* make new tree */
    tSplineIK_Tree *tree = MEM_callocN(sizeof(tSplineIK_Tree), "SplineIK Tree");
    tree->type = CONSTRAINT_TYPE_SPLINEIK;

    tree->chainlen = segcount;
    tree->totlength = totLength;

    /* copy over the array of links to bones in the chain (from tip to root) */
    tree->chain = MEM_mallocN(sizeof(bPoseChannel *) * segcount, "SplineIK Chain");
    memcpy(tree->chain, pchanChain, sizeof(bPoseChannel *) * segcount);

    /* store reference to joint position array */
    tree->points = ikData->points;

    /* store references to different parts of the chain */
    tree->root = pchanRoot;
    tree->con = con;
    tree->ikData = ikData;

    /* AND! link the tree to the root */
    BLI_addtail(&pchanRoot->siktree, tree);
  }

  /* mark root channel having an IK tree */
  pchanRoot->flag |= POSE_IKSPLINE;
}

/* Tag which bones are members of Spline IK chains */
static void splineik_init_tree(Scene *scene, Object *ob, float UNUSED(ctime))
{
  bPoseChannel *pchan;

  /* find the tips of Spline IK chains,
   * which are simply the bones which have been tagged as such */
  for (pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
    if (pchan->constflag & PCHAN_HAS_SPLINEIK) {
      splineik_init_tree_from_pchan(scene, ob, pchan);
    }
  }
}

/* ----------- */

typedef struct tSplineIk_EvalState {
  float curve_position;      /* Current position along the curve. */
  float curve_scale;         /* Global scale to apply to curve positions. */
  float locrot_offset[4][4]; /* Bone rotation and location offset inherited from parent. */
} tSplineIk_EvalState;

/* Prepare data to evaluate spline IK. */
static bool splineik_evaluate_init(tSplineIK_Tree *tree, tSplineIk_EvalState *state)
{
  bSplineIKConstraint *ikData = tree->ikData;

  /* Make sure that the constraint targets are ok, to avoid crashes
   * in case of a depsgraph bug or dependency cycle.
   */
  if (ikData->tar == NULL) {
    return false;
  }

  CurveCache *cache = ikData->tar->runtime.curve_cache;

  if (ELEM(NULL, cache, cache->path, cache->path->data)) {
    return false;
  }

  /* Initialize the evaluation state. */
  state->curve_position = 0.0f;
  state->curve_scale = 1.0f;
  unit_m4(state->locrot_offset);

  /* Apply corrections for sensitivity to scaling. */
  if ((ikData->yScaleMode != CONSTRAINT_SPLINEIK_YS_FIT_CURVE) && (tree->totlength != 0.0f)) {
    /* get the current length of the curve */
    /* NOTE: this is assumed to be correct even after the curve was resized */
    float splineLen = cache->path->totdist;

    /* calculate the scale factor to multiply all the path values by so that the
     * bone chain retains its current length, such that
     *     maxScale * splineLen = totLength
     */
    state->curve_scale = tree->totlength / splineLen;
  }

  return true;
}

/* Evaluate spline IK for a given bone. */
static void splineik_evaluate_bone(
    tSplineIK_Tree *tree, Object *ob, bPoseChannel *pchan, int index, tSplineIk_EvalState *state)
{
  bSplineIKConstraint *ikData = tree->ikData;
  float origHead[3], origTail[3], poseHead[3], poseTail[3], basePoseMat[3][3], poseMat[3][3];
  float splineVec[3], scaleFac, radius = 1.0f;
  float tailBlendFac = 0.0f;

  mul_v3_m4v3(poseHead, state->locrot_offset, pchan->pose_head);
  mul_v3_m4v3(poseTail, state->locrot_offset, pchan->pose_tail);

  copy_v3_v3(origHead, poseHead);

  /* first, adjust the point positions on the curve */
  float curveLen = tree->points[index] - tree->points[index + 1];
  float pointStart = state->curve_position;
  float poseScale = len_v3v3(poseHead, poseTail) / pchan->bone->length;
  float baseScale = 1.0f;

  if (ikData->yScaleMode == CONSTRAINT_SPLINEIK_YS_ORIGINAL) {
    /* Carry over the bone Y scale to the curve range. */
    baseScale = poseScale;
  }

  float pointEnd = pointStart + curveLen * baseScale * state->curve_scale;

  state->curve_position = pointEnd;

  /* step 1: determine the positions for the endpoints of the bone */
  if (pointStart < 1.0f) {
    float vec[4], dir[3], rad;

    /* determine if the bone should still be affected by SplineIK */
    if (pointEnd >= 1.0f) {
      /* blending factor depends on the amount of the bone still left on the chain */
      tailBlendFac = (1.0f - pointStart) / (pointEnd - pointStart);
    }
    else {
      tailBlendFac = 1.0f;
    }

    /* tail endpoint */
    if (where_on_path(ikData->tar, pointEnd, vec, dir, NULL, &rad, NULL)) {
      /* apply curve's object-mode transforms to the position
       * unless the option to allow curve to be positioned elsewhere is activated (i.e. no root)
       */
      if ((ikData->flag & CONSTRAINT_SPLINEIK_NO_ROOT) == 0) {
        mul_m4_v3(ikData->tar->obmat, vec);
      }

      /* convert the position to pose-space, then store it */
      mul_m4_v3(ob->imat, vec);
      copy_v3_v3(poseTail, vec);

      /* set the new radius */
      radius = rad;
    }

    /* head endpoint */
    if (where_on_path(ikData->tar, pointStart, vec, dir, NULL, &rad, NULL)) {
      /* apply curve's object-mode transforms to the position
       * unless the option to allow curve to be positioned elsewhere is activated (i.e. no root)
       */
      if ((ikData->flag & CONSTRAINT_SPLINEIK_NO_ROOT) == 0) {
        mul_m4_v3(ikData->tar->obmat, vec);
      }

      /* store the position, and convert it to pose space */
      mul_m4_v3(ob->imat, vec);
      copy_v3_v3(poseHead, vec);

      /* set the new radius (it should be the average value) */
      radius = (radius + rad) / 2;
    }
  }

  /* Step 2: determine the implied transform from these endpoints.
   * - splineVec: the vector direction that the spline applies on the bone.
   * - scaleFac: the factor that the bone length is scaled by to get the desired amount.
   */
  sub_v3_v3v3(splineVec, poseTail, poseHead);
  scaleFac = len_v3(splineVec) / pchan->bone->length;

  /* Adjust the scale factor towards the neutral state when rolling off the curve end. */
  scaleFac = interpf(scaleFac, baseScale, tailBlendFac);

  /* Step 3: compute the shortest rotation needed
   * to map from the bone rotation to the current axis.
   * - this uses the same method as is used for the Damped Track Constraint
   *   (see the code there for details).
   */
  {
    float dmat[3][3], rmat[3][3];
    float raxis[3], rangle;

    /* compute the raw rotation matrix from the bone's current matrix by extracting only the
     * orientation-relevant axes, and normalizing them
     */
    mul_m3_m4m4(basePoseMat, state->locrot_offset, pchan->pose_mat);
    normalize_m3_m3(rmat, basePoseMat);

    /* Also, normalize the orientation imposed by the bone,
     * now that we've extracted the scale factor. */
    normalize_v3(splineVec);

    /* calculate smallest axis-angle rotation necessary for getting from the
     * current orientation of the bone, to the spline-imposed direction
     */
    cross_v3_v3v3(raxis, rmat[1], splineVec);

    rangle = dot_v3v3(rmat[1], splineVec);
    CLAMP(rangle, -1.0f, 1.0f);
    rangle = acosf(rangle);

    /* multiply the magnitude of the angle by the influence of the constraint to
     * control the influence of the SplineIK effect
     */
    rangle *= tree->con->enforce * tailBlendFac;

    /* construct rotation matrix from the axis-angle rotation found above
     * - this call takes care to make sure that the axis provided is a unit vector first
     */
    axis_angle_to_mat3(dmat, raxis, rangle);

    /* Combine these rotations so that the y-axis of the bone is now aligned as the
     * spline dictates, while still maintaining roll control from the existing bone animation. */
    mul_m3_m3m3(poseMat, dmat, rmat);

    /* attempt to reduce shearing, though I doubt this'll really help too much now... */
    normalize_m3(poseMat);

    mul_m3_m3m3(basePoseMat, dmat, basePoseMat);

    /* apply rotation to the accumulated parent transform */
    mul_m4_m3m4(state->locrot_offset, dmat, state->locrot_offset);
  }

  /* step 4: set the scaling factors for the axes */

  /* Always multiply the y-axis by the scaling factor to get the correct length. */
  mul_v3_fl(poseMat[1], scaleFac);

  /* After that, apply x/z scaling modes. */
  if (ikData->xzScaleMode != CONSTRAINT_SPLINEIK_XZS_NONE) {
    /* First, apply the original scale if enabled. */
    if (ikData->xzScaleMode == CONSTRAINT_SPLINEIK_XZS_ORIGINAL ||
        (ikData->flag & CONSTRAINT_SPLINEIK_USE_ORIGINAL_SCALE) != 0) {
      float scale;

      /* x-axis scale */
      scale = len_v3(pchan->pose_mat[0]);
      mul_v3_fl(poseMat[0], scale);
      /* z-axis scale */
      scale = len_v3(pchan->pose_mat[2]);
      mul_v3_fl(poseMat[2], scale);

      /* Adjust the scale factor used for volume preservation
       * to consider the pre-IK scaling as the initial volume. */
      scaleFac /= poseScale;
    }

    /* Apply volume preservation. */
    switch (ikData->xzScaleMode) {
      case CONSTRAINT_SPLINEIK_XZS_INVERSE: {
        /* old 'volume preservation' method using the inverse scale */
        float scale;

        /* calculate volume preservation factor which is
         * basically the inverse of the y-scaling factor
         */
        if (fabsf(scaleFac) != 0.0f) {
          scale = 1.0f / fabsf(scaleFac);

          /* we need to clamp this within sensible values */
          /* NOTE: these should be fine for now, but should get sanitised in future */
          CLAMP(scale, 0.0001f, 100000.0f);
        }
        else {
          scale = 1.0f;
        }

        /* apply the scaling */
        mul_v3_fl(poseMat[0], scale);
        mul_v3_fl(poseMat[2], scale);
        break;
      }
      case CONSTRAINT_SPLINEIK_XZS_VOLUMETRIC: {
        /* improved volume preservation based on the Stretch To constraint */
        float final_scale;

        /* as the basis for volume preservation, we use the inverse scale factor... */
        if (fabsf(scaleFac) != 0.0f) {
          /* NOTE: The method here is taken wholesale from the Stretch To constraint */
          float bulge = powf(1.0f / fabsf(scaleFac), ikData->bulge);

          if (bulge > 1.0f) {
            if (ikData->flag & CONSTRAINT_SPLINEIK_USE_BULGE_MAX) {
              float bulge_max = max_ff(ikData->bulge_max, 1.0f);
              float hard = min_ff(bulge, bulge_max);

              float range = bulge_max - 1.0f;
              float scale = (range > 0.0f) ? 1.0f / range : 0.0f;
              float soft = 1.0f + range * atanf((bulge - 1.0f) * scale) / (float)M_PI_2;

              bulge = interpf(soft, hard, ikData->bulge_smooth);
            }
          }
          if (bulge < 1.0f) {
            if (ikData->flag & CONSTRAINT_SPLINEIK_USE_BULGE_MIN) {
              float bulge_min = CLAMPIS(ikData->bulge_min, 0.0f, 1.0f);
              float hard = max_ff(bulge, bulge_min);

              float range = 1.0f - bulge_min;
              float scale = (range > 0.0f) ? 1.0f / range : 0.0f;
              float soft = 1.0f - range * atanf((1.0f - bulge) * scale) / (float)M_PI_2;

              bulge = interpf(soft, hard, ikData->bulge_smooth);
            }
          }

          /* compute scale factor for xz axes from this value */
          final_scale = sqrtf(bulge);
        }
        else {
          /* no scaling, so scale factor is simple */
          final_scale = 1.0f;
        }

        /* apply the scaling (assuming normalised scale) */
        mul_v3_fl(poseMat[0], final_scale);
        mul_v3_fl(poseMat[2], final_scale);
        break;
      }
    }
  }

  /* Finally, multiply the x and z scaling by the radius of the curve too,
   * to allow automatic scales to get tweaked still.
   */
  if ((ikData->flag & CONSTRAINT_SPLINEIK_NO_CURVERAD) == 0) {
    mul_v3_fl(poseMat[0], radius);
    mul_v3_fl(poseMat[2], radius);
  }

  /* Blend the scaling of the matrix according to the influence. */
  sub_m3_m3m3(poseMat, poseMat, basePoseMat);
  madd_m3_m3m3fl(poseMat, basePoseMat, poseMat, tree->con->enforce * tailBlendFac);

  /* step 5: set the location of the bone in the matrix */
  if (ikData->flag & CONSTRAINT_SPLINEIK_NO_ROOT) {
    /* when the 'no-root' option is affected, the chain can retain
     * the shape but be moved elsewhere
     */
    copy_v3_v3(poseHead, origHead);
  }
  else if (tree->con->enforce < 1.0f) {
    /* when the influence is too low
     * - blend the positions for the 'root' bone
     * - stick to the parent for any other
     */
    if (index < tree->chainlen - 1) {
      copy_v3_v3(poseHead, origHead);
    }
    else {
      interp_v3_v3v3(poseHead, origHead, poseHead, tree->con->enforce);
    }
  }

  /* finally, store the new transform */
  copy_m4_m3(pchan->pose_mat, poseMat);
  copy_v3_v3(pchan->pose_mat[3], poseHead);
  copy_v3_v3(pchan->pose_head, poseHead);

  mul_v3_mat3_m4v3(origTail, state->locrot_offset, pchan->pose_tail);

  /* recalculate tail, as it's now outdated after the head gets adjusted above! */
  BKE_pose_where_is_bone_tail(pchan);

  /* update the offset in the accumulated parent transform */
  sub_v3_v3v3(state->locrot_offset[3], pchan->pose_tail, origTail);

  /* done! */
  pchan->flag |= POSE_DONE;
}

/* Evaluate the chain starting from the nominated bone */
static void splineik_execute_tree(
    struct Depsgraph *depsgraph, Scene *scene, Object *ob, bPoseChannel *pchan_root, float ctime)
{
  tSplineIK_Tree *tree;

  /* for each pose-tree, execute it if it is spline, otherwise just free it */
  while ((tree = pchan_root->siktree.first) != NULL) {
    int i;

    /* Firstly, calculate the bone matrix the standard way,
     * since this is needed for roll control. */
    for (i = tree->chainlen - 1; i >= 0; i--) {
      BKE_pose_where_is_bone(depsgraph, scene, ob, tree->chain[i], ctime, 1);
    }

    /* After that, evaluate the actual Spline IK, unless there are missing dependencies. */
    tSplineIk_EvalState state;

    if (splineik_evaluate_init(tree, &state)) {
      /* Walk over each bone in the chain, calculating the effects of spline IK
       * - the chain is traversed in the opposite order to storage order (i.e. parent to children)
       *   so that dependencies are correct
       */
      for (i = tree->chainlen - 1; i >= 0; i--) {
        bPoseChannel *pchan = tree->chain[i];
        splineik_evaluate_bone(tree, ob, pchan, i, &state);
      }
    }

    /* free the tree info specific to SplineIK trees now */
    if (tree->chain) {
      MEM_freeN(tree->chain);
    }

    /* free this tree */
    BLI_freelinkN(&pchan_root->siktree, tree);
  }
}

void BKE_pose_splineik_init_tree(Scene *scene, Object *ob, float ctime)
{
  splineik_init_tree(scene, ob, ctime);
}

void BKE_splineik_execute_tree(
    struct Depsgraph *depsgraph, Scene *scene, Object *ob, bPoseChannel *pchan_root, float ctime)
{
  splineik_execute_tree(depsgraph, scene, ob, pchan_root, ctime);
}

/* *************** Depsgraph evaluation callbacks ************ */

void BKE_pose_pchan_index_rebuild(bPose *pose)
{
  MEM_SAFE_FREE(pose->chan_array);
  const int num_channels = BLI_listbase_count(&pose->chanbase);
  pose->chan_array = MEM_malloc_arrayN(num_channels, sizeof(bPoseChannel *), "pose->chan_array");
  int pchan_index = 0;
  for (bPoseChannel *pchan = pose->chanbase.first; pchan != NULL; pchan = pchan->next) {
    pose->chan_array[pchan_index++] = pchan;
  }
}

BLI_INLINE bPoseChannel *pose_pchan_get_indexed(Object *ob, int pchan_index)
{
  bPose *pose = ob->pose;
  BLI_assert(pose != NULL);
  BLI_assert(pose->chan_array != NULL);
  BLI_assert(pchan_index >= 0);
  BLI_assert(pchan_index < MEM_allocN_len(pose->chan_array) / sizeof(bPoseChannel *));
  return pose->chan_array[pchan_index];
}

void BKE_pose_eval_init(struct Depsgraph *depsgraph, Scene *UNUSED(scene), Object *object)
{
  bPose *pose = object->pose;
  BLI_assert(pose != NULL);

  DEG_debug_print_eval(depsgraph, __func__, object->id.name, object);

  BLI_assert(object->type == OB_ARMATURE);

  /* We demand having proper pose. */
  BLI_assert(object->pose != NULL);
  BLI_assert((object->pose->flag & POSE_RECALC) == 0);

  /* imat is needed for solvers. */
  invert_m4_m4(object->imat, object->obmat);

  /* clear flags */
  for (bPoseChannel *pchan = pose->chanbase.first; pchan != NULL; pchan = pchan->next) {
    pchan->flag &= ~(POSE_DONE | POSE_CHAIN | POSE_IKTREE | POSE_IKSPLINE);

    /* Free B-Bone shape data cache if it's not a B-Bone. */
    if (pchan->bone == NULL || pchan->bone->segments <= 1) {
      BKE_pose_channel_free_bbone_cache(&pchan->runtime);
    }
  }

  BLI_assert(pose->chan_array != NULL || BLI_listbase_is_empty(&pose->chanbase));
}

void BKE_pose_eval_init_ik(struct Depsgraph *depsgraph, Scene *scene, Object *object)
{
  DEG_debug_print_eval(depsgraph, __func__, object->id.name, object);
  BLI_assert(object->type == OB_ARMATURE);
  const float ctime = BKE_scene_frame_get(scene); /* not accurate... */
  bArmature *armature = (bArmature *)object->data;
  if (armature->flag & ARM_RESTPOS) {
    return;
  }
  /* construct the IK tree (standard IK) */
  BIK_initialize_tree(depsgraph, scene, object, ctime);
  /* construct the Spline IK trees
   * - this is not integrated as an IK plugin, since it should be able
   *   to function in conjunction with standard IK.  */
  BKE_pose_splineik_init_tree(scene, object, ctime);
}

void BKE_pose_eval_bone(struct Depsgraph *depsgraph, Scene *scene, Object *object, int pchan_index)
{
  const bArmature *armature = (bArmature *)object->data;
  if (armature->edbo != NULL) {
    return;
  }
  bPoseChannel *pchan = pose_pchan_get_indexed(object, pchan_index);
  DEG_debug_print_eval_subdata(
      depsgraph, __func__, object->id.name, object, "pchan", pchan->name, pchan);
  BLI_assert(object->type == OB_ARMATURE);
  if (armature->flag & ARM_RESTPOS) {
    Bone *bone = pchan->bone;
    if (bone) {
      copy_m4_m4(pchan->pose_mat, bone->arm_mat);
      copy_v3_v3(pchan->pose_head, bone->arm_head);
      copy_v3_v3(pchan->pose_tail, bone->arm_tail);
    }
  }
  else {
    /* TODO(sergey): Currently if there are constraints full transform is
     * being evaluated in BKE_pose_constraints_evaluate. */
    if (pchan->constraints.first == NULL) {
      if (pchan->flag & POSE_IKTREE || pchan->flag & POSE_IKSPLINE) {
        /* pass */
      }
      else {
        if ((pchan->flag & POSE_DONE) == 0) {
          /* TODO(sergey): Use time source node for time. */
          float ctime = BKE_scene_frame_get(scene); /* not accurate... */
          BKE_pose_where_is_bone(depsgraph, scene, object, pchan, ctime, 1);
        }
      }
    }
  }
}

void BKE_pose_constraints_evaluate(struct Depsgraph *depsgraph,
                                   Scene *scene,
                                   Object *object,
                                   int pchan_index)
{
  const bArmature *armature = (bArmature *)object->data;
  if (armature->edbo != NULL) {
    return;
  }
  bPoseChannel *pchan = pose_pchan_get_indexed(object, pchan_index);
  DEG_debug_print_eval_subdata(
      depsgraph, __func__, object->id.name, object, "pchan", pchan->name, pchan);
  if (armature->flag & ARM_RESTPOS) {
    return;
  }
  else if (pchan->flag & POSE_IKTREE || pchan->flag & POSE_IKSPLINE) {
    /* IK are being solved separately/ */
  }
  else {
    if ((pchan->flag & POSE_DONE) == 0) {
      float ctime = BKE_scene_frame_get(scene); /* not accurate... */
      BKE_pose_where_is_bone(depsgraph, scene, object, pchan, ctime, 1);
    }
  }
}

void BKE_pose_bone_done(struct Depsgraph *depsgraph, struct Object *object, int pchan_index)
{
  const bArmature *armature = (bArmature *)object->data;
  if (armature->edbo != NULL) {
    return;
  }
  bPoseChannel *pchan = pose_pchan_get_indexed(object, pchan_index);
  float imat[4][4];
  DEG_debug_print_eval_subdata(
      depsgraph, __func__, object->id.name, object, "pchan", pchan->name, pchan);
  if (pchan->bone) {
    invert_m4_m4(imat, pchan->bone->arm_mat);
    mul_m4_m4m4(pchan->chan_mat, pchan->pose_mat, imat);
    if (!(pchan->bone->flag & BONE_NO_DEFORM)) {
      mat4_to_dquat(&pchan->runtime.deform_dual_quat, pchan->bone->arm_mat, pchan->chan_mat);
    }
  }
  if (DEG_is_active(depsgraph) && armature->edbo == NULL) {
    bPoseChannel *pchan_orig = pchan->orig_pchan;
    copy_m4_m4(pchan_orig->pose_mat, pchan->pose_mat);
    copy_m4_m4(pchan_orig->chan_mat, pchan->chan_mat);
    copy_v3_v3(pchan_orig->pose_head, pchan->pose_mat[3]);
    copy_m4_m4(pchan_orig->constinv, pchan->constinv);
    BKE_pose_where_is_bone_tail(pchan_orig);
    if (pchan->bone == NULL || pchan->bone->segments <= 1) {
      BKE_pose_channel_free_bbone_cache(&pchan_orig->runtime);
    }
  }
}

void BKE_pose_eval_bbone_segments(struct Depsgraph *depsgraph,
                                  struct Object *object,
                                  int pchan_index)
{
  const bArmature *armature = (bArmature *)object->data;
  if (armature->edbo != NULL) {
    return;
  }
  bPoseChannel *pchan = pose_pchan_get_indexed(object, pchan_index);
  DEG_debug_print_eval_subdata(
      depsgraph, __func__, object->id.name, object, "pchan", pchan->name, pchan);
  if (pchan->bone != NULL && pchan->bone->segments > 1) {
    BKE_pchan_bbone_segments_cache_compute(pchan);
    if (DEG_is_active(depsgraph)) {
      BKE_pchan_bbone_segments_cache_copy(pchan->orig_pchan, pchan);
    }
  }
}

void BKE_pose_iktree_evaluate(struct Depsgraph *depsgraph,
                              Scene *scene,
                              Object *object,
                              int rootchan_index)
{
  const bArmature *armature = (bArmature *)object->data;
  if (armature->edbo != NULL) {
    return;
  }
  bPoseChannel *rootchan = pose_pchan_get_indexed(object, rootchan_index);
  DEG_debug_print_eval_subdata(
      depsgraph, __func__, object->id.name, object, "rootchan", rootchan->name, rootchan);
  BLI_assert(object->type == OB_ARMATURE);
  const float ctime = BKE_scene_frame_get(scene); /* not accurate... */
  if (armature->flag & ARM_RESTPOS) {
    return;
  }
  BIK_execute_tree(depsgraph, scene, object, rootchan, ctime);
}

void BKE_pose_splineik_evaluate(struct Depsgraph *depsgraph,
                                Scene *scene,
                                Object *object,
                                int rootchan_index)

{
  const bArmature *armature = (bArmature *)object->data;
  if (armature->edbo != NULL) {
    return;
  }
  bPoseChannel *rootchan = pose_pchan_get_indexed(object, rootchan_index);
  DEG_debug_print_eval_subdata(
      depsgraph, __func__, object->id.name, object, "rootchan", rootchan->name, rootchan);
  BLI_assert(object->type == OB_ARMATURE);
  const float ctime = BKE_scene_frame_get(scene); /* not accurate... */
  if (armature->flag & ARM_RESTPOS) {
    return;
  }
  BKE_splineik_execute_tree(depsgraph, scene, object, rootchan, ctime);
}

/* Common part for both original and proxy armatrues. */
static void pose_eval_done_common(struct Depsgraph *depsgraph, Object *object)
{
  const bArmature *armature = (bArmature *)object->data;
  if (armature->edbo != NULL) {
    return;
  }
  bPose *pose = object->pose;
  UNUSED_VARS_NDEBUG(pose);
  BLI_assert(pose != NULL);
  BKE_object_eval_boundbox(depsgraph, object);
}
static void pose_eval_cleanup_common(Object *object)
{
  bPose *pose = object->pose;
  BLI_assert(pose != NULL);
  BLI_assert(pose->chan_array != NULL || BLI_listbase_is_empty(&pose->chanbase));
  UNUSED_VARS_NDEBUG(pose);
}

void BKE_pose_eval_done(struct Depsgraph *depsgraph, Object *object)
{
  bPose *pose = object->pose;
  BLI_assert(pose != NULL);
  UNUSED_VARS_NDEBUG(pose);
  DEG_debug_print_eval(depsgraph, __func__, object->id.name, object);
  BLI_assert(object->type == OB_ARMATURE);
  pose_eval_done_common(depsgraph, object);
}

void BKE_pose_eval_cleanup(struct Depsgraph *depsgraph, Scene *scene, Object *object)
{
  bPose *pose = object->pose;
  BLI_assert(pose != NULL);
  UNUSED_VARS_NDEBUG(pose);
  const float ctime = BKE_scene_frame_get(scene); /* not accurate... */
  DEG_debug_print_eval(depsgraph, __func__, object->id.name, object);
  BLI_assert(object->type == OB_ARMATURE);
  /* Release the IK tree. */
  BIK_release_tree(scene, object, ctime);
  pose_eval_cleanup_common(object);
}

void BKE_pose_eval_proxy_init(struct Depsgraph *depsgraph, Object *object)
{
  BLI_assert(ID_IS_LINKED(object) && object->proxy_from != NULL);
  DEG_debug_print_eval(depsgraph, __func__, object->id.name, object);

  BLI_assert(object->pose->chan_array != NULL || BLI_listbase_is_empty(&object->pose->chanbase));
}

void BKE_pose_eval_proxy_done(struct Depsgraph *depsgraph, Object *object)
{
  BLI_assert(ID_IS_LINKED(object) && object->proxy_from != NULL);
  DEG_debug_print_eval(depsgraph, __func__, object->id.name, object);
  pose_eval_done_common(depsgraph, object);
}

void BKE_pose_eval_proxy_cleanup(struct Depsgraph *depsgraph, Object *object)
{
  BLI_assert(ID_IS_LINKED(object) && object->proxy_from != NULL);
  DEG_debug_print_eval(depsgraph, __func__, object->id.name, object);
  pose_eval_cleanup_common(object);
}

void BKE_pose_eval_proxy_copy_bone(struct Depsgraph *depsgraph, Object *object, int pchan_index)
{
  const bArmature *armature = (bArmature *)object->data;
  if (armature->edbo != NULL) {
    return;
  }
  BLI_assert(ID_IS_LINKED(object) && object->proxy_from != NULL);
  bPoseChannel *pchan = pose_pchan_get_indexed(object, pchan_index);
  DEG_debug_print_eval_subdata(
      depsgraph, __func__, object->id.name, object, "pchan", pchan->name, pchan);
  /* TODO(sergey): Use indexec lookup, once it's guaranteed to be kept
   * around for the time while proxies are evaluating.
   */
#if 0
  bPoseChannel *pchan_from = pose_pchan_get_indexed(object->proxy_from, pchan_index);
#else
  bPoseChannel *pchan_from = BKE_pose_channel_find_name(object->proxy_from->pose, pchan->name);
#endif
  BLI_assert(pchan != NULL);
  BLI_assert(pchan_from != NULL);
  BKE_pose_copyesult_pchan_result(pchan, pchan_from);
  copy_dq_dq(&pchan->runtime.deform_dual_quat, &pchan_from->runtime.deform_dual_quat);
  BKE_pchan_bbone_segments_cache_copy(pchan, pchan_from);
}
