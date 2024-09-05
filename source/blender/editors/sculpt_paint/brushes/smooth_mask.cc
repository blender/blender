/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "editors/sculpt_paint/brushes/types.hh"

#include "DNA_brush_types.h"

#include "BKE_attribute.hh"
#include "BKE_mesh.hh"
#include "BKE_subdiv_ccg.hh"

#include "BLI_enumerable_thread_specific.hh"
#include "BLI_math_base.hh"

#include "editors/sculpt_paint/mesh_brush_common.hh"
#include "editors/sculpt_paint/paint_intern.hh"
#include "editors/sculpt_paint/paint_mask.hh"
#include "editors/sculpt_paint/sculpt_boundary.hh"
#include "editors/sculpt_paint/sculpt_intern.hh"
#include "editors/sculpt_paint/sculpt_smooth.hh"

namespace blender::ed::sculpt_paint {

inline namespace smooth_mask_cc {

struct LocalData {
  Vector<float3> positions;
  Vector<float> factors;
  Vector<float> distances;
  Vector<Vector<int>> vert_neighbors;
  Vector<float> masks;
  Vector<float> new_masks;
};

/* TODO: Extract this and the similarly named smooth.cc function
 * to a common location. */
static Vector<float> iteration_strengths(const float strength)
{
  constexpr int max_iterations = 4;

  BLI_assert_msg(strength >= 0.0f,
                 "The smooth brush expects a non-negative strength to behave properly");
  const float clamped_strength = std::min(strength, 1.0f);

  const int count = int(clamped_strength * max_iterations);
  const float last = max_iterations * (clamped_strength - float(count) / max_iterations);
  Vector<float> result;
  result.append_n_times(1.0f, count);
  result.append(last);
  return result;
}

static void calc_smooth_masks_faces(const OffsetIndices<int> faces,
                                    const Span<int> corner_verts,
                                    const GroupedSpan<int> vert_to_face_map,
                                    const Span<bool> hide_poly,
                                    const Span<int> verts,
                                    const Span<float> masks,
                                    LocalData &tls,
                                    const MutableSpan<float> new_masks)
{
  tls.vert_neighbors.resize(verts.size());
  calc_vert_neighbors(faces, corner_verts, vert_to_face_map, hide_poly, verts, tls.vert_neighbors);
  const Span<Vector<int>> vert_neighbors = tls.vert_neighbors;
  smooth::neighbor_data_average_mesh(masks, vert_neighbors, new_masks);
}

static void apply_masks_faces(const Depsgraph &depsgraph,
                              const Brush &brush,
                              const Span<float3> positions_eval,
                              const Span<float3> vert_normals,
                              const bke::pbvh::MeshNode &node,
                              const float strength,
                              Object &object,
                              LocalData &tls,
                              const Span<float> mask_averages,
                              MutableSpan<float> mask)
{
  SculptSession &ss = *object.sculpt;
  const StrokeCache &cache = *ss.cache;
  const Mesh &mesh = *static_cast<Mesh *>(object.data);

  const Span<int> verts = node.verts();

  tls.factors.resize(verts.size());
  const MutableSpan<float> factors = tls.factors;
  fill_factor_from_hide(mesh, verts, factors);
  filter_region_clip_factors(ss, positions_eval, verts, factors);
  if (brush.flag & BRUSH_FRONTFACE) {
    calc_front_face(cache.view_normal_symm, vert_normals, verts, factors);
  }

  tls.distances.resize(verts.size());
  const MutableSpan<float> distances = tls.distances;
  calc_brush_distances(
      ss, positions_eval, verts, eBrushFalloffShape(brush.falloff_shape), distances);
  filter_distances_with_radius(cache.radius, distances, factors);
  apply_hardness_to_distances(cache, distances);
  calc_brush_strength_factors(cache, brush, distances, factors);

  auto_mask::calc_vert_factors(
      depsgraph, object, ss.cache->automasking.get(), node, verts, factors);

  scale_factors(factors, strength);

  calc_brush_texture_factors(ss, brush, positions_eval, verts, factors);

  tls.new_masks.resize(verts.size());
  const MutableSpan<float> new_masks = tls.new_masks;
  gather_data_mesh(mask.as_span(), verts, new_masks);

  mask::mix_new_masks(mask_averages, factors, new_masks);
  mask::clamp_mask(new_masks);

  scatter_data_mesh(new_masks.as_span(), verts, mask);
}

static void do_smooth_brush_mesh(const Depsgraph &depsgraph,
                                 const Brush &brush,
                                 Object &object,
                                 const IndexMask &node_mask,
                                 const float brush_strength)
{
  const SculptSession &ss = *object.sculpt;
  MutableSpan<bke::pbvh::MeshNode> nodes = ss.pbvh->nodes<bke::pbvh::MeshNode>();
  Mesh &mesh = *static_cast<Mesh *>(object.data);
  const OffsetIndices faces = mesh.faces();
  const Span<int> corner_verts = mesh.corner_verts();
  const bke::AttributeAccessor attributes = mesh.attributes();
  const VArraySpan hide_poly = *attributes.lookup<bool>(".hide_poly", bke::AttrDomain::Face);

  const Span<float3> positions_eval = bke::pbvh::vert_positions_eval(depsgraph, object);
  const Span<float3> vert_normals = bke::pbvh::vert_normals_eval(depsgraph, object);

  Array<int> node_vert_offset_data;
  OffsetIndices node_vert_offsets = create_node_vert_offsets(
      nodes, node_mask, node_vert_offset_data);
  Array<float> new_masks(node_vert_offsets.total_size());

  bke::MutableAttributeAccessor write_attributes = mesh.attributes_for_write();

  bke::SpanAttributeWriter<float> mask = write_attributes.lookup_for_write_span<float>(
      ".sculpt_mask");

  threading::EnumerableThreadSpecific<LocalData> all_tls;
  for (const float strength : iteration_strengths(brush_strength)) {
    /* Calculate new masks into a separate array to avoid non-threadsafe access of values from
     * neighboring nodes. */
    node_mask.foreach_index(GrainSize(1), [&](const int i, const int pos) {
      LocalData &tls = all_tls.local();
      calc_smooth_masks_faces(faces,
                              corner_verts,
                              ss.vert_to_face_map,
                              hide_poly,
                              nodes[i].verts(),
                              mask.span.as_span(),
                              tls,
                              new_masks.as_mutable_span().slice(node_vert_offsets[pos]));
    });

    node_mask.foreach_index(GrainSize(1), [&](const int i, const int pos) {
      LocalData &tls = all_tls.local();
      apply_masks_faces(depsgraph,
                        brush,
                        positions_eval,
                        vert_normals,
                        nodes[i],
                        strength,
                        object,
                        tls,
                        new_masks.as_span().slice(node_vert_offsets[pos]),
                        mask.span);
    });
  }
  mask.finish();
}

static void calc_grids(const Depsgraph &depsgraph,
                       Object &object,
                       const Brush &brush,
                       const float strength,
                       const bke::pbvh::GridsNode &node,
                       LocalData &tls)
{
  SculptSession &ss = *object.sculpt;
  const StrokeCache &cache = *ss.cache;
  SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;

  const Span<int> grids = node.grids();
  const MutableSpan positions = gather_grids_positions(subdiv_ccg, grids, tls.positions);

  tls.factors.resize(positions.size());
  const MutableSpan<float> factors = tls.factors;
  fill_factor_from_hide(subdiv_ccg, grids, factors);
  filter_region_clip_factors(ss, positions, factors);
  if (brush.flag & BRUSH_FRONTFACE) {
    calc_front_face(cache.view_normal_symm, subdiv_ccg, grids, factors);
  }

  tls.distances.resize(positions.size());
  const MutableSpan<float> distances = tls.distances;
  calc_brush_distances(ss, positions, eBrushFalloffShape(brush.falloff_shape), distances);
  filter_distances_with_radius(cache.radius, distances, factors);
  apply_hardness_to_distances(cache, distances);
  calc_brush_strength_factors(cache, brush, distances, factors);

  if (ss.cache->automasking) {
    auto_mask::calc_grids_factors(
        depsgraph, object, cache.automasking.get(), node, grids, factors);
  }

  scale_factors(factors, strength);

  calc_brush_texture_factors(ss, brush, positions, factors);

  tls.masks.resize(positions.size());
  const MutableSpan<float> masks = tls.masks;
  mask::gather_mask_grids(subdiv_ccg, grids, masks);

  tls.new_masks.resize(positions.size());
  const MutableSpan<float> new_masks = tls.new_masks;
  mask::average_neighbor_mask_grids(subdiv_ccg, grids, new_masks);

  mask::mix_new_masks(new_masks, factors, masks);
  mask::clamp_mask(masks);

  mask::scatter_mask_grids(masks, subdiv_ccg, grids);
}

static void calc_bmesh(const Depsgraph &depsgraph,
                       Object &object,
                       const int mask_offset,
                       const Brush &brush,
                       const float strength,
                       bke::pbvh::BMeshNode &node,
                       LocalData &tls)
{
  SculptSession &ss = *object.sculpt;
  const StrokeCache &cache = *ss.cache;

  const Set<BMVert *, 0> &verts = BKE_pbvh_bmesh_node_unique_verts(&node);

  const MutableSpan positions = gather_bmesh_positions(verts, tls.positions);

  tls.factors.resize(verts.size());
  const MutableSpan<float> factors = tls.factors;
  fill_factor_from_hide(verts, factors);
  filter_region_clip_factors(ss, positions, factors);
  if (brush.flag & BRUSH_FRONTFACE) {
    calc_front_face(cache.view_normal_symm, verts, factors);
  }

  tls.distances.resize(verts.size());
  const MutableSpan<float> distances = tls.distances;
  calc_brush_distances(ss, positions, eBrushFalloffShape(brush.falloff_shape), distances);
  filter_distances_with_radius(cache.radius, distances, factors);
  apply_hardness_to_distances(cache, distances);
  calc_brush_strength_factors(cache, brush, distances, factors);

  if (ss.cache->automasking) {
    auto_mask::calc_vert_factors(depsgraph, object, cache.automasking.get(), node, verts, factors);
  }

  scale_factors(factors, strength);

  calc_brush_texture_factors(ss, brush, positions, factors);

  tls.masks.resize(verts.size());
  const MutableSpan<float> masks = tls.masks;
  mask::gather_mask_bmesh(*ss.bm, verts, masks);

  tls.new_masks.resize(verts.size());
  const MutableSpan<float> new_masks = tls.new_masks;
  mask::average_neighbor_mask_bmesh(mask_offset, verts, new_masks);

  mask::mix_new_masks(new_masks, factors, masks);
  mask::clamp_mask(masks);

  mask::scatter_mask_bmesh(masks, *ss.bm, verts);
}

}  // namespace smooth_mask_cc

void do_smooth_mask_brush(const Depsgraph &depsgraph,
                          const Sculpt &sd,
                          Object &object,
                          const IndexMask &node_mask,
                          float brush_strength)
{
  SculptSession &ss = *object.sculpt;
  const Brush &brush = *BKE_paint_brush_for_read(&sd.paint);
  boundary::ensure_boundary_info(object);
  switch (ss.pbvh->type()) {
    case bke::pbvh::Type::Mesh: {
      do_smooth_brush_mesh(depsgraph, brush, object, node_mask, brush_strength);
      break;
    }
    case bke::pbvh::Type::Grids: {
      threading::EnumerableThreadSpecific<LocalData> all_tls;
      for (const float strength : iteration_strengths(brush_strength)) {
        MutableSpan<bke::pbvh::GridsNode> nodes = ss.pbvh->nodes<bke::pbvh::GridsNode>();
        threading::parallel_for(node_mask.index_range(), 1, [&](const IndexRange range) {
          LocalData &tls = all_tls.local();
          node_mask.slice(range).foreach_index(
              [&](const int i) { calc_grids(depsgraph, object, brush, strength, nodes[i], tls); });
        });
      }
      break;
    }
    case bke::pbvh::Type::BMesh: {
      threading::EnumerableThreadSpecific<LocalData> all_tls;
      BM_mesh_elem_index_ensure(ss.bm, BM_VERT);
      BM_mesh_elem_table_ensure(ss.bm, BM_VERT);
      const int mask_offset = CustomData_get_offset_named(
          &ss.bm->vdata, CD_PROP_FLOAT, ".sculpt_mask");
      for (const float strength : iteration_strengths(brush_strength)) {
        MutableSpan<bke::pbvh::BMeshNode> nodes = ss.pbvh->nodes<bke::pbvh::BMeshNode>();
        threading::parallel_for(node_mask.index_range(), 1, [&](const IndexRange range) {
          LocalData &tls = all_tls.local();
          node_mask.slice(range).foreach_index([&](const int i) {
            calc_bmesh(depsgraph, object, mask_offset, brush, strength, nodes[i], tls);
          });
        });
      }
      break;
    }
  }
}
}  // namespace blender::ed::sculpt_paint
