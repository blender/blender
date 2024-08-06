/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "editors/sculpt_paint/brushes/types.hh"

#include "DNA_brush_types.h"
#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_key.hh"
#include "BKE_mesh.hh"
#include "BKE_paint.hh"
#include "BKE_pbvh.hh"
#include "BKE_subdiv_ccg.hh"

#include "BLI_array.hh"
#include "BLI_enumerable_thread_specific.hh"
#include "BLI_math_vector.h"
#include "BLI_math_vector.hh"
#include "BLI_task.h"
#include "BLI_task.hh"

#include "editors/sculpt_paint/mesh_brush_common.hh"
#include "editors/sculpt_paint/paint_intern.hh"
#include "editors/sculpt_paint/sculpt_intern.hh"

namespace blender::ed::sculpt_paint {

inline namespace layer_cc {

struct LocalData {
  Vector<float3> positions;
  Vector<float> factors;
  Vector<float> distances;
  Vector<float> masks;
  Vector<float> displacement_factors;
  Vector<float3> translations;
};

BLI_NOINLINE static void offset_displacement_factors(const MutableSpan<float> displacement_factors,
                                                     const Span<float> factors,
                                                     const float strength)
{
  for (const int i : displacement_factors.index_range()) {
    displacement_factors[i] += factors[i] * strength * (1.05f - std::abs(displacement_factors[i]));
  }
}

/**
 * When using persistent base, the layer brush (holding Control) invert mode resets the
 * height of the layer to 0. This makes possible to clean edges of previously added layers
 * on top of the base.
 *
 * The main direction of the layers is inverted using the regular brush strength with the
 * brush direction property.
 */
BLI_NOINLINE static void reset_displacement_factors(const MutableSpan<float> displacement_factors,
                                                    const Span<float> factors,
                                                    const float strength)
{
  for (const int i : displacement_factors.index_range()) {
    displacement_factors[i] += std::abs(factors[i] * strength * displacement_factors[i]) *
                               (displacement_factors[i] > 0.0f ? -1.0f : 1.0f);
  }
}

BLI_NOINLINE static void clamp_displacement_factors(const MutableSpan<float> displacement_factors,
                                                    const Span<float> masks)
{
  if (masks.is_empty()) {
    for (const int i : displacement_factors.index_range()) {
      displacement_factors[i] = std::clamp(displacement_factors[i], -1.0f, 1.0f);
    }
  }
  else {
    for (const int i : displacement_factors.index_range()) {
      const float clamp_mask = 1.0f - masks[i];
      displacement_factors[i] = std::clamp(displacement_factors[i], -clamp_mask, clamp_mask);
    }
  }
}

BLI_NOINLINE static void calc_translations(const Span<float3> orig_positions,
                                           const Span<float3> orig_normals,
                                           const Span<float3> positions,
                                           const Span<float> displacement_factors,
                                           const Span<float> factors,
                                           const float height,
                                           const MutableSpan<float3> r_translations)
{
  for (const int i : positions.index_range()) {
    const float3 offset = orig_normals[i] * height * displacement_factors[i];
    const float3 translation = orig_positions[i] + offset - positions[i];
    r_translations[i] = translation * factors[i];
  }
}

BLI_NOINLINE static void calc_translations(const Span<float3> base_positions,
                                           const Span<float3> base_normals,
                                           const Span<int> verts,
                                           const Span<float3> positions,
                                           const Span<float> displacement_factors,
                                           const Span<float> factors,
                                           const float height,
                                           const MutableSpan<float3> r_translations)
{
  for (const int i : positions.index_range()) {
    const float3 offset = base_normals[verts[i]] * height * displacement_factors[i];
    const float3 translation = base_positions[verts[i]] + offset - positions[i];
    r_translations[i] = translation * factors[i];
  }
}

static void calc_faces(const Sculpt &sd,
                       const Brush &brush,
                       const Span<float3> positions_eval,
                       const Span<float3> vert_normals,
                       const Span<float> mask_attribute,
                       const bool use_persistent_base,
                       const Span<float3> persistent_base_positions,
                       const Span<float3> persistent_base_normals,
                       Object &object,
                       bke::pbvh::Node &node,
                       LocalData &tls,
                       MutableSpan<float> layer_displacement_factor,
                       MutableSpan<float3> positions_orig)
{
  const SculptSession &ss = *object.sculpt;
  const StrokeCache &cache = *ss.cache;
  const Mesh &mesh = *static_cast<Mesh *>(object.data);

  const Span<int> verts = bke::pbvh::node_unique_verts(node);
  const MutableSpan positions = gather_data_mesh(positions_eval, verts, tls.positions);

  tls.factors.resize(verts.size());
  const MutableSpan<float> factors = tls.factors;
  fill_factor_from_hide_and_mask(mesh, verts, factors);
  filter_region_clip_factors(ss, positions, factors);
  if (brush.flag & BRUSH_FRONTFACE) {
    calc_front_face(cache.view_normal, vert_normals, verts, factors);
  }

  tls.distances.resize(verts.size());
  const MutableSpan<float> distances = tls.distances;
  calc_brush_distances(ss, positions, eBrushFalloffShape(brush.falloff_shape), distances);
  filter_distances_with_radius(cache.radius, distances, factors);
  apply_hardness_to_distances(cache, distances);
  calc_brush_strength_factors(cache, brush, distances, factors);

  if (cache.automasking) {
    auto_mask::calc_vert_factors(object, *cache.automasking, node, verts, factors);
  }

  calc_brush_texture_factors(ss, brush, positions, factors);

  if (mask_attribute.is_empty()) {
    tls.masks.clear();
  }
  else {
    tls.masks.resize(verts.size());
    gather_data_mesh(mask_attribute, verts, tls.masks.as_mutable_span());
  }
  const MutableSpan<float> masks = tls.masks;

  tls.displacement_factors.resize(verts.size());
  const MutableSpan<float> displacement_factors = tls.displacement_factors;
  gather_data_mesh(layer_displacement_factor.as_span(), verts, displacement_factors);

  if (use_persistent_base) {
    if (cache.invert) {
      reset_displacement_factors(displacement_factors, factors, cache.bstrength);
    }
    else {
      offset_displacement_factors(displacement_factors, factors, cache.bstrength);
    }
    clamp_displacement_factors(displacement_factors, masks);

    scatter_data_mesh(displacement_factors.as_span(), verts, layer_displacement_factor);

    tls.translations.resize(verts.size());
    const MutableSpan<float3> translations = tls.translations;
    calc_translations(persistent_base_positions,
                      persistent_base_normals,
                      verts,
                      positions,
                      displacement_factors,
                      factors,
                      brush.height,
                      translations);

    write_translations(sd, object, positions_eval, verts, translations, positions_orig);
  }
  else {
    offset_displacement_factors(displacement_factors, factors, cache.bstrength);
    clamp_displacement_factors(displacement_factors, masks);

    scatter_data_mesh(displacement_factors.as_span(), verts, layer_displacement_factor);

    const OrigPositionData orig_data = orig_position_data_get_mesh(object, node);

    tls.translations.resize(verts.size());
    const MutableSpan<float3> translations = tls.translations;
    calc_translations(orig_data.positions,
                      orig_data.normals,
                      positions,
                      displacement_factors,
                      factors,
                      brush.height,
                      translations);

    write_translations(sd, object, positions_eval, verts, translations, positions_orig);
  }
}

static void calc_grids(const Sculpt &sd,
                       const Brush &brush,
                       Object &object,
                       bke::pbvh::Node &node,
                       LocalData &tls,
                       MutableSpan<float> layer_displacement_factor)
{
  SculptSession &ss = *object.sculpt;
  const StrokeCache &cache = *ss.cache;
  SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;
  const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);

  const Span<int> grids = bke::pbvh::node_grid_indices(node);
  const MutableSpan positions = gather_grids_positions(subdiv_ccg, grids, tls.positions);

  tls.factors.resize(positions.size());
  const MutableSpan<float> factors = tls.factors;
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
    auto_mask::calc_grids_factors(object, *cache.automasking, node, grids, factors);
  }

  calc_brush_texture_factors(ss, brush, positions, factors);

  const MutableSpan<float> displacement_factors = gather_data_grids(
      subdiv_ccg, layer_displacement_factor.as_span(), grids, tls.displacement_factors);

  offset_displacement_factors(displacement_factors, factors, cache.bstrength);
  if (key.has_mask) {
    tls.masks.resize(positions.size());
    mask::gather_mask_grids(subdiv_ccg, grids, tls.masks);
  }
  else {
    tls.masks.clear();
  }
  clamp_displacement_factors(displacement_factors, tls.masks);

  scatter_data_grids(subdiv_ccg, displacement_factors.as_span(), grids, layer_displacement_factor);

  const OrigPositionData orig_data = orig_position_data_get_grids(object, node);

  tls.translations.resize(positions.size());
  const MutableSpan<float3> translations = tls.translations;
  calc_translations(orig_data.positions,
                    orig_data.normals,
                    positions,
                    displacement_factors,
                    factors,
                    brush.height,
                    translations);

  clip_and_lock_translations(sd, ss, positions, translations);
  apply_translations(translations, grids, subdiv_ccg);
}

static void calc_bmesh(const Sculpt &sd,
                       const Brush &brush,
                       Object &object,
                       bke::pbvh::Node &node,
                       LocalData &tls,
                       MutableSpan<float> layer_displacement_factor)
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
    calc_front_face(cache.view_normal, verts, factors);
  }

  tls.distances.resize(verts.size());
  const MutableSpan<float> distances = tls.distances;
  calc_brush_distances(ss, positions, eBrushFalloffShape(brush.falloff_shape), distances);
  filter_distances_with_radius(cache.radius, distances, factors);
  apply_hardness_to_distances(cache, distances);
  calc_brush_strength_factors(cache, brush, distances, factors);

  if (cache.automasking) {
    auto_mask::calc_vert_factors(object, *cache.automasking, node, verts, factors);
  }

  calc_brush_texture_factors(ss, brush, positions, factors);

  const MutableSpan<float> displacement_factors = gather_data_vert_bmesh(
      layer_displacement_factor.as_span(), verts, tls.displacement_factors);

  offset_displacement_factors(displacement_factors, factors, cache.bstrength);

  tls.masks.resize(verts.size());
  const MutableSpan<float> masks = tls.masks;
  mask::gather_mask_bmesh(*ss.bm, verts, masks);
  clamp_displacement_factors(displacement_factors, masks);

  scatter_data_vert_bmesh(displacement_factors.as_span(), verts, layer_displacement_factor);

  Array<float3> orig_positions(verts.size());
  Array<float3> orig_normals(verts.size());
  orig_position_data_gather_bmesh(*ss.bm_log, verts, orig_positions, orig_normals);

  tls.translations.resize(verts.size());
  const MutableSpan<float3> translations = tls.translations;
  calc_translations(orig_positions,
                    orig_normals,
                    positions,
                    displacement_factors,
                    factors,
                    brush.height,
                    translations);

  clip_and_lock_translations(sd, ss, positions, translations);
  apply_translations(translations, verts);
}

}  // namespace layer_cc

void do_layer_brush(const Sculpt &sd, Object &object, Span<bke::pbvh::Node *> nodes)
{
  SculptSession &ss = *object.sculpt;
  const Brush &brush = *BKE_paint_brush_for_read(&sd.paint);

  threading::EnumerableThreadSpecific<LocalData> all_tls;
  switch (object.sculpt->pbvh->type()) {
    case bke::pbvh::Type::Mesh: {
      Mesh &mesh = *static_cast<Mesh *>(object.data);
      const bke::pbvh::Tree &pbvh = *ss.pbvh;
      const Span<float3> positions_eval = BKE_pbvh_get_vert_positions(pbvh);
      const Span<float3> vert_normals = BKE_pbvh_get_vert_normals(pbvh);
      const MutableSpan<float3> positions_orig = mesh.vert_positions_for_write();

      bke::MutableAttributeAccessor attributes = mesh.attributes_for_write();
      const VArraySpan masks = *attributes.lookup<float>(".sculpt_mask", bke::AttrDomain::Point);
      const VArraySpan persistent_position = *attributes.lookup<float3>(".sculpt_persistent_co",
                                                                        bke::AttrDomain::Point);
      const VArraySpan persistent_normal = *attributes.lookup<float3>(".sculpt_persistent_no",
                                                                      bke::AttrDomain::Point);
      bke::SpanAttributeWriter persistent_disp_attr = attributes.lookup_for_write_span<float>(
          ".sculpt_persistent_disp");

      bool use_persistent_base = false;
      MutableSpan<float> displacement;
      if (brush.flag & BRUSH_PERSISTENT && !persistent_position.is_empty() &&
          !persistent_normal.is_empty() && bool(persistent_disp_attr) &&
          persistent_disp_attr.domain == bke::AttrDomain::Point)
      {
        use_persistent_base = true;
        displacement = persistent_disp_attr.span;
      }
      else {
        if (ss.cache->layer_displacement_factor.is_empty()) {
          ss.cache->layer_displacement_factor = Array<float>(SCULPT_vertex_count_get(ss), 0.0f);
        }
        displacement = ss.cache->layer_displacement_factor;
      }

      threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
        LocalData &tls = all_tls.local();
        for (const int i : range) {
          calc_faces(sd,
                     brush,
                     positions_eval,
                     vert_normals,
                     masks,
                     use_persistent_base,
                     persistent_position,
                     persistent_normal,
                     object,
                     *nodes[i],
                     tls,
                     displacement,
                     positions_orig);
        }
      });
      persistent_disp_attr.finish();
      break;
    }
    case bke::pbvh::Type::Grids: {
      if (ss.cache->layer_displacement_factor.is_empty()) {
        ss.cache->layer_displacement_factor = Array<float>(SCULPT_vertex_count_get(ss), 0.0f);
      }
      const MutableSpan<float> displacement = ss.cache->layer_displacement_factor;
      threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
        LocalData &tls = all_tls.local();
        for (const int i : range) {
          calc_grids(sd, brush, object, *nodes[i], tls, displacement);
        }
      });
      break;
    }
    case bke::pbvh::Type::BMesh: {
      if (ss.cache->layer_displacement_factor.is_empty()) {
        ss.cache->layer_displacement_factor = Array<float>(SCULPT_vertex_count_get(ss), 0.0f);
      }
      const MutableSpan<float> displacement = ss.cache->layer_displacement_factor;
      threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
        LocalData &tls = all_tls.local();
        for (const int i : range) {
          calc_bmesh(sd, brush, object, *nodes[i], tls, displacement);
        }
      });
      break;
    }
  }
}

}  // namespace blender::ed::sculpt_paint
