/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "MEM_guardedalloc.h"

#include <cfloat>

#include "DNA_curve_types.h"
#include "DNA_key_types.h"
#include "DNA_object_types.h"

#include "BLI_math_rotation.h"
#include "BLI_math_vector.h"

#include "BKE_anim_path.h"
#include "BKE_curve.hh"
#include "BKE_key.hh"
#include "BKE_object_types.hh"

#include "CLG_log.h"

static CLG_LogRef LOG = {"bke.anim"};

/* ******************************************************************** */
/* Curve Paths - for curve deforms and/or curve following */

static int get_bevlist_seg_array_size(const BevList *bl)
{
  if (bl->poly >= 0) {
    /* Cyclic curve. */
    return bl->nr;
  }

  return bl->nr - 1;
}

int BKE_anim_path_get_array_size(const CurveCache *curve_cache)
{
  BLI_assert(curve_cache != nullptr);

  BevList *bl = static_cast<BevList *>(curve_cache->bev.first);

  BLI_assert(bl != nullptr && bl->nr > 1);

  return get_bevlist_seg_array_size(bl);
}

float BKE_anim_path_get_length(const CurveCache *curve_cache)
{
  const int seg_size = BKE_anim_path_get_array_size(curve_cache);
  return curve_cache->anim_path_accum_length[seg_size - 1];
}

void BKE_anim_path_calc_data(Object *ob)
{
  if (ob == nullptr || ob->type != OB_CURVES_LEGACY) {
    return;
  }
  if (ob->runtime->curve_cache == nullptr) {
    CLOG_WARN(&LOG, "No curve cache!");
    return;
  }
  /* We only use the first curve. */
  BevList *bl = static_cast<BevList *>(ob->runtime->curve_cache->bev.first);
  if (bl == nullptr || !bl->nr) {
    CLOG_WARN(&LOG, "No bev list data!");
    return;
  }

  /* Free old data. */
  if (ob->runtime->curve_cache->anim_path_accum_length) {
    MEM_freeN((void *)ob->runtime->curve_cache->anim_path_accum_length);
  }

  /* We assume that we have at least two points.
   * If there is less than two points in the curve,
   * no BevList should have been generated.
   */
  BLI_assert(bl->nr > 1);

  const int seg_size = get_bevlist_seg_array_size(bl);
  float *len_data = (float *)MEM_mallocN(sizeof(float) * seg_size, "calcpathdist");
  ob->runtime->curve_cache->anim_path_accum_length = len_data;

  BevPoint *bp_arr = bl->bevpoints;
  float prev_len = 0.0f;
  for (int i = 0; i < bl->nr - 1; i++) {
    prev_len += len_v3v3(bp_arr[i].vec, bp_arr[i + 1].vec);
    len_data[i] = prev_len;
  }

  if (bl->poly >= 0) {
    /* Cyclic curve. */
    len_data[seg_size - 1] = prev_len + len_v3v3(bp_arr[0].vec, bp_arr[bl->nr - 1].vec);
  }
}

static void get_curve_points_from_idx(const int idx,
                                      const BevList *bl,
                                      const bool is_cyclic,
                                      BevPoint const **r_p0,
                                      BevPoint const **r_p1,
                                      BevPoint const **r_p2,
                                      BevPoint const **r_p3)
{
  BLI_assert(idx >= 0);
  BLI_assert(idx < bl->nr - 1 || (is_cyclic && idx < bl->nr));
  BLI_assert(bl->nr > 1);

  const BevPoint *bp_arr = bl->bevpoints;

  /* First segment. */
  if (idx == 0) {
    *r_p1 = &bp_arr[0];
    if (is_cyclic) {
      *r_p0 = &bp_arr[bl->nr - 1];
    }
    else {
      *r_p0 = *r_p1;
    }

    *r_p2 = &bp_arr[1];

    if (bl->nr > 2) {
      *r_p3 = &bp_arr[2];
    }
    else {
      *r_p3 = *r_p2;
    }
    return;
  }

  /* Last segment (or next to last in a cyclic curve). */
  if (idx == bl->nr - 2) {
    /* The case when the bl->nr == 2 falls in to the "first segment" check above.
     * So here we can assume that bl->nr > 2.
     */
    *r_p0 = &bp_arr[idx - 1];
    *r_p1 = &bp_arr[idx];
    *r_p2 = &bp_arr[idx + 1];

    if (is_cyclic) {
      *r_p3 = &bp_arr[0];
    }
    else {
      *r_p3 = *r_p2;
    }
    return;
  }

  if (idx == bl->nr - 1) {
    /* Last segment in a cyclic curve. This should only trigger if the curve is cyclic
     * as it gets an extra segment between the end and the start point. */
    *r_p0 = &bp_arr[idx - 1];
    *r_p1 = &bp_arr[idx];
    *r_p2 = &bp_arr[0];
    *r_p3 = &bp_arr[1];
    return;
  }

  /* To get here the curve has to have four curve points or more and idx can't
   * be the first or the last segment.
   * So we can assume that we can get four points without any special checks.
   */
  *r_p0 = &bp_arr[idx - 1];
  *r_p1 = &bp_arr[idx];
  *r_p2 = &bp_arr[idx + 1];
  *r_p3 = &bp_arr[idx + 2];
}

static bool binary_search_anim_path(const float *accum_len_arr,
                                    const int seg_size,
                                    const float goal_len,
                                    int *r_idx,
                                    float *r_frac)
{
  float left_len, right_len;
  int cur_idx = 0, cur_base = 0;
  int cur_step = seg_size - 1;

  while (true) {
    cur_idx = cur_base + cur_step / 2;
    left_len = accum_len_arr[cur_idx];
    right_len = accum_len_arr[cur_idx + 1];

    if (left_len <= goal_len && right_len > goal_len) {
      *r_idx = cur_idx + 1;
      *r_frac = (goal_len - left_len) / (right_len - left_len);
      return true;
    }
    if (cur_idx == 0) {
      /* We ended up at the first segment. The point must be in here. */
      *r_idx = 0;
      *r_frac = goal_len / accum_len_arr[0];
      return true;
    }

    if (UNLIKELY(cur_step == 0)) {
      /* This should never happen unless there is something horribly wrong. */
      CLOG_ERROR(&LOG, "Couldn't find any valid point on the animation path!");
      BLI_assert_msg(0, "Couldn't find any valid point on the animation path!");
      return false;
    }

    if (left_len < goal_len) {
      /* Go to the right. */
      cur_base = cur_idx + 1;
      cur_step--;
    } /* Else, go to the left. */

    cur_step /= 2;
  }
}

bool BKE_where_on_path(const Object *ob,
                       float ctime,
                       float r_vec[4],
                       float r_dir[3],
                       float r_quat[4],
                       float *r_radius,
                       float *r_weight)
{
  if (ob == nullptr || ob->type != OB_CURVES_LEGACY) {
    return false;
  }
  Curve *cu = static_cast<Curve *>(ob->data);
  if (ob->runtime->curve_cache == nullptr) {
    CLOG_WARN(&LOG, "No curve cache!");
    return false;
  }
  if (ob->runtime->curve_cache->anim_path_accum_length == nullptr) {
    CLOG_WARN(&LOG, "No anim path!");
    return false;
  }
  /* We only use the first curve. */
  BevList *bl = static_cast<BevList *>(ob->runtime->curve_cache->bev.first);
  if (bl == nullptr || !bl->nr) {
    CLOG_WARN(&LOG, "No bev list data!");
    return false;
  }

  /* Test for cyclic curve. */
  const bool is_cyclic = bl->poly >= 0;

  if (is_cyclic) {
    /* Wrap the time into a 0.0 - 1.0 range. */
    if (ctime < 0.0f || ctime > 1.0f) {
      ctime -= floorf(ctime);
    }
  }

  /* The curve points for this ctime value. */
  const BevPoint *p0, *p1, *p2, *p3;

  float frac;
  const int seg_size = get_bevlist_seg_array_size(bl);
  const float *accum_len_arr = ob->runtime->curve_cache->anim_path_accum_length;
  const float goal_len = ctime * accum_len_arr[seg_size - 1];

  /* Are we simply trying to get the start/end point? */
  if (ctime <= 0.0f || ctime >= 1.0f) {
    const float clamp_time = clamp_f(ctime, 0.0f, 1.0f);
    const int idx = clamp_time * (seg_size - 1);
    get_curve_points_from_idx(idx, bl, is_cyclic, &p0, &p1, &p2, &p3);

    if (idx == 0) {
      frac = goal_len / accum_len_arr[0];
    }
    else {
      frac = (goal_len - accum_len_arr[idx - 1]) / (accum_len_arr[idx] - accum_len_arr[idx - 1]);
    }
  }
  else {
    /* Do binary search to get the correct segment. */
    int idx;
    const bool found_idx = binary_search_anim_path(accum_len_arr, seg_size, goal_len, &idx, &frac);

    if (UNLIKELY(!found_idx)) {
      return false;
    }
    get_curve_points_from_idx(idx, bl, is_cyclic, &p0, &p1, &p2, &p3);
  }

  /* NOTE: commented out for follow constraint
   *
   *       If it's ever be uncommented watch out for BKE_curve_deform_coords()
   *       which used to temporary set CU_FOLLOW flag for the curve and no
   *       longer does it (because of threading issues of such a thing.
   */
  // if (cu->flag & CU_FOLLOW) {

  float w[4];

  key_curve_tangent_weights(frac, w, KEY_BSPLINE);

  if (r_dir) {
    interp_v3_v3v3v3v3(r_dir, p0->vec, p1->vec, p2->vec, p3->vec, w);

    /* Make compatible with #vec_to_quat. */
    negate_v3(r_dir);
  }
  //}

  const ListBase *nurbs = BKE_curve_editNurbs_get(cu);
  if (!nurbs) {
    nurbs = &cu->nurb;
  }
  const Nurb *nu = static_cast<const Nurb *>(nurbs->first);

  /* Make sure that first and last frame are included in the vectors here. */
  if (ELEM(nu->type, CU_POLY, CU_BEZIER, CU_NURBS)) {
    key_curve_position_weights(frac, w, KEY_LINEAR);
  }
  else if (p2 == p3) {
    key_curve_position_weights(frac, w, KEY_CARDINAL);
  }
  else {
    key_curve_position_weights(frac, w, KEY_BSPLINE);
  }

  if (r_vec) {
    /* X, Y, Z axis. */
    r_vec[0] = w[0] * p0->vec[0] + w[1] * p1->vec[0] + w[2] * p2->vec[0] + w[3] * p3->vec[0];
    r_vec[1] = w[0] * p0->vec[1] + w[1] * p1->vec[1] + w[2] * p2->vec[1] + w[3] * p3->vec[1];
    r_vec[2] = w[0] * p0->vec[2] + w[1] * p1->vec[2] + w[2] * p2->vec[2] + w[3] * p3->vec[2];
  }

  /* Clamp weights to 0-1 as we don't want to extrapolate other values than position. */
  clamp_v4(w, 0.0f, 1.0f);

  if (r_vec) {
    /* Tilt, should not be needed since we have quat still used. */
    r_vec[3] = w[0] * p0->tilt + w[1] * p1->tilt + w[2] * p2->tilt + w[3] * p3->tilt;
  }

  if (r_quat) {
    float totfac, q1[4], q2[4];

    totfac = w[0] + w[3];
    if (totfac > FLT_EPSILON) {
      interp_qt_qtqt(q1, p0->quat, p3->quat, w[3] / totfac);
    }
    else {
      copy_qt_qt(q1, p1->quat);
    }

    totfac = w[1] + w[2];
    if (totfac > FLT_EPSILON) {
      interp_qt_qtqt(q2, p1->quat, p2->quat, w[2] / totfac);
    }
    else {
      copy_qt_qt(q2, p3->quat);
    }

    totfac = w[0] + w[1] + w[2] + w[3];
    if (totfac > FLT_EPSILON) {
      interp_qt_qtqt(r_quat, q1, q2, (w[1] + w[2]) / totfac);
    }
    else {
      copy_qt_qt(r_quat, q2);
    }
  }

  if (r_radius) {
    *r_radius = w[0] * p0->radius + w[1] * p1->radius + w[2] * p2->radius + w[3] * p3->radius;
  }

  if (r_weight) {
    *r_weight = w[0] * p0->weight + w[1] * p1->weight + w[2] * p2->weight + w[3] * p3->weight;
  }

  return true;
}
