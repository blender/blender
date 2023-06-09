/* SPDX-FileCopyrightText: 2015 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "DNA_armature_types.h"
#include "DNA_constraint_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_action.h"
#include "BKE_anim_path.h"
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

  bConstraint *con;             /* constraint for this chain */
  bSplineIKConstraint *ik_data; /* constraint settings for this chain */
} tSplineIK_Tree;

/* ----------- */

/* Tag the bones in the chain formed by the given bone for IK. */
static void splineik_init_tree_from_pchan(Scene *UNUSED(scene),
                                          Object *UNUSED(ob),
                                          bPoseChannel *pchan_tip)
{
  bPoseChannel *pchan, *pchan_root = NULL;
  bPoseChannel *pchan_chain[255];
  bConstraint *con = NULL;
  bSplineIKConstraint *ik_data = NULL;
  float bone_lengths[255];
  float totlength = 0.0f;
  int segcount = 0;

  /* Find the SplineIK constraint. */
  for (con = pchan_tip->constraints.first; con; con = con->next) {
    if (con->type == CONSTRAINT_TYPE_SPLINEIK) {
      ik_data = con->data;

      /* Target can only be a curve. */
      if ((ik_data->tar == NULL) || (ik_data->tar->type != OB_CURVES_LEGACY)) {
        continue;
      }
      /* Skip if disabled. */
      if ((con->enforce == 0.0f) || (con->flag & (CONSTRAINT_DISABLE | CONSTRAINT_OFF))) {
        continue;
      }

      /* Otherwise, constraint is ok... */
      break;
    }
  }
  if (con == NULL) {
    return;
  }

  /* Find the root bone and the chain of bones from the root to the tip.
   * NOTE: this assumes that the bones are connected, but that may not be true... */
  for (pchan = pchan_tip; pchan && (segcount < ik_data->chainlen);
       pchan = pchan->parent, segcount++) {
    /* Store this segment in the chain. */
    pchan_chain[segcount] = pchan;

    /* If performing rebinding, calculate the length of the bone. */
    bone_lengths[segcount] = pchan->bone->length;
    totlength += bone_lengths[segcount];
  }

  if (segcount == 0) {
    return;
  }

  pchan_root = pchan_chain[segcount - 1];

  /* Perform binding step if required. */
  if ((ik_data->flag & CONSTRAINT_SPLINEIK_BOUND) == 0) {
    float segmentLen = (1.0f / (float)segcount);

    /* Setup new empty array for the points list. */
    if (ik_data->points) {
      MEM_freeN(ik_data->points);
    }
    ik_data->numpoints = ik_data->chainlen + 1;
    ik_data->points = MEM_mallocN(sizeof(float) * ik_data->numpoints, "Spline IK Binding");

    /* Bind 'tip' of chain (i.e. first joint = tip of bone with the Spline IK Constraint). */
    ik_data->points[0] = 1.0f;

    /* Perform binding of the joints to parametric positions along the curve based
     * proportion of the total length that each bone occupies.
     */
    for (int i = 0; i < segcount; i++) {
      /* 'head' joints, traveling towards the root of the chain.
       * - 2 methods; the one chosen depends on whether we've got usable lengths.
       */
      if ((ik_data->flag & CONSTRAINT_SPLINEIK_EVENSPLITS) || (totlength == 0.0f)) {
        /* 1) Equi-spaced joints. */
        ik_data->points[i + 1] = ik_data->points[i] - segmentLen;
      }
      else {
        /* 2) To find this point on the curve, we take a step from the previous joint
         *    a distance given by the proportion that this bone takes.
         */
        ik_data->points[i + 1] = ik_data->points[i] - (bone_lengths[i] / totlength);
      }
    }

    /* Spline has now been bound. */
    ik_data->flag |= CONSTRAINT_SPLINEIK_BOUND;
  }

  /* Disallow negative values (happens with float precision). */
  CLAMP_MIN(ik_data->points[segcount], 0.0f);

  /* Make a new Spline-IK chain, and store it in the IK chains. */
  /* TODO: we should check if there is already an IK chain on this,
   * since that would take precedence... */
  {
    /* Make a new tree. */
    tSplineIK_Tree *tree = MEM_callocN(sizeof(tSplineIK_Tree), "SplineIK Tree");
    tree->type = CONSTRAINT_TYPE_SPLINEIK;

    tree->chainlen = segcount;
    tree->totlength = totlength;

    /* Copy over the array of links to bones in the chain (from tip to root). */
    tree->chain = MEM_mallocN(sizeof(bPoseChannel *) * segcount, "SplineIK Chain");
    memcpy(tree->chain, pchan_chain, sizeof(bPoseChannel *) * segcount);

    /* Store reference to joint position array. */
    tree->points = ik_data->points;

    /* Store references to different parts of the chain. */
    tree->root = pchan_root;
    tree->con = con;
    tree->ik_data = ik_data;

    /* AND! Link the tree to the root. */
    BLI_addtail(&pchan_root->siktree, tree);
  }

  /* Mark root channel having an IK tree. */
  pchan_root->flag |= POSE_IKSPLINE;
}

/* Tag which bones are members of Spline IK chains. */
static void splineik_init_tree(Scene *scene, Object *ob, float UNUSED(ctime))
{
  bPoseChannel *pchan;

  /* Find the tips of Spline IK chains,
   * which are simply the bones which have been tagged as such. */
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
  float prev_tail_loc[3];    /* Tail location of the previous bone. */
  float prev_tail_radius;    /* Tail curve radius of the previous bone. */
  int prev_tail_seg_idx;     /* Curve segment the previous tail bone belongs to. */
} tSplineIk_EvalState;

/* Prepare data to evaluate spline IK. */
static bool splineik_evaluate_init(tSplineIK_Tree *tree, tSplineIk_EvalState *state)
{
  bSplineIKConstraint *ik_data = tree->ik_data;

  /* Make sure that the constraint targets are ok, to avoid crashes
   * in case of a depsgraph bug or dependency cycle.
   */
  if (ik_data->tar == NULL) {
    return false;
  }

  CurveCache *cache = ik_data->tar->runtime.curve_cache;

  if (ELEM(NULL, cache, cache->anim_path_accum_length)) {
    return false;
  }

  /* Initialize the evaluation state. */
  state->curve_position = 0.0f;
  state->curve_scale = 1.0f;
  unit_m4(state->locrot_offset);
  zero_v3(state->prev_tail_loc);
  state->prev_tail_radius = 1.0f;
  state->prev_tail_seg_idx = 0;

  /* Apply corrections for sensitivity to scaling. */
  if ((ik_data->yScaleMode != CONSTRAINT_SPLINEIK_YS_FIT_CURVE) && (tree->totlength != 0.0f)) {
    /* Get the current length of the curve. */
    /* NOTE: This is assumed to be correct even after the curve was resized. */
    const float spline_len = BKE_anim_path_get_length(cache);

    /* Calculate the scale factor to multiply all the path values by so that the
     * bone chain retains its current length, such that:
     *     maxScale * splineLen = totLength
     */
    state->curve_scale = tree->totlength / spline_len;
  }

  return true;
}

static void apply_curve_transform(
    bSplineIKConstraint *ik_data, Object *ob, float radius, float r_vec[3], float *r_radius)
{
  /* Apply the curve's object-mode transforms to the position
   * unless the option to allow curve to be positioned elsewhere is activated (i.e. no root).
   */
  if ((ik_data->flag & CONSTRAINT_SPLINEIK_NO_ROOT) == 0) {
    mul_m4_v3(ik_data->tar->object_to_world, r_vec);
  }

  /* Convert the position to pose-space. */
  mul_m4_v3(ob->world_to_object, r_vec);

  /* Set the new radius (it should be the average value). */
  *r_radius = (radius + *r_radius) / 2;
}

static float dist_to_sphere_shell(const float sphere_origin[3],
                                  const float sphere_radius,
                                  const float point[3])
{
  float vec[3];
  sub_v3_v3v3(vec, sphere_origin, point);
  return sphere_radius - len_v3(vec);
}

/* This function positions the tail of the bone so that it preserves the length of it.
 * The length of the bone can be seen as a sphere radius.
 */
static int position_tail_on_spline(bSplineIKConstraint *ik_data,
                                   const float head_pos[3],
                                   const float sphere_radius,
                                   int prev_seg_idx,
                                   float r_tail_pos[3],
                                   float *r_new_curve_pos,
                                   float *r_radius)
{
  /* This is using the tessellated curve data.
   * So we are working with piece-wise linear curve segments.
   * The same method is used in #BKE_where_on_path to get curve location data. */
  const CurveCache *cache = ik_data->tar->runtime.curve_cache;
  const float *seg_accum_len = cache->anim_path_accum_length;

  int max_seg_idx = BKE_anim_path_get_array_size(cache) - 1;

  /* Make an initial guess of where our intersection point will be.
   * If the curve was a straight line, then the fraction passed in r_new_curve_pos
   * would be the correct location.
   * So make it our first initial guess.
   */
  const float spline_len = BKE_anim_path_get_length(cache);
  const float guessed_len = *r_new_curve_pos * spline_len;

  BLI_assert(prev_seg_idx >= 0);
  int cur_seg_idx = prev_seg_idx;
  while (cur_seg_idx < max_seg_idx && guessed_len > seg_accum_len[cur_seg_idx]) {
    cur_seg_idx++;
  }

  /* Convert the segment to bev points.
   * For example, the segment with index 0 will have bev points 0 and 1.
   */
  int bp_idx = cur_seg_idx + 1;

  const BevList *bl = cache->bev.first;
  bool is_cyclic = bl->poly >= 0;
  BevPoint *bp = bl->bevpoints;
  BevPoint *prev_bp;
  bp = bp + bp_idx;
  prev_bp = bp - 1;

  /* Go to the next tessellated curve point until we cross to outside of the sphere. */
  while (len_v3v3(head_pos, bp->vec) < sphere_radius) {
    if (bp_idx > max_seg_idx) {
      /* We are outside the defined curve. We will now extrapolate the intersection point. */
      break;
    }
    prev_bp = bp;
    if (is_cyclic && bp_idx == max_seg_idx) {
      /* Wrap around to the start point.
       * Don't set the bp_idx to zero here as we use it to get the segment index later.
       */
      bp = bl->bevpoints;
    }
    else {
      bp++;
    }
    bp_idx++;
  }

  /* Calculate the intersection point using the secant root finding method */
  float x0 = 0.0f, x1 = 1.0f, x2 = 0.5f;
  float x0_point[3], x1_point[3], start_p[3];
  float epsilon = max_fff(1.0f, len_v3(head_pos), len_v3(bp->vec)) * FLT_EPSILON;

  if (prev_seg_idx == bp_idx - 1) {
    /* The intersection lies inside the same segment as the last point.
     * Set the last point to be the start search point so we minimize the risks of going backwards
     * on the curve.
     */
    copy_v3_v3(start_p, head_pos);
  }
  else {
    copy_v3_v3(start_p, prev_bp->vec);
  }

  for (int i = 0; i < 10; i++) {
    interp_v3_v3v3(x0_point, start_p, bp->vec, x0);
    interp_v3_v3v3(x1_point, start_p, bp->vec, x1);

    float f_x0 = dist_to_sphere_shell(head_pos, sphere_radius, x0_point);
    float f_x1 = dist_to_sphere_shell(head_pos, sphere_radius, x1_point);

    if (fabsf(f_x1) <= epsilon || f_x0 == f_x1) {
      break;
    }

    x2 = x1 - f_x1 * (x1 - x0) / (f_x1 - f_x0);
    x0 = x1;
    x1 = x2;
  }
  /* Found the bone tail position! */
  copy_v3_v3(r_tail_pos, x1_point);

  /* Because our intersection point lies inside the current segment,
   * Convert our bevpoint index back to the previous segment index (-2 instead of -1).
   * This is because our actual location is prev_seg_len + isect_seg_len.
   */
  prev_seg_idx = bp_idx - 2;
  float prev_seg_len = 0;

  if (prev_seg_idx < 0) {
    prev_seg_idx = 0;
    prev_seg_len = 0;
  }
  else {
    prev_seg_len = seg_accum_len[prev_seg_idx];
  }

  /* Convert the point back into the 0-1 interpolation range. */
  const float isect_seg_len = len_v3v3(prev_bp->vec, r_tail_pos);
  const float frac = isect_seg_len / len_v3v3(prev_bp->vec, bp->vec);
  *r_new_curve_pos = (prev_seg_len + isect_seg_len) / spline_len;

  if (*r_new_curve_pos > 1.0f) {
    *r_radius = bp->radius;
  }
  else {
    *r_radius = (1.0f - frac) * prev_bp->radius + frac * bp->radius;
  }

  /* Return the current segment. */
  return bp_idx - 1;
}

/* Evaluate spline IK for a given bone. */
static void splineik_evaluate_bone(
    tSplineIK_Tree *tree, Object *ob, bPoseChannel *pchan, int index, tSplineIk_EvalState *state)
{
  bSplineIKConstraint *ik_data = tree->ik_data;

  if (pchan->bone->length < FLT_EPSILON) {
    /* Only move the bone position with zero length bones. */
    float bone_pos[4], rad;
    BKE_where_on_path(ik_data->tar, state->curve_position, bone_pos, NULL, NULL, &rad, NULL);

    apply_curve_transform(ik_data, ob, rad, bone_pos, &rad);

    copy_v3_v3(pchan->pose_mat[3], bone_pos);
    copy_v3_v3(pchan->pose_head, bone_pos);
    copy_v3_v3(pchan->pose_tail, bone_pos);
    pchan->flag |= POSE_DONE;
    return;
  }

  float orig_head[3], orig_tail[3], pose_head[3], pose_tail[3];
  float base_pose_mat[3][3], pose_mat[3][3];
  float spline_vec[3], scale_fac, radius = 1.0f;
  float tail_blend_fac = 0.0f;

  mul_v3_m4v3(pose_head, state->locrot_offset, pchan->pose_head);
  mul_v3_m4v3(pose_tail, state->locrot_offset, pchan->pose_tail);

  copy_v3_v3(orig_head, pose_head);

  /* First, adjust the point positions on the curve. */
  float curveLen = tree->points[index] - tree->points[index + 1];
  float bone_len = len_v3v3(pose_head, pose_tail);
  float point_start = state->curve_position;
  float pose_scale = bone_len / pchan->bone->length;
  float base_scale = 1.0f;

  if (ik_data->yScaleMode == CONSTRAINT_SPLINEIK_YS_ORIGINAL) {
    /* Carry over the bone Y scale to the curve range. */
    base_scale = pose_scale;
  }

  float point_end = point_start + curveLen * base_scale * state->curve_scale;

  state->curve_position = point_end;

  /* Step 1: determine the positions for the endpoints of the bone. */
  if (point_start < 1.0f) {
    float vec[4], rad;
    radius = 0.0f;

    /* Calculate head position. */
    if (point_start == 0.0f) {
      /* Start of the path. We have no previous tail position to copy. */
      BKE_where_on_path(ik_data->tar, point_start, vec, NULL, NULL, &rad, NULL);
    }
    else {
      copy_v3_v3(vec, state->prev_tail_loc);
      rad = state->prev_tail_radius;
    }

    radius = rad;
    copy_v3_v3(pose_head, vec);
    apply_curve_transform(ik_data, ob, rad, pose_head, &radius);

    /* Calculate tail position. */
    if (ik_data->yScaleMode != CONSTRAINT_SPLINEIK_YS_FIT_CURVE) {
      float sphere_radius;

      if (ik_data->yScaleMode == CONSTRAINT_SPLINEIK_YS_ORIGINAL) {
        sphere_radius = bone_len;
      }
      else {
        /* Don't take bone scale into account. */
        sphere_radius = pchan->bone->length;
      }

      /* Calculate the tail position with sphere curve intersection. */
      state->prev_tail_seg_idx = position_tail_on_spline(
          ik_data, vec, sphere_radius, state->prev_tail_seg_idx, pose_tail, &point_end, &rad);

      state->prev_tail_radius = rad;
      copy_v3_v3(state->prev_tail_loc, pose_tail);

      apply_curve_transform(ik_data, ob, rad, pose_tail, &radius);
      state->curve_position = point_end;
    }
    else {
      /* Scale to fit curve end position. */
      if (BKE_where_on_path(ik_data->tar, point_end, vec, NULL, NULL, &rad, NULL)) {
        state->prev_tail_radius = rad;
        copy_v3_v3(state->prev_tail_loc, vec);
        copy_v3_v3(pose_tail, vec);
        apply_curve_transform(ik_data, ob, rad, pose_tail, &radius);
      }
    }

    /* Determine if the bone should still be affected by SplineIK.
     * This makes it so that the bone slowly becomes poseable again the further it rolls off the
     * curve. When the whole bone has rolled off the curve, the IK constraint will not influence it
     * anymore.
     */
    if (point_end >= 1.0f) {
      /* Blending factor depends on the amount of the bone still left on the chain. */
      tail_blend_fac = (1.0f - point_start) / (point_end - point_start);
    }
    else {
      tail_blend_fac = 1.0f;
    }
  }

  /* Step 2: determine the implied transform from these endpoints.
   * - splineVec: the vector direction that the spline applies on the bone.
   * - scaleFac: the factor that the bone length is scaled by to get the desired amount.
   */
  sub_v3_v3v3(spline_vec, pose_tail, pose_head);
  scale_fac = len_v3(spline_vec) / pchan->bone->length;

  /* Step 3: compute the shortest rotation needed
   * to map from the bone rotation to the current axis.
   * - this uses the same method as is used for the Damped Track Constraint
   *   (see the code there for details).
   */
  {
    float dmat[3][3], rmat[3][3];
    float raxis[3], rangle;

    /* Compute the raw rotation matrix from the bone's current matrix by extracting only the
     * orientation-relevant axes, and normalizing them.
     */
    mul_m3_m4m4(base_pose_mat, state->locrot_offset, pchan->pose_mat);
    normalize_m3_m3(rmat, base_pose_mat);

    /* Also, normalize the orientation imposed by the bone,
     * now that we've extracted the scale factor. */
    normalize_v3(spline_vec);

    /* Calculate smallest axis-angle rotation necessary for getting from the
     * current orientation of the bone, to the spline-imposed direction.
     */
    cross_v3_v3v3(raxis, rmat[1], spline_vec);

    /* Check if the old and new bone direction is parallel to each other.
     * If they are, then 'raxis' should be near zero and we will have to get the rotation axis in
     * some other way.
     */
    float norm = normalize_v3(raxis);

    if (norm < FLT_EPSILON) {
      /* Can't use cross product! */
      int order[3] = {0, 1, 2};
      float tmp_axis[3];
      zero_v3(tmp_axis);

      axis_sort_v3(spline_vec, order);

      /* Use the second largest axis as the basis for the rotation axis. */
      tmp_axis[order[1]] = 1.0f;
      cross_v3_v3v3(raxis, tmp_axis, spline_vec);
    }

    rangle = dot_v3v3(rmat[1], spline_vec);
    CLAMP(rangle, -1.0f, 1.0f);
    rangle = acosf(rangle);

    /* Multiply the magnitude of the angle by the influence of the constraint to
     * control the influence of the SplineIK effect.
     */
    rangle *= tree->con->enforce * tail_blend_fac;

    /* Construct rotation matrix from the axis-angle rotation found above.
     * - This call takes care to make sure that the axis provided is a unit vector first.
     */
    axis_angle_to_mat3(dmat, raxis, rangle);

    /* Combine these rotations so that the y-axis of the bone is now aligned as the
     * spline dictates, while still maintaining roll control from the existing bone animation. */
    mul_m3_m3m3(pose_mat, dmat, rmat);

    /* Attempt to reduce shearing, though I doubt this will really help too much now. */
    normalize_m3(pose_mat);

    mul_m3_m3m3(base_pose_mat, dmat, base_pose_mat);

    /* Apply rotation to the accumulated parent transform. */
    mul_m4_m3m4(state->locrot_offset, dmat, state->locrot_offset);
  }

  /* Step 4: Set the scaling factors for the axes. */

  /* Always multiply the y-axis by the scaling factor to get the correct length. */
  mul_v3_fl(pose_mat[1], scale_fac);

  /* After that, apply x/z scaling modes. */
  if (ik_data->xzScaleMode != CONSTRAINT_SPLINEIK_XZS_NONE) {
    /* First, apply the original scale if enabled. */
    if (ik_data->xzScaleMode == CONSTRAINT_SPLINEIK_XZS_ORIGINAL ||
        (ik_data->flag & CONSTRAINT_SPLINEIK_USE_ORIGINAL_SCALE) != 0)
    {
      float scale;

      /* X-axis scale. */
      scale = len_v3(pchan->pose_mat[0]);
      mul_v3_fl(pose_mat[0], scale);
      /* Z-axis scale. */
      scale = len_v3(pchan->pose_mat[2]);
      mul_v3_fl(pose_mat[2], scale);

      /* Adjust the scale factor used for volume preservation
       * to consider the pre-IK scaling as the initial volume. */
      scale_fac /= pose_scale;
    }

    /* Apply volume preservation. */
    switch (ik_data->xzScaleMode) {
      case CONSTRAINT_SPLINEIK_XZS_INVERSE: {
        /* Old 'volume preservation' method using the inverse scale. */
        float scale;

        /* Calculate volume preservation factor which is
         * basically the inverse of the y-scaling factor.
         */
        if (fabsf(scale_fac) != 0.0f) {
          scale = 1.0f / fabsf(scale_fac);

          /* We need to clamp this within sensible values. */
          /* NOTE: these should be fine for now, but should get sanitized in future. */
          CLAMP(scale, 0.0001f, 100000.0f);
        }
        else {
          scale = 1.0f;
        }

        /* Apply the scaling. */
        mul_v3_fl(pose_mat[0], scale);
        mul_v3_fl(pose_mat[2], scale);
        break;
      }
      case CONSTRAINT_SPLINEIK_XZS_VOLUMETRIC: {
        /* Improved volume preservation based on the Stretch To constraint. */
        float final_scale;

        /* As the basis for volume preservation, we use the inverse scale factor... */
        if (fabsf(scale_fac) != 0.0f) {
          /* NOTE: The method here is taken wholesale from the Stretch To constraint. */
          float bulge = powf(1.0f / fabsf(scale_fac), ik_data->bulge);

          if (bulge > 1.0f) {
            if (ik_data->flag & CONSTRAINT_SPLINEIK_USE_BULGE_MAX) {
              float bulge_max = max_ff(ik_data->bulge_max, 1.0f);
              float hard = min_ff(bulge, bulge_max);

              float range = bulge_max - 1.0f;
              float scale = (range > 0.0f) ? 1.0f / range : 0.0f;
              float soft = 1.0f + range * atanf((bulge - 1.0f) * scale) / (float)M_PI_2;

              bulge = interpf(soft, hard, ik_data->bulge_smooth);
            }
          }
          if (bulge < 1.0f) {
            if (ik_data->flag & CONSTRAINT_SPLINEIK_USE_BULGE_MIN) {
              float bulge_min = CLAMPIS(ik_data->bulge_min, 0.0f, 1.0f);
              float hard = max_ff(bulge, bulge_min);

              float range = 1.0f - bulge_min;
              float scale = (range > 0.0f) ? 1.0f / range : 0.0f;
              float soft = 1.0f - range * atanf((1.0f - bulge) * scale) / (float)M_PI_2;

              bulge = interpf(soft, hard, ik_data->bulge_smooth);
            }
          }

          /* Compute scale factor for xz axes from this value. */
          final_scale = sqrtf(bulge);
        }
        else {
          /* No scaling, so scale factor is simple. */
          final_scale = 1.0f;
        }

        /* Apply the scaling (assuming normalized scale). */
        mul_v3_fl(pose_mat[0], final_scale);
        mul_v3_fl(pose_mat[2], final_scale);
        break;
      }
    }
  }

  /* Finally, multiply the x and z scaling by the radius of the curve too,
   * to allow automatic scales to get tweaked still.
   */
  if ((ik_data->flag & CONSTRAINT_SPLINEIK_NO_CURVERAD) == 0) {
    mul_v3_fl(pose_mat[0], radius);
    mul_v3_fl(pose_mat[2], radius);
  }

  /* Blend the scaling of the matrix according to the influence. */
  sub_m3_m3m3(pose_mat, pose_mat, base_pose_mat);
  madd_m3_m3m3fl(pose_mat, base_pose_mat, pose_mat, tree->con->enforce * tail_blend_fac);

  /* Step 5: Set the location of the bone in the matrix. */
  if (ik_data->flag & CONSTRAINT_SPLINEIK_NO_ROOT) {
    /* When the 'no-root' option is affected, the chain can retain
     * the shape but be moved elsewhere.
     */
    copy_v3_v3(pose_head, orig_head);
  }
  else if (tree->con->enforce < 1.0f) {
    /* When the influence is too low:
     * - Blend the positions for the 'root' bone.
     * - Stick to the parent for any other.
     */
    if (index < tree->chainlen - 1) {
      copy_v3_v3(pose_head, orig_head);
    }
    else {
      interp_v3_v3v3(pose_head, orig_head, pose_head, tree->con->enforce);
    }
  }

  /* Finally, store the new transform. */
  copy_m4_m3(pchan->pose_mat, pose_mat);
  copy_v3_v3(pchan->pose_mat[3], pose_head);
  copy_v3_v3(pchan->pose_head, pose_head);

  mul_v3_mat3_m4v3(orig_tail, state->locrot_offset, pchan->pose_tail);

  /* Recalculate tail, as it's now outdated after the head gets adjusted above! */
  BKE_pose_where_is_bone_tail(pchan);

  /* Update the offset in the accumulated parent transform. */
  sub_v3_v3v3(state->locrot_offset[3], pchan->pose_tail, orig_tail);

  /* Done! */
  pchan->flag |= POSE_DONE;
}

/* Evaluate the chain starting from the nominated bone */
static void splineik_execute_tree(
    Depsgraph *depsgraph, Scene *scene, Object *ob, bPoseChannel *pchan_root, float ctime)
{
  tSplineIK_Tree *tree;

  /* for each pose-tree, execute it if it is spline, otherwise just free it */
  while ((tree = pchan_root->siktree.first) != NULL) {
    /* Firstly, calculate the bone matrix the standard way,
     * since this is needed for roll control. */
    for (int i = tree->chainlen - 1; i >= 0; i--) {
      BKE_pose_where_is_bone(depsgraph, scene, ob, tree->chain[i], ctime, 1);
    }

    /* After that, evaluate the actual Spline IK, unless there are missing dependencies. */
    tSplineIk_EvalState state;

    if (splineik_evaluate_init(tree, &state)) {
      /* Walk over each bone in the chain, calculating the effects of spline IK
       * - the chain is traversed in the opposite order to storage order
       *   (i.e. parent to children) so that dependencies are correct
       */
      for (int i = tree->chainlen - 1; i >= 0; i--) {
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
    Depsgraph *depsgraph, Scene *scene, Object *ob, bPoseChannel *pchan_root, float ctime)
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

void BKE_pose_eval_init(Depsgraph *depsgraph, Scene *UNUSED(scene), Object *object)
{
  bPose *pose = object->pose;
  BLI_assert(pose != NULL);

  DEG_debug_print_eval(depsgraph, __func__, object->id.name, object);

  BLI_assert(object->type == OB_ARMATURE);

  /* We demand having proper pose. */
  BLI_assert(object->pose != NULL);
  BLI_assert((object->pose->flag & POSE_RECALC) == 0);

  /* world_to_object is needed for solvers. */
  invert_m4_m4(object->world_to_object, object->object_to_world);

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

void BKE_pose_eval_init_ik(Depsgraph *depsgraph, Scene *scene, Object *object)
{
  DEG_debug_print_eval(depsgraph, __func__, object->id.name, object);
  BLI_assert(object->type == OB_ARMATURE);
  const float ctime = BKE_scene_ctime_get(scene); /* not accurate... */
  bArmature *armature = (bArmature *)object->data;
  if (armature->flag & ARM_RESTPOS) {
    return;
  }
  /* construct the IK tree (standard IK) */
  BIK_init_tree(depsgraph, scene, object, ctime);
  /* construct the Spline IK trees
   * - this is not integrated as an IK plugin, since it should be able
   *   to function in conjunction with standard IK. */
  BKE_pose_splineik_init_tree(scene, object, ctime);
}

void BKE_pose_eval_bone(Depsgraph *depsgraph, Scene *scene, Object *object, int pchan_index)
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
          float ctime = BKE_scene_ctime_get(scene); /* not accurate... */
          BKE_pose_where_is_bone(depsgraph, scene, object, pchan, ctime, 1);
        }
      }
    }
  }
}

void BKE_pose_constraints_evaluate(Depsgraph *depsgraph,
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
  if (pchan->flag & POSE_IKTREE || pchan->flag & POSE_IKSPLINE) {
    /* IK are being solved separately/ */
  }
  else {
    if ((pchan->flag & POSE_DONE) == 0) {
      float ctime = BKE_scene_ctime_get(scene); /* not accurate... */
      BKE_pose_where_is_bone(depsgraph, scene, object, pchan, ctime, 1);
    }
  }
}

static void pose_channel_flush_to_orig_if_needed(Depsgraph *depsgraph,
                                                 Object *object,
                                                 bPoseChannel *pchan)
{
  if (!DEG_is_active(depsgraph)) {
    return;
  }
  const bArmature *armature = (bArmature *)object->data;
  if (armature->edbo != NULL) {
    return;
  }
  bPoseChannel *pchan_orig = pchan->orig_pchan;
  /* TODO(sergey): Using BKE_pose_copy_pchan_result() introduces #70901, but why? */
  copy_m4_m4(pchan_orig->pose_mat, pchan->pose_mat);
  copy_m4_m4(pchan_orig->chan_mat, pchan->chan_mat);
  copy_v3_v3(pchan_orig->pose_head, pchan->pose_mat[3]);
  copy_m4_m4(pchan_orig->constinv, pchan->constinv);
  copy_v3_v3(pchan_orig->pose_tail, pchan->pose_tail);
}

void BKE_pose_bone_done(Depsgraph *depsgraph, Object *object, int pchan_index)
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
  pose_channel_flush_to_orig_if_needed(depsgraph, object, pchan);
  if (DEG_is_active(depsgraph)) {
    bPoseChannel *pchan_orig = pchan->orig_pchan;
    if (pchan->bone == NULL || pchan->bone->segments <= 1) {
      BKE_pose_channel_free_bbone_cache(&pchan_orig->runtime);
    }
  }
}

void BKE_pose_eval_bbone_segments(Depsgraph *depsgraph, Object *object, int pchan_index)
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

void BKE_pose_iktree_evaluate(Depsgraph *depsgraph,
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
  const float ctime = BKE_scene_ctime_get(scene); /* not accurate... */
  if (armature->flag & ARM_RESTPOS) {
    return;
  }
  BIK_execute_tree(depsgraph, scene, object, rootchan, ctime);
}

void BKE_pose_splineik_evaluate(Depsgraph *depsgraph,
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
  const float ctime = BKE_scene_ctime_get(scene); /* not accurate... */
  if (armature->flag & ARM_RESTPOS) {
    return;
  }
  BKE_splineik_execute_tree(depsgraph, scene, object, rootchan, ctime);
}

static void pose_eval_cleanup_common(Object *object)
{
  bPose *pose = object->pose;
  BLI_assert(pose != NULL);
  BLI_assert(pose->chan_array != NULL || BLI_listbase_is_empty(&pose->chanbase));
  UNUSED_VARS_NDEBUG(pose);
}

void BKE_pose_eval_done(Depsgraph *depsgraph, Object *object)
{
  bPose *pose = object->pose;
  BLI_assert(pose != NULL);
  UNUSED_VARS_NDEBUG(pose);
  DEG_debug_print_eval(depsgraph, __func__, object->id.name, object);
  BLI_assert(object->type == OB_ARMATURE);
}

void BKE_pose_eval_cleanup(Depsgraph *depsgraph, Scene *scene, Object *object)
{
  bPose *pose = object->pose;
  BLI_assert(pose != NULL);
  UNUSED_VARS_NDEBUG(pose);
  const float ctime = BKE_scene_ctime_get(scene); /* not accurate... */
  DEG_debug_print_eval(depsgraph, __func__, object->id.name, object);
  BLI_assert(object->type == OB_ARMATURE);
  /* Release the IK tree. */
  BIK_release_tree(scene, object, ctime);
  pose_eval_cleanup_common(object);
}
