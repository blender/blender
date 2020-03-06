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
 * along with this program; if not, write to the Free Software  Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2020 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup edsculpt
 */

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_blenlib.h"
#include "BLI_task.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_brush_types.h"

#include "BKE_brush.h"
#include "BKE_ccg.h"
#include "BKE_colortools.h"
#include "BKE_context.h"
#include "BKE_mesh.h"
#include "BKE_multires.h"
#include "BKE_node.h"
#include "BKE_object.h"
#include "BKE_paint.h"
#include "BKE_pbvh.h"
#include "BKE_scene.h"

#include "paint_intern.h"
#include "sculpt_intern.h"

#include "bmesh.h"

#include <math.h>
#include <stdlib.h>

static void pose_solve_ik_chain(SculptPoseIKChain *ik_chain,
                                const float initial_target[3],
                                const bool use_anchor)
{
  SculptPoseIKChainSegment *segments = ik_chain->segments;
  int tot_segments = ik_chain->tot_segments;

  float target[3];

  /* Set the initial target. */
  copy_v3_v3(target, initial_target);

  /* Solve the positions and rotations of all segments in the chain. */
  for (int i = 0; i < tot_segments; i++) {
    float initial_orientation[3];
    float current_orientation[3];
    float current_head_position[3];
    float current_origin_position[3];

    /* Calculate the rotation to orientate the segment to the target from its initial state. */
    sub_v3_v3v3(current_orientation, target, segments[i].orig);
    normalize_v3(current_orientation);
    sub_v3_v3v3(initial_orientation, segments[i].initial_head, segments[i].initial_orig);
    normalize_v3(initial_orientation);
    rotation_between_vecs_to_quat(segments[i].rot, initial_orientation, current_orientation);

    /* Rotate the segment by calculating a new head position. */
    madd_v3_v3v3fl(current_head_position, segments[i].orig, current_orientation, segments[i].len);

    /* Move the origin of the segment towards the target. */
    sub_v3_v3v3(current_origin_position, target, current_head_position);

    /* Store the new head and origin positions to the segment. */
    copy_v3_v3(segments[i].head, current_head_position);
    add_v3_v3(segments[i].orig, current_origin_position);

    /* Use the origin of this segment as target for the next segment in the chain. */
    copy_v3_v3(target, segments[i].orig);
  }

  /* Move back the whole chain to preserve the anchor point. */
  if (use_anchor) {
    float anchor_diff[3];
    sub_v3_v3v3(
        anchor_diff, segments[tot_segments - 1].initial_orig, segments[tot_segments - 1].orig);

    for (int i = 0; i < tot_segments; i++) {
      add_v3_v3(segments[i].orig, anchor_diff);
      add_v3_v3(segments[i].head, anchor_diff);
    }
  }
}

static void pose_solve_roll_chain(SculptPoseIKChain *ik_chain,
                                  const Brush *brush,
                                  const float roll)
{
  SculptPoseIKChainSegment *segments = ik_chain->segments;
  int tot_segments = ik_chain->tot_segments;

  for (int i = 0; i < tot_segments; i++) {
    float initial_orientation[3];
    float initial_rotation[4];
    float current_rotation[4];

    sub_v3_v3v3(initial_orientation, segments[i].initial_head, segments[i].initial_orig);
    normalize_v3(initial_orientation);

    /* Calculate the current roll angle using the brush curve. */
    float current_roll = roll * BKE_brush_curve_strength(brush, i, tot_segments);

    axis_angle_normalized_to_quat(initial_rotation, initial_orientation, 0.0f);
    axis_angle_normalized_to_quat(current_rotation, initial_orientation, current_roll);

    /* Store the difference of the rotations in the segment rotation. */
    rotation_between_quats_to_quat(segments[i].rot, current_rotation, initial_rotation);
  }
}

static void do_pose_brush_task_cb_ex(void *__restrict userdata,
                                     const int n,
                                     const TaskParallelTLS *__restrict UNUSED(tls))
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  SculptPoseIKChain *ik_chain = ss->cache->pose_ik_chain;
  SculptPoseIKChainSegment *segments = ik_chain->segments;

  PBVHVertexIter vd;
  float disp[3], new_co[3];
  float final_pos[3];

  SculptOrigVertData orig_data;
  SCULPT_orig_vert_data_init(&orig_data, data->ob, data->nodes[n]);

  BKE_pbvh_vertex_iter_begin(ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE)
  {
    SCULPT_orig_vert_data_update(&orig_data, &vd);

    float total_disp[3];
    zero_v3(total_disp);

    ePaintSymmetryAreas symm_area = SCULPT_get_vertex_symm_area(orig_data.co);

    /* Calculate the displacement of each vertex for all the segments in the chain. */
    for (int ik = 0; ik < ik_chain->tot_segments; ik++) {
      copy_v3_v3(new_co, orig_data.co);

      /* Get the transform matrix for the vertex symmetry area to calculate a displacement in the
       * vertex. */
      mul_m4_v3(segments[ik].pivot_mat_inv[(int)symm_area], new_co);
      mul_m4_v3(segments[ik].trans_mat[(int)symm_area], new_co);
      mul_m4_v3(segments[ik].pivot_mat[(int)symm_area], new_co);

      /* Apply the segment weight of the vertex to the displacement. */
      sub_v3_v3v3(disp, new_co, orig_data.co);
      mul_v3_fl(disp, segments[ik].weights[vd.index]);

      /* Apply the vertex mask to the displacement. */
      float mask = vd.mask ? *vd.mask : 0.0f;
      mul_v3_fl(disp, 1.0f - mask);

      /* Accumulate the displacement. */
      add_v3_v3(total_disp, disp);
    }

    /* Apply the accumulated displacement to the vertex. */
    add_v3_v3v3(final_pos, orig_data.co, total_disp);
    copy_v3_v3(vd.co, final_pos);

    if (vd.mvert) {
      vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
    }
  }
  BKE_pbvh_vertex_iter_end;
}

typedef struct PoseGrowFactorTLSData {
  float pos_avg[3];
  int pos_count;
} PoseGrowFactorTLSData;

static void pose_brush_grow_factor_task_cb_ex(void *__restrict userdata,
                                              const int n,
                                              const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = userdata;
  PoseGrowFactorTLSData *gftd = tls->userdata_chunk;
  SculptSession *ss = data->ob->sculpt;
  const char symm = data->sd->paint.symmetry_flags & PAINT_SYMM_AXIS_ALL;
  PBVHVertexIter vd;
  BKE_pbvh_vertex_iter_begin(ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE)
  {
    SculptVertexNeighborIter ni;
    float max = 0.0f;

    /* Grow the factor. */
    sculpt_vertex_neighbors_iter_begin(ss, vd.index, ni)
    {
      float vmask_f = data->prev_mask[ni.index];
      max = MAX2(vmask_f, max);
    }
    sculpt_vertex_neighbors_iter_end(ni);

    /* Keep the count of the vertices that where added to the factors in this grow iteration. */
    if (max > data->prev_mask[vd.index]) {
      data->pose_factor[vd.index] = max;
      if (SCULPT_check_vertex_pivot_symmetry(vd.co, data->pose_initial_co, symm)) {
        add_v3_v3(gftd->pos_avg, vd.co);
        gftd->pos_count++;
      }
    }
  }

  BKE_pbvh_vertex_iter_end;
}

static void pose_brush_grow_factor_reduce(const void *__restrict UNUSED(userdata),
                                          void *__restrict chunk_join,
                                          void *__restrict chunk)
{
  PoseGrowFactorTLSData *join = chunk_join;
  PoseGrowFactorTLSData *gftd = chunk;
  add_v3_v3(join->pos_avg, gftd->pos_avg);
  join->pos_count += gftd->pos_count;
}

/* Grow the factor until its boundary is near to the offset pose origin or outside the target
 * distance. */
static void sculpt_pose_grow_pose_factor(Sculpt *sd,
                                         Object *ob,
                                         SculptSession *ss,
                                         float pose_origin[3],
                                         float pose_target[3],
                                         float max_len,
                                         float *r_pose_origin,
                                         float *pose_factor)
{
  PBVHNode **nodes;
  PBVH *pbvh = ob->sculpt->pbvh;
  int totnode;

  BKE_pbvh_search_gather(pbvh, NULL, NULL, &nodes, &totnode);
  SculptThreadedTaskData data = {
      .sd = sd,
      .ob = ob,
      .nodes = nodes,
      .totnode = totnode,
      .pose_factor = pose_factor,
  };

  data.pose_initial_co = pose_target;
  PBVHParallelSettings settings;
  PoseGrowFactorTLSData gftd;
  gftd.pos_count = 0;
  zero_v3(gftd.pos_avg);
  BKE_pbvh_parallel_range_settings(&settings, (sd->flags & SCULPT_USE_OPENMP), totnode);
  settings.func_reduce = pose_brush_grow_factor_reduce;
  settings.userdata_chunk = &gftd;
  settings.userdata_chunk_size = sizeof(PoseGrowFactorTLSData);

  bool grow_next_iteration = true;
  float prev_len = FLT_MAX;
  data.prev_mask = MEM_mallocN(SCULPT_vertex_count_get(ss) * sizeof(float), "prev mask");
  while (grow_next_iteration) {
    zero_v3(gftd.pos_avg);
    gftd.pos_count = 0;
    memcpy(data.prev_mask, pose_factor, SCULPT_vertex_count_get(ss) * sizeof(float));
    BKE_pbvh_parallel_range(0, totnode, &data, pose_brush_grow_factor_task_cb_ex, &settings);

    if (gftd.pos_count != 0) {
      mul_v3_fl(gftd.pos_avg, 1.0f / (float)gftd.pos_count);
      if (pose_origin) {
        /* Test with pose origin. Used when growing the factors to compensate the Origin Offset. */
        /* Stop when the factor's avg_pos starts moving away from the origin instead of getting
         * closer to it. */
        float len = len_v3v3(gftd.pos_avg, pose_origin);
        if (len < prev_len) {
          prev_len = len;
          grow_next_iteration = true;
        }
        else {
          grow_next_iteration = false;
          memcpy(pose_factor, data.prev_mask, SCULPT_vertex_count_get(ss) * sizeof(float));
        }
      }
      else {
        /* Test with length. Used to calculate the origin positions of the IK chain. */
        /* Stops when the factors have grown enough to generate a new segment origin. */
        float len = len_v3v3(gftd.pos_avg, pose_target);
        if (len < max_len) {
          prev_len = len;
          grow_next_iteration = true;
        }
        else {
          grow_next_iteration = false;
          if (r_pose_origin) {
            copy_v3_v3(r_pose_origin, gftd.pos_avg);
          }
          memcpy(pose_factor, data.prev_mask, SCULPT_vertex_count_get(ss) * sizeof(float));
        }
      }
    }
    else {
      if (r_pose_origin) {
        copy_v3_v3(r_pose_origin, pose_target);
      }
      grow_next_iteration = false;
    }
  }
  MEM_freeN(data.prev_mask);

  MEM_SAFE_FREE(nodes);
}

static bool sculpt_pose_brush_is_vertex_inside_brush_radius(const float vertex[3],
                                                            const float br_co[3],
                                                            float radius,
                                                            char symm)
{
  for (char i = 0; i <= symm; ++i) {
    if (SCULPT_is_symmetry_iteration_valid(i, symm)) {
      float location[3];
      flip_v3_v3(location, br_co, (char)i);
      if (len_v3v3(location, vertex) < radius) {
        return true;
      }
    }
  }
  return false;
}

typedef struct PoseFloodFillData {
  float pose_initial_co[3];
  float radius;
  int symm;

  float *pose_factor;
  float pose_origin[3];
  int tot_co;
} PoseFloodFillData;

static bool pose_floodfill_cb(
    SculptSession *ss, int UNUSED(from_v), int to_v, bool is_duplicate, void *userdata)
{
  PoseFloodFillData *data = userdata;

  if (data->pose_factor) {
    data->pose_factor[to_v] = 1.0f;
  }

  const float *co = SCULPT_vertex_co_get(ss, to_v);
  if (sculpt_pose_brush_is_vertex_inside_brush_radius(
          co, data->pose_initial_co, data->radius, data->symm)) {
    return true;
  }
  else if (SCULPT_check_vertex_pivot_symmetry(co, data->pose_initial_co, data->symm)) {
    if (!is_duplicate) {
      add_v3_v3(data->pose_origin, co);
      data->tot_co++;
    }
  }

  return false;
}

/* Public functions. */

/* Calculate the pose origin and (Optionaly the pose factor) that is used when using the pose brush
 *
 * r_pose_origin must be a valid pointer. the r_pose_factor is optional. When set to NULL it won't
 * be calculated. */
void SCULPT_pose_calc_pose_data(Sculpt *sd,
                                Object *ob,
                                SculptSession *ss,
                                float initial_location[3],
                                float radius,
                                float pose_offset,
                                float *r_pose_origin,
                                float *r_pose_factor)
{
  SCULPT_vertex_random_access_init(ss);

  /* Calculate the pose rotation point based on the boundaries of the brush factor. */
  SculptFloodFill flood;
  SCULPT_floodfill_init(ss, &flood);
  SCULPT_floodfill_add_active(sd, ob, ss, &flood, (r_pose_factor) ? radius : 0.0f);

  PoseFloodFillData fdata = {
      .radius = radius,
      .symm = sd->paint.symmetry_flags & PAINT_SYMM_AXIS_ALL,
      .pose_factor = r_pose_factor,
      .tot_co = 0,
  };
  zero_v3(fdata.pose_origin);
  copy_v3_v3(fdata.pose_initial_co, initial_location);
  SCULPT_floodfill_execute(ss, &flood, pose_floodfill_cb, &fdata);
  SCULPT_floodfill_free(&flood);

  if (fdata.tot_co > 0) {
    mul_v3_fl(fdata.pose_origin, 1.0f / (float)fdata.tot_co);
  }

  /* Offset the pose origin. */
  float pose_d[3];
  sub_v3_v3v3(pose_d, fdata.pose_origin, fdata.pose_initial_co);
  normalize_v3(pose_d);
  madd_v3_v3fl(fdata.pose_origin, pose_d, radius * pose_offset);
  copy_v3_v3(r_pose_origin, fdata.pose_origin);

  /* Do the initial grow of the factors to get the first segment of the chain with Origin Offset.
   */
  if (pose_offset != 0.0f && r_pose_factor) {
    sculpt_pose_grow_pose_factor(
        sd, ob, ss, fdata.pose_origin, fdata.pose_origin, 0, NULL, r_pose_factor);
  }
}

static void pose_brush_init_task_cb_ex(void *__restrict userdata,
                                       const int n,
                                       const TaskParallelTLS *__restrict UNUSED(tls))
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  PBVHVertexIter vd;
  BKE_pbvh_vertex_iter_begin(ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE)
  {
    SculptVertexNeighborIter ni;
    float avg = 0.0f;
    int total = 0;
    sculpt_vertex_neighbors_iter_begin(ss, vd.index, ni)
    {
      avg += data->pose_factor[ni.index];
      total++;
    }
    sculpt_vertex_neighbors_iter_end(ni);

    if (total > 0) {
      data->pose_factor[vd.index] = avg / total;
    }
  }
  BKE_pbvh_vertex_iter_end;
}

SculptPoseIKChain *SCULPT_pose_ik_chain_init(Sculpt *sd,
                                             Object *ob,
                                             SculptSession *ss,
                                             Brush *br,
                                             const float initial_location[3],
                                             const float radius)
{

  const float chain_segment_len = radius * (1.0f + br->pose_offset);
  float next_chain_segment_target[3];

  int totvert = SCULPT_vertex_count_get(ss);
  int nearest_vertex_index = SCULPT_nearest_vertex_get(sd, ob, initial_location, FLT_MAX, true);

  /* Init the buffers used to keep track of the changes in the pose factors as more segments are
   * added to the IK chain. */

  /* This stores the whole pose factors values as they grow through the mesh. */
  float *pose_factor_grow = MEM_callocN(totvert * sizeof(float), "Pose Factor Grow");

  /* This stores the previous status of the factors when growing a new iteration. */
  float *pose_factor_grow_prev = MEM_callocN(totvert * sizeof(float),
                                             "Pose Factor Grow Prev Iteration");

  pose_factor_grow[nearest_vertex_index] = 1.0f;

  /* Init the IK chain with empty weights. */
  SculptPoseIKChain *ik_chain = MEM_callocN(sizeof(SculptPoseIKChain), "Pose IK Chain");
  ik_chain->tot_segments = br->pose_ik_segments;
  ik_chain->segments = MEM_callocN(ik_chain->tot_segments * sizeof(SculptPoseIKChainSegment),
                                   "Pose IK Chain Segments");
  for (int i = 0; i < br->pose_ik_segments; i++) {
    ik_chain->segments[i].weights = MEM_callocN(totvert * sizeof(float), "Pose IK weights");
  }

  /* Calculate the first segment in the chain using the brush radius and the pose origin offset. */
  copy_v3_v3(next_chain_segment_target, initial_location);
  SCULPT_pose_calc_pose_data(sd,
                             ob,
                             ss,
                             next_chain_segment_target,
                             radius,
                             br->pose_offset,
                             ik_chain->segments[0].orig,
                             pose_factor_grow);

  copy_v3_v3(next_chain_segment_target, ik_chain->segments[0].orig);

  /* Init the weights of this segment and store the status of the pose factors to start calculating
   * new segment origins. */
  for (int j = 0; j < totvert; j++) {
    ik_chain->segments[0].weights[j] = pose_factor_grow[j];
    pose_factor_grow_prev[j] = pose_factor_grow[j];
  }

  /* Calculate the next segments in the chain growing the pose factors. */
  for (int i = 1; i < ik_chain->tot_segments; i++) {

    /* Grow the factors to get the new segment origin. */
    sculpt_pose_grow_pose_factor(sd,
                                 ob,
                                 ss,
                                 NULL,
                                 next_chain_segment_target,
                                 chain_segment_len,
                                 ik_chain->segments[i].orig,
                                 pose_factor_grow);
    copy_v3_v3(next_chain_segment_target, ik_chain->segments[i].orig);

    /* Create the weights for this segment from the difference between the previous grow factor
     * iteration an the current iteration. */
    for (int j = 0; j < totvert; j++) {
      ik_chain->segments[i].weights[j] = pose_factor_grow[j] - pose_factor_grow_prev[j];
      /* Store the current grow factor status for the next interation. */
      pose_factor_grow_prev[j] = pose_factor_grow[j];
    }
  }

  /* Init the origin/head pairs of all the segments from the calculated origins. */
  float origin[3];
  float head[3];
  for (int i = 0; i < ik_chain->tot_segments; i++) {
    if (i == 0) {
      copy_v3_v3(head, initial_location);
      copy_v3_v3(origin, ik_chain->segments[i].orig);
    }
    else {
      copy_v3_v3(head, ik_chain->segments[i - 1].orig);
      copy_v3_v3(origin, ik_chain->segments[i].orig);
    }
    copy_v3_v3(ik_chain->segments[i].orig, origin);
    copy_v3_v3(ik_chain->segments[i].initial_orig, origin);
    copy_v3_v3(ik_chain->segments[i].initial_head, head);
    ik_chain->segments[i].len = len_v3v3(head, origin);
  }

  MEM_freeN(pose_factor_grow);
  MEM_freeN(pose_factor_grow_prev);

  return ik_chain;
}

void SCULPT_pose_brush_init(Sculpt *sd, Object *ob, SculptSession *ss, Brush *br)
{
  PBVHNode **nodes;
  PBVH *pbvh = ob->sculpt->pbvh;
  int totnode;

  BKE_pbvh_search_gather(pbvh, NULL, NULL, &nodes, &totnode);

  SculptThreadedTaskData data = {
      .sd = sd,
      .ob = ob,
      .brush = br,
      .nodes = nodes,
  };

  /* Init the IK chain that is going to be used to deform the vertices. */
  ss->cache->pose_ik_chain = SCULPT_pose_ik_chain_init(
      sd, ob, ss, br, ss->cache->true_location, ss->cache->radius);

  /* Smooth the weights of each segment for cleaner deformation. */
  for (int ik = 0; ik < ss->cache->pose_ik_chain->tot_segments; ik++) {
    data.pose_factor = ss->cache->pose_ik_chain->segments[ik].weights;
    for (int i = 0; i < br->pose_smooth_iterations; i++) {
      PBVHParallelSettings settings;
      BKE_pbvh_parallel_range_settings(&settings, (sd->flags & SCULPT_USE_OPENMP), totnode);
      BKE_pbvh_parallel_range(0, totnode, &data, pose_brush_init_task_cb_ex, &settings);
    }
  }

  MEM_SAFE_FREE(nodes);
}

/* Main Brush Function. */
void SCULPT_do_pose_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);
  float grab_delta[3];
  float ik_target[3];
  const ePaintSymmetryFlags symm = sd->paint.symmetry_flags & PAINT_SYMM_AXIS_ALL;

  /* The pose brush applies all enabled symmetry axis in a single iteration, so the rest can be
   * ignored. */
  if (ss->cache->mirror_symmetry_pass != 0) {
    return;
  }

  SculptPoseIKChain *ik_chain = ss->cache->pose_ik_chain;

  /* Solve the positions and rotations of the IK chain. */
  if (ss->cache->invert) {
    /* Roll Mode. */
    /* Calculate the maximum roll. 0.02 radians per pixel works fine. */
    float roll = (ss->cache->initial_mouse[0] - ss->cache->mouse[0]) * ss->cache->bstrength *
                 0.02f;
    BKE_curvemapping_initialize(brush->curve);
    pose_solve_roll_chain(ik_chain, brush, roll);
  }
  else {
    /* IK follow target mode. */
    /* Calculate the IK target. */

    copy_v3_v3(grab_delta, ss->cache->grab_delta);
    copy_v3_v3(ik_target, ss->cache->true_location);
    add_v3_v3(ik_target, ss->cache->grab_delta);

    /* Solve the IK positions. */
    pose_solve_ik_chain(ik_chain, ik_target, brush->flag2 & BRUSH_POSE_IK_ANCHORED);
  }

  /* Flip the segment chain in all symmetry axis and calculate the transform matrices for each
   * possible combination. */
  /* This can be optimized by skipping the calculation of matrices where the symmetry is not
   * enabled. */
  for (int symm_it = 0; symm_it < PAINT_SYMM_AREAS; symm_it++) {
    for (int i = 0; i < brush->pose_ik_segments; i++) {
      float symm_rot[4];
      float symm_orig[3];
      float symm_initial_orig[3];

      ePaintSymmetryAreas symm_area = symm_it;

      copy_qt_qt(symm_rot, ik_chain->segments[i].rot);
      copy_v3_v3(symm_orig, ik_chain->segments[i].orig);
      copy_v3_v3(symm_initial_orig, ik_chain->segments[i].initial_orig);

      /* Flip the origins and rotation quats of each segment. */
      SCULPT_flip_quat_by_symm_area(symm_rot, symm, symm_area, ss->cache->orig_grab_location);
      SCULPT_flip_v3_by_symm_area(symm_orig, symm, symm_area, ss->cache->orig_grab_location);
      SCULPT_flip_v3_by_symm_area(
          symm_initial_orig, symm, symm_area, ss->cache->orig_grab_location);

      /* Create the transform matrix and store it in the segment. */
      unit_m4(ik_chain->segments[i].pivot_mat[symm_it]);
      quat_to_mat4(ik_chain->segments[i].trans_mat[symm_it], symm_rot);

      translate_m4(ik_chain->segments[i].trans_mat[symm_it],
                   symm_orig[0] - symm_initial_orig[0],
                   symm_orig[1] - symm_initial_orig[1],
                   symm_orig[2] - symm_initial_orig[2]);
      translate_m4(
          ik_chain->segments[i].pivot_mat[symm_it], symm_orig[0], symm_orig[1], symm_orig[2]);
      invert_m4_m4(ik_chain->segments[i].pivot_mat_inv[symm_it],
                   ik_chain->segments[i].pivot_mat[symm_it]);
    }
  }

  SculptThreadedTaskData data = {
      .sd = sd,
      .ob = ob,
      .brush = brush,
      .nodes = nodes,
      .grab_delta = grab_delta,
  };

  PBVHParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, (sd->flags & SCULPT_USE_OPENMP), totnode);
  BKE_pbvh_parallel_range(0, totnode, &data, do_pose_brush_task_cb_ex, &settings);
}

void SCULPT_pose_ik_chain_free(SculptPoseIKChain *ik_chain)
{
  for (int i = 0; i < ik_chain->tot_segments; i++) {
    MEM_SAFE_FREE(ik_chain->segments[i].weights);
  }
  MEM_SAFE_FREE(ik_chain->segments);
  MEM_SAFE_FREE(ik_chain);
}
