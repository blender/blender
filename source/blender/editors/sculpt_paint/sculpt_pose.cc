/* SPDX-FileCopyrightText: 2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edsculpt
 */

#include "MEM_guardedalloc.h"

#include "BLI_math_geom.h"
#include "BLI_math_matrix.h"
#include "BLI_math_rotation.h"
#include "BLI_math_vector.h"
#include "BLI_task.h"

#include "DNA_brush_types.h"
#include "DNA_object_types.h"

#include "BKE_brush.hh"
#include "BKE_ccg.h"
#include "BKE_colortools.hh"
#include "BKE_paint.hh"
#include "BKE_pbvh_api.hh"

#include "paint_intern.hh"
#include "sculpt_intern.hh"

#include "bmesh.hh"

#include <cmath>
#include <cstdlib>

namespace blender::ed::sculpt_paint::pose {

static void pose_solve_ik_chain(SculptPoseIKChain &ik_chain,
                                const float initial_target[3],
                                const bool use_anchor)
{
  MutableSpan<SculptPoseIKChainSegment> segments = ik_chain.segments;

  float target[3];

  /* Set the initial target. */
  copy_v3_v3(target, initial_target);

  /* Solve the positions and rotations of all segments in the chain. */
  for (const int i : segments.index_range()) {
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
    sub_v3_v3v3(anchor_diff, segments.last().initial_orig, segments.last().orig);

    for (const int i : segments.index_range()) {
      add_v3_v3(segments[i].orig, anchor_diff);
      add_v3_v3(segments[i].head, anchor_diff);
    }
  }
}

static void pose_solve_roll_chain(SculptPoseIKChain &ik_chain,
                                  const Brush *brush,
                                  const float roll)
{
  MutableSpan<SculptPoseIKChainSegment> segments = ik_chain.segments;

  for (const int i : segments.index_range()) {
    float initial_orientation[3];
    float initial_rotation[4];
    float current_rotation[4];

    sub_v3_v3v3(initial_orientation, segments[i].initial_head, segments[i].initial_orig);
    normalize_v3(initial_orientation);

    /* Calculate the current roll angle using the brush curve. */
    float current_roll = roll * BKE_brush_curve_strength(brush, i, segments.size());

    axis_angle_normalized_to_quat(initial_rotation, initial_orientation, 0.0f);
    axis_angle_normalized_to_quat(current_rotation, initial_orientation, current_roll);

    /* Store the difference of the rotations in the segment rotation. */
    rotation_between_quats_to_quat(segments[i].rot, current_rotation, initial_rotation);
  }
}

static void pose_solve_translate_chain(SculptPoseIKChain &ik_chain, const float delta[3])
{
  for (SculptPoseIKChainSegment &segment : ik_chain.segments) {
    /* Move the origin and head of each segment by delta. */
    add_v3_v3v3(segment.head, segment.initial_head, delta);
    add_v3_v3v3(segment.orig, segment.initial_orig, delta);

    /* Reset the segment rotation. */
    unit_qt(segment.rot);
  }
}

static void pose_solve_scale_chain(SculptPoseIKChain &ik_chain, const float scale[3])
{
  for (SculptPoseIKChainSegment &segment : ik_chain.segments) {
    /* Assign the scale to each segment. */
    copy_v3_v3(segment.scale, scale);
  }
}

static void do_pose_brush_task(Object *ob, const Brush *brush, PBVHNode *node)
{
  SculptSession *ss = ob->sculpt;
  SculptPoseIKChain &ik_chain = *ss->cache->pose_ik_chain;

  PBVHVertexIter vd;
  float disp[3], new_co[3];
  float final_pos[3];

  SculptOrigVertData orig_data;
  SCULPT_orig_vert_data_init(orig_data, *ob, *node, undo::Type::Position);
  auto_mask::NodeData automask_data = auto_mask::node_begin(
      *ob, ss->cache->automasking.get(), *node);

  BKE_pbvh_vertex_iter_begin (*ss->pbvh, node, vd, PBVH_ITER_UNIQUE) {
    SCULPT_orig_vert_data_update(orig_data, vd);
    auto_mask::node_update(automask_data, vd);

    float total_disp[3];
    zero_v3(total_disp);

    ePaintSymmetryAreas symm_area = SCULPT_get_vertex_symm_area(orig_data.co);

    /* Calculate the displacement of each vertex for all the segments in the chain. */
    for (SculptPoseIKChainSegment &segment : ik_chain.segments) {
      copy_v3_v3(new_co, orig_data.co);

      /* Get the transform matrix for the vertex symmetry area to calculate a displacement in the
       * vertex. */
      mul_m4_v3(segment.pivot_mat_inv[int(symm_area)].ptr(), new_co);
      mul_m4_v3(segment.trans_mat[int(symm_area)].ptr(), new_co);
      mul_m4_v3(segment.pivot_mat[int(symm_area)].ptr(), new_co);

      /* Apply the segment weight of the vertex to the displacement. */
      sub_v3_v3v3(disp, new_co, orig_data.co);
      mul_v3_fl(disp, segment.weights[vd.index]);

      /* Apply the vertex mask to the displacement. */
      const float mask = 1.0f - vd.mask;
      const float automask = auto_mask::factor_get(
          ss->cache->automasking.get(), ss, vd.vertex, &automask_data);
      mul_v3_fl(disp, mask * automask);

      /* Accumulate the displacement. */
      add_v3_v3(total_disp, disp);
    }

    /* Apply the accumulated displacement to the vertex. */
    add_v3_v3v3(final_pos, orig_data.co, total_disp);

    float *target_co = SCULPT_brush_deform_target_vertex_co_get(ss, brush->deform_target, &vd);
    copy_v3_v3(target_co, final_pos);
  }
  BKE_pbvh_vertex_iter_end;
}

struct PoseGrowFactorData {
  float pos_avg[3];
  int pos_count;
};

static void pose_brush_grow_factor_task(Object *ob,
                                        const float pose_initial_co[3],
                                        const float *prev_mask,
                                        MutableSpan<float> pose_factor,
                                        PBVHNode *node,
                                        PoseGrowFactorData *gftd)
{
  SculptSession *ss = ob->sculpt;
  const char symm = SCULPT_mesh_symmetry_xyz_get(ob);
  PBVHVertexIter vd;
  BKE_pbvh_vertex_iter_begin (*ss->pbvh, node, vd, PBVH_ITER_UNIQUE) {
    SculptVertexNeighborIter ni;
    float max = 0.0f;

    /* Grow the factor. */
    SCULPT_VERTEX_NEIGHBORS_ITER_BEGIN (ss, vd.vertex, ni) {
      float vmask_f = prev_mask[ni.index];
      max = std::max(vmask_f, max);
    }
    SCULPT_VERTEX_NEIGHBORS_ITER_END(ni);

    /* Keep the count of the vertices that where added to the factors in this grow iteration. */
    if (max > prev_mask[vd.index]) {
      pose_factor[vd.index] = max;
      if (SCULPT_check_vertex_pivot_symmetry(vd.co, pose_initial_co, symm)) {
        add_v3_v3(gftd->pos_avg, vd.co);
        gftd->pos_count++;
      }
    }
  }

  BKE_pbvh_vertex_iter_end;
}

/* Grow the factor until its boundary is near to the offset pose origin or outside the target
 * distance. */
static void sculpt_pose_grow_pose_factor(Object *ob,
                                         SculptSession *ss,
                                         float pose_origin[3],
                                         float pose_target[3],
                                         float max_len,
                                         float *r_pose_origin,
                                         MutableSpan<float> pose_factor)
{
  PBVH &pbvh = *ob->sculpt->pbvh;

  Vector<PBVHNode *> nodes = bke::pbvh::search_gather(pbvh, {});

  PoseGrowFactorData gftd;
  gftd.pos_count = 0;
  zero_v3(gftd.pos_avg);

  bool grow_next_iteration = true;
  float prev_len = FLT_MAX;
  float *prev_mask = static_cast<float *>(
      MEM_malloc_arrayN(SCULPT_vertex_count_get(ss), sizeof(float), __func__));
  while (grow_next_iteration) {
    zero_v3(gftd.pos_avg);
    gftd.pos_count = 0;
    memcpy(prev_mask, pose_factor.data(), SCULPT_vertex_count_get(ss) * sizeof(float));

    gftd = threading::parallel_reduce(
        nodes.index_range(),
        1,
        gftd,
        [&](const IndexRange range, PoseGrowFactorData gftd) {
          for (const int i : range) {
            pose_brush_grow_factor_task(ob, pose_target, prev_mask, pose_factor, nodes[i], &gftd);
          }
          return gftd;
        },
        [](const PoseGrowFactorData &a, const PoseGrowFactorData &b) {
          PoseGrowFactorData joined;
          add_v3_v3v3(joined.pos_avg, a.pos_avg, b.pos_avg);
          joined.pos_count = a.pos_count + b.pos_count;
          return joined;
        });

    if (gftd.pos_count != 0) {
      mul_v3_fl(gftd.pos_avg, 1.0f / float(gftd.pos_count));
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
          memcpy(pose_factor.data(), prev_mask, SCULPT_vertex_count_get(ss) * sizeof(float));
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
          memcpy(pose_factor.data(), prev_mask, SCULPT_vertex_count_get(ss) * sizeof(float));
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
  MEM_freeN(prev_mask);
}

static bool sculpt_pose_brush_is_vertex_inside_brush_radius(const float vertex[3],
                                                            const float br_co[3],
                                                            float radius,
                                                            char symm)
{
  for (char i = 0; i <= symm; ++i) {
    if (SCULPT_is_symmetry_iteration_valid(i, symm)) {
      float location[3];
      flip_v3_v3(location, br_co, ePaintSymmetryFlags(i));
      if (len_v3v3(location, vertex) < radius) {
        return true;
      }
    }
  }
  return false;
}

/**
 * \param fallback_floodfill_origin: In topology mode this stores the furthest point from the
 * stroke origin for cases when a pose origin based on the brush radius can't be set.
 */
static bool pose_topology_floodfill(const SculptSession *ss,
                                    const float3 &pose_initial_co,
                                    const float radius,
                                    const int symm,
                                    const PBVHVertRef to_v,
                                    const bool is_duplicate,
                                    MutableSpan<float> pose_factor,
                                    float3 &fallback_floodfill_origin,
                                    float3 &pose_origin,
                                    int &tot_co)
{
  int to_v_i = BKE_pbvh_vertex_to_index(*ss->pbvh, to_v);

  const float *co = SCULPT_vertex_co_get(ss, to_v);

  if (!pose_factor.is_empty()) {
    pose_factor[to_v_i] = 1.0f;
  }

  if (len_squared_v3v3(pose_initial_co, fallback_floodfill_origin) <
      len_squared_v3v3(pose_initial_co, co))
  {
    copy_v3_v3(fallback_floodfill_origin, co);
  }

  if (sculpt_pose_brush_is_vertex_inside_brush_radius(co, pose_initial_co, radius, symm)) {
    return true;
  }
  if (SCULPT_check_vertex_pivot_symmetry(co, pose_initial_co, symm)) {
    if (!is_duplicate) {
      add_v3_v3(pose_origin, co);
      tot_co++;
    }
  }

  return false;
}

/**
 * \param fallback_origin: If we can't find any face set to continue, use the position of all
 * vertices that have the current face set.
 */
static bool pose_face_sets_floodfill(const SculptSession *ss,
                                     const float3 &pose_initial_co,
                                     const float radius,
                                     const int symm,
                                     const bool is_first_iteration,
                                     const PBVHVertRef to_v,
                                     const bool is_duplicate,
                                     MutableSpan<float> pose_factor,
                                     Set<int> &visited_face_sets,
                                     MutableBoundedBitSpan is_weighted,
                                     float3 &fallback_origin,
                                     int &fallback_count,
                                     int &current_face_set,
                                     bool &next_face_set_found,
                                     int &next_face_set,
                                     PBVHVertRef &next_vertex,
                                     float3 &pose_origin,
                                     int &tot_co)
{
  const int index = BKE_pbvh_vertex_to_index(*ss->pbvh, to_v);
  const PBVHVertRef vertex = to_v;
  bool visit_next = false;

  const float *co = SCULPT_vertex_co_get(ss, vertex);
  const bool symmetry_check = SCULPT_check_vertex_pivot_symmetry(co, pose_initial_co, symm) &&
                              !is_duplicate;

  /* First iteration. Continue expanding using topology until a vertex is outside the brush radius
   * to determine the first face set. */
  if (current_face_set == SCULPT_FACE_SET_NONE) {

    pose_factor[index] = 1.0f;
    is_weighted[index].set();

    if (sculpt_pose_brush_is_vertex_inside_brush_radius(co, pose_initial_co, radius, symm)) {
      const int visited_face_set = face_set::vert_face_set_get(ss, vertex);
      visited_face_sets.add(visited_face_set);
    }
    else if (symmetry_check) {
      current_face_set = face_set::vert_face_set_get(ss, vertex);
      visited_face_sets.add(current_face_set);
    }
    return true;
  }

  /* We already have a current face set, so we can start checking the face sets of the vertices. */
  /* In the first iteration we need to check all face sets we already visited as the flood fill may
   * still not be finished in some of them. */
  bool is_vertex_valid = false;
  if (is_first_iteration) {
    for (const int visited_face_set : visited_face_sets) {
      is_vertex_valid |= face_set::vert_has_face_set(ss, vertex, visited_face_set);
    }
  }
  else {
    is_vertex_valid = face_set::vert_has_face_set(ss, vertex, current_face_set);
  }

  if (!is_vertex_valid) {
    return visit_next;
  }

  if (!is_weighted[index]) {
    pose_factor[index] = 1.0f;
    is_weighted[index].set();
    visit_next = true;
  }

  /* Fallback origin accumulation. */
  if (symmetry_check) {
    add_v3_v3(fallback_origin, SCULPT_vertex_co_get(ss, vertex));
    fallback_count++;
  }

  if (!symmetry_check || face_set::vert_has_unique_face_set(ss, vertex)) {
    return visit_next;
  }

  /* We only add coordinates for calculating the origin when it is possible to go from this
   * vertex to another vertex in a valid face set for the next iteration. */
  bool count_as_boundary = false;

  SculptVertexNeighborIter ni;
  SCULPT_VERTEX_NEIGHBORS_ITER_BEGIN (ss, vertex, ni) {
    int next_face_set_candidate = face_set::vert_face_set_get(ss, ni.vertex);

    /* Check if we can get a valid face set for the next iteration from this neighbor. */
    if (face_set::vert_has_unique_face_set(ss, ni.vertex) &&
        !visited_face_sets.contains(next_face_set_candidate))
    {
      if (!next_face_set_found) {
        next_face_set = next_face_set_candidate;
        next_vertex = ni.vertex;
        next_face_set_found = true;
      }
      count_as_boundary = true;
    }
  }
  SCULPT_VERTEX_NEIGHBORS_ITER_END(ni);

  /* Origin accumulation. */
  if (count_as_boundary) {
    add_v3_v3(pose_origin, SCULPT_vertex_co_get(ss, vertex));
    tot_co++;
  }
  return visit_next;
}

/* Public functions. */

void calc_pose_data(Object *ob,
                    SculptSession *ss,
                    const float3 &initial_location,
                    float radius,
                    float pose_offset,
                    float3 &r_pose_origin,
                    MutableSpan<float> r_pose_factor)
{
  SCULPT_vertex_random_access_ensure(ss);

  /* Calculate the pose rotation point based on the boundaries of the brush factor. */
  flood_fill::FillData flood = flood_fill::init_fill(ss);
  flood_fill::add_active(ob, ss, &flood, !r_pose_factor.is_empty() ? radius : 0.0f);

  const int symm = SCULPT_mesh_symmetry_xyz_get(ob);

  int tot_co = 0;
  float3 pose_origin(0);
  float3 fallback_floodfill_origin = initial_location;
  flood_fill::execute(
      ss, &flood, [&](PBVHVertRef /*from_v*/, PBVHVertRef to_v, bool is_duplicate) {
        return pose_topology_floodfill(ss,
                                       initial_location,
                                       radius,
                                       symm,
                                       to_v,
                                       is_duplicate,
                                       r_pose_factor,
                                       fallback_floodfill_origin,
                                       pose_origin,
                                       tot_co);
      });

  if (tot_co > 0) {
    mul_v3_fl(pose_origin, 1.0f / float(tot_co));
  }
  else {
    copy_v3_v3(pose_origin, fallback_floodfill_origin);
  }

  /* Offset the pose origin. */
  float pose_d[3];
  sub_v3_v3v3(pose_d, pose_origin, initial_location);
  normalize_v3(pose_d);
  madd_v3_v3fl(pose_origin, pose_d, radius * pose_offset);
  copy_v3_v3(r_pose_origin, pose_origin);

  /* Do the initial grow of the factors to get the first segment of the chain with Origin Offset.
   */
  if (pose_offset != 0.0f && !r_pose_factor.is_empty()) {
    sculpt_pose_grow_pose_factor(ob, ss, pose_origin, pose_origin, 0, nullptr, r_pose_factor);
  }
}

static void pose_brush_init_task(SculptSession *ss, MutableSpan<float> pose_factor, PBVHNode *node)
{
  PBVHVertexIter vd;
  BKE_pbvh_vertex_iter_begin (*ss->pbvh, node, vd, PBVH_ITER_UNIQUE) {
    SculptVertexNeighborIter ni;
    float avg = 0.0f;
    int total = 0;
    SCULPT_VERTEX_NEIGHBORS_ITER_BEGIN (ss, vd.vertex, ni) {
      avg += pose_factor[ni.index];
      total++;
    }
    SCULPT_VERTEX_NEIGHBORS_ITER_END(ni);

    if (total > 0) {
      pose_factor[vd.index] = avg / total;
    }
  }
  BKE_pbvh_vertex_iter_end;
}

/* Init the IK chain with empty weights. */
static std::unique_ptr<SculptPoseIKChain> pose_ik_chain_new(const int totsegments,
                                                            const int totverts)
{
  std::unique_ptr<SculptPoseIKChain> ik_chain = std::make_unique<SculptPoseIKChain>();
  ik_chain->segments.reinitialize(totsegments);
  for (SculptPoseIKChainSegment &segment : ik_chain->segments) {
    segment.weights = Array<float>(totverts, 0.0f);
  }
  return ik_chain;
}

/* Init the origin/head pairs of all the segments from the calculated origins. */
static void pose_ik_chain_origin_heads_init(SculptPoseIKChain &ik_chain,
                                            const float initial_location[3])
{
  float origin[3];
  float head[3];
  for (const int i : ik_chain.segments.index_range()) {
    if (i == 0) {
      copy_v3_v3(head, initial_location);
      copy_v3_v3(origin, ik_chain.segments[i].orig);
    }
    else {
      copy_v3_v3(head, ik_chain.segments[i - 1].orig);
      copy_v3_v3(origin, ik_chain.segments[i].orig);
    }
    copy_v3_v3(ik_chain.segments[i].orig, origin);
    copy_v3_v3(ik_chain.segments[i].initial_orig, origin);
    copy_v3_v3(ik_chain.segments[i].head, head);
    copy_v3_v3(ik_chain.segments[i].initial_head, head);
    ik_chain.segments[i].len = len_v3v3(head, origin);
    copy_v3_fl(ik_chain.segments[i].scale, 1.0f);
  }
}

static int pose_brush_num_effective_segments(const Brush *brush)
{
  /* Scaling multiple segments at the same time is not supported as the IK solver can't handle
   * changes in the segment's length. It will also required a better weight distribution to avoid
   * artifacts in the areas affected by multiple segments. */
  if (ELEM(brush->pose_deform_type,
           BRUSH_POSE_DEFORM_SCALE_TRASLATE,
           BRUSH_POSE_DEFORM_SQUASH_STRETCH))
  {
    return 1;
  }
  return brush->pose_ik_segments;
}

static std::unique_ptr<SculptPoseIKChain> pose_ik_chain_init_topology(
    Object *ob, SculptSession *ss, Brush *br, const float initial_location[3], const float radius)
{

  const float chain_segment_len = radius * (1.0f + br->pose_offset);
  float next_chain_segment_target[3];

  int totvert = SCULPT_vertex_count_get(ss);
  PBVHVertRef nearest_vertex = SCULPT_nearest_vertex_get(ob, initial_location, FLT_MAX, true);
  int nearest_vertex_index = BKE_pbvh_vertex_to_index(*ss->pbvh, nearest_vertex);

  /* Init the buffers used to keep track of the changes in the pose factors as more segments are
   * added to the IK chain. */

  /* This stores the whole pose factors values as they grow through the mesh. */
  Array<float> pose_factor_grow(totvert, 0.0f);

  /* This stores the previous status of the factors when growing a new iteration. */
  Array<float> pose_factor_grow_prev(totvert, 0.0f);

  pose_factor_grow[nearest_vertex_index] = 1.0f;

  const int tot_segments = pose_brush_num_effective_segments(br);
  std::unique_ptr<SculptPoseIKChain> ik_chain = pose_ik_chain_new(tot_segments, totvert);

  /* Calculate the first segment in the chain using the brush radius and the pose origin offset. */
  copy_v3_v3(next_chain_segment_target, initial_location);
  calc_pose_data(ob,
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
  for (const int i : ik_chain->segments.index_range().drop_front(1)) {

    /* Grow the factors to get the new segment origin. */
    sculpt_pose_grow_pose_factor(ob,
                                 ss,
                                 nullptr,
                                 next_chain_segment_target,
                                 chain_segment_len,
                                 ik_chain->segments[i].orig,
                                 pose_factor_grow);
    copy_v3_v3(next_chain_segment_target, ik_chain->segments[i].orig);

    /* Create the weights for this segment from the difference between the previous grow factor
     * iteration an the current iteration. */
    for (int j = 0; j < totvert; j++) {
      ik_chain->segments[i].weights[j] = pose_factor_grow[j] - pose_factor_grow_prev[j];
      /* Store the current grow factor status for the next iteration. */
      pose_factor_grow_prev[j] = pose_factor_grow[j];
    }
  }

  pose_ik_chain_origin_heads_init(*ik_chain, initial_location);

  return ik_chain;
}

static std::unique_ptr<SculptPoseIKChain> pose_ik_chain_init_face_sets(Object *ob,
                                                                       SculptSession *ss,
                                                                       Brush *br,
                                                                       const float radius)
{

  int totvert = SCULPT_vertex_count_get(ss);

  const int tot_segments = pose_brush_num_effective_segments(br);
  const int symm = SCULPT_mesh_symmetry_xyz_get(ob);

  std::unique_ptr<SculptPoseIKChain> ik_chain = pose_ik_chain_new(tot_segments, totvert);

  Set<int> visited_face_sets;

  /* Each vertex can only be assigned to one face set. */
  BitVector<> is_weighted(totvert);

  int current_face_set = SCULPT_FACE_SET_NONE;

  PBVHVertRef current_vertex = SCULPT_active_vertex_get(ss);

  for (const int i : ik_chain->segments.index_range()) {
    const bool is_first_iteration = i == 0;

    flood_fill::FillData flood = flood_fill::init_fill(ss);
    flood_fill::add_initial_with_symmetry(ob, ss, &flood, current_vertex, FLT_MAX);

    visited_face_sets.add(current_face_set);

    MutableSpan<float> pose_factor = ik_chain->segments[i].weights;
    int tot_co = 0;
    bool next_face_set_found = false;
    int next_face_set = SCULPT_FACE_SET_NONE;
    PBVHVertRef next_vertex{};
    float3 pose_origin(0);
    float3 fallback_origin(0);
    int fallback_count = 0;

    const float3 pose_initial_co = SCULPT_vertex_co_get(ss, current_vertex);
    flood_fill::execute(
        ss, &flood, [&](PBVHVertRef /*from_v*/, PBVHVertRef to_v, bool is_duplicate) {
          return pose_face_sets_floodfill(ss,
                                          pose_initial_co,
                                          radius,
                                          symm,
                                          is_first_iteration,
                                          to_v,
                                          is_duplicate,
                                          pose_factor,
                                          visited_face_sets,
                                          is_weighted,
                                          fallback_origin,
                                          fallback_count,
                                          current_face_set,
                                          next_face_set_found,
                                          next_face_set,
                                          next_vertex,
                                          pose_origin,
                                          tot_co);
        });

    if (tot_co > 0) {
      mul_v3_fl(pose_origin, 1.0f / float(tot_co));
      copy_v3_v3(ik_chain->segments[i].orig, pose_origin);
    }
    else if (fallback_count > 0) {
      mul_v3_fl(fallback_origin, 1.0f / float(fallback_count));
      copy_v3_v3(ik_chain->segments[i].orig, fallback_origin);
    }
    else {
      zero_v3(ik_chain->segments[i].orig);
    }

    current_face_set = next_face_set;
    current_vertex = next_vertex;
  }

  pose_ik_chain_origin_heads_init(*ik_chain, SCULPT_active_vertex_co_get(ss));

  return ik_chain;
}

static bool pose_face_sets_fk_find_masked_floodfill(const SculptSession *ss,
                                                    const int initial_face_set,
                                                    const PBVHVertRef from_v,
                                                    const PBVHVertRef to_v,
                                                    const bool is_duplicate,
                                                    Set<int> &visited_face_sets,
                                                    MutableSpan<int> floodfill_it,
                                                    int &masked_face_set_it,
                                                    int &masked_face_set,
                                                    int &target_face_set)
{
  int from_v_i = BKE_pbvh_vertex_to_index(*ss->pbvh, from_v);
  int to_v_i = BKE_pbvh_vertex_to_index(*ss->pbvh, to_v);

  if (!is_duplicate) {
    floodfill_it[to_v_i] = floodfill_it[from_v_i] + 1;
  }
  else {
    floodfill_it[to_v_i] = floodfill_it[from_v_i];
  }

  const int to_face_set = face_set::vert_face_set_get(ss, to_v);
  if (!visited_face_sets.contains(to_face_set)) {
    if (face_set::vert_has_unique_face_set(ss, to_v) &&
        !face_set::vert_has_unique_face_set(ss, from_v) &&
        face_set::vert_has_face_set(ss, from_v, to_face_set))
    {

      visited_face_sets.add(to_face_set);

      if (floodfill_it[to_v_i] >= masked_face_set_it) {
        masked_face_set = to_face_set;
        masked_face_set_it = floodfill_it[to_v_i];
      }

      if (target_face_set == SCULPT_FACE_SET_NONE) {
        target_face_set = to_face_set;
      }
    }
  }

  return face_set::vert_has_face_set(ss, to_v, initial_face_set);
}

static bool pose_face_sets_fk_set_weights_floodfill(const SculptSession *ss,
                                                    const PBVHVertRef to_v,
                                                    const int masked_face_set,
                                                    MutableSpan<float> fk_weights)
{
  int to_v_i = BKE_pbvh_vertex_to_index(*ss->pbvh, to_v);

  fk_weights[to_v_i] = 1.0f;
  return !face_set::vert_has_face_set(ss, to_v, masked_face_set);
}

static std::unique_ptr<SculptPoseIKChain> pose_ik_chain_init_face_sets_fk(
    Object *ob, SculptSession *ss, const float radius, const float *initial_location)
{
  const int totvert = SCULPT_vertex_count_get(ss);

  std::unique_ptr<SculptPoseIKChain> ik_chain = pose_ik_chain_new(1, totvert);

  const PBVHVertRef active_vertex = SCULPT_active_vertex_get(ss);
  int active_vertex_index = BKE_pbvh_vertex_to_index(*ss->pbvh, active_vertex);

  const int active_face_set = face_set::active_face_set_get(ss);

  Set<int> visited_face_sets;
  Array<int> floodfill_it(totvert);
  floodfill_it[active_vertex_index] = 1;

  int masked_face_set = SCULPT_FACE_SET_NONE;
  int target_face_set = SCULPT_FACE_SET_NONE;
  {
    int masked_face_set_it = 0;
    flood_fill::FillData flood = flood_fill::init_fill(ss);
    flood_fill::add_initial(&flood, active_vertex);
    flood_fill::execute(ss, &flood, [&](PBVHVertRef from_v, PBVHVertRef to_v, bool is_duplicate) {
      return pose_face_sets_fk_find_masked_floodfill(ss,
                                                     active_face_set,
                                                     from_v,
                                                     to_v,
                                                     is_duplicate,
                                                     visited_face_sets,
                                                     floodfill_it,
                                                     masked_face_set_it,
                                                     masked_face_set,
                                                     target_face_set);
    });
  }

  int origin_count = 0;
  float origin_acc[3] = {0.0f};
  for (int i = 0; i < totvert; i++) {
    PBVHVertRef vertex = BKE_pbvh_index_to_vertex(*ss->pbvh, i);

    if (floodfill_it[i] != 0 && face_set::vert_has_face_set(ss, vertex, active_face_set) &&
        face_set::vert_has_face_set(ss, vertex, masked_face_set))
    {
      add_v3_v3(origin_acc, SCULPT_vertex_co_get(ss, vertex));
      origin_count++;
    }
  }

  int target_count = 0;
  float target_acc[3] = {0.0f};
  if (target_face_set != masked_face_set) {
    for (int i = 0; i < totvert; i++) {
      PBVHVertRef vertex = BKE_pbvh_index_to_vertex(*ss->pbvh, i);

      if (floodfill_it[i] != 0 && face_set::vert_has_face_set(ss, vertex, active_face_set) &&
          face_set::vert_has_face_set(ss, vertex, target_face_set))
      {
        add_v3_v3(target_acc, SCULPT_vertex_co_get(ss, vertex));
        target_count++;
      }
    }
  }

  if (origin_count > 0) {
    copy_v3_v3(ik_chain->segments[0].orig, origin_acc);
    mul_v3_fl(ik_chain->segments[0].orig, 1.0f / origin_count);
  }
  else {
    zero_v3(ik_chain->segments[0].orig);
  }

  if (target_count > 0) {
    copy_v3_v3(ik_chain->segments[0].head, target_acc);
    mul_v3_fl(ik_chain->segments[0].head, 1.0f / target_count);
    sub_v3_v3v3(ik_chain->grab_delta_offset, ik_chain->segments[0].head, initial_location);
  }
  else {
    copy_v3_v3(ik_chain->segments[0].head, initial_location);
  }

  {
    flood_fill::FillData flood = flood_fill::init_fill(ss);
    flood_fill::add_active(ob, ss, &flood, radius);
    MutableSpan<float> fk_weights = ik_chain->segments[0].weights;
    flood_fill::execute(
        ss, &flood, [&](PBVHVertRef /*from_v*/, PBVHVertRef to_v, bool /*is_duplicate*/) {
          return pose_face_sets_fk_set_weights_floodfill(ss, to_v, masked_face_set, fk_weights);
        });
  }

  pose_ik_chain_origin_heads_init(*ik_chain, ik_chain->segments[0].head);
  return ik_chain;
}

std::unique_ptr<SculptPoseIKChain> ik_chain_init(
    Object *ob, SculptSession *ss, Brush *br, const float3 &initial_location, const float radius)
{
  std::unique_ptr<SculptPoseIKChain> ik_chain;

  const bool use_fake_neighbors = !(br->flag2 & BRUSH_USE_CONNECTED_ONLY);

  if (use_fake_neighbors) {
    SCULPT_fake_neighbors_ensure(ob, br->disconnected_distance_max);
    SCULPT_fake_neighbors_enable(ob);
  }

  switch (br->pose_origin_type) {
    case BRUSH_POSE_ORIGIN_TOPOLOGY:
      ik_chain = pose_ik_chain_init_topology(ob, ss, br, initial_location, radius);
      break;
    case BRUSH_POSE_ORIGIN_FACE_SETS:
      ik_chain = pose_ik_chain_init_face_sets(ob, ss, br, radius);
      break;
    case BRUSH_POSE_ORIGIN_FACE_SETS_FK:
      ik_chain = pose_ik_chain_init_face_sets_fk(ob, ss, radius, initial_location);
      break;
  }

  if (use_fake_neighbors) {
    SCULPT_fake_neighbors_disable(ob);
  }

  return ik_chain;
}

void pose_brush_init(Object *ob, SculptSession *ss, Brush *br)
{
  PBVH &pbvh = *ob->sculpt->pbvh;

  Vector<PBVHNode *> nodes = bke::pbvh::search_gather(pbvh, {});

  /* Init the IK chain that is going to be used to deform the vertices. */
  ss->cache->pose_ik_chain = ik_chain_init(
      ob, ss, br, ss->cache->true_location, ss->cache->radius);

  /* Smooth the weights of each segment for cleaner deformation. */
  for (SculptPoseIKChainSegment &segment : ss->cache->pose_ik_chain->segments) {
    MutableSpan<float> pose_factor = segment.weights;
    for (int i = 0; i < br->pose_smooth_iterations; i++) {
      threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
        for (const int i : range) {
          pose_brush_init_task(ss, pose_factor, nodes[i]);
        }
      });
    }
  }
}

static void sculpt_pose_do_translate_deform(SculptSession *ss, Brush *brush)
{
  SculptPoseIKChain &ik_chain = *ss->cache->pose_ik_chain;
  BKE_curvemapping_init(brush->curve);
  pose_solve_translate_chain(ik_chain, ss->cache->grab_delta);
}

/* Calculate a scale factor based on the grab delta. */
static float sculpt_pose_get_scale_from_grab_delta(SculptSession *ss, const float ik_target[3])
{
  SculptPoseIKChain &ik_chain = *ss->cache->pose_ik_chain;
  float plane[4];
  float segment_dir[3];
  sub_v3_v3v3(segment_dir, ik_chain.segments[0].initial_head, ik_chain.segments[0].initial_orig);
  normalize_v3(segment_dir);
  plane_from_point_normal_v3(plane, ik_chain.segments[0].initial_head, segment_dir);
  const float segment_len = ik_chain.segments[0].len;
  return segment_len / (segment_len - dist_signed_to_plane_v3(ik_target, plane));
}

static void sculpt_pose_do_scale_deform(SculptSession *ss, Brush *brush)
{
  float ik_target[3];
  SculptPoseIKChain &ik_chain = *ss->cache->pose_ik_chain;

  copy_v3_v3(ik_target, ss->cache->true_location);
  add_v3_v3(ik_target, ss->cache->grab_delta);

  /* Solve the IK for the first segment to include rotation as part of scale if enabled. */
  if (!(brush->flag2 & BRUSH_POSE_USE_LOCK_ROTATION)) {
    pose_solve_ik_chain(ik_chain, ik_target, brush->flag2 & BRUSH_POSE_IK_ANCHORED);
  }

  float scale[3];
  copy_v3_fl(scale, sculpt_pose_get_scale_from_grab_delta(ss, ik_target));

  /* Write the scale into the segments. */
  pose_solve_scale_chain(ik_chain, scale);
}

static void sculpt_pose_do_twist_deform(SculptSession *ss, Brush *brush)
{
  SculptPoseIKChain &ik_chain = *ss->cache->pose_ik_chain;

  /* Calculate the maximum roll. 0.02 radians per pixel works fine. */
  float roll = (ss->cache->initial_mouse[0] - ss->cache->mouse[0]) * ss->cache->bstrength * 0.02f;
  BKE_curvemapping_init(brush->curve);
  pose_solve_roll_chain(ik_chain, brush, roll);
}

static void sculpt_pose_do_rotate_deform(SculptSession *ss, Brush *brush)
{
  float ik_target[3];
  SculptPoseIKChain &ik_chain = *ss->cache->pose_ik_chain;

  /* Calculate the IK target. */
  copy_v3_v3(ik_target, ss->cache->true_location);
  add_v3_v3(ik_target, ss->cache->grab_delta);
  add_v3_v3(ik_target, ik_chain.grab_delta_offset);

  /* Solve the IK positions. */
  pose_solve_ik_chain(ik_chain, ik_target, brush->flag2 & BRUSH_POSE_IK_ANCHORED);
}

static void sculpt_pose_do_rotate_twist_deform(SculptSession *ss, Brush *brush)
{
  if (ss->cache->invert) {
    sculpt_pose_do_twist_deform(ss, brush);
  }
  else {
    sculpt_pose_do_rotate_deform(ss, brush);
  }
}

static void sculpt_pose_do_scale_translate_deform(SculptSession *ss, Brush *brush)
{
  if (ss->cache->invert) {
    sculpt_pose_do_translate_deform(ss, brush);
  }
  else {
    sculpt_pose_do_scale_deform(ss, brush);
  }
}

static void sculpt_pose_do_squash_stretch_deform(SculptSession *ss, Brush * /*brush*/)
{
  float ik_target[3];
  SculptPoseIKChain &ik_chain = *ss->cache->pose_ik_chain;

  copy_v3_v3(ik_target, ss->cache->true_location);
  add_v3_v3(ik_target, ss->cache->grab_delta);

  float scale[3];
  scale[2] = sculpt_pose_get_scale_from_grab_delta(ss, ik_target);
  scale[0] = scale[1] = sqrtf(1.0f / scale[2]);

  /* Write the scale into the segments. */
  pose_solve_scale_chain(ik_chain, scale);
}

static void sculpt_pose_align_pivot_local_space(float r_mat[4][4],
                                                ePaintSymmetryFlags symm,
                                                ePaintSymmetryAreas symm_area,
                                                SculptPoseIKChainSegment *segment,
                                                const float grab_location[3])
{
  float segment_origin_head[3];
  float symm_head[3];
  float symm_orig[3];

  copy_v3_v3(symm_head, segment->head);
  copy_v3_v3(symm_orig, segment->orig);

  SCULPT_flip_v3_by_symm_area(symm_head, symm, symm_area, grab_location);
  SCULPT_flip_v3_by_symm_area(symm_orig, symm, symm_area, grab_location);

  sub_v3_v3v3(segment_origin_head, symm_head, symm_orig);
  normalize_v3(segment_origin_head);

  copy_v3_v3(r_mat[2], segment_origin_head);
  ortho_basis_v3v3_v3(r_mat[0], r_mat[1], r_mat[2]);
}

void do_pose_brush(Sculpt *sd, Object *ob, Span<PBVHNode *> nodes)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);
  const ePaintSymmetryFlags symm = SCULPT_mesh_symmetry_xyz_get(ob);

  /* The pose brush applies all enabled symmetry axis in a single iteration, so the rest can be
   * ignored. */
  if (ss->cache->mirror_symmetry_pass != 0) {
    return;
  }

  SculptPoseIKChain &ik_chain = *ss->cache->pose_ik_chain;

  switch (brush->pose_deform_type) {
    case BRUSH_POSE_DEFORM_ROTATE_TWIST:
      sculpt_pose_do_rotate_twist_deform(ss, brush);
      break;
    case BRUSH_POSE_DEFORM_SCALE_TRASLATE:
      sculpt_pose_do_scale_translate_deform(ss, brush);
      break;
    case BRUSH_POSE_DEFORM_SQUASH_STRETCH:
      sculpt_pose_do_squash_stretch_deform(ss, brush);
      break;
  }

  /* Flip the segment chain in all symmetry axis and calculate the transform matrices for each
   * possible combination. */
  /* This can be optimized by skipping the calculation of matrices where the symmetry is not
   * enabled. */
  for (int symm_it = 0; symm_it < PAINT_SYMM_AREAS; symm_it++) {
    for (const int i : ik_chain.segments.index_range()) {
      float symm_rot[4];
      float symm_orig[3];
      float symm_initial_orig[3];

      ePaintSymmetryAreas symm_area = ePaintSymmetryAreas(symm_it);

      copy_qt_qt(symm_rot, ik_chain.segments[i].rot);
      copy_v3_v3(symm_orig, ik_chain.segments[i].orig);
      copy_v3_v3(symm_initial_orig, ik_chain.segments[i].initial_orig);

      /* Flip the origins and rotation quats of each segment. */
      SCULPT_flip_quat_by_symm_area(symm_rot, symm, symm_area, ss->cache->orig_grab_location);
      SCULPT_flip_v3_by_symm_area(symm_orig, symm, symm_area, ss->cache->orig_grab_location);
      SCULPT_flip_v3_by_symm_area(
          symm_initial_orig, symm, symm_area, ss->cache->orig_grab_location);

      float pivot_local_space[4][4];
      unit_m4(pivot_local_space);

      /* Align the segment pivot local space to the Z axis. */
      if (brush->pose_deform_type == BRUSH_POSE_DEFORM_SQUASH_STRETCH) {
        sculpt_pose_align_pivot_local_space(pivot_local_space,
                                            symm,
                                            symm_area,
                                            &ik_chain.segments[i],
                                            ss->cache->orig_grab_location);
        unit_m4(ik_chain.segments[i].trans_mat[symm_it].ptr());
      }
      else {
        quat_to_mat4(ik_chain.segments[i].trans_mat[symm_it].ptr(), symm_rot);
      }

      /* Apply segment scale to the transform. */
      for (int scale_i = 0; scale_i < 3; scale_i++) {
        mul_v3_fl(ik_chain.segments[i].trans_mat[symm_it][scale_i],
                  ik_chain.segments[i].scale[scale_i]);
      }

      translate_m4(ik_chain.segments[i].trans_mat[symm_it].ptr(),
                   symm_orig[0] - symm_initial_orig[0],
                   symm_orig[1] - symm_initial_orig[1],
                   symm_orig[2] - symm_initial_orig[2]);

      unit_m4(ik_chain.segments[i].pivot_mat[symm_it].ptr());
      translate_m4(
          ik_chain.segments[i].pivot_mat[symm_it].ptr(), symm_orig[0], symm_orig[1], symm_orig[2]);
      mul_m4_m4_post(ik_chain.segments[i].pivot_mat[symm_it].ptr(), pivot_local_space);

      invert_m4_m4(ik_chain.segments[i].pivot_mat_inv[symm_it].ptr(),
                   ik_chain.segments[i].pivot_mat[symm_it].ptr());
    }
  }

  threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
    for (const int i : range) {
      do_pose_brush_task(ob, brush, nodes[i]);
    }
  });
}

}  // namespace blender::ed::sculpt_paint::pose
