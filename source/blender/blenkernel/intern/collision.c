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
 * The Original Code is Copyright (C) Blender Foundation
 * All rights reserved.
 */

/** \file
 * \ingroup bke
 */

#include "MEM_guardedalloc.h"

#include "DNA_cloth_types.h"
#include "DNA_collection_types.h"
#include "DNA_effect_types.h"
#include "DNA_object_types.h"
#include "DNA_object_force_types.h"
#include "DNA_scene_types.h"
#include "DNA_meshdata_types.h"

#include "BLI_utildefines.h"
#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_task.h"
#include "BLI_threads.h"

#include "BKE_cloth.h"
#include "BKE_collection.h"
#include "BKE_effect.h"
#include "BKE_layer.h"
#include "BKE_modifier.h"
#include "BKE_scene.h"

#include "BLI_kdopbvh.h"
#include "BKE_collision.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_physics.h"
#include "DEG_depsgraph_query.h"

#ifdef WITH_ELTOPO
#  include "eltopo-capi.h"
#endif

typedef struct ColDetectData {
  ClothModifierData *clmd;
  CollisionModifierData *collmd;
  BVHTreeOverlap *overlap;
  CollPair *collisions;
  bool culling;
  bool use_normal;
  bool collided;
} ColDetectData;

typedef struct SelfColDetectData {
  ClothModifierData *clmd;
  BVHTreeOverlap *overlap;
  CollPair *collisions;
  bool collided;
} SelfColDetectData;

/***********************************
 * Collision modifier code start
 ***********************************/

/* step is limited from 0 (frame start position) to 1 (frame end position) */
void collision_move_object(CollisionModifierData *collmd, float step, float prevstep)
{
  float oldx[3];
  unsigned int i = 0;

  /* the collider doesn't move this frame */
  if (collmd->is_static) {
    for (i = 0; i < collmd->mvert_num; i++) {
      zero_v3(collmd->current_v[i].co);
    }

    return;
  }

  for (i = 0; i < collmd->mvert_num; i++) {
    interp_v3_v3v3(oldx, collmd->x[i].co, collmd->xnew[i].co, prevstep);
    interp_v3_v3v3(collmd->current_x[i].co, collmd->x[i].co, collmd->xnew[i].co, step);
    sub_v3_v3v3(collmd->current_v[i].co, collmd->current_x[i].co, oldx);
  }

  bvhtree_update_from_mvert(
      collmd->bvhtree, collmd->current_x, NULL, collmd->tri, collmd->tri_num, false);
}

BVHTree *bvhtree_build_from_mvert(const MVert *mvert,
                                  const struct MVertTri *tri,
                                  int tri_num,
                                  float epsilon)
{
  BVHTree *tree;
  const MVertTri *vt;
  int i;

  tree = BLI_bvhtree_new(tri_num, epsilon, 4, 26);

  /* fill tree */
  for (i = 0, vt = tri; i < tri_num; i++, vt++) {
    float co[3][3];

    copy_v3_v3(co[0], mvert[vt->tri[0]].co);
    copy_v3_v3(co[1], mvert[vt->tri[1]].co);
    copy_v3_v3(co[2], mvert[vt->tri[2]].co);

    BLI_bvhtree_insert(tree, i, co[0], 3);
  }

  /* balance tree */
  BLI_bvhtree_balance(tree);

  return tree;
}

void bvhtree_update_from_mvert(BVHTree *bvhtree,
                               const MVert *mvert,
                               const MVert *mvert_moving,
                               const MVertTri *tri,
                               int tri_num,
                               bool moving)
{
  const MVertTri *vt;
  int i;

  if ((bvhtree == NULL) || (mvert == NULL)) {
    return;
  }

  if (mvert_moving == NULL) {
    moving = false;
  }

  for (i = 0, vt = tri; i < tri_num; i++, vt++) {
    float co[3][3];
    bool ret;

    copy_v3_v3(co[0], mvert[vt->tri[0]].co);
    copy_v3_v3(co[1], mvert[vt->tri[1]].co);
    copy_v3_v3(co[2], mvert[vt->tri[2]].co);

    /* copy new locations into array */
    if (moving) {
      float co_moving[3][3];
      /* update moving positions */
      copy_v3_v3(co_moving[0], mvert_moving[vt->tri[0]].co);
      copy_v3_v3(co_moving[1], mvert_moving[vt->tri[1]].co);
      copy_v3_v3(co_moving[2], mvert_moving[vt->tri[2]].co);

      ret = BLI_bvhtree_update_node(bvhtree, i, &co[0][0], &co_moving[0][0], 3);
    }
    else {
      ret = BLI_bvhtree_update_node(bvhtree, i, &co[0][0], NULL, 3);
    }

    /* check if tree is already full */
    if (ret == false) {
      break;
    }
  }

  BLI_bvhtree_update_tree(bvhtree);
}

/* ***************************
 * Collision modifier code end
 * *************************** */

BLI_INLINE int next_ind(int i)
{
  return (++i < 3) ? i : 0;
}

static float compute_collision_point(float a1[3],
                                     float a2[3],
                                     float a3[3],
                                     float b1[3],
                                     float b2[3],
                                     float b3[3],
                                     bool culling,
                                     bool use_normal,
                                     float r_a[3],
                                     float r_b[3],
                                     float r_vec[3])
{
  float a[3][3];
  float b[3][3];
  float dist = FLT_MAX;
  float tmp_co1[3], tmp_co2[3];
  float isect_a[3], isect_b[3];
  int isect_count = 0;
  float tmp, tmp_vec[3];
  float normal[3], cent[3];
  bool backside = false;

  copy_v3_v3(a[0], a1);
  copy_v3_v3(a[1], a2);
  copy_v3_v3(a[2], a3);

  copy_v3_v3(b[0], b1);
  copy_v3_v3(b[1], b2);
  copy_v3_v3(b[2], b3);

  /* Find intersections. */
  for (int i = 0; i < 3; i++) {
    if (isect_line_segment_tri_v3(a[i], a[next_ind(i)], b[0], b[1], b[2], &tmp, NULL)) {
      interp_v3_v3v3(isect_a, a[i], a[next_ind(i)], tmp);
      isect_count++;
    }
  }

  if (isect_count == 0) {
    for (int i = 0; i < 3; i++) {
      if (isect_line_segment_tri_v3(b[i], b[next_ind(i)], a[0], a[1], a[2], &tmp, NULL)) {
        isect_count++;
      }
    }
  }
  else if (isect_count == 1) {
    for (int i = 0; i < 3; i++) {
      if (isect_line_segment_tri_v3(b[i], b[next_ind(i)], a[0], a[1], a[2], &tmp, NULL)) {
        interp_v3_v3v3(isect_b, b[i], b[next_ind(i)], tmp);
        break;
      }
    }
  }

  /* Determine collision side. */
  if (culling) {
    normal_tri_v3(normal, b[0], b[1], b[2]);
    mid_v3_v3v3v3(cent, b[0], b[1], b[2]);

    if (isect_count == 2) {
      backside = true;
    }
    else if (isect_count == 0) {
      for (int i = 0; i < 3; i++) {
        sub_v3_v3v3(tmp_vec, a[i], cent);
        if (dot_v3v3(tmp_vec, normal) < 0.0f) {
          backside = true;
          break;
        }
      }
    }
  }
  else if (use_normal) {
    normal_tri_v3(normal, b[0], b[1], b[2]);
  }

  if (isect_count == 1) {
    /* Edge intersection. */
    copy_v3_v3(r_a, isect_a);
    copy_v3_v3(r_b, isect_b);

    if (use_normal) {
      copy_v3_v3(r_vec, normal);
    }
    else {
      sub_v3_v3v3(r_vec, r_b, r_a);
    }

    return 0.0f;
  }

  if (backside) {
    float maxdist = 0.0f;
    bool found = false;

    /* Point projections. */
    for (int i = 0; i < 3; i++) {
      if (isect_ray_tri_v3(a[i], normal, b[0], b[1], b[2], &tmp, NULL)) {
        if (tmp > maxdist) {
          maxdist = tmp;
          copy_v3_v3(r_a, a[i]);
          madd_v3_v3v3fl(r_b, a[i], normal, tmp);
          found = true;
        }
      }
    }

    negate_v3(normal);

    for (int i = 0; i < 3; i++) {
      if (isect_ray_tri_v3(b[i], normal, a[0], a[1], a[2], &tmp, NULL)) {
        if (tmp > maxdist) {
          maxdist = tmp;
          madd_v3_v3v3fl(r_a, b[i], normal, tmp);
          copy_v3_v3(r_b, b[i]);
          found = true;
        }
      }
    }

    negate_v3(normal);

    /* Edge projections. */
    for (int i = 0; i < 3; i++) {
      float dir[3];

      sub_v3_v3v3(tmp_vec, b[next_ind(i)], b[i]);
      cross_v3_v3v3(dir, tmp_vec, normal);

      for (int j = 0; j < 3; j++) {
        if (isect_line_plane_v3(tmp_co1, a[j], a[next_ind(j)], b[i], dir) &&
            point_in_slice_seg(tmp_co1, a[j], a[next_ind(j)]) &&
            point_in_slice_seg(tmp_co1, b[i], b[next_ind(i)])) {
          closest_to_line_v3(tmp_co2, tmp_co1, b[i], b[next_ind(i)]);
          sub_v3_v3v3(tmp_vec, tmp_co1, tmp_co2);
          tmp = len_v3(tmp_vec);

          if ((tmp > maxdist) && (dot_v3v3(tmp_vec, normal) < 0.0f)) {
            maxdist = tmp;
            copy_v3_v3(r_a, tmp_co1);
            copy_v3_v3(r_b, tmp_co2);
            found = true;
          }
        }
      }
    }

    /* If no point is found, will fallback onto regular proximity test below. */
    if (found) {
      sub_v3_v3v3(r_vec, r_b, r_a);

      if (use_normal) {
        if (dot_v3v3(normal, r_vec) >= 0.0f) {
          copy_v3_v3(r_vec, normal);
        }
        else {
          negate_v3_v3(r_vec, normal);
        }
      }

      return 0.0f;
    }
  }

  /* Closest point. */
  for (int i = 0; i < 3; i++) {
    closest_on_tri_to_point_v3(tmp_co1, a[i], b[0], b[1], b[2]);
    tmp = len_squared_v3v3(tmp_co1, a[i]);

    if (tmp < dist) {
      dist = tmp;
      copy_v3_v3(r_a, a[i]);
      copy_v3_v3(r_b, tmp_co1);
    }
  }

  for (int i = 0; i < 3; i++) {
    closest_on_tri_to_point_v3(tmp_co1, b[i], a[0], a[1], a[2]);
    tmp = len_squared_v3v3(tmp_co1, b[i]);

    if (tmp < dist) {
      dist = tmp;
      copy_v3_v3(r_a, tmp_co1);
      copy_v3_v3(r_b, b[i]);
    }
  }

  /* Closest edge. */
  if (isect_count == 0) {
    for (int i = 0; i < 3; i++) {
      for (int j = 0; j < 3; j++) {
        isect_seg_seg_v3(a[i], a[next_ind(i)], b[j], b[next_ind(j)], tmp_co1, tmp_co2);
        tmp = len_squared_v3v3(tmp_co1, tmp_co2);

        if (tmp < dist) {
          dist = tmp;
          copy_v3_v3(r_a, tmp_co1);
          copy_v3_v3(r_b, tmp_co2);
        }
      }
    }
  }

  if (isect_count == 0) {
    sub_v3_v3v3(r_vec, r_a, r_b);
    dist = sqrtf(dist);
  }
  else {
    sub_v3_v3v3(r_vec, r_b, r_a);
    dist = 0.0f;
  }

  if (culling && use_normal) {
    copy_v3_v3(r_vec, normal);
  }
  else if (use_normal) {
    if (dot_v3v3(normal, r_vec) >= 0.0f) {
      copy_v3_v3(r_vec, normal);
    }
    else {
      negate_v3_v3(r_vec, normal);
    }
  }
  else if (culling && (dot_v3v3(r_vec, normal) < 0.0f)) {
    return FLT_MAX;
  }

  return dist;
}

// w3 is not perfect
static void collision_compute_barycentric(
    float pv[3], float p1[3], float p2[3], float p3[3], float *w1, float *w2, float *w3)
{
  /* dot_v3v3 */
#define INPR(v1, v2) ((v1)[0] * (v2)[0] + (v1)[1] * (v2)[1] + (v1)[2] * (v2)[2])

  double tempV1[3], tempV2[3], tempV4[3];
  double a, b, c, d, e, f;

  sub_v3db_v3fl_v3fl(tempV1, p1, p3);
  sub_v3db_v3fl_v3fl(tempV2, p2, p3);
  sub_v3db_v3fl_v3fl(tempV4, pv, p3);

  a = INPR(tempV1, tempV1);
  b = INPR(tempV1, tempV2);
  c = INPR(tempV2, tempV2);
  e = INPR(tempV1, tempV4);
  f = INPR(tempV2, tempV4);

  d = (a * c - b * b);

  if (ABS(d) < (double)ALMOST_ZERO) {
    *w1 = *w2 = *w3 = 1.0 / 3.0;
    return;
  }

  w1[0] = (float)((e * c - b * f) / d);

  if (w1[0] < 0) {
    w1[0] = 0;
  }

  w2[0] = (float)((f - b * (double)w1[0]) / c);

  if (w2[0] < 0) {
    w2[0] = 0;
  }

  w3[0] = 1.0f - w1[0] - w2[0];

#undef INPR
}

#ifdef __GNUC__
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wdouble-promotion"
#endif

DO_INLINE void collision_interpolateOnTriangle(
    float to[3], float v1[3], float v2[3], float v3[3], double w1, double w2, double w3)
{
  zero_v3(to);
  VECADDMUL(to, v1, w1);
  VECADDMUL(to, v2, w2);
  VECADDMUL(to, v3, w3);
}

static int cloth_collision_response_static(ClothModifierData *clmd,
                                           CollisionModifierData *collmd,
                                           Object *collob,
                                           CollPair *collpair,
                                           uint collision_count,
                                           const float dt)
{
  int result = 0;
  Cloth *cloth1;
  float w1, w2, w3, u1, u2, u3;
  float v1[3], v2[3], relativeVelocity[3];
  float magrelVel;
  float epsilon2 = BLI_bvhtree_get_epsilon(collmd->bvhtree);

  cloth1 = clmd->clothObject;

  for (int i = 0; i < collision_count; i++, collpair++) {
    float i1[3], i2[3], i3[3];

    zero_v3(i1);
    zero_v3(i2);
    zero_v3(i3);

    /* Only handle static collisions here. */
    if (collpair->flag & (COLLISION_IN_FUTURE | COLLISION_INACTIVE)) {
      continue;
    }

    /* Compute barycentric coordinates for both collision points. */
    collision_compute_barycentric(collpair->pa,
                                  cloth1->verts[collpair->ap1].tx,
                                  cloth1->verts[collpair->ap2].tx,
                                  cloth1->verts[collpair->ap3].tx,
                                  &w1,
                                  &w2,
                                  &w3);

    collision_compute_barycentric(collpair->pb,
                                  collmd->current_x[collpair->bp1].co,
                                  collmd->current_x[collpair->bp2].co,
                                  collmd->current_x[collpair->bp3].co,
                                  &u1,
                                  &u2,
                                  &u3);

    /* Calculate relative "velocity". */
    collision_interpolateOnTriangle(v1,
                                    cloth1->verts[collpair->ap1].tv,
                                    cloth1->verts[collpair->ap2].tv,
                                    cloth1->verts[collpair->ap3].tv,
                                    w1,
                                    w2,
                                    w3);

    collision_interpolateOnTriangle(v2,
                                    collmd->current_v[collpair->bp1].co,
                                    collmd->current_v[collpair->bp2].co,
                                    collmd->current_v[collpair->bp3].co,
                                    u1,
                                    u2,
                                    u3);

    sub_v3_v3v3(relativeVelocity, v2, v1);

    /* Calculate the normal component of the relative velocity
     * (actually only the magnitude - the direction is stored in 'normal'). */
    magrelVel = dot_v3v3(relativeVelocity, collpair->normal);

    /* If magrelVel < 0 the edges are approaching each other. */
    if (magrelVel > 0.0f) {
      /* Calculate Impulse magnitude to stop all motion in normal direction. */
      float magtangent = 0, repulse = 0, d = 0;
      double impulse = 0.0;
      float vrel_t_pre[3];
      float temp[3];
      float time_multiplier;

      /* Calculate tangential velocity. */
      copy_v3_v3(temp, collpair->normal);
      mul_v3_fl(temp, magrelVel);
      sub_v3_v3v3(vrel_t_pre, relativeVelocity, temp);

      /* Decrease in magnitude of relative tangential velocity due to coulomb friction
       * in original formula "magrelVel" should be the
       * "change of relative velocity in normal direction". */
      magtangent = min_ff(collob->pd->pdef_cfrict * 0.01f * magrelVel, len_v3(vrel_t_pre));

      /* Apply friction impulse. */
      if (magtangent > ALMOST_ZERO) {
        normalize_v3(vrel_t_pre);

        impulse = magtangent / 1.5;

        VECADDMUL(i1, vrel_t_pre, w1 * impulse);
        VECADDMUL(i2, vrel_t_pre, w2 * impulse);
        VECADDMUL(i3, vrel_t_pre, w3 * impulse);
      }

      /* Apply velocity stopping impulse. */
      impulse = magrelVel / 1.5f;

      VECADDMUL(i1, collpair->normal, w1 * impulse);
      cloth1->verts[collpair->ap1].impulse_count++;

      VECADDMUL(i2, collpair->normal, w2 * impulse);
      cloth1->verts[collpair->ap2].impulse_count++;

      VECADDMUL(i3, collpair->normal, w3 * impulse);
      cloth1->verts[collpair->ap3].impulse_count++;

      time_multiplier = 1.0f / (clmd->sim_parms->dt * clmd->sim_parms->timescale);

      d = clmd->coll_parms->epsilon * 8.0f / 9.0f + epsilon2 * 8.0f / 9.0f - collpair->distance;

      if ((magrelVel < 0.1f * d * time_multiplier) && (d > ALMOST_ZERO)) {
        repulse = MIN2(d / time_multiplier, 0.1f * d * time_multiplier - magrelVel);

        /* Stay on the safe side and clamp repulse. */
        if (impulse > ALMOST_ZERO) {
          repulse = min_ff(repulse, 5.0f * impulse);
        }

        repulse = max_ff(impulse, repulse);

        impulse = repulse / 1.5f;

        VECADDMUL(i1, collpair->normal, impulse);
        VECADDMUL(i2, collpair->normal, impulse);
        VECADDMUL(i3, collpair->normal, impulse);
      }

      result = 1;
    }
    else {
      float time_multiplier = 1.0f / (clmd->sim_parms->dt * clmd->sim_parms->timescale);
      float d;

      d = clmd->coll_parms->epsilon * 8.0f / 9.0f + epsilon2 * 8.0f / 9.0f - collpair->distance;

      if (d > ALMOST_ZERO) {
        /* Stay on the safe side and clamp repulse. */
        float repulse = d / time_multiplier;
        float impulse = repulse / 4.5f;

        VECADDMUL(i1, collpair->normal, w1 * impulse);
        VECADDMUL(i2, collpair->normal, w2 * impulse);
        VECADDMUL(i3, collpair->normal, w3 * impulse);

        cloth1->verts[collpair->ap1].impulse_count++;
        cloth1->verts[collpair->ap2].impulse_count++;
        cloth1->verts[collpair->ap3].impulse_count++;

        result = 1;
      }
    }

    if (result) {
      float clamp = clmd->coll_parms->clamp * dt;

      if ((clamp > 0.0f) &&
          ((len_v3(i1) > clamp) || (len_v3(i2) > clamp) || (len_v3(i3) > clamp))) {
        return 0;
      }

      for (int j = 0; j < 3; j++) {
        if (cloth1->verts[collpair->ap1].impulse_count > 0 &&
            ABS(cloth1->verts[collpair->ap1].impulse[j]) < ABS(i1[j])) {
          cloth1->verts[collpair->ap1].impulse[j] = i1[j];
        }

        if (cloth1->verts[collpair->ap2].impulse_count > 0 &&
            ABS(cloth1->verts[collpair->ap2].impulse[j]) < ABS(i2[j])) {
          cloth1->verts[collpair->ap2].impulse[j] = i2[j];
        }

        if (cloth1->verts[collpair->ap3].impulse_count > 0 &&
            ABS(cloth1->verts[collpair->ap3].impulse[j]) < ABS(i3[j])) {
          cloth1->verts[collpair->ap3].impulse[j] = i3[j];
        }
      }
    }
  }

  return result;
}

static int cloth_selfcollision_response_static(ClothModifierData *clmd,
                                               CollPair *collpair,
                                               uint collision_count,
                                               const float dt)
{
  int result = 0;
  Cloth *cloth1;
  float w1, w2, w3, u1, u2, u3;
  float v1[3], v2[3], relativeVelocity[3];
  float magrelVel;

  cloth1 = clmd->clothObject;

  for (int i = 0; i < collision_count; i++, collpair++) {
    float i1[3], i2[3], i3[3];

    zero_v3(i1);
    zero_v3(i2);
    zero_v3(i3);

    /* Only handle static collisions here. */
    if (collpair->flag & (COLLISION_IN_FUTURE | COLLISION_INACTIVE)) {
      continue;
    }

    /* Compute barycentric coordinates for both collision points. */
    collision_compute_barycentric(collpair->pa,
                                  cloth1->verts[collpair->ap1].tx,
                                  cloth1->verts[collpair->ap2].tx,
                                  cloth1->verts[collpair->ap3].tx,
                                  &w1,
                                  &w2,
                                  &w3);

    collision_compute_barycentric(collpair->pb,
                                  cloth1->verts[collpair->bp1].tx,
                                  cloth1->verts[collpair->bp2].tx,
                                  cloth1->verts[collpair->bp3].tx,
                                  &u1,
                                  &u2,
                                  &u3);

    /* Calculate relative "velocity". */
    collision_interpolateOnTriangle(v1,
                                    cloth1->verts[collpair->ap1].tv,
                                    cloth1->verts[collpair->ap2].tv,
                                    cloth1->verts[collpair->ap3].tv,
                                    w1,
                                    w2,
                                    w3);

    collision_interpolateOnTriangle(v2,
                                    cloth1->verts[collpair->bp1].tv,
                                    cloth1->verts[collpair->bp2].tv,
                                    cloth1->verts[collpair->bp3].tv,
                                    u1,
                                    u2,
                                    u3);

    sub_v3_v3v3(relativeVelocity, v2, v1);

    /* Calculate the normal component of the relative velocity
     * (actually only the magnitude - the direction is stored in 'normal'). */
    magrelVel = dot_v3v3(relativeVelocity, collpair->normal);

    /* TODO: Impulses should be weighed by mass as this is self col,
     * this has to be done after mass distribution is implemented. */

    /* If magrelVel < 0 the edges are approaching each other. */
    if (magrelVel > 0.0f) {
      /* Calculate Impulse magnitude to stop all motion in normal direction. */
      float magtangent = 0, repulse = 0, d = 0;
      double impulse = 0.0;
      float vrel_t_pre[3];
      float temp[3], time_multiplier;

      /* Calculate tangential velocity. */
      copy_v3_v3(temp, collpair->normal);
      mul_v3_fl(temp, magrelVel);
      sub_v3_v3v3(vrel_t_pre, relativeVelocity, temp);

      /* Decrease in magnitude of relative tangential velocity due to coulomb friction
       * in original formula "magrelVel" should be the
       * "change of relative velocity in normal direction". */
      magtangent = min_ff(clmd->coll_parms->self_friction * 0.01f * magrelVel, len_v3(vrel_t_pre));

      /* Apply friction impulse. */
      if (magtangent > ALMOST_ZERO) {
        normalize_v3(vrel_t_pre);

        impulse = magtangent / 1.5;

        VECADDMUL(i1, vrel_t_pre, w1 * impulse);
        VECADDMUL(i2, vrel_t_pre, w2 * impulse);
        VECADDMUL(i3, vrel_t_pre, w3 * impulse);
      }

      /* Apply velocity stopping impulse. */
      impulse = magrelVel / 3.0f;

      VECADDMUL(i1, collpair->normal, w1 * impulse);
      cloth1->verts[collpair->ap1].impulse_count++;

      VECADDMUL(i2, collpair->normal, w2 * impulse);
      cloth1->verts[collpair->ap2].impulse_count++;

      VECADDMUL(i3, collpair->normal, w3 * impulse);
      cloth1->verts[collpair->ap3].impulse_count++;

      time_multiplier = 1.0f / (clmd->sim_parms->dt * clmd->sim_parms->timescale);

      d = clmd->coll_parms->selfepsilon * 8.0f / 9.0f * 2.0f - collpair->distance;

      if ((magrelVel < 0.1f * d * time_multiplier) && (d > ALMOST_ZERO)) {
        repulse = MIN2(d / time_multiplier, 0.1f * d * time_multiplier - magrelVel);

        if (impulse > ALMOST_ZERO) {
          repulse = min_ff(repulse, 5.0 * impulse);
        }

        repulse = max_ff(impulse, repulse);

        impulse = repulse / 1.5f;

        VECADDMUL(i1, collpair->normal, w1 * impulse);
        VECADDMUL(i2, collpair->normal, w2 * impulse);
        VECADDMUL(i3, collpair->normal, w3 * impulse);
      }

      result = 1;
    }
    else {
      float time_multiplier = 1.0f / (clmd->sim_parms->dt * clmd->sim_parms->timescale);
      float d;

      d = clmd->coll_parms->selfepsilon * 8.0f / 9.0f * 2.0f - collpair->distance;

      if (d > ALMOST_ZERO) {
        /* Stay on the safe side and clamp repulse. */
        float repulse = d * 1.0f / time_multiplier;
        float impulse = repulse / 9.0f;

        VECADDMUL(i1, collpair->normal, w1 * impulse);
        VECADDMUL(i2, collpair->normal, w2 * impulse);
        VECADDMUL(i3, collpair->normal, w3 * impulse);

        cloth1->verts[collpair->ap1].impulse_count++;
        cloth1->verts[collpair->ap2].impulse_count++;
        cloth1->verts[collpair->ap3].impulse_count++;

        result = 1;
      }
    }

    if (result) {
      float clamp = clmd->coll_parms->self_clamp * dt;

      if ((clamp > 0.0f) &&
          ((len_v3(i1) > clamp) || (len_v3(i2) > clamp) || (len_v3(i3) > clamp))) {
        return 0;
      }

      for (int j = 0; j < 3; j++) {
        if (cloth1->verts[collpair->ap1].impulse_count > 0 &&
            ABS(cloth1->verts[collpair->ap1].impulse[j]) < ABS(i1[j])) {
          cloth1->verts[collpair->ap1].impulse[j] = i1[j];
        }

        if (cloth1->verts[collpair->ap2].impulse_count > 0 &&
            ABS(cloth1->verts[collpair->ap2].impulse[j]) < ABS(i2[j])) {
          cloth1->verts[collpair->ap2].impulse[j] = i2[j];
        }

        if (cloth1->verts[collpair->ap3].impulse_count > 0 &&
            ABS(cloth1->verts[collpair->ap3].impulse[j]) < ABS(i3[j])) {
          cloth1->verts[collpair->ap3].impulse[j] = i3[j];
        }
      }
    }
  }

  return result;
}

#ifdef __GNUC__
#  pragma GCC diagnostic pop
#endif

static void cloth_collision(void *__restrict userdata,
                            const int index,
                            const ParallelRangeTLS *__restrict UNUSED(tls))
{
  ColDetectData *data = (ColDetectData *)userdata;

  ClothModifierData *clmd = data->clmd;
  CollisionModifierData *collmd = data->collmd;
  CollPair *collpair = data->collisions;
  const MVertTri *tri_a, *tri_b;
  ClothVertex *verts1 = clmd->clothObject->verts;
  float distance = 0.0f;
  float epsilon1 = clmd->coll_parms->epsilon;
  float epsilon2 = BLI_bvhtree_get_epsilon(collmd->bvhtree);
  float pa[3], pb[3], vect[3];

  tri_a = &clmd->clothObject->tri[data->overlap[index].indexA];
  tri_b = &collmd->tri[data->overlap[index].indexB];

  /* Compute distance and normal. */
  distance = compute_collision_point(verts1[tri_a->tri[0]].tx,
                                     verts1[tri_a->tri[1]].tx,
                                     verts1[tri_a->tri[2]].tx,
                                     collmd->current_x[tri_b->tri[0]].co,
                                     collmd->current_x[tri_b->tri[1]].co,
                                     collmd->current_x[tri_b->tri[2]].co,
                                     data->culling,
                                     data->use_normal,
                                     pa,
                                     pb,
                                     vect);

  if ((distance <= (epsilon1 + epsilon2 + ALMOST_ZERO)) && (len_squared_v3(vect) > ALMOST_ZERO)) {
    collpair[index].ap1 = tri_a->tri[0];
    collpair[index].ap2 = tri_a->tri[1];
    collpair[index].ap3 = tri_a->tri[2];

    collpair[index].bp1 = tri_b->tri[0];
    collpair[index].bp2 = tri_b->tri[1];
    collpair[index].bp3 = tri_b->tri[2];

    copy_v3_v3(collpair[index].pa, pa);
    copy_v3_v3(collpair[index].pb, pb);
    copy_v3_v3(collpair[index].vector, vect);

    normalize_v3_v3(collpair[index].normal, collpair[index].vector);

    collpair[index].distance = distance;
    collpair[index].flag = 0;

    data->collided = true;
  }
  else {
    collpair[index].flag = COLLISION_INACTIVE;
  }
}

static void cloth_selfcollision(void *__restrict userdata,
                                const int index,
                                const ParallelRangeTLS *__restrict UNUSED(tls))
{
  SelfColDetectData *data = (SelfColDetectData *)userdata;

  ClothModifierData *clmd = data->clmd;
  CollPair *collpair = data->collisions;
  const MVertTri *tri_a, *tri_b;
  ClothVertex *verts1 = clmd->clothObject->verts;
  float distance = 0.0f;
  float epsilon = clmd->coll_parms->selfepsilon;
  float pa[3], pb[3], vect[3];

  tri_a = &clmd->clothObject->tri[data->overlap[index].indexA];
  tri_b = &clmd->clothObject->tri[data->overlap[index].indexB];

  for (uint i = 0; i < 3; i++) {
    for (uint j = 0; j < 3; j++) {
      if (tri_a->tri[i] == tri_b->tri[j]) {
        collpair[index].flag = COLLISION_INACTIVE;
        return;
      }
    }
  }

  if (((verts1[tri_a->tri[0]].flags & verts1[tri_a->tri[1]].flags & verts1[tri_a->tri[2]].flags) |
       (verts1[tri_b->tri[0]].flags & verts1[tri_b->tri[1]].flags & verts1[tri_b->tri[2]].flags)) &
      CLOTH_VERT_FLAG_NOSELFCOLL) {
    collpair[index].flag = COLLISION_INACTIVE;
    return;
  }

  /* Compute distance and normal. */
  distance = compute_collision_point(verts1[tri_a->tri[0]].tx,
                                     verts1[tri_a->tri[1]].tx,
                                     verts1[tri_a->tri[2]].tx,
                                     verts1[tri_b->tri[0]].tx,
                                     verts1[tri_b->tri[1]].tx,
                                     verts1[tri_b->tri[2]].tx,
                                     false,
                                     false,
                                     pa,
                                     pb,
                                     vect);

  if ((distance <= (epsilon * 2.0f + ALMOST_ZERO)) && (len_squared_v3(vect) > ALMOST_ZERO)) {
    collpair[index].ap1 = tri_a->tri[0];
    collpair[index].ap2 = tri_a->tri[1];
    collpair[index].ap3 = tri_a->tri[2];

    collpair[index].bp1 = tri_b->tri[0];
    collpair[index].bp2 = tri_b->tri[1];
    collpair[index].bp3 = tri_b->tri[2];

    copy_v3_v3(collpair[index].pa, pa);
    copy_v3_v3(collpair[index].pb, pb);
    copy_v3_v3(collpair[index].vector, vect);

    normalize_v3_v3(collpair[index].normal, collpair[index].vector);

    collpair[index].distance = distance;
    collpair[index].flag = 0;

    data->collided = true;
  }
  else {
    collpair[index].flag = COLLISION_INACTIVE;
  }
}

static void add_collision_object(ListBase *relations,
                                 Object *ob,
                                 int level,
                                 unsigned int modifier_type)
{
  CollisionModifierData *cmd = NULL;

  /* only get objects with collision modifier */
  if (((modifier_type == eModifierType_Collision) && ob->pd && ob->pd->deflect) ||
      (modifier_type != eModifierType_Collision)) {
    cmd = (CollisionModifierData *)modifiers_findByType(ob, modifier_type);
  }

  if (cmd) {
    CollisionRelation *relation = MEM_callocN(sizeof(CollisionRelation), "CollisionRelation");
    relation->ob = ob;
    BLI_addtail(relations, relation);
  }

  /* objects in dupli groups, one level only for now */
  /* TODO: this doesn't really work, we are not taking into account the
   * dupli transforms and can get objects in the list multiple times. */
  if (ob->instance_collection && level == 0) {
    Collection *collection = ob->instance_collection;

    /* add objects */
    FOREACH_COLLECTION_OBJECT_RECURSIVE_BEGIN (collection, object) {
      add_collision_object(relations, object, level + 1, modifier_type);
    }
    FOREACH_COLLECTION_OBJECT_RECURSIVE_END;
  }
}

/* Create list of collision relations in the collection or entire scene.
 * This is used by the depsgraph to build relations, as well as faster
 * lookup of colliders during evaluation. */
ListBase *BKE_collision_relations_create(Depsgraph *depsgraph,
                                         Collection *collection,
                                         unsigned int modifier_type)
{
  ViewLayer *view_layer = DEG_get_input_view_layer(depsgraph);
  Base *base = BKE_collection_or_layer_objects(view_layer, collection);
  const bool for_render = (DEG_get_mode(depsgraph) == DAG_EVAL_RENDER);
  const int base_flag = (for_render) ? BASE_ENABLED_RENDER : BASE_ENABLED_VIEWPORT;

  ListBase *relations = MEM_callocN(sizeof(ListBase), "CollisionRelation list");

  for (; base; base = base->next) {
    if (base->flag & base_flag) {
      add_collision_object(relations, base->object, 0, modifier_type);
    }
  }

  return relations;
}

void BKE_collision_relations_free(ListBase *relations)
{
  if (relations) {
    BLI_freelistN(relations);
    MEM_freeN(relations);
  }
}

/* Create effective list of colliders from relations built beforehand.
 * Self will be excluded. */
Object **BKE_collision_objects_create(Depsgraph *depsgraph,
                                      Object *self,
                                      Collection *collection,
                                      unsigned int *numcollobj,
                                      unsigned int modifier_type)
{
  ListBase *relations = DEG_get_collision_relations(depsgraph, collection, modifier_type);

  if (!relations) {
    *numcollobj = 0;
    return NULL;
  }

  int maxnum = BLI_listbase_count(relations);
  int num = 0;
  Object **objects = MEM_callocN(sizeof(Object *) * maxnum, __func__);

  for (CollisionRelation *relation = relations->first; relation; relation = relation->next) {
    /* Get evaluated object. */
    Object *ob = (Object *)DEG_get_evaluated_id(depsgraph, &relation->ob->id);

    if (ob != self) {
      objects[num] = ob;
      num++;
    }
  }

  if (num == 0) {
    MEM_freeN(objects);
    objects = NULL;
  }

  *numcollobj = num;
  return objects;
}

void BKE_collision_objects_free(Object **objects)
{
  if (objects) {
    MEM_freeN(objects);
  }
}

/* Create effective list of colliders from relations built beforehand.
 * Self will be excluded. */
ListBase *BKE_collider_cache_create(Depsgraph *depsgraph, Object *self, Collection *collection)
{
  ListBase *relations = DEG_get_collision_relations(
      depsgraph, collection, eModifierType_Collision);
  ListBase *cache = NULL;

  if (!relations) {
    return NULL;
  }

  for (CollisionRelation *relation = relations->first; relation; relation = relation->next) {
    /* Get evaluated object. */
    Object *ob = (Object *)DEG_get_evaluated_id(depsgraph, &relation->ob->id);

    if (ob == self) {
      continue;
    }

    CollisionModifierData *cmd = (CollisionModifierData *)modifiers_findByType(
        ob, eModifierType_Collision);
    if (cmd && cmd->bvhtree) {
      if (cache == NULL) {
        cache = MEM_callocN(sizeof(ListBase), "ColliderCache array");
      }

      ColliderCache *col = MEM_callocN(sizeof(ColliderCache), "ColliderCache");
      col->ob = ob;
      col->collmd = cmd;
      /* make sure collider is properly set up */
      collision_move_object(cmd, 1.0, 0.0);
      BLI_addtail(cache, col);
    }
  }

  return cache;
}

void BKE_collider_cache_free(ListBase **colliders)
{
  if (*colliders) {
    BLI_freelistN(*colliders);
    MEM_freeN(*colliders);
    *colliders = NULL;
  }
}

static bool cloth_bvh_objcollisions_nearcheck(ClothModifierData *clmd,
                                              CollisionModifierData *collmd,
                                              CollPair **collisions,
                                              int numresult,
                                              BVHTreeOverlap *overlap,
                                              bool culling,
                                              bool use_normal)
{
  *collisions = (CollPair *)MEM_mallocN(sizeof(CollPair) * numresult, "collision array");

  ColDetectData data = {
      .clmd = clmd,
      .collmd = collmd,
      .overlap = overlap,
      .collisions = *collisions,
      .culling = culling,
      .use_normal = use_normal,
      .collided = false,
  };

  ParallelRangeSettings settings;
  BLI_parallel_range_settings_defaults(&settings);
  settings.use_threading = true;
  BLI_task_parallel_range(0, numresult, &data, cloth_collision, &settings);

  return data.collided;
}

static bool cloth_bvh_selfcollisions_nearcheck(ClothModifierData *clmd,
                                               CollPair *collisions,
                                               int numresult,
                                               BVHTreeOverlap *overlap)
{
  SelfColDetectData data = {
      .clmd = clmd,
      .overlap = overlap,
      .collisions = collisions,
      .collided = false,
  };

  ParallelRangeSettings settings;
  BLI_parallel_range_settings_defaults(&settings);
  settings.use_threading = true;
  BLI_task_parallel_range(0, numresult, &data, cloth_selfcollision, &settings);

  return data.collided;
}

static int cloth_bvh_objcollisions_resolve(ClothModifierData *clmd,
                                           Object **collobjs,
                                           CollPair **collisions,
                                           uint *collision_counts,
                                           const uint numcollobj,
                                           const float dt)
{
  Cloth *cloth = clmd->clothObject;
  int i = 0, j = 0, mvert_num = 0;
  ClothVertex *verts = NULL;
  int ret = 0;
  int result = 0;

  mvert_num = clmd->clothObject->mvert_num;
  verts = cloth->verts;

  result = 1;

  for (j = 0; j < 2; j++) {
    result = 0;

    for (i = 0; i < numcollobj; i++) {
      Object *collob = collobjs[i];
      CollisionModifierData *collmd = (CollisionModifierData *)modifiers_findByType(
          collob, eModifierType_Collision);

      if (collmd->bvhtree) {
        result += cloth_collision_response_static(
            clmd, collmd, collob, collisions[i], collision_counts[i], dt);
      }
    }

    /* Apply impulses in parallel. */
    if (result) {
      for (i = 0; i < mvert_num; i++) {
        // calculate "velocities" (just xnew = xold + v; no dt in v)
        if (verts[i].impulse_count) {
          add_v3_v3(verts[i].tv, verts[i].impulse);
          add_v3_v3(verts[i].dcvel, verts[i].impulse);
          zero_v3(verts[i].impulse);
          verts[i].impulse_count = 0;

          ret++;
        }
      }
    }
    else {
      break;
    }
  }
  return ret;
}

static int cloth_bvh_selfcollisions_resolve(ClothModifierData *clmd,
                                            CollPair *collisions,
                                            int collision_count,
                                            const float dt)
{
  Cloth *cloth = clmd->clothObject;
  int i = 0, j = 0, mvert_num = 0;
  ClothVertex *verts = NULL;
  int ret = 0;
  int result = 0;

  mvert_num = clmd->clothObject->mvert_num;
  verts = cloth->verts;

  for (j = 0; j < 2; j++) {
    result = 0;

    result += cloth_selfcollision_response_static(clmd, collisions, collision_count, dt);

    /* Apply impulses in parallel. */
    if (result) {
      for (i = 0; i < mvert_num; i++) {
        if (verts[i].impulse_count) {
          // VECADDMUL ( verts[i].tv, verts[i].impulse, 1.0f / verts[i].impulse_count );
          add_v3_v3(verts[i].tv, verts[i].impulse);
          add_v3_v3(verts[i].dcvel, verts[i].impulse);
          zero_v3(verts[i].impulse);
          verts[i].impulse_count = 0;

          ret++;
        }
      }
    }

    if (!result) {
      break;
    }
  }
  return ret;
}

int cloth_bvh_collision(
    Depsgraph *depsgraph, Object *ob, ClothModifierData *clmd, float step, float dt)
{
  Cloth *cloth = clmd->clothObject;
  BVHTree *cloth_bvh = cloth->bvhtree;
  uint i = 0, mvert_num = 0;
  int rounds = 0;
  ClothVertex *verts = NULL;
  int ret = 0, ret2 = 0;
  Object **collobjs = NULL;
  unsigned int numcollobj = 0;
  uint *coll_counts_obj = NULL;
  BVHTreeOverlap **overlap_obj = NULL;
  uint coll_count_self = 0;
  BVHTreeOverlap *overlap_self = NULL;

  if ((clmd->sim_parms->flags & CLOTH_SIMSETTINGS_FLAG_COLLOBJ) || cloth_bvh == NULL) {
    return 0;
  }

  verts = cloth->verts;
  mvert_num = cloth->mvert_num;

  if (clmd->coll_parms->flags & CLOTH_COLLSETTINGS_FLAG_ENABLED) {
    bvhtree_update_from_cloth(clmd, false, false);

    collobjs = BKE_collision_objects_create(
        depsgraph, ob, clmd->coll_parms->group, &numcollobj, eModifierType_Collision);

    if (collobjs) {
      coll_counts_obj = MEM_callocN(sizeof(uint) * numcollobj, "CollCounts");
      overlap_obj = MEM_callocN(sizeof(*overlap_obj) * numcollobj, "BVHOverlap");

      for (i = 0; i < numcollobj; i++) {
        Object *collob = collobjs[i];
        CollisionModifierData *collmd = (CollisionModifierData *)modifiers_findByType(
            collob, eModifierType_Collision);

        if (!collmd->bvhtree) {
          continue;
        }

        /* Move object to position (step) in time. */
        collision_move_object(collmd, step + dt, step);

        overlap_obj[i] = BLI_bvhtree_overlap(
            cloth_bvh, collmd->bvhtree, &coll_counts_obj[i], NULL, NULL);
      }
    }
  }

  if (clmd->coll_parms->flags & CLOTH_COLLSETTINGS_FLAG_SELF) {
    bvhtree_update_from_cloth(clmd, false, true);

    overlap_self = BLI_bvhtree_overlap(
        cloth->bvhselftree, cloth->bvhselftree, &coll_count_self, NULL, NULL);
  }

  do {
    ret2 = 0;

    /* Object collisions. */
    if ((clmd->coll_parms->flags & CLOTH_COLLSETTINGS_FLAG_ENABLED) && collobjs) {
      CollPair **collisions;
      bool collided = false;

      collisions = MEM_callocN(sizeof(CollPair *) * numcollobj, "CollPair");

      for (i = 0; i < numcollobj; i++) {
        Object *collob = collobjs[i];
        CollisionModifierData *collmd = (CollisionModifierData *)modifiers_findByType(
            collob, eModifierType_Collision);

        if (!collmd->bvhtree) {
          continue;
        }

        if (coll_counts_obj[i] && overlap_obj[i]) {
          collided = cloth_bvh_objcollisions_nearcheck(
                         clmd,
                         collmd,
                         &collisions[i],
                         coll_counts_obj[i],
                         overlap_obj[i],
                         (collob->pd->flag & PFIELD_CLOTH_USE_CULLING),
                         (collob->pd->flag & PFIELD_CLOTH_USE_NORMAL)) ||
                     collided;
        }
      }

      if (collided) {
        ret += cloth_bvh_objcollisions_resolve(
            clmd, collobjs, collisions, coll_counts_obj, numcollobj, dt);
        ret2 += ret;
      }

      for (i = 0; i < numcollobj; i++) {
        MEM_SAFE_FREE(collisions[i]);
      }

      MEM_freeN(collisions);
    }

    /* Self collisions. */
    if (clmd->coll_parms->flags & CLOTH_COLLSETTINGS_FLAG_SELF) {
      CollPair *collisions = NULL;

      verts = cloth->verts;
      mvert_num = cloth->mvert_num;

      if (cloth->bvhselftree) {
        if (coll_count_self && overlap_self) {
          collisions = (CollPair *)MEM_mallocN(sizeof(CollPair) * coll_count_self,
                                               "collision array");

          if (cloth_bvh_selfcollisions_nearcheck(
                  clmd, collisions, coll_count_self, overlap_self)) {
            ret += cloth_bvh_selfcollisions_resolve(clmd, collisions, coll_count_self, dt);
            ret2 += ret;
          }
        }
      }

      MEM_SAFE_FREE(collisions);
    }

    /* Apply all collision resolution. */
    if (ret2) {
      for (i = 0; i < mvert_num; i++) {
        if (clmd->sim_parms->vgroup_mass > 0) {
          if (verts[i].flags & CLOTH_VERT_FLAG_PINNED) {
            continue;
          }
        }

        add_v3_v3v3(verts[i].tx, verts[i].txold, verts[i].tv);
      }
    }

    rounds++;
  } while (ret2 && (clmd->coll_parms->loop_count > rounds));

  if (overlap_obj) {
    for (i = 0; i < numcollobj; i++) {
      MEM_SAFE_FREE(overlap_obj[i]);
    }

    MEM_freeN(overlap_obj);
  }

  MEM_SAFE_FREE(coll_counts_obj);

  MEM_SAFE_FREE(overlap_self);

  BKE_collision_objects_free(collobjs);

  return MIN2(ret, 1);
}

BLI_INLINE void max_v3_v3v3(float r[3], const float a[3], const float b[3])
{
  r[0] = max_ff(a[0], b[0]);
  r[1] = max_ff(a[1], b[1]);
  r[2] = max_ff(a[2], b[2]);
}

void collision_get_collider_velocity(float vel_old[3],
                                     float vel_new[3],
                                     CollisionModifierData *collmd,
                                     CollPair *collpair)
{
  float u1, u2, u3;

  /* compute barycentric coordinates */
  collision_compute_barycentric(collpair->pb,
                                collmd->current_x[collpair->bp1].co,
                                collmd->current_x[collpair->bp2].co,
                                collmd->current_x[collpair->bp3].co,
                                &u1,
                                &u2,
                                &u3);

  collision_interpolateOnTriangle(vel_new,
                                  collmd->current_v[collpair->bp1].co,
                                  collmd->current_v[collpair->bp2].co,
                                  collmd->current_v[collpair->bp3].co,
                                  u1,
                                  u2,
                                  u3);
  /* XXX assume constant velocity of the collider for now */
  copy_v3_v3(vel_old, vel_new);
}

BLI_INLINE bool cloth_point_face_collision_params(const float p1[3],
                                                  const float p2[3],
                                                  const float v0[3],
                                                  const float v1[3],
                                                  const float v2[3],
                                                  float r_nor[3],
                                                  float *r_lambda,
                                                  float r_w[3])
{
  float edge1[3], edge2[3], p2face[3], p1p2[3], v0p2[3];
  float nor_v0p2, nor_p1p2;

  sub_v3_v3v3(edge1, v1, v0);
  sub_v3_v3v3(edge2, v2, v0);
  cross_v3_v3v3(r_nor, edge1, edge2);
  normalize_v3(r_nor);

  sub_v3_v3v3(v0p2, p2, v0);
  nor_v0p2 = dot_v3v3(v0p2, r_nor);
  madd_v3_v3v3fl(p2face, p2, r_nor, -nor_v0p2);
  interp_weights_tri_v3(r_w, v0, v1, v2, p2face);

  sub_v3_v3v3(p1p2, p2, p1);
  nor_p1p2 = dot_v3v3(p1p2, r_nor);
  *r_lambda = (nor_p1p2 != 0.0f ? nor_v0p2 / nor_p1p2 : 0.0f);

  return r_w[1] >= 0.0f && r_w[2] >= 0.0f && r_w[1] + r_w[2] <= 1.0f;
}

static CollPair *cloth_point_collpair(float p1[3],
                                      float p2[3],
                                      const MVert *mverts,
                                      int bp1,
                                      int bp2,
                                      int bp3,
                                      int index_cloth,
                                      int index_coll,
                                      float epsilon,
                                      CollPair *collpair)
{
  const float *co1 = mverts[bp1].co, *co2 = mverts[bp2].co, *co3 = mverts[bp3].co;
  float lambda /*, distance1 */, distance2;
  float facenor[3], v1p1[3], v1p2[3];
  float w[3];

  if (!cloth_point_face_collision_params(p1, p2, co1, co2, co3, facenor, &lambda, w)) {
    return collpair;
  }

  sub_v3_v3v3(v1p1, p1, co1);
  //  distance1 = dot_v3v3(v1p1, facenor);
  sub_v3_v3v3(v1p2, p2, co1);
  distance2 = dot_v3v3(v1p2, facenor);
  //  if (distance2 > epsilon || (distance1 < 0.0f && distance2 < 0.0f))
  if (distance2 > epsilon) {
    return collpair;
  }

  collpair->face1 = index_cloth; /* XXX actually not a face, but equivalent index for point */
  collpair->face2 = index_coll;
  collpair->ap1 = index_cloth;
  collpair->ap2 = collpair->ap3 = -1; /* unused */
  collpair->bp1 = bp1;
  collpair->bp2 = bp2;
  collpair->bp3 = bp3;

  /* note: using the second point here, which is
   * the current updated position that needs to be corrected
   */
  copy_v3_v3(collpair->pa, p2);
  collpair->distance = distance2;
  mul_v3_v3fl(collpair->vector, facenor, -distance2);

  interp_v3_v3v3v3(collpair->pb, co1, co2, co3, w);

  copy_v3_v3(collpair->normal, facenor);
  collpair->time = lambda;
  collpair->flag = 0;

  collpair++;
  return collpair;
}

/* Determines collisions on overlap,
 * collisions are written to collpair[i] and collision+number_collision_found is returned. */
static CollPair *cloth_point_collision(ModifierData *md1,
                                       ModifierData *md2,
                                       BVHTreeOverlap *overlap,
                                       float epsilon,
                                       CollPair *collpair,
                                       float UNUSED(dt))
{
  ClothModifierData *clmd = (ClothModifierData *)md1;
  CollisionModifierData *collmd = (CollisionModifierData *)md2;
  /* Cloth *cloth = clmd->clothObject; */ /* UNUSED */
  ClothVertex *vert = NULL;
  const MVertTri *vt;
  const MVert *mverts = collmd->current_x;

  vert = &clmd->clothObject->verts[overlap->indexA];
  vt = &collmd->tri[overlap->indexB];

  collpair = cloth_point_collpair(vert->tx,
                                  vert->x,
                                  mverts,
                                  vt->tri[0],
                                  vt->tri[1],
                                  vt->tri[2],
                                  overlap->indexA,
                                  overlap->indexB,
                                  epsilon,
                                  collpair);

  return collpair;
}

static void cloth_points_objcollisions_nearcheck(ClothModifierData *clmd,
                                                 CollisionModifierData *collmd,
                                                 CollPair **collisions,
                                                 CollPair **collisions_index,
                                                 int numresult,
                                                 BVHTreeOverlap *overlap,
                                                 float epsilon,
                                                 double dt)
{
  int i;

  /* can return 2 collisions in total */
  *collisions = (CollPair *)MEM_mallocN(sizeof(CollPair) * numresult * 2, "collision array");
  *collisions_index = *collisions;

  for (i = 0; i < numresult; i++) {
    *collisions_index = cloth_point_collision(
        (ModifierData *)clmd, (ModifierData *)collmd, overlap + i, epsilon, *collisions_index, dt);
  }
}

void cloth_find_point_contacts(Depsgraph *depsgraph,
                               Object *ob,
                               ClothModifierData *clmd,
                               float step,
                               float dt,
                               ColliderContacts **r_collider_contacts,
                               int *r_totcolliders)
{
  Cloth *cloth = clmd->clothObject;
  BVHTree *cloth_bvh;
  unsigned int i = 0, mvert_num = 0;
  ClothVertex *verts = NULL;

  ColliderContacts *collider_contacts;

  Object **collobjs = NULL;
  unsigned int numcollobj = 0;

  verts = cloth->verts;
  mvert_num = cloth->mvert_num;

  ////////////////////////////////////////////////////////////
  // static collisions
  ////////////////////////////////////////////////////////////

  /* Check we do have collision objects to test against, before doing anything else. */
  collobjs = BKE_collision_objects_create(
      depsgraph, ob, clmd->coll_parms->group, &numcollobj, eModifierType_Collision);
  if (!collobjs) {
    *r_collider_contacts = NULL;
    *r_totcolliders = 0;
    return;
  }

  // create temporary cloth points bvh
  cloth_bvh = BLI_bvhtree_new(mvert_num, clmd->coll_parms->epsilon, 4, 6);
  /* fill tree */
  for (i = 0; i < mvert_num; i++) {
    float co[6];

    copy_v3_v3(&co[0 * 3], verts[i].x);
    copy_v3_v3(&co[1 * 3], verts[i].tx);

    BLI_bvhtree_insert(cloth_bvh, i, co, 2);
  }
  /* balance tree */
  BLI_bvhtree_balance(cloth_bvh);

  /* move object to position (step) in time */
  for (i = 0; i < numcollobj; i++) {
    Object *collob = collobjs[i];
    CollisionModifierData *collmd = (CollisionModifierData *)modifiers_findByType(
        collob, eModifierType_Collision);
    if (!collmd->bvhtree) {
      continue;
    }

    /* move object to position (step) in time */
    collision_move_object(collmd, step + dt, step);
  }

  collider_contacts = MEM_callocN(sizeof(ColliderContacts) * numcollobj, "CollPair");

  // check all collision objects
  for (i = 0; i < numcollobj; i++) {
    ColliderContacts *ct = collider_contacts + i;
    Object *collob = collobjs[i];
    CollisionModifierData *collmd = (CollisionModifierData *)modifiers_findByType(
        collob, eModifierType_Collision);
    BVHTreeOverlap *overlap;
    unsigned int result = 0;
    float epsilon;

    ct->ob = collob;
    ct->collmd = collmd;
    ct->collisions = NULL;
    ct->totcollisions = 0;

    if (!collmd->bvhtree) {
      continue;
    }

    /* search for overlapping collision pairs */
    overlap = BLI_bvhtree_overlap(cloth_bvh, collmd->bvhtree, &result, NULL, NULL);
    epsilon = BLI_bvhtree_get_epsilon(collmd->bvhtree);

    // go to next object if no overlap is there
    if (result && overlap) {
      CollPair *collisions_index;

      /* check if collisions really happen (costly near check) */
      cloth_points_objcollisions_nearcheck(
          clmd, collmd, &ct->collisions, &collisions_index, result, overlap, epsilon, dt);
      ct->totcollisions = (int)(collisions_index - ct->collisions);

      /* Resolve nearby collisions. */
#if 0
      ret += cloth_points_objcollisions_resolve(
          clmd, collmd, collob->pd, collisions[i], collisions_index[i], dt);
#endif
    }

    if (overlap) {
      MEM_freeN(overlap);
    }
  }

  BKE_collision_objects_free(collobjs);

  BLI_bvhtree_free(cloth_bvh);

  ////////////////////////////////////////////////////////////
  // update positions
  // this is needed for bvh_calc_DOP_hull_moving() [kdop.c]
  ////////////////////////////////////////////////////////////

  // verts come from clmd
  for (i = 0; i < mvert_num; i++) {
    if (clmd->sim_parms->vgroup_mass > 0) {
      if (verts[i].flags & CLOTH_VERT_FLAG_PINNED) {
        continue;
      }
    }

    add_v3_v3v3(verts[i].tx, verts[i].txold, verts[i].tv);
  }
  ////////////////////////////////////////////////////////////

  *r_collider_contacts = collider_contacts;
  *r_totcolliders = numcollobj;
}

void cloth_free_contacts(ColliderContacts *collider_contacts, int totcolliders)
{
  if (collider_contacts) {
    int i;
    for (i = 0; i < totcolliders; ++i) {
      ColliderContacts *ct = collider_contacts + i;
      if (ct->collisions) {
        MEM_freeN(ct->collisions);
      }
    }
    MEM_freeN(collider_contacts);
  }
}
