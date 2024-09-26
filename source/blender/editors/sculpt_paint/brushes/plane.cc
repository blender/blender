/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edsculpt
 *
 * "Plane" related brushes, all three of these perform a similar displacement with an optional
 * additional filtering step.
 */

#include "editors/sculpt_paint/brushes/types.hh"

#include "DNA_brush_types.h"
#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_mesh.hh"
#include "BKE_paint.hh"
#include "BKE_pbvh.hh"
#include "BKE_subdiv_ccg.hh"

#include "BLI_array.hh"
#include "BLI_enumerable_thread_specific.hh"
#include "BLI_math_geom.h"
#include "BLI_math_matrix.hh"
#include "BLI_math_vector.hh"
#include "BLI_task.h"
#include "BLI_task.hh"

#include "editors/sculpt_paint/mesh_brush_common.hh"
#include "editors/sculpt_paint/sculpt_automask.hh"
#include "editors/sculpt_paint/sculpt_intern.hh"

namespace blender::ed::sculpt_paint {

inline namespace flatten_cc {

using IndexedFilterFn =
    FunctionRef<void(Span<float3>, Span<int>, const float4 &, MutableSpan<float>)>;
using GenericFilterFn = FunctionRef<void(Span<float3>, const float4 &, MutableSpan<float>)>;

struct LocalData {
  Vector<float3> positions;
  Vector<float> factors;
  Vector<float> distances;
  Vector<float3> translations;
};

static void calc_faces(const Depsgraph &depsgraph,
                       const Sculpt &sd,
                       const Brush &brush,
                       const float4 &plane,
                       const float strength,
                       const Span<float3> vert_normals,
                       const bke::pbvh::MeshNode &node,
                       Object &object,
                       LocalData &tls,
                       const PositionDeformData &position_data,
                       const IndexedFilterFn filter)
{
  SculptSession &ss = *object.sculpt;
  const StrokeCache &cache = *ss.cache;
  Mesh &mesh = *static_cast<Mesh *>(object.data);

  const Span<int> verts = node.verts();

  tls.factors.resize(verts.size());
  const MutableSpan<float> factors = tls.factors;
  fill_factor_from_hide_and_mask(mesh, verts, factors);
  filter_region_clip_factors(ss, position_data.eval, verts, factors);
  if (brush.flag & BRUSH_FRONTFACE) {
    calc_front_face(cache.view_normal_symm, vert_normals, verts, factors);
  }

  tls.distances.resize(verts.size());
  const MutableSpan<float> distances = tls.distances;
  calc_brush_distances(
      ss, position_data.eval, verts, eBrushFalloffShape(brush.falloff_shape), distances);
  filter_distances_with_radius(cache.radius, distances, factors);
  apply_hardness_to_distances(cache, distances);
  calc_brush_strength_factors(cache, brush, distances, factors);

  auto_mask::calc_vert_factors(depsgraph, object, cache.automasking.get(), node, verts, factors);

  calc_brush_texture_factors(ss, brush, position_data.eval, verts, factors);

  scale_factors(factors, strength);

  filter(position_data.eval, verts, plane, factors);

  tls.translations.resize(verts.size());
  const MutableSpan<float3> translations = tls.translations;
  calc_translations_to_plane(position_data.eval, verts, plane, translations);
  filter_plane_trim_limit_factors(brush, cache, translations, factors);
  scale_translations(translations, factors);

  clip_and_lock_translations(sd, ss, position_data.eval, verts, translations);
  position_data.deform(translations, verts);
}

static void calc_grids(const Depsgraph &depsgraph,
                       const Sculpt &sd,
                       Object &object,
                       const Brush &brush,
                       const float4 &plane,
                       const float strength,
                       bke::pbvh::GridsNode &node,
                       LocalData &tls,
                       const GenericFilterFn filter)
{
  SculptSession &ss = *object.sculpt;
  const StrokeCache &cache = *ss.cache;
  SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;

  const Span<int> grids = node.grids();
  const MutableSpan positions = gather_grids_positions(subdiv_ccg, grids, tls.positions);

  tls.factors.resize(positions.size());
  const MutableSpan<float> factors = tls.factors;
  fill_factor_from_hide_and_mask(subdiv_ccg, grids, factors);
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

  scale_factors(factors, strength);

  filter(positions, plane, factors);

  tls.translations.resize(positions.size());
  const MutableSpan<float3> translations = tls.translations;
  calc_translations_to_plane(positions, plane, translations);
  filter_plane_trim_limit_factors(brush, cache, translations, factors);
  scale_translations(translations, factors);

  clip_and_lock_translations(sd, ss, positions, translations);
  apply_translations(translations, grids, subdiv_ccg);
}

static void calc_bmesh(const Depsgraph &depsgraph,
                       const Sculpt &sd,
                       Object &object,
                       const Brush &brush,
                       const float4 &plane,
                       const float strength,
                       bke::pbvh::BMeshNode &node,
                       LocalData &tls,
                       const GenericFilterFn filter)
{
  SculptSession &ss = *object.sculpt;
  const StrokeCache &cache = *ss.cache;

  const Set<BMVert *, 0> &verts = BKE_pbvh_bmesh_node_unique_verts(&node);
  const MutableSpan positions = gather_bmesh_positions(verts, tls.positions);

  tls.factors.resize(verts.size());
  const MutableSpan<float> factors = tls.factors;
  fill_factor_from_hide_and_mask(*ss.bm, verts, factors);
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

  scale_factors(factors, strength);

  filter(positions, plane, factors);

  tls.translations.resize(verts.size());
  const MutableSpan<float3> translations = tls.translations;
  calc_translations_to_plane(positions, plane, translations);
  filter_plane_trim_limit_factors(brush, cache, translations, factors);
  scale_translations(translations, factors);

  clip_and_lock_translations(sd, ss, positions, translations);
  apply_translations(translations, verts);
}

static void do_plane_brush(const Depsgraph &depsgraph,
                           const Sculpt &sd,
                           Object &object,
                           const IndexMask &node_mask,
                           const float direction,
                           const IndexedFilterFn indexed_filter,
                           const GenericFilterFn generic_filter)
{
  const SculptSession &ss = *object.sculpt;
  bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
  const Brush &brush = *BKE_paint_brush_for_read(&sd.paint);

  float3 area_no;
  float3 area_co;
  calc_brush_plane(depsgraph, brush, object, node_mask, area_no, area_co);
  SCULPT_tilt_apply_to_normal(area_no, ss.cache, brush.tilt_strength_factor);

  const float offset = SCULPT_brush_plane_offset_get(sd, ss);
  const float displace = direction * ss.cache->radius * offset;
  area_co += area_no * ss.cache->scale * displace;

  float4 plane;
  plane_from_point_normal_v3(plane, area_co, area_no);

  threading::EnumerableThreadSpecific<LocalData> all_tls;
  switch (pbvh.type()) {
    case bke::pbvh::Type::Mesh: {
      const PositionDeformData position_data(depsgraph, object);
      const Span<float3> vert_normals = bke::pbvh::vert_normals_eval(depsgraph, object);
      MutableSpan<bke::pbvh::MeshNode> nodes = pbvh.nodes<bke::pbvh::MeshNode>();
      threading::parallel_for(node_mask.index_range(), 1, [&](const IndexRange range) {
        LocalData &tls = all_tls.local();
        node_mask.slice(range).foreach_index([&](const int i) {
          calc_faces(depsgraph,
                     sd,
                     brush,
                     plane,
                     ss.cache->bstrength,
                     vert_normals,
                     nodes[i],
                     object,
                     tls,
                     position_data,
                     indexed_filter);
          bke::pbvh::update_node_bounds_mesh(position_data.eval, nodes[i]);
        });
      });
      break;
    }
    case bke::pbvh::Type::Grids: {
      SubdivCCG &subdiv_ccg = *object.sculpt->subdiv_ccg;
      MutableSpan<float3> positions = subdiv_ccg.positions;
      MutableSpan<bke::pbvh::GridsNode> nodes = pbvh.nodes<bke::pbvh::GridsNode>();
      threading::parallel_for(node_mask.index_range(), 1, [&](const IndexRange range) {
        LocalData &tls = all_tls.local();
        node_mask.slice(range).foreach_index([&](const int i) {
          calc_grids(depsgraph,
                     sd,
                     object,
                     brush,
                     plane,
                     ss.cache->bstrength,
                     nodes[i],
                     tls,
                     generic_filter);
          bke::pbvh::update_node_bounds_grids(subdiv_ccg.grid_area, positions, nodes[i]);
        });
      });
      break;
    }
    case bke::pbvh::Type::BMesh: {
      MutableSpan<bke::pbvh::BMeshNode> nodes = pbvh.nodes<bke::pbvh::BMeshNode>();
      threading::parallel_for(node_mask.index_range(), 1, [&](const IndexRange range) {
        LocalData &tls = all_tls.local();
        node_mask.slice(range).foreach_index([&](const int i) {
          calc_bmesh(depsgraph,
                     sd,
                     object,
                     brush,
                     plane,
                     ss.cache->bstrength,
                     nodes[i],
                     tls,
                     generic_filter);
          bke::pbvh::update_node_bounds_bmesh(nodes[i]);
        });
      });
      break;
    }
  }
  pbvh.tag_positions_changed(node_mask);
  bke::pbvh::flush_bounds_to_parents(pbvh);
}

}  // namespace flatten_cc

void do_flatten_brush(const Depsgraph &depsgraph,
                      const Sculpt &sd,
                      Object &object,
                      const IndexMask &node_mask)
{
  do_plane_brush(
      depsgraph,
      sd,
      object,
      node_mask,
      1.0f,
      [](const Span<float3> /*vert_positions*/,
         const Span<int> /*verts*/,
         const float4 & /*plane*/,
         const MutableSpan<float> /*factors*/) {},
      [](const Span<float3> /*positions*/,
         const float4 & /*plane*/,
         const MutableSpan<float> /*factors*/) {});
}

void do_fill_brush(const Depsgraph &depsgraph,
                   const Sculpt &sd,
                   Object &object,
                   const IndexMask &node_mask)
{
  do_plane_brush(
      depsgraph,
      sd,
      object,
      node_mask,
      1.0f,
      [](const Span<float3> vert_positions,
         const Span<int> verts,
         const float4 &plane,
         const MutableSpan<float> factors) {
        filter_above_plane_factors(vert_positions, verts, plane, factors);
      },
      [](const Span<float3> positions, const float4 &plane, const MutableSpan<float> factors) {
        filter_above_plane_factors(positions, plane, factors);
      });
}

void do_scrape_brush(const Depsgraph &depsgraph,
                     const Sculpt &sd,
                     Object &object,
                     const IndexMask &node_mask)
{
  do_plane_brush(
      depsgraph,
      sd,
      object,
      node_mask,
      -1.0f,
      [](const Span<float3> vert_positions,
         const Span<int> verts,
         const float4 &plane,
         const MutableSpan<float> factors) {
        filter_below_plane_factors(vert_positions, verts, plane, factors);
      },
      [](const Span<float3> positions, const float4 &plane, const MutableSpan<float> factors) {
        filter_below_plane_factors(positions, plane, factors);
      });
}

}  // namespace blender::ed::sculpt_paint
