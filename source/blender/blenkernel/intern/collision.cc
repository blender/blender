/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright Blender Foundation */

/** \file
 * \ingroup bke
 */

#include "MEM_guardedalloc.h"

#include "DNA_cloth_types.h"
#include "DNA_collection_types.h"
#include "DNA_effect_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_force_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BLI_blenlib.h"
#include "BLI_edgehash.h"
#include "BLI_linklist.h"
#include "BLI_math.h"
#include "BLI_task.h"
#include "BLI_threads.h"
#include "BLI_utildefines.h"

#include "BKE_cloth.h"
#include "BKE_collection.h"
#include "BKE_effect.h"
#include "BKE_layer.h"
#include "BKE_modifier.h"
#include "BKE_scene.h"

#include "BKE_collision.h"
#include "BLI_kdopbvh.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_physics.h"
#include "DEG_depsgraph_query.h"

#ifdef WITH_ELTOPO
#  include "eltopo-capi.h"
#endif

struct ColDetectData {
  ClothModifierData *clmd;
  CollisionModifierData *collmd;
  BVHTreeOverlap *overlap;
  CollPair *collisions;
  bool culling;
  bool use_normal;
  bool collided;
};

struct SelfColDetectData {
  ClothModifierData *clmd;
  BVHTreeOverlap *overlap;
  CollPair *collisions;
  bool collided;
};

/***********************************
 * Collision modifier code start
 ***********************************/

void collision_move_object(CollisionModifierData *collmd,
                           const float step,
                           const float prevstep,
                           const bool moving_bvh)
{
  uint i = 0;

  /* the collider doesn't move this frame */
  if (collmd->is_static) {
    for (i = 0; i < collmd->mvert_num; i++) {
      zero_v3(collmd->current_v[i]);
    }

    return;
  }

  for (i = 0; i < collmd->mvert_num; i++) {
    interp_v3_v3v3(collmd->current_x[i], collmd->x[i], collmd->xnew[i], prevstep);
    interp_v3_v3v3(collmd->current_xnew[i], collmd->x[i], collmd->xnew[i], step);
    sub_v3_v3v3(collmd->current_v[i], collmd->current_xnew[i], collmd->current_x[i]);
  }

  bvhtree_update_from_mvert(collmd->bvhtree,
                            collmd->current_xnew,
                            collmd->current_x,
                            collmd->tri,
                            collmd->tri_num,
                            moving_bvh);
}

BVHTree *bvhtree_build_from_mvert(const float (*positions)[3],
                                  const MVertTri *tri,
                                  int tri_num,
                                  float epsilon)
{
  BVHTree *tree = BLI_bvhtree_new(tri_num, epsilon, 4, 26);

  /* fill tree */
  int i;
  const MVertTri *vt;
  for (i = 0, vt = tri; i < tri_num; i++, vt++) {
    float co[3][3];

    copy_v3_v3(co[0], positions[vt->tri[0]]);
    copy_v3_v3(co[1], positions[vt->tri[1]]);
    copy_v3_v3(co[2], positions[vt->tri[2]]);

    BLI_bvhtree_insert(tree, i, co[0], 3);
  }

  /* balance tree */
  BLI_bvhtree_balance(tree);

  return tree;
}

void bvhtree_update_from_mvert(BVHTree *bvhtree,
                               const float (*positions)[3],
                               const float (*positions_moving)[3],
                               const MVertTri *tri,
                               int tri_num,
                               bool moving)
{

  if ((bvhtree == nullptr) || (positions == nullptr)) {
    return;
  }

  if (positions_moving == nullptr) {
    moving = false;
  }

  const MVertTri *vt;
  int i;
  for (i = 0, vt = tri; i < tri_num; i++, vt++) {
    float co[3][3];
    bool ret;

    copy_v3_v3(co[0], positions[vt->tri[0]]);
    copy_v3_v3(co[1], positions[vt->tri[1]]);
    copy_v3_v3(co[2], positions[vt->tri[2]]);

    /* copy new locations into array */
    if (moving) {
      float co_moving[3][3];
      /* update moving positions */
      copy_v3_v3(co_moving[0], positions_moving[vt->tri[0]]);
      copy_v3_v3(co_moving[1], positions_moving[vt->tri[1]]);
      copy_v3_v3(co_moving[2], positions_moving[vt->tri[2]]);

      ret = BLI_bvhtree_update_node(bvhtree, i, &co[0][0], &co_moving[0][0], 3);
    }
    else {
      ret = BLI_bvhtree_update_node(bvhtree, i, &co[0][0], nullptr, 3);
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

static float compute_collision_point_tri_tri(const float a1[3],
                                             const float a2[3],
                                             const float a3[3],
                                             const float b1[3],
                                             const float b2[3],
                                             const float b3[3],
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
  int tri_a_edge_isect_count;
  const bool is_intersecting = isect_tri_tri_v3_ex(
      a, b, isect_a, isect_b, &tri_a_edge_isect_count);

  /* Determine collision side. */
  if (culling) {
    normal_tri_v3(normal, b[0], b[1], b[2]);
    mid_v3_v3v3v3(cent, b[0], b[1], b[2]);

    if (!is_intersecting) {
      for (int i = 0; i < 3; i++) {
        sub_v3_v3v3(tmp_vec, a[i], cent);
        if (dot_v3v3(tmp_vec, normal) < 0.0f) {
          backside = true;
          break;
        }
      }
    }
    else if (tri_a_edge_isect_count != 1) {
      /* It is not Edge intersection. */
      backside = true;
    }
  }
  else if (use_normal) {
    normal_tri_v3(normal, b[0], b[1], b[2]);
  }

  if (tri_a_edge_isect_count == 1) {
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
      if (isect_ray_tri_v3(a[i], normal, b[0], b[1], b[2], &tmp, nullptr)) {
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
      if (isect_ray_tri_v3(b[i], normal, a[0], a[1], a[2], &tmp, nullptr)) {
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
            point_in_slice_seg(tmp_co1, b[i], b[next_ind(i)]))
        {
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
  if (!is_intersecting) {
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

  if (!is_intersecting) {
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

static float compute_collision_point_edge_tri(const float a1[3],
                                              const float a2[3],
                                              const float b1[3],
                                              const float b2[3],
                                              const float b3[3],
                                              bool culling,
                                              bool use_normal,
                                              float r_a[3],
                                              float r_b[3],
                                              float r_vec[3])
{
  float a[2][3];
  float b[3][3];
  float dist = FLT_MAX;
  float tmp_co1[3], tmp_co2[3];
  float isect_a[3];
  bool isect = false;
  float tmp, tmp_vec[3];
  float normal[3], cent[3];
  bool backside = false;

  copy_v3_v3(a[0], a1);
  copy_v3_v3(a[1], a2);

  copy_v3_v3(b[0], b1);
  copy_v3_v3(b[1], b2);
  copy_v3_v3(b[2], b3);

  normal_tri_v3(normal, b[0], b[1], b[2]);

  /* Find intersection. */
  if (isect_line_segment_tri_v3(a[0], a[1], b[0], b[1], b[2], &tmp, nullptr)) {
    interp_v3_v3v3(isect_a, a[0], a[1], tmp);
    isect = true;
  }

  /* Determine collision side. */
  if (culling) {
    if (isect) {
      backside = true;
    }
    else {
      mid_v3_v3v3v3(cent, b[0], b[1], b[2]);

      for (int i = 0; i < 2; i++) {
        sub_v3_v3v3(tmp_vec, a[i], cent);
        if (dot_v3v3(tmp_vec, normal) < 0.0f) {
          backside = true;
          break;
        }
      }
    }
  }

  if (isect) {
    /* Edge intersection. */
    copy_v3_v3(r_a, isect_a);
    copy_v3_v3(r_b, isect_a);

    copy_v3_v3(r_vec, normal);

    return 0.0f;
  }

  if (backside) {
    float maxdist = 0.0f;
    bool found = false;

    /* Point projections. */
    for (int i = 0; i < 2; i++) {
      if (isect_ray_tri_v3(a[i], normal, b[0], b[1], b[2], &tmp, nullptr)) {
        if (tmp > maxdist) {
          maxdist = tmp;
          copy_v3_v3(r_a, a[i]);
          madd_v3_v3v3fl(r_b, a[i], normal, tmp);
          found = true;
        }
      }
    }

    /* Edge projections. */
    for (int i = 0; i < 3; i++) {
      float dir[3];

      sub_v3_v3v3(tmp_vec, b[next_ind(i)], b[i]);
      cross_v3_v3v3(dir, tmp_vec, normal);

      if (isect_line_plane_v3(tmp_co1, a[0], a[1], b[i], dir) &&
          point_in_slice_seg(tmp_co1, a[0], a[1]) &&
          point_in_slice_seg(tmp_co1, b[i], b[next_ind(i)]))
      {
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
  for (int i = 0; i < 2; i++) {
    closest_on_tri_to_point_v3(tmp_co1, a[i], b[0], b[1], b[2]);
    tmp = len_squared_v3v3(tmp_co1, a[i]);

    if (tmp < dist) {
      dist = tmp;
      copy_v3_v3(r_a, a[i]);
      copy_v3_v3(r_b, tmp_co1);
    }
  }

  /* Closest edge. */
  if (!isect) {
    for (int j = 0; j < 3; j++) {
      isect_seg_seg_v3(a[0], a[1], b[j], b[next_ind(j)], tmp_co1, tmp_co2);
      tmp = len_squared_v3v3(tmp_co1, tmp_co2);

      if (tmp < dist) {
        dist = tmp;
        copy_v3_v3(r_a, tmp_co1);
        copy_v3_v3(r_b, tmp_co2);
      }
    }
  }

  if (isect) {
    sub_v3_v3v3(r_vec, r_b, r_a);
    dist = 0.0f;
  }
  else {
    sub_v3_v3v3(r_vec, r_a, r_b);
    dist = sqrtf(dist);
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

/* `w3` is not perfect. */
static void collision_compute_barycentric(const float pv[3],
                                          const float p1[3],
                                          const float p2[3],
                                          const float p3[3],
                                          float *w1,
                                          float *w2,
                                          float *w3)
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

  if (fabs(d) < double(ALMOST_ZERO)) {
    *w1 = *w2 = *w3 = 1.0 / 3.0;
    return;
  }

  w1[0] = float((e * c - b * f) / d);

  if (w1[0] < 0) {
    w1[0] = 0;
  }

  w2[0] = float((f - b * double(w1[0])) / c);

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

DO_INLINE void collision_interpolateOnTriangle(float to[3],
                                               const float v1[3],
                                               const float v2[3],
                                               const float v3[3],
                                               const double w1,
                                               const double w2,
                                               const double w3)
{
  zero_v3(to);
  VECADDMUL(to, v1, w1);
  VECADDMUL(to, v2, w2);
  VECADDMUL(to, v3, w3);
}

static void cloth_collision_impulse_vert(const float clamp_sq,
                                         const float impulse[3],
                                         ClothVertex *vert)
{
  float impulse_len_sq = len_squared_v3(impulse);

  if ((clamp_sq > 0.0f) && (impulse_len_sq > clamp_sq)) {
    return;
  }

  if (fabsf(vert->impulse[0]) < fabsf(impulse[0])) {
    vert->impulse[0] = impulse[0];
  }

  if (fabsf(vert->impulse[1]) < fabsf(impulse[1])) {
    vert->impulse[1] = impulse[1];
  }

  if (fabsf(vert->impulse[2]) < fabsf(impulse[2])) {
    vert->impulse[2] = impulse[2];
  }

  vert->impulse_count++;
}

static int cloth_collision_response_static(ClothModifierData *clmd,
                                           CollisionModifierData *collmd,
                                           Object *collob,
                                           CollPair *collpair,
                                           uint collision_count,
                                           const float dt)
{
  int result = 0;
  Cloth *cloth = clmd->clothObject;
  const float clamp_sq = square_f(clmd->coll_parms->clamp * dt);
  const float time_multiplier = 1.0f / (clmd->sim_parms->dt * clmd->sim_parms->timescale);
  const float epsilon2 = BLI_bvhtree_get_epsilon(collmd->bvhtree);
  const float min_distance = (clmd->coll_parms->epsilon + epsilon2) * (8.0f / 9.0f);

  const bool is_hair = (clmd->hairdata != nullptr);
  for (int i = 0; i < collision_count; i++, collpair++) {
    float i1[3], i2[3], i3[3];
    float v1[3], v2[3], relativeVelocity[3];
    zero_v3(i1);
    zero_v3(i2);
    zero_v3(i3);

    /* Only handle static collisions here. */
    if (collpair->flag & (COLLISION_IN_FUTURE | COLLISION_INACTIVE)) {
      continue;
    }

    /* Compute barycentric coordinates and relative "velocity" for both collision points. */
    float w1 = collpair->aw1, w2 = collpair->aw2, w3 = collpair->aw3;
    float u1 = collpair->bw1, u2 = collpair->bw2, u3 = collpair->bw3;

    if (is_hair) {
      interp_v3_v3v3(v1, cloth->verts[collpair->ap1].tv, cloth->verts[collpair->ap2].tv, w2);
    }
    else {
      collision_interpolateOnTriangle(v1,
                                      cloth->verts[collpair->ap1].tv,
                                      cloth->verts[collpair->ap2].tv,
                                      cloth->verts[collpair->ap3].tv,
                                      w1,
                                      w2,
                                      w3);
    }

    collision_interpolateOnTriangle(v2,
                                    collmd->current_v[collpair->bp1],
                                    collmd->current_v[collpair->bp2],
                                    collmd->current_v[collpair->bp3],
                                    u1,
                                    u2,
                                    u3);

    sub_v3_v3v3(relativeVelocity, v2, v1);

    /* Calculate the normal component of the relative velocity
     * (actually only the magnitude - the direction is stored in 'normal'). */
    const float magrelVel = dot_v3v3(relativeVelocity, collpair->normal);
    const float d = min_distance - collpair->distance;

    /* If magrelVel < 0 the edges are approaching each other. */
    if (magrelVel > 0.0f) {
      /* Calculate Impulse magnitude to stop all motion in normal direction. */
      float magtangent = 0, repulse = 0;
      double impulse = 0.0;
      float vrel_t_pre[3];
      float temp[3];

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

        VECADDMUL(i1, vrel_t_pre, double(w1) * impulse);
        VECADDMUL(i2, vrel_t_pre, double(w2) * impulse);

        if (!is_hair) {
          VECADDMUL(i3, vrel_t_pre, double(w3) * impulse);
        }
      }

      /* Apply velocity stopping impulse. */
      impulse = magrelVel / 1.5f;

      VECADDMUL(i1, collpair->normal, double(w1) * impulse);
      VECADDMUL(i2, collpair->normal, double(w2) * impulse);
      if (!is_hair) {
        VECADDMUL(i3, collpair->normal, double(w3) * impulse);
      }

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
        if (!is_hair) {
          VECADDMUL(i3, collpair->normal, impulse);
        }
      }

      result = 1;
    }
    else if (d > ALMOST_ZERO) {
      /* Stay on the safe side and clamp repulse. */
      float repulse = d / time_multiplier;
      float impulse = repulse / 4.5f;

      VECADDMUL(i1, collpair->normal, w1 * impulse);
      VECADDMUL(i2, collpair->normal, w2 * impulse);

      if (!is_hair) {
        VECADDMUL(i3, collpair->normal, w3 * impulse);
      }

      result = 1;
    }

    if (result) {
      cloth_collision_impulse_vert(clamp_sq, i1, &cloth->verts[collpair->ap1]);
      cloth_collision_impulse_vert(clamp_sq, i2, &cloth->verts[collpair->ap2]);
      if (!is_hair) {
        cloth_collision_impulse_vert(clamp_sq, i3, &cloth->verts[collpair->ap3]);
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
  Cloth *cloth = clmd->clothObject;
  const float clamp_sq = square_f(clmd->coll_parms->self_clamp * dt);
  const float time_multiplier = 1.0f / (clmd->sim_parms->dt * clmd->sim_parms->timescale);
  const float min_distance = (2.0f * clmd->coll_parms->selfepsilon) * (8.0f / 9.0f);

  for (int i = 0; i < collision_count; i++, collpair++) {
    float ia[3][3] = {{0.0f}};
    float ib[3][3] = {{0.0f}};
    float v1[3], v2[3], relativeVelocity[3];

    /* Only handle static collisions here. */
    if (collpair->flag & (COLLISION_IN_FUTURE | COLLISION_INACTIVE)) {
      continue;
    }

    /* Retrieve barycentric coordinates for both collision points. */
    float w1 = collpair->aw1, w2 = collpair->aw2, w3 = collpair->aw3;
    float u1 = collpair->bw1, u2 = collpair->bw2, u3 = collpair->bw3;

    /* Calculate relative "velocity". */
    collision_interpolateOnTriangle(v1,
                                    cloth->verts[collpair->ap1].tv,
                                    cloth->verts[collpair->ap2].tv,
                                    cloth->verts[collpair->ap3].tv,
                                    w1,
                                    w2,
                                    w3);

    collision_interpolateOnTriangle(v2,
                                    cloth->verts[collpair->bp1].tv,
                                    cloth->verts[collpair->bp2].tv,
                                    cloth->verts[collpair->bp3].tv,
                                    u1,
                                    u2,
                                    u3);

    sub_v3_v3v3(relativeVelocity, v2, v1);

    /* Calculate the normal component of the relative velocity
     * (actually only the magnitude - the direction is stored in 'normal'). */
    const float magrelVel = dot_v3v3(relativeVelocity, collpair->normal);
    const float d = min_distance - collpair->distance;

    /* TODO: Impulses should be weighed by mass as this is self col,
     * this has to be done after mass distribution is implemented. */

    /* If magrelVel < 0 the edges are approaching each other. */
    if (magrelVel > 0.0f) {
      /* Calculate Impulse magnitude to stop all motion in normal direction. */
      float magtangent = 0, repulse = 0;
      double impulse = 0.0;
      float vrel_t_pre[3];
      float temp[3];

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

        VECADDMUL(ia[0], vrel_t_pre, double(w1) * impulse);
        VECADDMUL(ia[1], vrel_t_pre, double(w2) * impulse);
        VECADDMUL(ia[2], vrel_t_pre, double(w3) * impulse);

        VECADDMUL(ib[0], vrel_t_pre, double(u1) * -impulse);
        VECADDMUL(ib[1], vrel_t_pre, double(u2) * -impulse);
        VECADDMUL(ib[2], vrel_t_pre, double(u3) * -impulse);
      }

      /* Apply velocity stopping impulse. */
      impulse = magrelVel / 3.0f;

      VECADDMUL(ia[0], collpair->normal, double(w1) * impulse);
      VECADDMUL(ia[1], collpair->normal, double(w2) * impulse);
      VECADDMUL(ia[2], collpair->normal, double(w3) * impulse);

      VECADDMUL(ib[0], collpair->normal, double(u1) * -impulse);
      VECADDMUL(ib[1], collpair->normal, double(u2) * -impulse);
      VECADDMUL(ib[2], collpair->normal, double(u3) * -impulse);

      if ((magrelVel < 0.1f * d * time_multiplier) && (d > ALMOST_ZERO)) {
        repulse = MIN2(d / time_multiplier, 0.1f * d * time_multiplier - magrelVel);

        if (impulse > ALMOST_ZERO) {
          repulse = min_ff(repulse, 5.0 * impulse);
        }

        repulse = max_ff(impulse, repulse);
        impulse = repulse / 1.5f;

        VECADDMUL(ia[0], collpair->normal, double(w1) * impulse);
        VECADDMUL(ia[1], collpair->normal, double(w2) * impulse);
        VECADDMUL(ia[2], collpair->normal, double(w3) * impulse);

        VECADDMUL(ib[0], collpair->normal, double(u1) * -impulse);
        VECADDMUL(ib[1], collpair->normal, double(u2) * -impulse);
        VECADDMUL(ib[2], collpair->normal, double(u3) * -impulse);
      }

      result = 1;
    }
    else if (d > ALMOST_ZERO) {
      /* Stay on the safe side and clamp repulse. */
      float repulse = d * 1.0f / time_multiplier;
      float impulse = repulse / 9.0f;

      VECADDMUL(ia[0], collpair->normal, w1 * impulse);
      VECADDMUL(ia[1], collpair->normal, w2 * impulse);
      VECADDMUL(ia[2], collpair->normal, w3 * impulse);

      VECADDMUL(ib[0], collpair->normal, u1 * -impulse);
      VECADDMUL(ib[1], collpair->normal, u2 * -impulse);
      VECADDMUL(ib[2], collpair->normal, u3 * -impulse);

      result = 1;
    }

    if (result) {
      cloth_collision_impulse_vert(clamp_sq, ia[0], &cloth->verts[collpair->ap1]);
      cloth_collision_impulse_vert(clamp_sq, ia[1], &cloth->verts[collpair->ap2]);
      cloth_collision_impulse_vert(clamp_sq, ia[2], &cloth->verts[collpair->ap3]);

      cloth_collision_impulse_vert(clamp_sq, ib[0], &cloth->verts[collpair->bp1]);
      cloth_collision_impulse_vert(clamp_sq, ib[1], &cloth->verts[collpair->bp2]);
      cloth_collision_impulse_vert(clamp_sq, ib[2], &cloth->verts[collpair->bp3]);
    }
  }

  return result;
}

#ifdef __GNUC__
#  pragma GCC diagnostic pop
#endif

static bool cloth_bvh_collision_is_active(const ClothModifierData * /*clmd*/,
                                          const Cloth *cloth,
                                          const MVertTri *tri_a)
{
  const ClothVertex *verts = cloth->verts;

  /* Fully pinned triangles don't need collision processing. */
  const int flags_a = verts[tri_a->tri[0]].flags & verts[tri_a->tri[1]].flags &
                      verts[tri_a->tri[2]].flags;

  if (flags_a & (CLOTH_VERT_FLAG_PINNED | CLOTH_VERT_FLAG_NOOBJCOLL)) {
    return false;
  }

  return true;
}

static void cloth_collision(void *__restrict userdata,
                            const int index,
                            const TaskParallelTLS *__restrict /*tls*/)
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
  distance = compute_collision_point_tri_tri(verts1[tri_a->tri[0]].tx,
                                             verts1[tri_a->tri[1]].tx,
                                             verts1[tri_a->tri[2]].tx,
                                             collmd->current_xnew[tri_b->tri[0]],
                                             collmd->current_xnew[tri_b->tri[1]],
                                             collmd->current_xnew[tri_b->tri[2]],
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

    /* Compute barycentric coordinates for both collision points. */
    collision_compute_barycentric(pa,
                                  verts1[tri_a->tri[0]].tx,
                                  verts1[tri_a->tri[1]].tx,
                                  verts1[tri_a->tri[2]].tx,
                                  &collpair[index].aw1,
                                  &collpair[index].aw2,
                                  &collpair[index].aw3);

    collision_compute_barycentric(pb,
                                  collmd->current_xnew[tri_b->tri[0]],
                                  collmd->current_xnew[tri_b->tri[1]],
                                  collmd->current_xnew[tri_b->tri[2]],
                                  &collpair[index].bw1,
                                  &collpair[index].bw2,
                                  &collpair[index].bw3);
  }
  else {
    collpair[index].flag = COLLISION_INACTIVE;
  }
}

static bool cloth_bvh_selfcollision_is_active(const ClothModifierData *clmd,
                                              const Cloth *cloth,
                                              const MVertTri *tri_a,
                                              const MVertTri *tri_b)
{
  const ClothVertex *verts = cloth->verts;

  /* Skip when either triangle is excluded. */
  const int flags_a = verts[tri_a->tri[0]].flags & verts[tri_a->tri[1]].flags &
                      verts[tri_a->tri[2]].flags;
  const int flags_b = verts[tri_b->tri[0]].flags & verts[tri_b->tri[1]].flags &
                      verts[tri_b->tri[2]].flags;

  if ((flags_a | flags_b) & CLOTH_VERT_FLAG_NOSELFCOLL) {
    return false;
  }

  /* Skip when both triangles are pinned. */
  if ((flags_a & flags_b) & CLOTH_VERT_FLAG_PINNED) {
    return false;
  }

  /* Ignore overlap of neighboring triangles and triangles connected by a sewing edge. */
  bool sewing_active = (clmd->sim_parms->flags & CLOTH_SIMSETTINGS_FLAG_SEW);

  for (uint i = 0; i < 3; i++) {
    for (uint j = 0; j < 3; j++) {
      if (tri_a->tri[i] == tri_b->tri[j]) {
        return false;
      }

      if (sewing_active) {
        if (BLI_edgeset_haskey(cloth->sew_edge_graph, tri_a->tri[i], tri_b->tri[j])) {
          return false;
        }
      }
    }
  }

  return true;
}

static void cloth_selfcollision(void *__restrict userdata,
                                const int index,
                                const TaskParallelTLS *__restrict /*tls*/)
{
  SelfColDetectData *data = (SelfColDetectData *)userdata;

  ClothModifierData *clmd = data->clmd;
  CollPair *collpair = data->collisions;
  const MVertTri *tri_a, *tri_b;
  ClothVertex *verts1 = clmd->clothObject->verts;
  float distance = 0.0f;
  float epsilon = clmd->coll_parms->selfepsilon;
  float pa[3], pb[3], vect[3];

  /* Collision math is currently not symmetric, so ensure a stable order for each pair. */
  int indexA = data->overlap[index].indexA, indexB = data->overlap[index].indexB;

  if (indexA > indexB) {
    SWAP(int, indexA, indexB);
  }

  tri_a = &clmd->clothObject->tri[indexA];
  tri_b = &clmd->clothObject->tri[indexB];

  BLI_assert(cloth_bvh_selfcollision_is_active(clmd, clmd->clothObject, tri_a, tri_b));

  /* Compute distance and normal. */
  distance = compute_collision_point_tri_tri(verts1[tri_a->tri[0]].tx,
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

    /* Compute barycentric coordinates for both collision points. */
    collision_compute_barycentric(pa,
                                  verts1[tri_a->tri[0]].tx,
                                  verts1[tri_a->tri[1]].tx,
                                  verts1[tri_a->tri[2]].tx,
                                  &collpair[index].aw1,
                                  &collpair[index].aw2,
                                  &collpair[index].aw3);

    collision_compute_barycentric(pb,
                                  verts1[tri_b->tri[0]].tx,
                                  verts1[tri_b->tri[1]].tx,
                                  verts1[tri_b->tri[2]].tx,
                                  &collpair[index].bw1,
                                  &collpair[index].bw2,
                                  &collpair[index].bw3);
  }
  else {
    collpair[index].flag = COLLISION_INACTIVE;
  }
}

static void hair_collision(void *__restrict userdata,
                           const int index,
                           const TaskParallelTLS *__restrict /*tls*/)
{
  ColDetectData *data = (ColDetectData *)userdata;

  ClothModifierData *clmd = data->clmd;
  CollisionModifierData *collmd = data->collmd;
  CollPair *collpair = data->collisions;
  const MVertTri *tri_coll;
  ClothVertex *verts1 = clmd->clothObject->verts;
  float distance = 0.0f;
  float epsilon1 = clmd->coll_parms->epsilon;
  float epsilon2 = BLI_bvhtree_get_epsilon(collmd->bvhtree);
  float pa[3], pb[3], vect[3];

  /* TODO: This is not efficient. Might be wise to instead build an array before iterating, to
   * avoid walking the list every time. */
  const blender::int2 &edge_coll = reinterpret_cast<const blender::int2 *>(
      clmd->clothObject->edges)[data->overlap[index].indexA];
  tri_coll = &collmd->tri[data->overlap[index].indexB];

  /* Compute distance and normal. */
  distance = compute_collision_point_edge_tri(verts1[edge_coll[0]].tx,
                                              verts1[edge_coll[1]].tx,
                                              collmd->current_x[tri_coll->tri[0]],
                                              collmd->current_x[tri_coll->tri[1]],
                                              collmd->current_x[tri_coll->tri[2]],
                                              data->culling,
                                              data->use_normal,
                                              pa,
                                              pb,
                                              vect);

  if ((distance <= (epsilon1 + epsilon2 + ALMOST_ZERO)) && (len_squared_v3(vect) > ALMOST_ZERO)) {
    collpair[index].ap1 = edge_coll[0];
    collpair[index].ap2 = edge_coll[1];

    collpair[index].bp1 = tri_coll->tri[0];
    collpair[index].bp2 = tri_coll->tri[1];
    collpair[index].bp3 = tri_coll->tri[2];

    copy_v3_v3(collpair[index].pa, pa);
    copy_v3_v3(collpair[index].pb, pb);
    copy_v3_v3(collpair[index].vector, vect);

    normalize_v3_v3(collpair[index].normal, collpair[index].vector);

    collpair[index].distance = distance;
    collpair[index].flag = 0;

    data->collided = true;

    /* Compute barycentric coordinates for the collision points. */
    collpair[index].aw2 = line_point_factor_v3(
        pa, verts1[edge_coll[0]].tx, verts1[edge_coll[1]].tx);

    collpair[index].aw1 = 1.0f - collpair[index].aw2;

    collision_compute_barycentric(pb,
                                  collmd->current_xnew[tri_coll->tri[0]],
                                  collmd->current_xnew[tri_coll->tri[1]],
                                  collmd->current_xnew[tri_coll->tri[2]],
                                  &collpair[index].bw1,
                                  &collpair[index].bw2,
                                  &collpair[index].bw3);
  }
  else {
    collpair[index].flag = COLLISION_INACTIVE;
  }
}

static void add_collision_object(ListBase *relations,
                                 Object *ob,
                                 int level,
                                 const ModifierType modifier_type)
{
  /* only get objects with collision modifier */
  ModifierData *cmd = BKE_modifiers_findby_type(ob, modifier_type);

  if (cmd) {
    CollisionRelation *relation = MEM_cnew<CollisionRelation>(__func__);
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

ListBase *BKE_collision_relations_create(Depsgraph *depsgraph,
                                         Collection *collection,
                                         uint modifier_type)
{
  const Scene *scene = DEG_get_input_scene(depsgraph);
  ViewLayer *view_layer = DEG_get_input_view_layer(depsgraph);
  Base *base = BKE_collection_or_layer_objects(scene, view_layer, collection);
  const bool for_render = (DEG_get_mode(depsgraph) == DAG_EVAL_RENDER);
  const int base_flag = (for_render) ? BASE_ENABLED_RENDER : BASE_ENABLED_VIEWPORT;

  ListBase *relations = MEM_cnew<ListBase>(__func__);

  for (; base; base = base->next) {
    if (base->flag & base_flag) {
      add_collision_object(relations, base->object, 0, ModifierType(modifier_type));
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

Object **BKE_collision_objects_create(Depsgraph *depsgraph,
                                      Object *self,
                                      Collection *collection,
                                      uint *numcollobj,
                                      uint modifier_type)
{
  ListBase *relations = DEG_get_collision_relations(depsgraph, collection, modifier_type);

  if (!relations) {
    *numcollobj = 0;
    return nullptr;
  }

  int maxnum = BLI_listbase_count(relations);
  int num = 0;
  Object **objects = MEM_cnew_array<Object *>(maxnum, __func__);

  LISTBASE_FOREACH (CollisionRelation *, relation, relations) {
    /* Get evaluated object. */
    Object *ob = (Object *)DEG_get_evaluated_id(depsgraph, &relation->ob->id);

    if (modifier_type == eModifierType_Collision && !(ob->pd && ob->pd->deflect)) {
      continue;
    }

    if (ob != self) {
      objects[num] = ob;
      num++;
    }
  }

  if (num == 0) {
    MEM_freeN(objects);
    objects = nullptr;
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

ListBase *BKE_collider_cache_create(Depsgraph *depsgraph, Object *self, Collection *collection)
{
  ListBase *relations = DEG_get_collision_relations(
      depsgraph, collection, eModifierType_Collision);
  ListBase *cache = nullptr;

  if (!relations) {
    return nullptr;
  }

  LISTBASE_FOREACH (CollisionRelation *, relation, relations) {
    /* Get evaluated object. */
    Object *ob = (Object *)DEG_get_evaluated_id(depsgraph, &relation->ob->id);

    if (ob == self) {
      continue;
    }

    CollisionModifierData *cmd = (CollisionModifierData *)BKE_modifiers_findby_type(
        ob, eModifierType_Collision);
    if (cmd && cmd->bvhtree) {
      if (cache == nullptr) {
        cache = MEM_cnew<ListBase>(__func__);
      }

      ColliderCache *col = MEM_cnew<ColliderCache>(__func__);
      col->ob = ob;
      col->collmd = cmd;
      /* make sure collider is properly set up */
      collision_move_object(cmd, 1.0, 0.0, true);
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
    *colliders = nullptr;
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
  const bool is_hair = (clmd->hairdata != nullptr);
  *collisions = (CollPair *)MEM_mallocN(sizeof(CollPair) * numresult, "collision array");

  ColDetectData data{};
  data.clmd = clmd;
  data.collmd = collmd;
  data.overlap = overlap;
  data.collisions = *collisions;
  data.culling = culling;
  data.use_normal = use_normal;
  data.collided = false;

  TaskParallelSettings settings;
  BLI_parallel_range_settings_defaults(&settings);
  settings.use_threading = true;
  BLI_task_parallel_range(
      0, numresult, &data, is_hair ? hair_collision : cloth_collision, &settings);

  return data.collided;
}

static bool cloth_bvh_selfcollisions_nearcheck(ClothModifierData *clmd,
                                               CollPair *collisions,
                                               int numresult,
                                               BVHTreeOverlap *overlap)
{
  SelfColDetectData data{};
  data.clmd = clmd;
  data.overlap = overlap;
  data.collisions = collisions;
  data.collided = false;

  TaskParallelSettings settings;
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
  ClothVertex *verts = nullptr;
  int ret = 0;
  int result = 0;

  mvert_num = clmd->clothObject->mvert_num;
  verts = cloth->verts;

  result = 1;

  for (j = 0; j < 2; j++) {
    result = 0;

    for (i = 0; i < numcollobj; i++) {
      Object *collob = collobjs[i];
      CollisionModifierData *collmd = (CollisionModifierData *)BKE_modifiers_findby_type(
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
  ClothVertex *verts = nullptr;
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

static bool cloth_bvh_obj_overlap_cb(void *userdata, int index_a, int /*index_b*/, int /*thread*/)
{
  ClothModifierData *clmd = (ClothModifierData *)userdata;
  Cloth *clothObject = clmd->clothObject;
  const MVertTri *tri_a = &clothObject->tri[index_a];

  return cloth_bvh_collision_is_active(clmd, clothObject, tri_a);
}

static bool cloth_bvh_self_overlap_cb(void *userdata, int index_a, int index_b, int /*thread*/)
{
  /* This shouldn't happen, but just in case. Note that equal combinations
   * (eg. (0,1) & (1,0)) would be filtered out by BLI_bvhtree_overlap_self. */
  if (index_a != index_b) {
    ClothModifierData *clmd = (ClothModifierData *)userdata;
    Cloth *clothObject = clmd->clothObject;
    const MVertTri *tri_a, *tri_b;
    tri_a = &clothObject->tri[index_a];
    tri_b = &clothObject->tri[index_b];

    if (cloth_bvh_selfcollision_is_active(clmd, clothObject, tri_a, tri_b)) {
      return true;
    }
  }
  return false;
}

int cloth_bvh_collision(
    Depsgraph *depsgraph, Object *ob, ClothModifierData *clmd, float step, float dt)
{
  Cloth *cloth = clmd->clothObject;
  BVHTree *cloth_bvh = cloth->bvhtree;
  uint i = 0, mvert_num = 0;
  int rounds = 0;
  ClothVertex *verts = nullptr;
  int ret = 0, ret2 = 0;
  Object **collobjs = nullptr;
  uint numcollobj = 0;
  uint *coll_counts_obj = nullptr;
  BVHTreeOverlap **overlap_obj = nullptr;
  uint coll_count_self = 0;
  BVHTreeOverlap *overlap_self = nullptr;
  bool bvh_updated = false;

  if ((clmd->sim_parms->flags & CLOTH_SIMSETTINGS_FLAG_COLLOBJ) || cloth_bvh == nullptr) {
    return 0;
  }

  verts = cloth->verts;
  mvert_num = cloth->mvert_num;

  if (clmd->coll_parms->flags & CLOTH_COLLSETTINGS_FLAG_ENABLED) {
    bvhtree_update_from_cloth(clmd, false, false);
    bvh_updated = true;

    /* Enable self collision if this is a hair sim */
    const bool is_hair = (clmd->hairdata != nullptr);

    collobjs = BKE_collision_objects_create(depsgraph,
                                            is_hair ? nullptr : ob,
                                            clmd->coll_parms->group,
                                            &numcollobj,
                                            eModifierType_Collision);

    if (collobjs) {
      coll_counts_obj = MEM_cnew_array<uint>(numcollobj, "CollCounts");
      overlap_obj = MEM_cnew_array<BVHTreeOverlap *>(numcollobj, "BVHOverlap");

      for (i = 0; i < numcollobj; i++) {
        Object *collob = collobjs[i];
        CollisionModifierData *collmd = (CollisionModifierData *)BKE_modifiers_findby_type(
            collob, eModifierType_Collision);

        if (!collmd->bvhtree) {
          continue;
        }

        /* Move object to position (step) in time. */
        collision_move_object(collmd, step + dt, step, false);

        overlap_obj[i] = BLI_bvhtree_overlap(cloth_bvh,
                                             collmd->bvhtree,
                                             &coll_counts_obj[i],
                                             is_hair ? nullptr : cloth_bvh_obj_overlap_cb,
                                             clmd);
      }
    }
  }

  if (clmd->coll_parms->flags & CLOTH_COLLSETTINGS_FLAG_SELF) {
    if (cloth->bvhselftree != cloth->bvhtree || !bvh_updated) {
      bvhtree_update_from_cloth(clmd, false, true);
    }

    overlap_self = BLI_bvhtree_overlap_self(
        cloth->bvhselftree, &coll_count_self, cloth_bvh_self_overlap_cb, clmd);
  }

  do {
    ret2 = 0;

    /* Object collisions. */
    if ((clmd->coll_parms->flags & CLOTH_COLLSETTINGS_FLAG_ENABLED) && collobjs) {
      CollPair **collisions;
      bool collided = false;

      collisions = MEM_cnew_array<CollPair *>(numcollobj, "CollPair");

      for (i = 0; i < numcollobj; i++) {
        Object *collob = collobjs[i];
        CollisionModifierData *collmd = (CollisionModifierData *)BKE_modifiers_findby_type(
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
      CollPair *collisions = nullptr;

      verts = cloth->verts;
      mvert_num = cloth->mvert_num;

      if (cloth->bvhselftree) {
        if (coll_count_self && overlap_self) {
          collisions = (CollPair *)MEM_mallocN(sizeof(CollPair) * coll_count_self,
                                               "collision array");

          if (cloth_bvh_selfcollisions_nearcheck(clmd, collisions, coll_count_self, overlap_self))
          {
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
                                collmd->current_x[collpair->bp1],
                                collmd->current_x[collpair->bp2],
                                collmd->current_x[collpair->bp3],
                                &u1,
                                &u2,
                                &u3);

  collision_interpolateOnTriangle(vel_new,
                                  collmd->current_v[collpair->bp1],
                                  collmd->current_v[collpair->bp2],
                                  collmd->current_v[collpair->bp3],
                                  u1,
                                  u2,
                                  u3);
  /* XXX assume constant velocity of the collider for now */
  copy_v3_v3(vel_old, vel_new);
}
