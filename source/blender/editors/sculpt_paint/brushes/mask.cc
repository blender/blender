/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "editors/sculpt_paint/brushes/brushes.hh"

#include "DNA_brush_types.h"
#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_attribute.hh"
#include "BKE_paint_bvh.hh"
#include "BKE_subdiv_ccg.hh"

#include "BLI_enumerable_thread_specific.hh"
#include "BLI_span.hh"

#include "editors/sculpt_paint/mesh_brush_common.hh"
#include "editors/sculpt_paint/paint_intern.hh"
#include "editors/sculpt_paint/paint_mask.hh"
#include "editors/sculpt_paint/sculpt_automask.hh"
#include "editors/sculpt_paint/sculpt_intern.hh"

#include "bmesh.hh"

namespace blender::ed::sculpt_paint::brushes {
inline namespace mask_cc {

struct LocalData {
  Vector<float3> positions;
  Vector<float> factors;
  Vector<float> distances;
  Vector<float> current_masks;
  Vector<float> new_masks;
};

BLI_NOINLINE static void apply_factors(const float strength,
                                       const Span<float> current_masks,
                                       const Span<float> factors,
                                       const MutableSpan<float> masks)
{
  BLI_assert(current_masks.size() == masks.size());
  BLI_assert(factors.size() == masks.size());
  for (const int i : masks.index_range()) {
    masks[i] += factors[i] * current_masks[i] * strength;
  }
}

BLI_NOINLINE static void clamp_mask(const MutableSpan<float> masks)
{
  for (float &mask : masks) {
    mask = std::clamp(mask, 0.0f, 1.0f);
  }
}

static void calc_faces(const Depsgraph &depsgraph,
                       const Brush &brush,
                       const float strength,
                       const Span<float3> positions,
                       const Span<float3> vert_normals,
                       const bke::pbvh::MeshNode &node,
                       Object &object,
                       const Span<bool> hide_vert,
                       LocalData &tls,
                       const MutableSpan<float> mask)
{
  SculptSession &ss = *object.sculpt;
  const StrokeCache &cache = *ss.cache;

  const Span<int> verts = node.verts();

  tls.factors.resize(verts.size());
  const MutableSpan<float> factors = tls.factors;
  fill_factor_from_hide(hide_vert, verts, factors);
  filter_region_clip_factors(ss, positions, verts, factors);
  if (brush.flag & BRUSH_FRONTFACE) {
    calc_front_face(cache.view_normal_symm, vert_normals, verts, factors);
  }

  tls.distances.resize(verts.size());
  const MutableSpan<float> distances = tls.distances;
  calc_brush_distances(ss, positions, verts, eBrushFalloffShape(brush.falloff_shape), distances);
  filter_distances_with_radius(cache.radius, distances, factors);
  apply_hardness_to_distances(cache, distances);
  calc_brush_strength_factors(cache, brush, distances, factors);

  auto_mask::calc_vert_factors(depsgraph, object, cache.automasking.get(), node, verts, factors);

  calc_brush_texture_factors(ss, brush, positions, verts, factors);

  tls.new_masks.resize(verts.size());
  const MutableSpan<float> new_masks = tls.new_masks;
  gather_data_mesh(mask.as_span(), verts, new_masks);

  tls.current_masks = tls.new_masks;
  const MutableSpan<float> current_masks = tls.current_masks;
  if (strength > 0.0f) {
    mask::invert_mask(current_masks);
  }
  apply_factors(strength, current_masks, factors, new_masks);
  clamp_mask(new_masks);

  scatter_data_mesh(new_masks.as_span(), verts, mask);
}

static void calc_grids(const Depsgraph &depsgraph,
                       Object &object,
                       const Brush &brush,
                       const float strength,
                       bke::pbvh::GridsNode &node,
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

  auto_mask::calc_grids_factors(depsgraph, object, cache.automasking.get(), node, grids, factors);

  calc_brush_texture_factors(ss, brush, positions, factors);

  tls.new_masks.resize(positions.size());
  const MutableSpan<float> new_masks = tls.new_masks;
  mask::gather_mask_grids(subdiv_ccg, grids, new_masks);

  tls.current_masks = tls.new_masks;
  const MutableSpan<float> current_masks = tls.current_masks;
  if (strength > 0.0f) {
    mask::invert_mask(current_masks);
  }
  apply_factors(strength, current_masks, factors, new_masks);
  clamp_mask(new_masks);

  mask::scatter_mask_grids(new_masks.as_span(), subdiv_ccg, grids);
}

static void calc_bmesh(const Depsgraph &depsgraph,
                       Object &object,
                       const Brush &brush,
                       const float strength,
                       bke::pbvh::BMeshNode &node,
                       LocalData &tls)
{
  SculptSession &ss = *object.sculpt;
  const BMesh &bm = *ss.bm;
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

  auto_mask::calc_vert_factors(depsgraph, object, cache.automasking.get(), node, verts, factors);

  calc_brush_texture_factors(ss, brush, positions, factors);

  tls.new_masks.resize(verts.size());
  const MutableSpan<float> new_masks = tls.new_masks;
  mask::gather_mask_bmesh(bm, verts, new_masks);

  tls.current_masks = tls.new_masks;
  const MutableSpan<float> current_masks = tls.current_masks;
  if (strength > 0.0f) {
    mask::invert_mask(current_masks);
  }
  apply_factors(strength, current_masks, factors, new_masks);
  clamp_mask(new_masks);

  mask::scatter_mask_bmesh(new_masks.as_span(), bm, verts);
}

}  // namespace mask_cc

void do_mask_brush(const Depsgraph &depsgraph,
                   const Sculpt &sd,
                   Object &object,
                   const IndexMask &node_mask)
{
  SculptSession &ss = *object.sculpt;
  bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
  const Brush &brush = *BKE_paint_brush_for_read(&sd.paint);
  const float bstrength = ss.cache->bstrength;

  threading::EnumerableThreadSpecific<LocalData> all_tls;
  switch (pbvh.type()) {
    case blender::bke::pbvh::Type::Mesh: {
      MutableSpan<bke::pbvh::MeshNode> nodes = pbvh.nodes<bke::pbvh::MeshNode>();
      Mesh &mesh = *static_cast<Mesh *>(object.data);
      const Span<float3> positions = bke::pbvh::vert_positions_eval(depsgraph, object);
      const Span<float3> vert_normals = bke::pbvh::vert_normals_eval(depsgraph, object);

      bke::MutableAttributeAccessor attributes = mesh.attributes_for_write();

      bke::SpanAttributeWriter<float> mask = attributes.lookup_or_add_for_write_span<float>(
          ".sculpt_mask", bke::AttrDomain::Point);
      const VArraySpan hide_vert = *attributes.lookup<bool>(".hide_vert", bke::AttrDomain::Point);

      node_mask.foreach_index(GrainSize(1), [&](const int i) {
        LocalData &tls = all_tls.local();
        calc_faces(depsgraph,
                   brush,
                   bstrength,
                   positions,
                   vert_normals,
                   nodes[i],
                   object,
                   hide_vert,
                   tls,
                   mask.span);
        bke::pbvh::node_update_mask_mesh(mask.span, nodes[i]);
      });
      mask.finish();
      break;
    }
    case blender::bke::pbvh::Type::Grids: {
      SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;
      const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);
      MutableSpan<float> masks = subdiv_ccg.masks;
      MutableSpan<bke::pbvh::GridsNode> nodes = pbvh.nodes<bke::pbvh::GridsNode>();
      node_mask.foreach_index(GrainSize(1), [&](const int i) {
        LocalData &tls = all_tls.local();
        calc_grids(depsgraph, object, brush, bstrength, nodes[i], tls);
        bke::pbvh::node_update_mask_grids(key, masks, nodes[i]);
      });
      break;
    }
    case blender::bke::pbvh::Type::BMesh: {
      const int mask_offset = CustomData_get_offset_named(
          &ss.bm->vdata, CD_PROP_FLOAT, ".sculpt_mask");
      MutableSpan<bke::pbvh::BMeshNode> nodes = pbvh.nodes<bke::pbvh::BMeshNode>();
      node_mask.foreach_index(GrainSize(1), [&](const int i) {
        LocalData &tls = all_tls.local();
        calc_bmesh(depsgraph, object, brush, bstrength, nodes[i], tls);
        bke::pbvh::node_update_mask_bmesh(mask_offset, nodes[i]);
      });
      break;
    }
  }
  pbvh.tag_masks_changed(node_mask);
}

}  // namespace blender::ed::sculpt_paint::brushes
