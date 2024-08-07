/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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
#include "BLI_array_utils.hh"
#include "BLI_enumerable_thread_specific.hh"
#include "BLI_math_vector.hh"
#include "BLI_task.hh"
#include "BLI_virtual_array.hh"

#include "editors/sculpt_paint/mesh_brush_common.hh"
#include "editors/sculpt_paint/sculpt_intern.hh"

namespace blender::ed::sculpt_paint {

inline namespace surface_smooth_cc {

struct LocalData {
  Vector<float3> positions;
  Vector<float> factors;
  Vector<float> distances;
  Vector<Vector<int>> vert_neighbors;
  Vector<float3> laplacian_disp;
  Vector<float3> average_positions;
  Vector<float3> translations;
};

BLI_NOINLINE static void clamp_factors(const MutableSpan<float> factors)
{
  for (float &factor : factors) {
    factor = std::clamp(factor, 0.0f, 1.0f);
  }
}

BLI_NOINLINE static void do_surface_smooth_brush_mesh(const Sculpt &sd,
                                                      const Brush &brush,
                                                      const Span<bke::pbvh::Node *> nodes,
                                                      Object &object,
                                                      const MutableSpan<float3> all_laplacian_disp)
{
  const SculptSession &ss = *object.sculpt;
  const StrokeCache &cache = *ss.cache;
  const float alpha = brush.surface_smooth_shape_preservation;
  const float beta = brush.surface_smooth_current_vertex;

  Mesh &mesh = *static_cast<Mesh *>(object.data);
  const OffsetIndices faces = mesh.faces();
  const Span<int> corner_verts = mesh.corner_verts();
  const bke::AttributeAccessor attributes = mesh.attributes();
  const VArraySpan hide_poly = *attributes.lookup<bool>(".hide_poly", bke::AttrDomain::Face);

  const bke::pbvh::Tree &pbvh = *ss.pbvh;

  const Span<float3> positions_eval = BKE_pbvh_get_vert_positions(pbvh);
  const Span<float3> vert_normals = BKE_pbvh_get_vert_normals(pbvh);
  MutableSpan<float3> positions_orig = mesh.vert_positions_for_write();

  Array<int> node_offset_data;
  const OffsetIndices node_offsets = create_node_vert_offsets(nodes, node_offset_data);
  Array<float> all_factors(node_offsets.total_size());

  threading::EnumerableThreadSpecific<LocalData> all_tls;
  threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
    LocalData &tls = all_tls.local();
    for (const int i : range) {
      const Span<int> verts = bke::pbvh::node_unique_verts(*nodes[i]);

      const MutableSpan<float> factors = all_factors.as_mutable_span().slice(node_offsets[i]);
      fill_factor_from_hide_and_mask(mesh, verts, factors);
      filter_region_clip_factors(ss, positions_eval, verts, factors);
      if (brush.flag & BRUSH_FRONTFACE) {
        calc_front_face(cache.view_normal, vert_normals, verts, factors);
      }

      tls.distances.resize(verts.size());
      const MutableSpan<float> distances = tls.distances;
      calc_brush_distances(
          ss, positions_eval, verts, eBrushFalloffShape(brush.falloff_shape), distances);
      filter_distances_with_radius(cache.radius, distances, factors);
      apply_hardness_to_distances(cache, distances);
      calc_brush_strength_factors(cache, brush, distances, factors);

      if (cache.automasking) {
        auto_mask::calc_vert_factors(object, *cache.automasking, *nodes[i], verts, factors);
      }

      calc_brush_texture_factors(ss, brush, positions_eval, verts, factors);

      scale_factors(factors, cache.bstrength);
      clamp_factors(factors);
    }
  });

  for ([[maybe_unused]] const int iteration : IndexRange(brush.surface_smooth_iterations)) {
    threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
      LocalData &tls = all_tls.local();
      for (const int i : range) {
        const Span<int> verts = bke::pbvh::node_unique_verts(*nodes[i]);
        const MutableSpan positions = gather_data_mesh(positions_eval, verts, tls.positions);
        const OrigPositionData orig_data = orig_position_data_get_mesh(object, *nodes[i]);
        const Span<float> factors = all_factors.as_span().slice(node_offsets[i]);

        tls.vert_neighbors.resize(verts.size());
        calc_vert_neighbors(
            faces, corner_verts, ss.vert_to_face_map, hide_poly, verts, tls.vert_neighbors);

        tls.average_positions.resize(verts.size());
        const MutableSpan<float3> average_positions = tls.average_positions;
        smooth::neighbor_data_average_mesh(positions_eval, tls.vert_neighbors, average_positions);

        tls.laplacian_disp.resize(verts.size());
        const MutableSpan<float3> laplacian_disp = tls.laplacian_disp;
        tls.translations.resize(verts.size());
        const MutableSpan<float3> translations = tls.translations;
        smooth::surface_smooth_laplacian_step(positions,
                                              orig_data.positions,
                                              average_positions,
                                              alpha,
                                              laplacian_disp,
                                              translations);
        scale_translations(translations, factors);

        scatter_data_mesh(laplacian_disp.as_span(), verts, all_laplacian_disp);

        write_translations(sd, object, positions_eval, verts, translations, positions_orig);
      }
    });

    threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
      LocalData &tls = all_tls.local();
      for (const int i : range) {
        const Span<int> verts = bke::pbvh::node_unique_verts(*nodes[i]);
        const Span<float> factors = all_factors.as_span().slice(node_offsets[i]);

        const MutableSpan<float3> laplacian_disp = gather_data_mesh(
            all_laplacian_disp.as_span(), verts, tls.laplacian_disp);

        tls.vert_neighbors.resize(verts.size());
        calc_vert_neighbors(
            faces, corner_verts, ss.vert_to_face_map, hide_poly, verts, tls.vert_neighbors);

        tls.average_positions.resize(verts.size());
        const MutableSpan<float3> average_laplacian_disps = tls.average_positions;
        smooth::neighbor_data_average_mesh(
            all_laplacian_disp.as_span(), tls.vert_neighbors, average_laplacian_disps);

        tls.translations.resize(verts.size());
        const MutableSpan<float3> translations = tls.translations;
        smooth::surface_smooth_displace_step(
            laplacian_disp, average_laplacian_disps, beta, translations);
        scale_translations(translations, factors);

        write_translations(sd, object, positions_eval, verts, translations, positions_orig);
      }
    });
  }
}

BLI_NOINLINE static void do_surface_smooth_brush_grids(
    const Sculpt &sd,
    const Brush &brush,
    const Span<bke::pbvh::Node *> nodes,
    Object &object,
    const MutableSpan<float3> all_laplacian_disp)
{
  const SculptSession &ss = *object.sculpt;
  const StrokeCache &cache = *ss.cache;
  const float alpha = brush.surface_smooth_shape_preservation;
  const float beta = brush.surface_smooth_current_vertex;

  SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;

  Array<int> node_offset_data;
  const OffsetIndices node_offsets = create_node_vert_offsets(
      nodes, BKE_subdiv_ccg_key_top_level(subdiv_ccg), node_offset_data);
  Array<float> all_factors(node_offsets.total_size());

  threading::EnumerableThreadSpecific<LocalData> all_tls;
  threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
    LocalData &tls = all_tls.local();
    for (const int i : range) {
      const Span<int> grids = bke::pbvh::node_grid_indices(*nodes[i]);
      const MutableSpan positions = gather_grids_positions(subdiv_ccg, grids, tls.positions);

      const MutableSpan<float> factors = all_factors.as_mutable_span().slice(node_offsets[i]);
      fill_factor_from_hide_and_mask(subdiv_ccg, grids, factors);
      filter_region_clip_factors(ss, positions, factors);
      if (brush.flag & BRUSH_FRONTFACE) {
        calc_front_face(cache.view_normal, subdiv_ccg, grids, factors);
      }

      tls.distances.resize(positions.size());
      const MutableSpan<float> distances = tls.distances;
      calc_brush_distances(ss, positions, eBrushFalloffShape(brush.falloff_shape), distances);
      filter_distances_with_radius(cache.radius, distances, factors);
      apply_hardness_to_distances(cache, distances);
      calc_brush_strength_factors(cache, brush, distances, factors);

      if (cache.automasking) {
        auto_mask::calc_grids_factors(object, *cache.automasking, *nodes[i], grids, factors);
      }

      calc_brush_texture_factors(ss, brush, positions, factors);

      scale_factors(factors, cache.bstrength);
      clamp_factors(factors);
    }
  });

  for ([[maybe_unused]] const int iteration : IndexRange(brush.surface_smooth_iterations)) {
    threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
      LocalData &tls = all_tls.local();
      for (const int i : range) {
        const Span<int> grids = bke::pbvh::node_grid_indices(*nodes[i]);
        const MutableSpan positions = gather_grids_positions(subdiv_ccg, grids, tls.positions);
        const OrigPositionData orig_data = orig_position_data_get_mesh(object, *nodes[i]);
        const Span<float> factors = all_factors.as_span().slice(node_offsets[i]);

        tls.average_positions.resize(positions.size());
        const MutableSpan<float3> average_positions = tls.average_positions;
        smooth::neighbor_position_average_grids(subdiv_ccg, grids, average_positions);

        tls.laplacian_disp.resize(positions.size());
        const MutableSpan<float3> laplacian_disp = tls.laplacian_disp;
        tls.translations.resize(positions.size());
        const MutableSpan<float3> translations = tls.translations;
        smooth::surface_smooth_laplacian_step(positions,
                                              orig_data.positions,
                                              average_positions,
                                              alpha,
                                              laplacian_disp,
                                              translations);
        scale_translations(translations, factors);

        scatter_data_grids(subdiv_ccg, laplacian_disp.as_span(), grids, all_laplacian_disp);

        clip_and_lock_translations(sd, ss, positions, translations);
        apply_translations(translations, grids, subdiv_ccg);
      }
    });

    threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
      LocalData &tls = all_tls.local();
      for (const int i : range) {
        const Span<int> grids = bke::pbvh::node_grid_indices(*nodes[i]);
        const MutableSpan positions = gather_grids_positions(subdiv_ccg, grids, tls.positions);
        const Span<float> factors = all_factors.as_span().slice(node_offsets[i]);

        const MutableSpan<float3> laplacian_disp = gather_data_grids(
            subdiv_ccg, all_laplacian_disp.as_span(), grids, tls.laplacian_disp);

        tls.average_positions.resize(positions.size());
        const MutableSpan<float3> average_laplacian_disps = tls.average_positions;
        smooth::average_data_grids(
            subdiv_ccg, all_laplacian_disp.as_span(), grids, average_laplacian_disps);

        tls.translations.resize(positions.size());
        const MutableSpan<float3> translations = tls.translations;
        smooth::surface_smooth_displace_step(
            laplacian_disp, average_laplacian_disps, beta, translations);
        scale_translations(translations, factors);

        clip_and_lock_translations(sd, ss, positions, translations);
        apply_translations(translations, grids, subdiv_ccg);
      }
    });
  }
}

BLI_NOINLINE static void do_surface_smooth_brush_bmesh(
    const Sculpt &sd,
    const Brush &brush,
    const Span<bke::pbvh::Node *> nodes,
    Object &object,
    const MutableSpan<float3> all_laplacian_disp)
{
  const SculptSession &ss = *object.sculpt;
  const StrokeCache &cache = *ss.cache;
  const float alpha = brush.surface_smooth_shape_preservation;
  const float beta = brush.surface_smooth_current_vertex;

  Array<int> node_offset_data;
  const OffsetIndices node_offsets = create_node_vert_offsets_bmesh(nodes, node_offset_data);
  Array<float> all_factors(node_offsets.total_size());

  threading::EnumerableThreadSpecific<LocalData> all_tls;
  threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
    LocalData &tls = all_tls.local();
    for (const int i : range) {
      const Set<BMVert *, 0> &verts = BKE_pbvh_bmesh_node_unique_verts(nodes[i]);
      const MutableSpan positions = gather_bmesh_positions(verts, tls.positions);

      const MutableSpan<float> factors = all_factors.as_mutable_span().slice(node_offsets[i]);
      fill_factor_from_hide_and_mask(*ss.bm, verts, factors);
      filter_region_clip_factors(ss, positions, factors);
      if (brush.flag & BRUSH_FRONTFACE) {
        calc_front_face(cache.view_normal, verts, factors);
      }

      tls.distances.resize(positions.size());
      const MutableSpan<float> distances = tls.distances;
      calc_brush_distances(ss, positions, eBrushFalloffShape(brush.falloff_shape), distances);
      filter_distances_with_radius(cache.radius, distances, factors);
      apply_hardness_to_distances(cache, distances);
      calc_brush_strength_factors(cache, brush, distances, factors);

      if (cache.automasking) {
        auto_mask::calc_vert_factors(object, *cache.automasking, *nodes[i], verts, factors);
      }

      calc_brush_texture_factors(ss, brush, positions, factors);

      scale_factors(factors, cache.bstrength);
      clamp_factors(factors);
    }
  });

  for ([[maybe_unused]] const int iteration : IndexRange(brush.surface_smooth_iterations)) {
    threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
      LocalData &tls = all_tls.local();
      for (const int i : range) {
        const Set<BMVert *, 0> &verts = BKE_pbvh_bmesh_node_unique_verts(nodes[i]);
        const MutableSpan positions = gather_bmesh_positions(verts, tls.positions);
        Array<float3> orig_positions(verts.size());
        Array<float3> orig_normals(verts.size());
        orig_position_data_gather_bmesh(*ss.bm_log, verts, orig_positions, orig_normals);
        const Span<float> factors = all_factors.as_span().slice(node_offsets[i]);

        tls.average_positions.resize(positions.size());
        const MutableSpan<float3> average_positions = tls.average_positions;
        smooth::neighbor_position_average_bmesh(verts, average_positions);

        tls.laplacian_disp.resize(positions.size());
        const MutableSpan<float3> laplacian_disp = tls.laplacian_disp;
        tls.translations.resize(positions.size());
        const MutableSpan<float3> translations = tls.translations;
        smooth::surface_smooth_laplacian_step(
            positions, orig_positions, average_positions, alpha, laplacian_disp, translations);
        scale_translations(translations, factors);

        scatter_data_vert_bmesh(laplacian_disp.as_span(), verts, all_laplacian_disp);

        clip_and_lock_translations(sd, ss, positions, translations);
        apply_translations(translations, verts);
      }
    });

    threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
      LocalData &tls = all_tls.local();
      for (const int i : range) {
        const Set<BMVert *, 0> &verts = BKE_pbvh_bmesh_node_unique_verts(nodes[i]);
        const MutableSpan positions = gather_bmesh_positions(verts, tls.positions);
        const Span<float> factors = all_factors.as_span().slice(node_offsets[i]);

        const MutableSpan<float3> laplacian_disp = gather_data_vert_bmesh(
            all_laplacian_disp.as_span(), verts, tls.laplacian_disp);

        tls.average_positions.resize(positions.size());
        const MutableSpan<float3> average_laplacian_disps = tls.average_positions;
        smooth::average_data_bmesh(all_laplacian_disp.as_span(), verts, average_laplacian_disps);

        tls.translations.resize(positions.size());
        const MutableSpan<float3> translations = tls.translations;
        smooth::surface_smooth_displace_step(
            laplacian_disp, average_laplacian_disps, beta, translations);
        scale_translations(translations, factors);

        clip_and_lock_translations(sd, ss, positions, translations);
        apply_translations(translations, verts);
      }
    });
  }
}

}  // namespace surface_smooth_cc

void do_surface_smooth_brush(const Sculpt &sd, Object &object, const Span<bke::pbvh::Node *> nodes)
{
  SculptSession &ss = *object.sculpt;
  const Brush &brush = *BKE_paint_brush_for_read(&sd.paint);

  switch (ss.pbvh->type()) {
    case bke::pbvh::Type::Mesh:
      do_surface_smooth_brush_mesh(
          sd, brush, nodes, object, ss.cache->surface_smooth_laplacian_disp);
      break;
    case bke::pbvh::Type::Grids: {
      do_surface_smooth_brush_grids(
          sd, brush, nodes, object, ss.cache->surface_smooth_laplacian_disp);
      break;
    }
    case bke::pbvh::Type::BMesh: {
      BM_mesh_elem_index_ensure(ss.bm, BM_VERT);
      do_surface_smooth_brush_bmesh(
          sd, brush, nodes, object, ss.cache->surface_smooth_laplacian_disp);
      break;
    }
  }
}

}  // namespace blender::ed::sculpt_paint
