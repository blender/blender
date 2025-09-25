/* SPDX-FileCopyrightText: 2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edsculpt
 */

#include "BLI_enumerable_thread_specific.hh"
#include "BLI_math_geom.h"
#include "BLI_math_matrix.h"
#include "BLI_math_matrix.hh"
#include "BLI_math_rotation.h"
#include "BLI_math_vector.h"
#include "BLI_math_vector.hh"

#include "DNA_brush_types.h"
#include "DNA_object_types.h"

#include "BKE_brush.hh"
#include "BKE_ccg.hh"
#include "BKE_colortools.hh"
#include "BKE_mesh.hh"
#include "BKE_paint.hh"
#include "BKE_paint_bvh.hh"

#include "mesh_brush_common.hh"
#include "paint_intern.hh"
#include "sculpt_automask.hh"
#include "sculpt_cloth.hh"
#include "sculpt_face_set.hh"
#include "sculpt_flood_fill.hh"
#include "sculpt_hide.hh"
#include "sculpt_intern.hh"
#include "sculpt_pose.hh"
#include "sculpt_smooth.hh"

#include "bmesh.hh"

#include <cmath>

namespace blender::ed::sculpt_paint::pose {

static void solve_ik_chain(IKChain &ik_chain, const float3 &initial_target, const bool use_anchor)
{
  MutableSpan<IKChainSegment> segments = ik_chain.segments;

  /* Set the initial target. */
  float3 target = initial_target;

  /* Solve the positions and rotations of all segments in the chain. */
  for (const int i : segments.index_range()) {
    /* Calculate the rotation to orientate the segment to the target from its initial state. */
    float3 current_orientation = math::normalize(target - segments[i].orig);
    float3 initial_orientation = math::normalize(segments[i].initial_head -
                                                 segments[i].initial_orig);
    rotation_between_vecs_to_quat(segments[i].rot, initial_orientation, current_orientation);

    /* Rotate the segment by calculating a new head position. */
    float3 current_head_position = segments[i].orig + current_orientation * segments[i].len;

    /* Move the origin of the segment towards the target. */
    float3 current_origin_position = target - current_head_position;

    /* Store the new head and origin positions to the segment. */
    segments[i].head = current_head_position;
    segments[i].orig += current_origin_position;

    /* Use the origin of this segment as target for the next segment in the chain. */
    target = segments[i].orig;
  }

  /* Move back the whole chain to preserve the anchor point. */
  if (use_anchor) {
    float3 anchor_diff = segments.last().initial_orig - segments.last().orig;
    for (const int i : segments.index_range()) {
      segments[i].orig += anchor_diff;
      segments[i].head += anchor_diff;
    }
  }
}

static void solve_roll_chain(IKChain &ik_chain, const Brush &brush, const float roll)
{
  MutableSpan<IKChainSegment> segments = ik_chain.segments;

  for (const int i : segments.index_range()) {
    float3 initial_orientation = math::normalize(segments[i].initial_head -
                                                 segments[i].initial_orig);
    float initial_rotation[4];
    float current_rotation[4];

    /* Calculate the current roll angle using the brush curve. */
    float current_roll = roll * BKE_brush_curve_strength(&brush, i, segments.size());

    axis_angle_normalized_to_quat(initial_rotation, initial_orientation, 0.0f);
    axis_angle_normalized_to_quat(current_rotation, initial_orientation, current_roll);

    /* Store the difference of the rotations in the segment rotation. */
    rotation_between_quats_to_quat(segments[i].rot, current_rotation, initial_rotation);
  }
}

static void solve_translate_chain(IKChain &ik_chain, const float delta[3])
{
  for (IKChainSegment &segment : ik_chain.segments) {
    /* Move the origin and head of each segment by delta. */
    add_v3_v3v3(segment.head, segment.initial_head, delta);
    add_v3_v3v3(segment.orig, segment.initial_orig, delta);

    /* Reset the segment rotation. */
    unit_qt(segment.rot);
  }
}

static void solve_scale_chain(IKChain &ik_chain, const float scale[3])
{
  for (IKChainSegment &segment : ik_chain.segments) {
    /* Assign the scale to each segment. */
    copy_v3_v3(segment.scale, scale);
  }
}

struct BrushLocalData {
  Vector<float3> positions;
  Vector<float> factors;
  Vector<float> segment_weights;
  Vector<float3> segment_translations;
  Vector<float3> translations;
};

BLI_NOINLINE static void calc_segment_translations(const Span<float3> positions,
                                                   const IKChainSegment &segment,
                                                   const MutableSpan<float3> translations)
{
  BLI_assert(positions.size() == translations.size());
  for (const int i : positions.index_range()) {
    float3 position = positions[i];
    const ePaintSymmetryAreas symm_area = SCULPT_get_vertex_symm_area(position);
    position = math::transform_point(segment.pivot_mat_inv[int(symm_area)], position);
    position = math::transform_point(segment.trans_mat[int(symm_area)], position);
    position = math::transform_point(segment.pivot_mat[int(symm_area)], position);
    translations[i] = position - positions[i];
  }
}

BLI_NOINLINE static void add_arrays(const MutableSpan<float3> a, const Span<float3> b)
{
  BLI_assert(a.size() == b.size());
  for (const int i : a.index_range()) {
    a[i] += b[i];
  }
}

static void calc_mesh(const Depsgraph &depsgraph,
                      const Sculpt &sd,
                      const Brush &brush,
                      const MeshAttributeData &attribute_data,
                      const bke::pbvh::MeshNode &node,
                      Object &object,
                      BrushLocalData &tls,
                      const PositionDeformData &position_data)
{
  SculptSession &ss = *object.sculpt;
  const StrokeCache &cache = *ss.cache;

  const Span<int> verts = node.verts();
  const Span<float3> positions = gather_data_mesh(position_data.eval, verts, tls.positions);
  const OrigPositionData orig_data = orig_position_data_get_mesh(object, node);

  tls.factors.resize(verts.size());
  const MutableSpan<float> factors = tls.factors;
  fill_factor_from_hide_and_mask(attribute_data.hide_vert, attribute_data.mask, verts, factors);
  auto_mask::calc_vert_factors(depsgraph, object, cache.automasking.get(), node, verts, factors);

  tls.translations.resize(verts.size());
  const MutableSpan<float3> translations = tls.translations;
  translations.fill(float3(0));

  tls.segment_weights.resize(verts.size());
  tls.segment_translations.resize(verts.size());
  const MutableSpan<float> segment_weights = tls.segment_weights;
  const MutableSpan<float3> segment_translations = tls.segment_translations;

  for (const IKChainSegment &segment : cache.pose_ik_chain->segments) {
    calc_segment_translations(orig_data.positions, segment, segment_translations);
    gather_data_mesh(segment.weights.as_span(), verts, segment_weights);
    scale_translations(segment_translations, segment_weights);
    add_arrays(translations, segment_translations);
  }
  scale_translations(translations, factors);

  switch (eBrushDeformTarget(brush.deform_target)) {
    case BRUSH_DEFORM_TARGET_GEOMETRY:
      reset_translations_to_original(translations, positions, orig_data.positions);
      clip_and_lock_translations(sd, ss, position_data.eval, verts, translations);
      position_data.deform(translations, verts);
      break;
    case BRUSH_DEFORM_TARGET_CLOTH_SIM:
      add_arrays(translations, orig_data.positions);
      scatter_data_mesh(
          translations.as_span(), verts, cache.cloth_sim->deformation_pos.as_mutable_span());
      break;
  }
}

static void calc_grids(const Depsgraph &depsgraph,
                       const Sculpt &sd,
                       const Brush &brush,
                       const bke::pbvh::GridsNode &node,
                       Object &object,
                       BrushLocalData &tls)
{
  SculptSession &ss = *object.sculpt;
  const StrokeCache &cache = *ss.cache;
  SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;

  const Span<int> grids = node.grids();
  const Span<float3> positions = gather_grids_positions(subdiv_ccg, grids, tls.positions);
  const OrigPositionData orig_data = orig_position_data_get_grids(object, node);

  tls.factors.resize(positions.size());
  const MutableSpan<float> factors = tls.factors;
  fill_factor_from_hide_and_mask(subdiv_ccg, grids, factors);
  auto_mask::calc_grids_factors(depsgraph, object, cache.automasking.get(), node, grids, factors);

  tls.translations.resize(positions.size());
  const MutableSpan<float3> translations = tls.translations;
  translations.fill(float3(0));

  tls.segment_weights.resize(positions.size());
  tls.segment_translations.resize(positions.size());
  const MutableSpan<float> segment_weights = tls.segment_weights;
  const MutableSpan<float3> segment_translations = tls.segment_translations;

  for (const IKChainSegment &segment : cache.pose_ik_chain->segments) {
    calc_segment_translations(orig_data.positions, segment, segment_translations);
    gather_data_grids(subdiv_ccg, segment.weights.as_span(), grids, segment_weights);
    scale_translations(segment_translations, segment_weights);
    add_arrays(translations, segment_translations);
  }
  scale_translations(translations, factors);

  switch (eBrushDeformTarget(brush.deform_target)) {
    case BRUSH_DEFORM_TARGET_GEOMETRY:
      reset_translations_to_original(translations, positions, orig_data.positions);
      clip_and_lock_translations(sd, ss, orig_data.positions, translations);
      apply_translations(translations, grids, subdiv_ccg);
      break;
    case BRUSH_DEFORM_TARGET_CLOTH_SIM:
      add_arrays(translations, orig_data.positions);
      scatter_data_grids(subdiv_ccg,
                         translations.as_span(),
                         grids,
                         cache.cloth_sim->deformation_pos.as_mutable_span());
      break;
  }
}

static void calc_bmesh(const Depsgraph &depsgraph,
                       const Sculpt &sd,
                       const Brush &brush,
                       bke::pbvh::BMeshNode &node,
                       Object &object,
                       BrushLocalData &tls)
{
  SculptSession &ss = *object.sculpt;
  const StrokeCache &cache = *ss.cache;

  const Set<BMVert *, 0> &verts = BKE_pbvh_bmesh_node_unique_verts(&node);
  const Span<float3> positions = gather_bmesh_positions(verts, tls.positions);
  Array<float3> orig_positions(verts.size());
  Array<float3> orig_normals(verts.size());
  orig_position_data_gather_bmesh(*ss.bm_log, verts, orig_positions, orig_normals);

  tls.factors.resize(verts.size());
  const MutableSpan<float> factors = tls.factors;
  fill_factor_from_hide_and_mask(*ss.bm, verts, factors);
  auto_mask::calc_vert_factors(depsgraph, object, cache.automasking.get(), node, verts, factors);

  tls.translations.resize(verts.size());
  const MutableSpan<float3> translations = tls.translations;
  translations.fill(float3(0));

  tls.segment_weights.resize(verts.size());
  tls.segment_translations.resize(verts.size());
  const MutableSpan<float> segment_weights = tls.segment_weights;
  const MutableSpan<float3> segment_translations = tls.segment_translations;

  for (const IKChainSegment &segment : cache.pose_ik_chain->segments) {
    calc_segment_translations(orig_positions, segment, segment_translations);
    gather_data_bmesh(segment.weights.as_span(), verts, segment_weights);
    scale_translations(segment_translations, segment_weights);
    add_arrays(translations, segment_translations);
  }
  scale_translations(translations, factors);

  switch (eBrushDeformTarget(brush.deform_target)) {
    case BRUSH_DEFORM_TARGET_GEOMETRY:
      reset_translations_to_original(translations, positions, orig_positions);
      clip_and_lock_translations(sd, ss, orig_positions, translations);
      apply_translations(translations, verts);
      break;
    case BRUSH_DEFORM_TARGET_CLOTH_SIM:
      add_arrays(translations, orig_positions);
      scatter_data_bmesh(
          translations.as_span(), verts, cache.cloth_sim->deformation_pos.as_mutable_span());
      break;
  }
}

struct PoseGrowFactorData {
  float3 pos_avg;
  int pos_count;
  static PoseGrowFactorData join(const PoseGrowFactorData &a, const PoseGrowFactorData &b)
  {
    PoseGrowFactorData joined;
    joined.pos_avg = a.pos_avg + b.pos_avg;
    joined.pos_count = a.pos_count + b.pos_count;
    return joined;
  }
};

struct GrowFactorLocalData {
  Vector<int> vert_indices;
  Vector<int> neighbor_offsets;
  Vector<int> neighbor_data;
  Vector<int> neighbor_data_with_fake;
};

BLI_NOINLINE static void add_fake_neighbors(const Span<int> fake_neighbors,
                                            const Span<int> verts,
                                            const Span<int> orig_neighbor_data,
                                            MutableSpan<int> neighbor_offsets,
                                            Vector<int> &neighbor_data_with_fake)
{
  const OffsetIndices<int> offsets(neighbor_offsets);
  for (const int i : verts.index_range()) {
    const Span<int> orig_neighbors = orig_neighbor_data.slice(offsets[i]);

    /* Modify the offsets in-place after using them to slice the current neighbor data. */
    neighbor_offsets[i] = neighbor_data_with_fake.size();
    neighbor_data_with_fake.extend(orig_neighbors);
    const int neighbor = fake_neighbors[verts[i]];
    if (neighbor != FAKE_NEIGHBOR_NONE) {
      neighbor_data_with_fake.append(neighbor);
    }
  }
  neighbor_offsets.last() = neighbor_data_with_fake.size();
}

static void grow_factors_mesh(const ePaintSymmetryFlags symm,
                              const float3 &pose_initial_position,
                              const Span<float3> vert_positions,
                              const OffsetIndices<int> faces,
                              const Span<int> corner_verts,
                              const GroupedSpan<int> vert_to_face_map,
                              const Span<bool> hide_vert,
                              const Span<bool> hide_poly,
                              const Span<int> fake_neighbors,
                              const Span<float> prev_mask,
                              const bke::pbvh::MeshNode &node,
                              GrowFactorLocalData &tls,
                              const MutableSpan<float> pose_factor,
                              PoseGrowFactorData &gftd)
{
  const Span<int> verts = hide::node_visible_verts(node, hide_vert, tls.vert_indices);

  calc_vert_neighbors(faces,
                      corner_verts,
                      vert_to_face_map,
                      hide_poly,
                      verts,
                      tls.neighbor_offsets,
                      tls.neighbor_data);
  if (!fake_neighbors.is_empty()) {
    add_fake_neighbors(fake_neighbors,
                       verts,
                       tls.neighbor_data,
                       tls.neighbor_offsets,
                       tls.neighbor_data_with_fake);
  }
  const GroupedSpan<int> neighbors(tls.neighbor_offsets.as_span(),
                                   fake_neighbors.is_empty() ?
                                       tls.neighbor_data.as_span() :
                                       tls.neighbor_data_with_fake.as_span());

  for (const int i : verts.index_range()) {
    const int vert = verts[i];

    float max = 0.0f;
    for (const int neighbor : neighbors[i]) {
      max = std::max(max, prev_mask[neighbor]);
    }

    if (max > prev_mask[vert]) {
      const float3 &position = vert_positions[verts[i]];
      pose_factor[vert] = max;
      if (SCULPT_check_vertex_pivot_symmetry(position, pose_initial_position, symm)) {
        gftd.pos_avg += position;
        gftd.pos_count++;
      }
    }
  }
}

static void grow_factors_grids(const ePaintSymmetryFlags symm,
                               const float3 &pose_initial_position,
                               const SubdivCCG &subdiv_ccg,
                               const Span<int> fake_neighbors,
                               const Span<float> prev_mask,
                               const bke::pbvh::GridsNode &node,
                               const MutableSpan<float> pose_factor,
                               PoseGrowFactorData &gftd)
{
  const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);
  const Span<float3> positions = subdiv_ccg.positions;
  const BitGroupVector<> &grid_hidden = subdiv_ccg.grid_hidden;
  const Span<int> grids = node.grids();

  for (const int i : grids.index_range()) {
    const int grid = grids[i];
    const IndexRange grid_range = bke::ccg::grid_range(key, grid);
    for (const short y : IndexRange(key.grid_size)) {
      for (const short x : IndexRange(key.grid_size)) {
        const int offset = CCG_grid_xy_to_index(key.grid_size, x, y);
        if (!grid_hidden.is_empty() && grid_hidden[grid][offset]) {
          continue;
        }
        const int vert = grid_range[offset];

        SubdivCCGNeighbors neighbors;
        BKE_subdiv_ccg_neighbor_coords_get(
            subdiv_ccg, SubdivCCGCoord{grid, x, y}, false, neighbors);

        float max = 0.0f;
        for (const SubdivCCGCoord neighbor : neighbors.coords) {
          max = std::max(max, prev_mask[neighbor.to_index(key)]);
        }
        if (!fake_neighbors.is_empty()) {
          if (fake_neighbors[vert] != FAKE_NEIGHBOR_NONE) {
            max = std::max(max, prev_mask[fake_neighbors[vert]]);
          }
        }

        if (max > prev_mask[vert]) {
          const float3 &position = positions[vert];
          pose_factor[vert] = max;
          if (SCULPT_check_vertex_pivot_symmetry(position, pose_initial_position, symm)) {
            gftd.pos_avg += position;
            gftd.pos_count++;
          }
        }
      }
    }
  }
}

static void grow_factors_bmesh(const ePaintSymmetryFlags symm,
                               const float3 &pose_initial_position,
                               const Span<int> fake_neighbors,
                               const Span<float> prev_mask,
                               bke::pbvh::BMeshNode &node,
                               const MutableSpan<float> pose_factor,
                               PoseGrowFactorData &gftd)
{
  const Set<BMVert *, 0> &verts = BKE_pbvh_bmesh_node_unique_verts(&node);

  BMeshNeighborVerts neighbors;

  for (BMVert *bm_vert : verts) {
    const int vert = BM_elem_index_get(bm_vert);

    float max = 0.0f;
    for (const BMVert *neighbor : vert_neighbors_get_bmesh(*bm_vert, neighbors)) {
      max = std::max(max, prev_mask[BM_elem_index_get(neighbor)]);
    }
    if (!fake_neighbors.is_empty()) {
      if (fake_neighbors[vert] != FAKE_NEIGHBOR_NONE) {
        max = std::max(max, prev_mask[fake_neighbors[vert]]);
      }
    }

    if (max > prev_mask[vert]) {
      const float3 position = bm_vert->co;
      pose_factor[vert] = max;
      if (SCULPT_check_vertex_pivot_symmetry(position, pose_initial_position, symm)) {
        gftd.pos_avg += position;
        gftd.pos_count++;
      }
    }
  }
}

/* Grow the factor until its boundary is near to the offset pose origin or outside the target
 * distance. */
static void grow_pose_factor(const Depsgraph &depsgraph,
                             Object &ob,
                             SculptSession &ss,
                             float pose_origin[3],
                             float pose_target[3],
                             float max_len,
                             float *r_pose_origin,
                             MutableSpan<float> pose_factor)
{
  bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(ob);
  const ePaintSymmetryFlags symm = SCULPT_mesh_symmetry_xyz_get(ob);

  IndexMaskMemory memory;
  const IndexMask node_mask = bke::pbvh::all_leaf_nodes(pbvh, memory);
  const Span<int> fake_neighbors = ss.fake_neighbors.fake_neighbor_index;

  bool grow_next_iteration = true;
  float prev_len = FLT_MAX;
  Array<float> prev_mask(SCULPT_vertex_count_get(ob));
  while (grow_next_iteration) {
    prev_mask.as_mutable_span().copy_from(pose_factor);

    PoseGrowFactorData gftd;
    threading::EnumerableThreadSpecific<GrowFactorLocalData> all_tls;
    switch (pbvh.type()) {
      case bke::pbvh::Type::Mesh: {
        MutableSpan<bke::pbvh::MeshNode> nodes = pbvh.nodes<bke::pbvh::MeshNode>();
        const Mesh &mesh = *static_cast<const Mesh *>(ob.data);
        const Span<float3> vert_positions = bke::pbvh::vert_positions_eval(depsgraph, ob);
        const OffsetIndices faces = mesh.faces();
        const Span<int> corner_verts = mesh.corner_verts();
        const GroupedSpan<int> vert_to_face_map = mesh.vert_to_face_map();
        const bke::AttributeAccessor attributes = mesh.attributes();
        const VArraySpan<bool> hide_vert = *attributes.lookup<bool>(".hide_vert",
                                                                    bke::AttrDomain::Point);
        const VArraySpan<bool> hide_poly = *attributes.lookup<bool>(".hide_poly",
                                                                    bke::AttrDomain::Face);
        gftd = threading::parallel_reduce(
            node_mask.index_range(),
            1,
            PoseGrowFactorData{},
            [&](const IndexRange range, PoseGrowFactorData gftd) {
              GrowFactorLocalData &tls = all_tls.local();
              node_mask.slice(range).foreach_index([&](const int i) {
                grow_factors_mesh(symm,
                                  pose_target,
                                  vert_positions,
                                  faces,
                                  corner_verts,
                                  vert_to_face_map,
                                  hide_vert,
                                  hide_poly,
                                  fake_neighbors,
                                  prev_mask,
                                  nodes[i],
                                  tls,
                                  pose_factor,
                                  gftd);
              });
              return gftd;
            },
            PoseGrowFactorData::join);
        break;
      }
      case bke::pbvh::Type::Grids: {
        MutableSpan<bke::pbvh::GridsNode> nodes = pbvh.nodes<bke::pbvh::GridsNode>();
        const SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;
        gftd = threading::parallel_reduce(
            node_mask.index_range(),
            1,
            PoseGrowFactorData{},
            [&](const IndexRange range, PoseGrowFactorData gftd) {
              node_mask.slice(range).foreach_index([&](const int i) {
                grow_factors_grids(symm,
                                   pose_target,
                                   subdiv_ccg,
                                   fake_neighbors,
                                   prev_mask,
                                   nodes[i],
                                   pose_factor,
                                   gftd);
              });
              return gftd;
            },
            PoseGrowFactorData::join);
        break;
      }
      case bke::pbvh::Type::BMesh: {
        MutableSpan<bke::pbvh::BMeshNode> nodes = pbvh.nodes<bke::pbvh::BMeshNode>();
        gftd = threading::parallel_reduce(
            node_mask.index_range(),
            1,
            PoseGrowFactorData{},
            [&](const IndexRange range, PoseGrowFactorData gftd) {
              node_mask.slice(range).foreach_index([&](const int i) {
                grow_factors_bmesh(
                    symm, pose_target, fake_neighbors, prev_mask, nodes[i], pose_factor, gftd);
              });
              return gftd;
            },
            PoseGrowFactorData::join);
        break;
      }
    }

    if (gftd.pos_count != 0) {
      gftd.pos_avg /= float(gftd.pos_count);
      if (pose_origin) {
        /* Test with pose origin. Used when growing the factors to compensate the Origin Offset. */
        /* Stop when the factor's avg_pos starts moving away from the origin instead of getting
         * closer to it. */
        float len = math::distance(gftd.pos_avg, float3(pose_origin));
        if (len < prev_len) {
          prev_len = len;
          grow_next_iteration = true;
        }
        else {
          grow_next_iteration = false;
          pose_factor.copy_from(prev_mask);
        }
      }
      else {
        /* Test with length. Used to calculate the origin positions of the IK chain. */
        /* Stops when the factors have grown enough to generate a new segment origin. */
        float len = math::distance(gftd.pos_avg, float3(pose_target));
        if (len < max_len) {
          prev_len = len;
          grow_next_iteration = true;
        }
        else {
          grow_next_iteration = false;
          if (r_pose_origin) {
            copy_v3_v3(r_pose_origin, gftd.pos_avg);
          }
          pose_factor.copy_from(prev_mask);
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
}

static bool vert_inside_brush_radius(const float3 &vertex,
                                     const float3 &br_co,
                                     float radius,
                                     char symm)
{
  for (char i = 0; i <= symm; ++i) {
    if (is_symmetry_iteration_valid(i, symm)) {
      const float3 location = symmetry_flip(br_co, ePaintSymmetryFlags(i));
      if (math::distance(location, vertex) < radius) {
        return true;
      }
    }
  }
  return false;
}

/**
 * fallback_floodfill_origin: In topology mode this stores the furthest point from the
 * stroke origin for cases when a pose origin based on the brush radius can't be set.
 */
static void calc_pose_origin_and_factor_mesh(const Depsgraph &depsgraph,
                                             Object &object,
                                             SculptSession &ss,
                                             const float3 &initial_location,
                                             float radius,
                                             float3 &r_pose_origin,
                                             MutableSpan<float> r_pose_factor)
{
  BLI_assert(!r_pose_factor.is_empty());

  const Mesh &mesh = *static_cast<const Mesh *>(object.data);
  const GroupedSpan<int> vert_to_face_map = mesh.vert_to_face_map();
  const Span<float3> positions_eval = bke::pbvh::vert_positions_eval(depsgraph, object);

  /* Calculate the pose rotation point based on the boundaries of the brush factor. */
  flood_fill::FillDataMesh flood(positions_eval.size(), ss.fake_neighbors.fake_neighbor_index);
  flood.add_initial(find_symm_verts_mesh(depsgraph, object, ss.active_vert_index(), radius));

  const int symm = SCULPT_mesh_symmetry_xyz_get(object);

  int tot_co = 0;
  float3 pose_origin(0);
  float3 fallback_floodfill_origin = initial_location;
  flood.execute(object, vert_to_face_map, [&](int /*from_v*/, int to_v) {
    r_pose_factor[to_v] = 1.0f;

    const float3 co = positions_eval[to_v];
    if (math::distance_squared(initial_location, fallback_floodfill_origin) <
        math::distance_squared(initial_location, co))
    {
      fallback_floodfill_origin = co;
    }

    if (vert_inside_brush_radius(co, initial_location, radius, symm)) {
      return true;
    }

    if (SCULPT_check_vertex_pivot_symmetry(co, initial_location, symm)) {
      pose_origin += co;
      tot_co++;
    }

    return false;
  });

  if (tot_co > 0) {
    r_pose_origin = pose_origin / float(tot_co);
  }
  else {
    r_pose_origin = fallback_floodfill_origin;
  }
}

static void calc_pose_origin_and_factor_grids(Object &object,
                                              SculptSession &ss,
                                              const float3 &initial_location,
                                              float radius,
                                              float3 &r_pose_origin,
                                              MutableSpan<float> r_pose_factor)
{
  BLI_assert(!r_pose_factor.is_empty());

  const SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;

  const Span<float3> positions = subdiv_ccg.positions;
  const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);
  /* Calculate the pose rotation point based on the boundaries of the brush factor. */
  flood_fill::FillDataGrids flood(positions.size(), ss.fake_neighbors.fake_neighbor_index);
  flood.add_initial(key, find_symm_verts_grids(object, ss.active_vert_index(), radius));

  const int symm = SCULPT_mesh_symmetry_xyz_get(object);

  int tot_co = 0;
  float3 pose_origin(0);
  float3 fallback_floodfill_origin = initial_location;
  flood.execute(
      object, subdiv_ccg, [&](SubdivCCGCoord /*from_v*/, SubdivCCGCoord to_v, bool is_duplicate) {
        const int to_v_i = to_v.to_index(key);

        r_pose_factor[to_v_i] = 1.0f;

        const float3 &co = positions[to_v_i];
        if (math::distance_squared(initial_location, fallback_floodfill_origin) <
            math::distance_squared(initial_location, co))
        {
          fallback_floodfill_origin = co;
        }

        if (vert_inside_brush_radius(co, initial_location, radius, symm)) {
          return true;
        }

        if (SCULPT_check_vertex_pivot_symmetry(co, initial_location, symm)) {
          if (!is_duplicate) {
            pose_origin += co;
            tot_co++;
          }
        }

        return false;
      });

  if (tot_co > 0) {
    r_pose_origin = pose_origin / float(tot_co);
  }
  else {
    r_pose_origin = fallback_floodfill_origin;
  }
}

static void calc_pose_origin_and_factor_bmesh(Object &object,
                                              SculptSession &ss,
                                              const float3 &initial_location,
                                              float radius,
                                              float3 &r_pose_origin,
                                              MutableSpan<float> r_pose_factor)
{
  BLI_assert(!r_pose_factor.is_empty());
  vert_random_access_ensure(object);

  /* Calculate the pose rotation point based on the boundaries of the brush factor. */
  flood_fill::FillDataBMesh flood(BM_mesh_elem_count(ss.bm, BM_VERT),
                                  ss.fake_neighbors.fake_neighbor_index);
  flood.add_initial(*ss.bm, find_symm_verts_bmesh(object, ss.active_vert_index(), radius));

  const int symm = SCULPT_mesh_symmetry_xyz_get(object);

  int tot_co = 0;
  float3 pose_origin(0);
  float3 fallback_floodfill_origin = initial_location;
  flood.execute(object, [&](BMVert * /*from_v*/, BMVert *to_v) {
    const int to_v_i = BM_elem_index_get(to_v);
    r_pose_factor[to_v_i] = 1.0f;

    const float3 co = to_v->co;
    if (math::distance_squared(initial_location, fallback_floodfill_origin) <
        math::distance_squared(initial_location, co))
    {
      fallback_floodfill_origin = co;
    }

    if (vert_inside_brush_radius(co, initial_location, radius, symm)) {
      return true;
    }

    if (SCULPT_check_vertex_pivot_symmetry(co, initial_location, symm)) {
      pose_origin += co;
      tot_co++;
    }

    return false;
  });

  if (tot_co > 0) {
    r_pose_origin = pose_origin / float(tot_co);
  }
  else {
    r_pose_origin = fallback_floodfill_origin;
  }
}

static void calc_pose_data(const Depsgraph &depsgraph,
                           Object &object,
                           SculptSession &ss,
                           const float3 &initial_location,
                           float radius,
                           float pose_offset,
                           float3 &r_pose_origin,
                           MutableSpan<float> r_pose_factor)
{
  BLI_assert(!r_pose_factor.is_empty());

  float3 pose_origin;
  switch (bke::object::pbvh_get(object)->type()) {
    case bke::pbvh::Type::Mesh:
      calc_pose_origin_and_factor_mesh(
          depsgraph, object, ss, initial_location, radius, pose_origin, r_pose_factor);
      break;
    case bke::pbvh::Type::Grids:
      calc_pose_origin_and_factor_grids(
          object, ss, initial_location, radius, pose_origin, r_pose_factor);
      break;
    case bke::pbvh::Type::BMesh:
      calc_pose_origin_and_factor_bmesh(
          object, ss, initial_location, radius, pose_origin, r_pose_factor);
      break;
  }

  /* Offset the pose origin. */
  const float3 pose_dir = math::normalize(pose_origin - initial_location);
  pose_origin += pose_dir * radius * pose_offset;
  r_pose_origin = pose_origin;

  /* Do the initial grow of the factors to get the first segment of the chain with Origin Offset.
   */
  if (pose_offset != 0.0f) {
    grow_pose_factor(depsgraph, object, ss, pose_origin, pose_origin, 0, nullptr, r_pose_factor);
  }
}

/* Init the IK chain with empty weights. */
static std::unique_ptr<IKChain> ik_chain_new(const int totsegments, const int totverts)
{
  std::unique_ptr<IKChain> ik_chain = std::make_unique<IKChain>();
  ik_chain->segments.reinitialize(totsegments);
  for (IKChainSegment &segment : ik_chain->segments) {
    segment.weights = Array<float>(totverts, 0.0f);
  }
  return ik_chain;
}

/* Init the origin/head pairs of all the segments from the calculated origins. */
static void ik_chain_origin_heads_init(IKChain &ik_chain, const float3 &initial_location)
{
  float3 origin;
  float3 head;
  for (const int i : ik_chain.segments.index_range()) {
    if (i == 0) {
      head = initial_location;
      origin = ik_chain.segments[i].orig;
    }
    else {
      head = ik_chain.segments[i - 1].orig;
      origin = ik_chain.segments[i].orig;
    }
    ik_chain.segments[i].orig = origin;
    ik_chain.segments[i].initial_orig = origin;
    ik_chain.segments[i].head = head;
    ik_chain.segments[i].initial_head = head;
    ik_chain.segments[i].len = math::distance(head, origin);
    ik_chain.segments[i].scale = float3(1.0f);
  }
}

static int brush_num_effective_segments(const Brush &brush)
{
  /* Scaling multiple segments at the same time is not supported as the IK solver can't handle
   * changes in the segment's length. It will also required a better weight distribution to avoid
   * artifacts in the areas affected by multiple segments. */
  if (ELEM(brush.pose_deform_type,
           BRUSH_POSE_DEFORM_SCALE_TRASLATE,
           BRUSH_POSE_DEFORM_SQUASH_STRETCH))
  {
    return 1;
  }
  return brush.pose_ik_segments;
}

static std::unique_ptr<IKChain> ik_chain_init_topology(const Depsgraph &depsgraph,
                                                       Object &object,
                                                       SculptSession &ss,
                                                       const Brush &brush,
                                                       const float3 &initial_location,
                                                       const float radius)
{

  const float chain_segment_len = radius * (1.0f + brush.pose_offset);

  const int totvert = SCULPT_vertex_count_get(object);
  const bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);

  int nearest_vertex_index = -1;
  /* TODO: How should this function handle not being able to find the nearest vert? */
  switch (pbvh.type()) {
    case bke::pbvh::Type::Mesh: {
      const Mesh &mesh = *static_cast<const Mesh *>(object.data);
      const Span<float3> vert_positions = bke::pbvh::vert_positions_eval(depsgraph, object);
      const bke::AttributeAccessor attributes = mesh.attributes();
      VArraySpan<bool> hide_vert = *attributes.lookup<bool>(".hide_vert", bke::AttrDomain::Point);
      std::optional<int> nearest = nearest_vert_calc_mesh(pbvh,
                                                          vert_positions,
                                                          hide_vert,
                                                          initial_location,
                                                          std::numeric_limits<float>::max(),
                                                          true);
      nearest_vertex_index = *nearest;
      break;
    }
    case bke::pbvh::Type::Grids: {
      const SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;
      const std::optional<SubdivCCGCoord> nearest = nearest_vert_calc_grids(
          pbvh, subdiv_ccg, initial_location, std::numeric_limits<float>::max(), true);
      const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);
      nearest_vertex_index = nearest->to_index(key);
      break;
    }
    case bke::pbvh::Type::BMesh: {
      const std::optional<BMVert *> nearest = nearest_vert_calc_bmesh(
          pbvh, initial_location, std::numeric_limits<float>::max(), false);
      nearest_vertex_index = BM_elem_index_get(*nearest);
      break;
    }
  }

  /* Init the buffers used to keep track of the changes in the pose factors as more segments are
   * added to the IK chain. */

  /* This stores the whole pose factors values as they grow through the mesh. */
  Array<float> pose_factor_grow(totvert, 0.0f);

  /* This stores the previous status of the factors when growing a new iteration. */
  Array<float> pose_factor_grow_prev(totvert, 0.0f);

  pose_factor_grow[nearest_vertex_index] = 1.0f;

  const int tot_segments = brush_num_effective_segments(brush);
  std::unique_ptr<IKChain> ik_chain = ik_chain_new(tot_segments, totvert);

  /* Calculate the first segment in the chain using the brush radius and the pose origin offset. */
  calc_pose_data(depsgraph,
                 object,
                 ss,
                 initial_location,
                 radius,
                 brush.pose_offset,
                 ik_chain->segments[0].orig,
                 pose_factor_grow);

  float3 next_chain_segment_target = ik_chain->segments[0].orig;

  /* Init the weights of this segment and store the status of the pose factors to start calculating
   * new segment origins. */
  for (int j = 0; j < totvert; j++) {
    ik_chain->segments[0].weights[j] = pose_factor_grow[j];
    pose_factor_grow_prev[j] = pose_factor_grow[j];
  }

  /* Calculate the next segments in the chain growing the pose factors. */
  for (const int i : ik_chain->segments.index_range().drop_front(1)) {

    /* Grow the factors to get the new segment origin. */
    grow_pose_factor(depsgraph,
                     object,
                     ss,
                     nullptr,
                     next_chain_segment_target,
                     chain_segment_len,
                     ik_chain->segments[i].orig,
                     pose_factor_grow);
    next_chain_segment_target = ik_chain->segments[i].orig;

    /* Create the weights for this segment from the difference between the previous grow factor
     * iteration an the current iteration. */
    for (int j = 0; j < totvert; j++) {
      ik_chain->segments[i].weights[j] = pose_factor_grow[j] - pose_factor_grow_prev[j];
      /* Store the current grow factor status for the next iteration. */
      pose_factor_grow_prev[j] = pose_factor_grow[j];
    }
  }

  ik_chain_origin_heads_init(*ik_chain, initial_location);

  return ik_chain;
}

static std::unique_ptr<IKChain> ik_chain_init_face_sets_mesh(const Depsgraph &depsgraph,
                                                             Object &object,
                                                             SculptSession &ss,
                                                             const Brush &brush,
                                                             const float radius)
{
  struct SegmentData {
    int vert;
    int face_set;
  };

  Mesh &mesh = *static_cast<Mesh *>(object.data);
  Span<float3> vert_positions = bke::pbvh::vert_positions_eval(depsgraph, object);
  const OffsetIndices faces = mesh.faces();
  const Span<int> corner_verts = mesh.corner_verts();
  const GroupedSpan<int> vert_to_face_map = mesh.vert_to_face_map();
  const bke::AttributeAccessor attributes = mesh.attributes();
  const VArraySpan<bool> hide_vert = *attributes.lookup<bool>(".hide_vert",
                                                              bke::AttrDomain::Point);
  const VArraySpan<bool> hide_poly = *attributes.lookup<bool>(".hide_poly", bke::AttrDomain::Face);
  const VArraySpan face_sets = *attributes.lookup_or_default<int>(
      ".sculpt_face_set", bke::AttrDomain::Face, 0);

  std::unique_ptr<IKChain> ik_chain = ik_chain_new(brush_num_effective_segments(brush),
                                                   vert_positions.size());

  /* Each vertex can only be assigned to one face set. */
  BitVector<> is_weighted(vert_positions.size());
  Set<int> visited_face_sets;

  SegmentData current_data = {std::get<int>(ss.active_vert()), SCULPT_FACE_SET_NONE};

  const int symm = SCULPT_mesh_symmetry_xyz_get(object);
  Vector<int> neighbors;
  for (const int i : ik_chain->segments.index_range()) {
    const bool is_first_iteration = i == 0;

    flood_fill::FillDataMesh flood_fill(vert_positions.size(),
                                        ss.fake_neighbors.fake_neighbor_index);
    flood_fill.add_initial(find_symm_verts_mesh(depsgraph, object, current_data.vert, radius));

    visited_face_sets.add(current_data.face_set);

    MutableSpan<float> pose_factor = ik_chain->segments[i].weights;
    std::optional<SegmentData> next_segment_data;

    float3 face_set_boundary_accum(0);
    int face_set_boundary_count = 0;

    float3 fallback_accum(0);
    int fallback_count = 0;

    const float3 &pose_initial_co = vert_positions[current_data.vert];
    flood_fill.execute(object, vert_to_face_map, [&](int /*from_v*/, int to_v) {
      const float3 &to_v_position = vert_positions[to_v];
      const bool symmetry_check = SCULPT_check_vertex_pivot_symmetry(
          to_v_position, pose_initial_co, symm);

      /* First iteration. Continue expanding using topology until a vertex is outside the brush
       * radius to determine the first face set. */
      if (current_data.face_set == SCULPT_FACE_SET_NONE) {

        pose_factor[to_v] = 1.0f;
        is_weighted[to_v].set();

        if (vert_inside_brush_radius(to_v_position, pose_initial_co, radius, symm)) {
          const int visited_face_set = face_set::vert_face_set_get(
              vert_to_face_map, face_sets, to_v);
          visited_face_sets.add(visited_face_set);
        }
        else if (symmetry_check) {
          current_data.face_set = face_set::vert_face_set_get(vert_to_face_map, face_sets, to_v);
          visited_face_sets.add(current_data.face_set);
        }
        return true;
      }

      /* We already have a current face set, so we can start checking the face sets of the
       * vertices. */
      /* In the first iteration we need to check all face sets we already visited as the flood
       * fill may still not be finished in some of them. */
      bool is_vertex_valid = false;
      if (is_first_iteration) {
        for (const int visited_face_set : visited_face_sets) {
          is_vertex_valid |= face_set::vert_has_face_set(
              vert_to_face_map, face_sets, to_v, visited_face_set);
        }
      }
      else {
        is_vertex_valid = face_set::vert_has_face_set(
            vert_to_face_map, face_sets, to_v, current_data.face_set);
      }

      if (!is_vertex_valid) {
        return false;
      }

      bool visit_next = false;
      if (!is_weighted[to_v]) {
        pose_factor[to_v] = 1.0f;
        is_weighted[to_v].set();
        visit_next = true;
      }

      /* Fallback origin accumulation. */
      if (symmetry_check) {
        fallback_accum += to_v_position;
        fallback_count++;
      }

      if (!symmetry_check || face_set::vert_has_unique_face_set(vert_to_face_map, face_sets, to_v))
      {
        return visit_next;
      }

      /* We only add coordinates for calculating the origin when it is possible to go from this
       * vertex to another vertex in a valid face set for the next iteration. */
      bool count_as_boundary = false;

      for (const int neighbor_idx : vert_neighbors_get_mesh(
               faces, corner_verts, vert_to_face_map, hide_poly, to_v, neighbors))
      {
        const int next_face_set_candidate = face_set::vert_face_set_get(
            vert_to_face_map, face_sets, neighbor_idx);

        /* Check if we can get a valid face set for the next iteration from this neighbor. */
        if (face_set::vert_has_unique_face_set(vert_to_face_map, face_sets, neighbor_idx) &&
            !visited_face_sets.contains(next_face_set_candidate))
        {
          if (!next_segment_data) {
            next_segment_data = {neighbor_idx, next_face_set_candidate};
          }
          count_as_boundary = true;
        }
      }

      /* Origin accumulation. */
      if (count_as_boundary) {
        face_set_boundary_accum += to_v_position;
        face_set_boundary_count++;
      }
      return visit_next;
    });

    if (face_set_boundary_count > 0) {
      ik_chain->segments[i].orig = face_set_boundary_accum / float(face_set_boundary_count);
    }
    else if (fallback_count > 0) {
      ik_chain->segments[i].orig = fallback_accum / float(fallback_count);
    }
    else {
      ik_chain->segments[i].orig = float3(0);
    }

    current_data = *next_segment_data;
  }

  ik_chain_origin_heads_init(*ik_chain, vert_positions[std::get<int>(ss.active_vert())]);

  return ik_chain;
}

static std::unique_ptr<IKChain> ik_chain_init_face_sets_grids(Object &object,
                                                              SculptSession &ss,
                                                              const Brush &brush,
                                                              const float radius)
{
  struct SegmentData {
    int vert;
    int face_set;
  };

  const Mesh &mesh = *static_cast<const Mesh *>(object.data);
  const OffsetIndices<int> faces = mesh.faces();
  const Span<int> corner_verts = mesh.corner_verts();
  const GroupedSpan<int> vert_to_face_map = mesh.vert_to_face_map();
  const bke::AttributeAccessor attributes = mesh.attributes();
  const VArraySpan face_sets = *attributes.lookup_or_default<int>(
      ".sculpt_face_set", bke::AttrDomain::Face, 0);

  const SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;
  const Span<float3> positions = subdiv_ccg.positions;
  const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);
  const int grids_num = ss.subdiv_ccg->grids_num * key.grid_area;

  std::unique_ptr<IKChain> ik_chain = ik_chain_new(brush_num_effective_segments(brush), grids_num);

  /* Each vertex can only be assigned to one face set. */
  BitVector<> is_weighted(grids_num);
  Set<int> visited_face_sets;

  SegmentData current_data = {ss.active_vert_index(), SCULPT_FACE_SET_NONE};

  const int symm = SCULPT_mesh_symmetry_xyz_get(object);
  SubdivCCGNeighbors neighbors;
  for (const int i : ik_chain->segments.index_range()) {
    const bool is_first_iteration = i == 0;

    flood_fill::FillDataGrids flood_fill(grids_num, ss.fake_neighbors.fake_neighbor_index);
    flood_fill.add_initial(key, find_symm_verts_grids(object, current_data.vert, radius));

    visited_face_sets.add(current_data.face_set);

    MutableSpan<float> pose_factor = ik_chain->segments[i].weights;
    std::optional<SegmentData> next_segment_data;

    float3 face_set_boundary_accum(0);
    int face_set_boundary_count = 0;

    float3 fallback_accum(0);
    int fallback_count = 0;

    const float3 &pose_initial_co = positions[current_data.vert];
    flood_fill.execute(
        object,
        subdiv_ccg,
        [&](SubdivCCGCoord /*from_v*/, SubdivCCGCoord to_v, bool is_duplicate) {
          const int to_v_i = to_v.to_index(key);

          const float3 to_v_position = positions[to_v_i];
          const bool symmetry_check = SCULPT_check_vertex_pivot_symmetry(
                                          to_v_position, pose_initial_co, symm) &&
                                      !is_duplicate;

          /* First iteration. Continue expanding using topology until a vertex is outside the brush
           * radius to determine the first face set. */
          if (current_data.face_set == SCULPT_FACE_SET_NONE) {

            pose_factor[to_v_i] = 1.0f;
            is_weighted[to_v_i].set();

            if (vert_inside_brush_radius(to_v_position, pose_initial_co, radius, symm)) {
              const int visited_face_set = face_set::vert_face_set_get(
                  subdiv_ccg, face_sets, to_v.grid_index);
              visited_face_sets.add(visited_face_set);
            }
            else if (symmetry_check) {
              current_data.face_set = face_set::vert_face_set_get(
                  subdiv_ccg, face_sets, to_v.grid_index);
              visited_face_sets.add(current_data.face_set);
            }
            return true;
          }

          /* We already have a current face set, so we can start checking the face sets of the
           * vertices. */
          /* In the first iteration we need to check all face sets we already visited as the flood
           * fill may still not be finished in some of them. */
          bool is_vertex_valid = false;
          if (is_first_iteration) {
            for (const int visited_face_set : visited_face_sets) {
              is_vertex_valid |= face_set::vert_has_face_set(
                  subdiv_ccg, face_sets, to_v.grid_index, visited_face_set);
            }
          }
          else {
            is_vertex_valid = face_set::vert_has_face_set(
                subdiv_ccg, face_sets, to_v.grid_index, current_data.face_set);
          }

          if (!is_vertex_valid) {
            return false;
          }

          bool visit_next = false;
          if (!is_weighted[to_v_i]) {
            pose_factor[to_v_i] = 1.0f;
            is_weighted[to_v_i].set();
            visit_next = true;
          }

          /* Fallback origin accumulation. */
          if (symmetry_check) {
            fallback_accum += to_v_position;
            fallback_count++;
          }

          if (!symmetry_check ||
              face_set::vert_has_unique_face_set(
                  faces, corner_verts, vert_to_face_map, face_sets, subdiv_ccg, to_v))
          {
            return visit_next;
          }

          /* We only add coordinates for calculating the origin when it is possible to go from this
           * vertex to another vertex in a valid face set for the next iteration. */
          bool count_as_boundary = false;

          BKE_subdiv_ccg_neighbor_coords_get(subdiv_ccg, to_v, false, neighbors);
          for (const SubdivCCGCoord neighbor : neighbors.coords) {
            const int next_face_set_candidate = face_set::vert_face_set_get(
                subdiv_ccg, face_sets, neighbor.grid_index);

            /* Check if we can get a valid face set for the next iteration from this neighbor. */
            if (face_set::vert_has_unique_face_set(
                    faces, corner_verts, vert_to_face_map, face_sets, subdiv_ccg, neighbor) &&
                !visited_face_sets.contains(next_face_set_candidate))
            {
              if (!next_segment_data) {
                next_segment_data = {neighbor.to_index(key), next_face_set_candidate};
              }
              count_as_boundary = true;
            }
          }

          /* Origin accumulation. */
          if (count_as_boundary) {
            face_set_boundary_accum += to_v_position;
            face_set_boundary_count++;
          }
          return visit_next;
        });

    if (face_set_boundary_count > 0) {
      ik_chain->segments[i].orig = face_set_boundary_accum / float(face_set_boundary_count);
    }
    else if (fallback_count > 0) {
      ik_chain->segments[i].orig = fallback_accum / float(fallback_count);
    }
    else {
      ik_chain->segments[i].orig = float3(0);
    }

    current_data = *next_segment_data;
  }

  ik_chain_origin_heads_init(*ik_chain, positions[ss.active_vert_index()]);

  return ik_chain;
}

static std::unique_ptr<IKChain> ik_chain_init_face_sets_bmesh(Object &object,
                                                              SculptSession &ss,
                                                              const Brush &brush,
                                                              const float radius)
{
  struct SegmentData {
    BMVert *vert;
    int face_set;
  };

  const int verts_num = BM_mesh_elem_count(ss.bm, BM_VERT);
  const int face_set_offset = CustomData_get_offset_named(
      &ss.bm->pdata, CD_PROP_INT32, ".sculpt_face_set");
  std::unique_ptr<IKChain> ik_chain = ik_chain_new(brush_num_effective_segments(brush), verts_num);

  /* Each vertex can only be assigned to one face set. */
  BitVector<> is_weighted(verts_num);
  Set<int> visited_face_sets;

  SegmentData current_data = {std::get<BMVert *>(ss.active_vert()), SCULPT_FACE_SET_NONE};

  const int symm = SCULPT_mesh_symmetry_xyz_get(object);
  BMeshNeighborVerts neighbors;
  for (const int i : ik_chain->segments.index_range()) {
    const bool is_first_iteration = i == 0;

    flood_fill::FillDataBMesh flood_fill(verts_num, ss.fake_neighbors.fake_neighbor_index);
    flood_fill.add_initial(
        *ss.bm, find_symm_verts_bmesh(object, BM_elem_index_get(current_data.vert), radius));

    visited_face_sets.add(current_data.face_set);

    MutableSpan<float> pose_factor = ik_chain->segments[i].weights;
    std::optional<SegmentData> next_segment_data;

    float3 face_set_boundary_accum(0);
    int face_set_boundary_count = 0;

    float3 fallback_accum(0);
    int fallback_count = 0;

    const float3 pose_initial_co = current_data.vert->co;
    flood_fill.execute(object, [&](BMVert * /*from_v*/, BMVert *to_v) {
      const int to_v_i = BM_elem_index_get(to_v);

      const float3 to_v_position = to_v->co;
      const bool symmetry_check = SCULPT_check_vertex_pivot_symmetry(
          to_v_position, pose_initial_co, symm);

      /* First iteration. Continue expanding using topology until a vertex is outside the brush
       * radius to determine the first face set. */
      if (current_data.face_set == SCULPT_FACE_SET_NONE) {

        pose_factor[to_v_i] = 1.0f;
        is_weighted[to_v_i].set();

        if (vert_inside_brush_radius(to_v_position, pose_initial_co, radius, symm)) {
          const int visited_face_set = face_set::vert_face_set_get(face_set_offset, *to_v);
          visited_face_sets.add(visited_face_set);
        }
        else if (symmetry_check) {
          current_data.face_set = face_set::vert_face_set_get(face_set_offset, *to_v);
          visited_face_sets.add(current_data.face_set);
        }
        return true;
      }

      /* We already have a current face set, so we can start checking the face sets of the
       * vertices. */
      /* In the first iteration we need to check all face sets we already visited as the flood
       * fill may still not be finished in some of them. */
      bool is_vertex_valid = false;
      if (is_first_iteration) {
        for (const int visited_face_set : visited_face_sets) {
          is_vertex_valid |= face_set::vert_has_face_set(face_set_offset, *to_v, visited_face_set);
        }
      }
      else {
        is_vertex_valid = face_set::vert_has_face_set(
            face_set_offset, *to_v, current_data.face_set);
      }

      if (!is_vertex_valid) {
        return false;
      }

      bool visit_next = false;
      if (!is_weighted[to_v_i]) {
        pose_factor[to_v_i] = 1.0f;
        is_weighted[to_v_i].set();
        visit_next = true;
      }

      /* Fallback origin accumulation. */
      if (symmetry_check) {
        fallback_accum += to_v_position;
        fallback_count++;
      }

      if (!symmetry_check || face_set::vert_has_unique_face_set(face_set_offset, *to_v)) {
        return visit_next;
      }

      /* We only add coordinates for calculating the origin when it is possible to go from this
       * vertex to another vertex in a valid face set for the next iteration. */
      bool count_as_boundary = false;

      for (BMVert *neighbor : vert_neighbors_get_bmesh(*to_v, neighbors)) {
        const int next_face_set_candidate = face_set::vert_face_set_get(face_set_offset,
                                                                        *neighbor);

        /* Check if we can get a valid face set for the next iteration from this neighbor. */
        if (face_set::vert_has_unique_face_set(face_set_offset, *neighbor) &&
            !visited_face_sets.contains(next_face_set_candidate))
        {
          if (!next_segment_data) {
            next_segment_data = {neighbor, next_face_set_candidate};
          }
          count_as_boundary = true;
        }
      }

      /* Origin accumulation. */
      if (count_as_boundary) {
        face_set_boundary_accum += to_v_position;
        face_set_boundary_count++;
      }
      return visit_next;
    });

    if (face_set_boundary_count > 0) {
      ik_chain->segments[i].orig = face_set_boundary_accum / float(face_set_boundary_count);
    }
    else if (fallback_count > 0) {
      ik_chain->segments[i].orig = fallback_accum / float(fallback_count);
    }
    else {
      ik_chain->segments[i].orig = float3(0);
    }

    current_data = *next_segment_data;
  }

  ik_chain_origin_heads_init(*ik_chain, std::get<BMVert *>(ss.active_vert())->co);

  return ik_chain;
}

static std::unique_ptr<IKChain> ik_chain_init_face_sets(const Depsgraph &depsgraph,
                                                        Object &object,
                                                        SculptSession &ss,
                                                        const Brush &brush,
                                                        const float radius)
{
  switch (bke::object::pbvh_get(object)->type()) {
    case bke::pbvh::Type::Mesh:
      return ik_chain_init_face_sets_mesh(depsgraph, object, ss, brush, radius);
    case bke::pbvh::Type::Grids:
      return ik_chain_init_face_sets_grids(object, ss, brush, radius);
    case bke::pbvh::Type::BMesh:
      return ik_chain_init_face_sets_bmesh(object, ss, brush, radius);
  }

  BLI_assert_unreachable();
  return nullptr;
}

static std::optional<float3> calc_average_face_set_center(const Depsgraph &depsgraph,
                                                          Object &object,
                                                          const Span<int> floodfill_step,
                                                          const int active_face_set,
                                                          const int target_face_set)
{
  int count = 0;
  float3 sum(0.0f);

  switch (bke::object::pbvh_get(object)->type()) {
    case bke::pbvh::Type::Mesh: {
      const Mesh &mesh = *static_cast<Mesh *>(object.data);
      const GroupedSpan<int> vert_to_face_map = mesh.vert_to_face_map();
      const Span<float3> vert_positions = bke::pbvh::vert_positions_eval(depsgraph, object);
      const bke::AttributeAccessor attributes = mesh.attributes();
      const VArraySpan face_sets = *attributes.lookup_or_default<int>(
          ".sculpt_face_set", bke::AttrDomain::Face, 0);

      for (const int vert : vert_positions.index_range()) {
        if (floodfill_step[vert] != 0 &&
            face_set::vert_has_face_set(vert_to_face_map, face_sets, vert, active_face_set) &&
            face_set::vert_has_face_set(vert_to_face_map, face_sets, vert, target_face_set))
        {
          sum += vert_positions[vert];
          count++;
        }
      }
      break;
    }
    case bke::pbvh::Type::Grids: {
      const SubdivCCG &subdiv_ccg = *object.sculpt->subdiv_ccg;
      const Span<float3> positions = subdiv_ccg.positions;

      const Mesh &mesh = *static_cast<Mesh *>(object.data);
      const bke::AttributeAccessor attributes = mesh.attributes();
      const VArraySpan face_sets = *attributes.lookup_or_default<int>(
          ".sculpt_face_set", bke::AttrDomain::Face, 0);

      const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);
      for (const int grid : IndexRange(subdiv_ccg.grids_num)) {
        for (const int index : bke::ccg::grid_range(key, grid)) {
          if (floodfill_step[index] != 0 &&
              face_set::vert_has_face_set(subdiv_ccg, face_sets, grid, active_face_set) &&
              face_set::vert_has_face_set(subdiv_ccg, face_sets, grid, target_face_set))
          {
            sum += positions[index];
            count++;
          }
        }
      }
      break;
    }
    case bke::pbvh::Type::BMesh: {
      vert_random_access_ensure(object);
      BMesh &bm = *object.sculpt->bm;
      const int face_set_offset = CustomData_get_offset_named(
          &bm.pdata, CD_PROP_INT32, ".sculpt_face_set");
      for (const int vert : IndexRange(BM_mesh_elem_count(&bm, BM_VERT))) {
        const BMVert *bm_vert = BM_vert_at_index(&bm, vert);
        if (floodfill_step[vert] != 0 &&
            face_set::vert_has_face_set(face_set_offset, *bm_vert, active_face_set) &&
            face_set::vert_has_face_set(face_set_offset, *bm_vert, target_face_set))
        {
          sum += bm_vert->co;
          count++;
        }
      }

      break;
    }
  }

  if (count != 0) {
    return sum / float(count);
  }

  return std::nullopt;
}

static std::unique_ptr<IKChain> ik_chain_init_face_sets_fk_mesh(const Depsgraph &depsgraph,
                                                                Object &object,
                                                                SculptSession &ss,
                                                                const float radius,
                                                                const float3 &initial_location)
{
  const Mesh &mesh = *static_cast<Mesh *>(object.data);
  const GroupedSpan<int> vert_to_face_map = mesh.vert_to_face_map();
  const bke::AttributeAccessor attributes = mesh.attributes();
  const VArraySpan face_sets = *attributes.lookup_or_default<int>(
      ".sculpt_face_set", bke::AttrDomain::Face, 0);

  std::unique_ptr<IKChain> ik_chain = ik_chain_new(1, mesh.verts_num);

  const int active_vert = std::get<int>(ss.active_vert());

  const int active_face_set = face_set::active_face_set_get(object);

  Set<int> visited_face_sets;
  Array<int> floodfill_step(mesh.verts_num);
  floodfill_step[active_vert] = 1;

  int masked_face_set = SCULPT_FACE_SET_NONE;
  int target_face_set = SCULPT_FACE_SET_NONE;
  int masked_face_set_it = 0;
  flood_fill::FillDataMesh step_floodfill(mesh.verts_num, ss.fake_neighbors.fake_neighbor_index);
  step_floodfill.add_initial(active_vert);
  step_floodfill.execute(object, vert_to_face_map, [&](int from_v, int to_v) {
    floodfill_step[to_v] = floodfill_step[from_v] + 1;

    const int to_face_set = face_set::vert_face_set_get(vert_to_face_map, face_sets, to_v);
    if (!visited_face_sets.contains(to_face_set)) {
      if (face_set::vert_has_unique_face_set(vert_to_face_map, face_sets, to_v) &&
          !face_set::vert_has_unique_face_set(vert_to_face_map, face_sets, from_v) &&
          face_set::vert_has_face_set(vert_to_face_map, face_sets, from_v, to_face_set))
      {

        visited_face_sets.add(to_face_set);

        if (floodfill_step[to_v] >= masked_face_set_it) {
          masked_face_set = to_face_set;
          masked_face_set_it = floodfill_step[to_v];
        }

        if (target_face_set == SCULPT_FACE_SET_NONE) {
          target_face_set = to_face_set;
        }
      }
    }

    return face_set::vert_has_face_set(vert_to_face_map, face_sets, to_v, active_face_set);
  });

  const std::optional<float3> origin = calc_average_face_set_center(
      depsgraph, object, floodfill_step, active_face_set, masked_face_set);
  ik_chain->segments[0].orig = origin.value_or(float3(0));

  std::optional<float3> head = std::nullopt;
  if (target_face_set != masked_face_set) {
    head = calc_average_face_set_center(
        depsgraph, object, floodfill_step, active_face_set, target_face_set);
  }

  ik_chain->segments[0].head = head.value_or(initial_location);
  ik_chain->grab_delta_offset = ik_chain->segments[0].head - initial_location;

  flood_fill::FillDataMesh weight_floodfill(mesh.verts_num, ss.fake_neighbors.fake_neighbor_index);
  weight_floodfill.add_initial(find_symm_verts_mesh(depsgraph, object, active_vert, radius));
  MutableSpan<float> fk_weights = ik_chain->segments[0].weights;
  weight_floodfill.execute(object, vert_to_face_map, [&](int /*from_v*/, int to_v) {
    fk_weights[to_v] = 1.0f;
    return !face_set::vert_has_face_set(vert_to_face_map, face_sets, to_v, masked_face_set);
  });

  ik_chain_origin_heads_init(*ik_chain, ik_chain->segments[0].head);
  return ik_chain;
}

static std::unique_ptr<IKChain> ik_chain_init_face_sets_fk_grids(const Depsgraph &depsgraph,
                                                                 Object &object,
                                                                 SculptSession &ss,
                                                                 const float radius,
                                                                 const float3 &initial_location)
{
  const Mesh &mesh = *static_cast<const Mesh *>(object.data);
  const OffsetIndices<int> faces = mesh.faces();
  const Span<int> corner_verts = mesh.corner_verts();
  const GroupedSpan<int> vert_to_face_map = mesh.vert_to_face_map();
  const bke::AttributeAccessor attributes = mesh.attributes();
  const VArraySpan face_sets = *attributes.lookup_or_default<int>(
      ".sculpt_face_set", bke::AttrDomain::Face, 0);

  const SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;
  const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);
  const Span<int> grid_to_face_map = subdiv_ccg.grid_to_face_map;
  const int grids_num = ss.subdiv_ccg->grids_num * key.grid_area;

  std::unique_ptr<IKChain> ik_chain = ik_chain_new(1, grids_num);

  const int active_vert_index = ss.active_vert_index();

  const int active_face_set = face_set::active_face_set_get(object);

  Set<int> visited_face_sets;
  Array<int> floodfill_step(grids_num);
  floodfill_step[active_vert_index] = 1;

  int masked_face_set = SCULPT_FACE_SET_NONE;
  int target_face_set = SCULPT_FACE_SET_NONE;
  int masked_face_set_it = 0;
  flood_fill::FillDataGrids step_floodfill(grids_num, ss.fake_neighbors.fake_neighbor_index);
  step_floodfill.add_initial(SubdivCCGCoord::from_index(key, active_vert_index));
  step_floodfill.execute(
      object, subdiv_ccg, [&](SubdivCCGCoord from_v, SubdivCCGCoord to_v, bool is_duplicate) {
        const int from_v_i = from_v.to_index(key);
        const int to_v_i = to_v.to_index(key);

        if (!is_duplicate) {
          floodfill_step[to_v_i] = floodfill_step[from_v_i] + 1;
        }
        else {
          floodfill_step[to_v_i] = floodfill_step[from_v_i];
        }

        const int to_face_set = face_sets[grid_to_face_map[to_v.grid_index]];
        if (!visited_face_sets.contains(to_face_set)) {
          if (face_set::vert_has_unique_face_set(
                  faces, corner_verts, vert_to_face_map, face_sets, subdiv_ccg, to_v) &&
              !face_set::vert_has_unique_face_set(
                  faces, corner_verts, vert_to_face_map, face_sets, subdiv_ccg, from_v) &&
              face_set::vert_has_face_set(subdiv_ccg, face_sets, from_v.grid_index, to_face_set))
          {

            visited_face_sets.add(to_face_set);

            if (floodfill_step[to_v_i] >= masked_face_set_it) {
              masked_face_set = to_face_set;
              masked_face_set_it = floodfill_step[to_v_i];
            }

            if (target_face_set == SCULPT_FACE_SET_NONE) {
              target_face_set = to_face_set;
            }
          }
        }

        return face_set::vert_has_face_set(
            subdiv_ccg, face_sets, to_v.grid_index, active_face_set);
      });

  const std::optional<float3> origin = calc_average_face_set_center(
      depsgraph, object, floodfill_step, active_face_set, masked_face_set);
  ik_chain->segments[0].orig = origin.value_or(float3(0));

  std::optional<float3> head = std::nullopt;
  if (target_face_set != masked_face_set) {
    head = calc_average_face_set_center(
        depsgraph, object, floodfill_step, active_face_set, target_face_set);
  }

  ik_chain->segments[0].head = head.value_or(initial_location);
  ik_chain->grab_delta_offset = ik_chain->segments[0].head - initial_location;

  flood_fill::FillDataGrids weight_floodfill(grids_num, ss.fake_neighbors.fake_neighbor_index);
  weight_floodfill.add_initial(key, find_symm_verts_grids(object, active_vert_index, radius));
  MutableSpan<float> fk_weights = ik_chain->segments[0].weights;
  weight_floodfill.execute(
      object,
      subdiv_ccg,
      [&](SubdivCCGCoord /*from_v*/, SubdivCCGCoord to_v, bool /*is_duplicate*/) {
        int to_v_i = to_v.to_index(key);

        fk_weights[to_v_i] = 1.0f;
        return !face_set::vert_has_face_set(
            subdiv_ccg, face_sets, to_v.grid_index, masked_face_set);
      });

  ik_chain_origin_heads_init(*ik_chain, ik_chain->segments[0].head);
  return ik_chain;
}

static std::unique_ptr<IKChain> ik_chain_init_face_sets_fk_bmesh(const Depsgraph &depsgraph,
                                                                 Object &object,
                                                                 SculptSession &ss,
                                                                 const float radius,
                                                                 const float3 &initial_location)
{
  vert_random_access_ensure(object);

  BMesh &bm = *ss.bm;
  const int face_set_offset = CustomData_get_offset_named(
      &bm.pdata, CD_PROP_INT32, ".sculpt_face_set");
  const int verts_num = BM_mesh_elem_count(&bm, BM_VERT);

  std::unique_ptr<IKChain> ik_chain = ik_chain_new(1, verts_num);

  BMVert *active_vert = std::get<BMVert *>(ss.active_vert());
  const int active_vert_index = BM_elem_index_get(active_vert);

  const int active_face_set = face_set::active_face_set_get(object);

  Set<int> visited_face_sets;
  Array<int> floodfill_step(verts_num);
  floodfill_step[active_vert_index] = 1;

  int masked_face_set = SCULPT_FACE_SET_NONE;
  int target_face_set = SCULPT_FACE_SET_NONE;
  int masked_face_set_it = 0;
  flood_fill::FillDataBMesh step_floodfill(verts_num, ss.fake_neighbors.fake_neighbor_index);
  step_floodfill.add_initial(active_vert);
  step_floodfill.execute(object, [&](BMVert *from_v, BMVert *to_v) {
    const int from_v_i = BM_elem_index_get(from_v);
    const int to_v_i = BM_elem_index_get(to_v);

    floodfill_step[to_v_i] = floodfill_step[from_v_i] + 1;

    const int to_face_set = face_set::vert_face_set_get(face_set_offset, *to_v);
    if (!visited_face_sets.contains(to_face_set)) {
      if (face_set::vert_has_unique_face_set(face_set_offset, *to_v) &&
          !face_set::vert_has_unique_face_set(face_set_offset, *from_v) &&
          face_set::vert_has_face_set(face_set_offset, *from_v, to_face_set))
      {

        visited_face_sets.add(to_face_set);

        if (floodfill_step[to_v_i] >= masked_face_set_it) {
          masked_face_set = to_face_set;
          masked_face_set_it = floodfill_step[to_v_i];
        }

        if (target_face_set == SCULPT_FACE_SET_NONE) {
          target_face_set = to_face_set;
        }
      }
    }

    return face_set::vert_has_face_set(face_set_offset, *to_v, active_face_set);
  });

  const std::optional<float3> origin = calc_average_face_set_center(
      depsgraph, object, floodfill_step, active_face_set, masked_face_set);
  ik_chain->segments[0].orig = origin.value_or(float3(0));

  std::optional<float3> head = std::nullopt;
  if (target_face_set != masked_face_set) {
    head = calc_average_face_set_center(
        depsgraph, object, floodfill_step, active_face_set, target_face_set);
  }

  ik_chain->segments[0].head = head.value_or(initial_location);
  ik_chain->grab_delta_offset = ik_chain->segments[0].head - initial_location;

  flood_fill::FillDataBMesh weight_floodfill(verts_num, ss.fake_neighbors.fake_neighbor_index);
  weight_floodfill.add_initial(
      *ss.bm, find_symm_verts_bmesh(object, BM_elem_index_get(active_vert), radius));
  MutableSpan<float> fk_weights = ik_chain->segments[0].weights;
  weight_floodfill.execute(object, [&](BMVert * /*from_v*/, BMVert *to_v) {
    int to_v_i = BM_elem_index_get(to_v);

    fk_weights[to_v_i] = 1.0f;
    return !face_set::vert_has_face_set(face_set_offset, *to_v, masked_face_set);
  });

  ik_chain_origin_heads_init(*ik_chain, ik_chain->segments[0].head);
  return ik_chain;
}

static std::unique_ptr<IKChain> ik_chain_init_face_sets_fk(const Depsgraph &depsgraph,
                                                           Object &object,
                                                           SculptSession &ss,
                                                           const float radius,
                                                           const float3 &initial_location)
{
  switch (bke::object::pbvh_get(object)->type()) {
    case bke::pbvh::Type::Mesh:
      return ik_chain_init_face_sets_fk_mesh(depsgraph, object, ss, radius, initial_location);
    case bke::pbvh::Type::Grids:
      return ik_chain_init_face_sets_fk_grids(depsgraph, object, ss, radius, initial_location);
    case bke::pbvh::Type::BMesh:
      return ik_chain_init_face_sets_fk_bmesh(depsgraph, object, ss, radius, initial_location);
  }
  BLI_assert_unreachable();
  return nullptr;
}

static std::unique_ptr<IKChain> ik_chain_init(const Depsgraph &depsgraph,
                                              Object &ob,
                                              SculptSession &ss,
                                              const Brush &brush,
                                              const float3 &initial_location,
                                              const float radius)
{
  std::unique_ptr<IKChain> ik_chain;

  const bool use_fake_neighbors = !(brush.flag2 & BRUSH_USE_CONNECTED_ONLY);

  if (use_fake_neighbors) {
    SCULPT_fake_neighbors_ensure(depsgraph, ob, brush.disconnected_distance_max);
  }
  else {
    SCULPT_fake_neighbors_free(ob);
  }

  switch (brush.pose_origin_type) {
    case BRUSH_POSE_ORIGIN_TOPOLOGY:
      ik_chain = ik_chain_init_topology(depsgraph, ob, ss, brush, initial_location, radius);
      break;
    case BRUSH_POSE_ORIGIN_FACE_SETS:
      ik_chain = ik_chain_init_face_sets(depsgraph, ob, ss, brush, radius);
      break;
    case BRUSH_POSE_ORIGIN_FACE_SETS_FK:
      ik_chain = ik_chain_init_face_sets_fk(depsgraph, ob, ss, radius, initial_location);
      break;
  }

  return ik_chain;
}

static void pose_brush_init(const Depsgraph &depsgraph,
                            Object &ob,
                            SculptSession &ss,
                            const Brush &brush)
{
  /* Init the IK chain that is going to be used to deform the vertices. */
  ss.cache->pose_ik_chain = ik_chain_init(
      depsgraph, ob, ss, brush, ss.cache->location, ss.cache->radius);

  /* Smooth the weights of each segment for cleaner deformation. */
  for (IKChainSegment &segment : ss.cache->pose_ik_chain->segments) {
    smooth::blur_geometry_data_array(ob, brush.pose_smooth_iterations, segment.weights);
  }
}

std::unique_ptr<SculptPoseIKChainPreview> preview_ik_chain_init(const Depsgraph &depsgraph,
                                                                Object &ob,
                                                                SculptSession &ss,
                                                                const Brush &brush,
                                                                const float3 &initial_location,
                                                                const float radius)
{
  const IKChain chain = *ik_chain_init(depsgraph, ob, ss, brush, initial_location, radius);
  std::unique_ptr<SculptPoseIKChainPreview> preview = std::make_unique<SculptPoseIKChainPreview>();

  preview->initial_head_coords.reinitialize(chain.segments.size());
  preview->initial_orig_coords.reinitialize(chain.segments.size());
  for (const int i : chain.segments.index_range()) {
    preview->initial_head_coords[i] = chain.segments[i].initial_head;
    preview->initial_orig_coords[i] = chain.segments[i].initial_orig;
  }

  return preview;
}

static void sculpt_pose_do_translate_deform(SculptSession &ss, const Brush &brush)
{
  IKChain &ik_chain = *ss.cache->pose_ik_chain;
  BKE_curvemapping_init(brush.curve_distance_falloff);
  solve_translate_chain(ik_chain, ss.cache->grab_delta);
}

/* Calculate a scale factor based on the grab delta. */
static float calc_scale_from_grab_delta(SculptSession &ss, const float3 &ik_target)
{
  IKChain &ik_chain = *ss.cache->pose_ik_chain;
  const float3 segment_dir = math::normalize(ik_chain.segments[0].initial_head -
                                             ik_chain.segments[0].initial_orig);
  float4 plane;
  plane_from_point_normal_v3(plane, ik_chain.segments[0].initial_head, segment_dir);
  const float segment_len = ik_chain.segments[0].len;
  return segment_len / (segment_len - dist_signed_to_plane_v3(ik_target, plane));
}

static void calc_scale_deform(SculptSession &ss, const Brush &brush)
{
  IKChain &ik_chain = *ss.cache->pose_ik_chain;

  float3 ik_target = ss.cache->location + ss.cache->grab_delta;

  /* Solve the IK for the first segment to include rotation as part of scale if enabled. */
  if (!(brush.flag2 & BRUSH_POSE_USE_LOCK_ROTATION)) {
    solve_ik_chain(ik_chain, ik_target, brush.flag2 & BRUSH_POSE_IK_ANCHORED);
  }

  float3 scale(calc_scale_from_grab_delta(ss, ik_target));

  /* Write the scale into the segments. */
  solve_scale_chain(ik_chain, scale);
}

static void calc_twist_deform(SculptSession &ss, const Brush &brush)
{
  IKChain &ik_chain = *ss.cache->pose_ik_chain;

  /* Calculate the maximum roll. 0.02 radians per pixel works fine. */
  float roll = (ss.cache->initial_mouse[0] - ss.cache->mouse[0]) * ss.cache->bstrength * 0.02f;
  BKE_curvemapping_init(brush.curve_distance_falloff);
  solve_roll_chain(ik_chain, brush, roll);
}

static void calc_rotate_deform(SculptSession &ss, const Brush &brush)
{
  IKChain &ik_chain = *ss.cache->pose_ik_chain;

  /* Calculate the IK target. */
  float3 ik_target = ss.cache->location + ss.cache->grab_delta + ik_chain.grab_delta_offset;

  /* Solve the IK positions. */
  solve_ik_chain(ik_chain, ik_target, brush.flag2 & BRUSH_POSE_IK_ANCHORED);
}

static void calc_rotate_twist_deform(SculptSession &ss, const Brush &brush)
{
  if (ss.cache->invert) {
    calc_twist_deform(ss, brush);
  }
  else {
    calc_rotate_deform(ss, brush);
  }
}

static void calc_scale_translate_deform(SculptSession &ss, const Brush &brush)
{
  if (ss.cache->invert) {
    sculpt_pose_do_translate_deform(ss, brush);
  }
  else {
    calc_scale_deform(ss, brush);
  }
}

static void calc_squash_stretch_deform(SculptSession &ss, const Brush & /*brush*/)
{
  IKChain &ik_chain = *ss.cache->pose_ik_chain;

  float3 ik_target = ss.cache->location + ss.cache->grab_delta;

  float3 scale;
  scale[2] = calc_scale_from_grab_delta(ss, ik_target);
  scale[0] = scale[1] = sqrtf(1.0f / scale[2]);

  /* Write the scale into the segments. */
  solve_scale_chain(ik_chain, scale);
}

static void align_pivot_local_space(float r_mat[4][4],
                                    ePaintSymmetryFlags symm,
                                    ePaintSymmetryAreas symm_area,
                                    IKChainSegment *segment,
                                    const float3 &grab_location)
{
  const float3 symm_head = SCULPT_flip_v3_by_symm_area(
      segment->head, symm, symm_area, grab_location);
  const float3 symm_orig = SCULPT_flip_v3_by_symm_area(
      segment->orig, symm, symm_area, grab_location);

  float3 segment_origin_head = math::normalize(symm_head - symm_orig);

  copy_v3_v3(r_mat[2], segment_origin_head);
  ortho_basis_v3v3_v3(r_mat[0], r_mat[1], r_mat[2]);
}

void do_pose_brush(const Depsgraph &depsgraph,
                   const Sculpt &sd,
                   Object &ob,
                   const IndexMask &node_mask)
{
  SculptSession &ss = *ob.sculpt;
  bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(ob);
  const Brush &brush = *BKE_paint_brush_for_read(&sd.paint);
  const ePaintSymmetryFlags symm = SCULPT_mesh_symmetry_xyz_get(ob);

  if (!ss.cache->pose_ik_chain) {
    pose_brush_init(depsgraph, ob, ss, brush);
  }

  /* The pose brush applies all enabled symmetry axis in a single iteration, so the rest can be
   * ignored. */
  if (ss.cache->mirror_symmetry_pass != 0) {
    return;
  }

  IKChain &ik_chain = *ss.cache->pose_ik_chain;

  switch (brush.pose_deform_type) {
    case BRUSH_POSE_DEFORM_ROTATE_TWIST:
      calc_rotate_twist_deform(ss, brush);
      break;
    case BRUSH_POSE_DEFORM_SCALE_TRASLATE:
      calc_scale_translate_deform(ss, brush);
      break;
    case BRUSH_POSE_DEFORM_SQUASH_STRETCH:
      calc_squash_stretch_deform(ss, brush);
      break;
  }

  /* Flip the segment chain in all symmetry axis and calculate the transform matrices for each
   * possible combination. */
  /* This can be optimized by skipping the calculation of matrices where the symmetry is not
   * enabled. */
  for (int symm_it = 0; symm_it < PAINT_SYMM_AREAS; symm_it++) {
    for (const int i : ik_chain.segments.index_range()) {
      ePaintSymmetryAreas symm_area = ePaintSymmetryAreas(symm_it);

      float symm_rot[4];
      copy_qt_qt(symm_rot, ik_chain.segments[i].rot);

      /* Flip the origins and rotation quats of each segment. */
      SCULPT_flip_quat_by_symm_area(symm_rot, symm, symm_area, ss.cache->orig_grab_location);
      float3 symm_orig = SCULPT_flip_v3_by_symm_area(
          ik_chain.segments[i].orig, symm, symm_area, ss.cache->orig_grab_location);
      float3 symm_initial_orig = SCULPT_flip_v3_by_symm_area(
          ik_chain.segments[i].initial_orig, symm, symm_area, ss.cache->orig_grab_location);

      float pivot_local_space[4][4];
      unit_m4(pivot_local_space);

      /* Align the segment pivot local space to the Z axis. */
      if (brush.pose_deform_type == BRUSH_POSE_DEFORM_SQUASH_STRETCH) {
        align_pivot_local_space(pivot_local_space,
                                symm,
                                symm_area,
                                &ik_chain.segments[i],
                                ss.cache->orig_grab_location);
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

  threading::EnumerableThreadSpecific<BrushLocalData> all_tls;
  switch (pbvh.type()) {
    case bke::pbvh::Type::Mesh: {
      Mesh &mesh = *static_cast<Mesh *>(ob.data);
      const MeshAttributeData attribute_data(mesh);
      MutableSpan<bke::pbvh::MeshNode> nodes = pbvh.nodes<bke::pbvh::MeshNode>();
      const PositionDeformData position_data(depsgraph, ob);
      node_mask.foreach_index(GrainSize(1), [&](const int i) {
        BrushLocalData &tls = all_tls.local();
        calc_mesh(depsgraph, sd, brush, attribute_data, nodes[i], ob, tls, position_data);
        bke::pbvh::update_node_bounds_mesh(position_data.eval, nodes[i]);
      });
      break;
    }
    case bke::pbvh::Type::Grids: {
      SubdivCCG &subdiv_ccg = *ob.sculpt->subdiv_ccg;
      MutableSpan<float3> positions = subdiv_ccg.positions;
      MutableSpan<bke::pbvh::GridsNode> nodes = pbvh.nodes<bke::pbvh::GridsNode>();
      node_mask.foreach_index(GrainSize(1), [&](const int i) {
        BrushLocalData &tls = all_tls.local();
        calc_grids(depsgraph, sd, brush, nodes[i], ob, tls);
        bke::pbvh::update_node_bounds_grids(subdiv_ccg.grid_area, positions, nodes[i]);
      });
      break;
    }
    case bke::pbvh::Type::BMesh: {
      MutableSpan<bke::pbvh::BMeshNode> nodes = pbvh.nodes<bke::pbvh::BMeshNode>();
      node_mask.foreach_index(GrainSize(1), [&](const int i) {
        BrushLocalData &tls = all_tls.local();
        calc_bmesh(depsgraph, sd, brush, nodes[i], ob, tls);
        bke::pbvh::update_node_bounds_bmesh(nodes[i]);
      });
      break;
    }
  }
  pbvh.tag_positions_changed(node_mask);
  pbvh.flush_bounds_to_parents();
}

}  // namespace blender::ed::sculpt_paint::pose
