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
#include "editors/sculpt_paint/sculpt_intern.hh"

namespace blender::ed::sculpt_paint {

inline namespace layer_cc {

struct LocalData {
  Vector<float3> positions;
  Vector<float> factors;
  Vector<float> distances;
  Vector<float> masks;
};

static void calc_faces(const Sculpt &sd,
                       const Brush &brush,
                       const Span<float3> positions_eval,
                       const Span<float3> vert_normals,
                       const bool use_persistent_base,
                       const Span<float3> persistent_base_positions,
                       const Span<float3> persistent_base_normals,
                       Object &object,
                       PBVHNode &node,
                       LocalData &tls,
                       MutableSpan<float> layer_displacement_factor)
{
  const SculptSession &ss = *object.sculpt;
  const StrokeCache &cache = *ss.cache;
  const Mesh &mesh = *static_cast<Mesh *>(object.data);

  const Span<int> verts = bke::pbvh::node_unique_verts(node);

  tls.factors.reinitialize(verts.size());
  const MutableSpan<float> factors = tls.factors;
  fill_factor_from_hide_and_mask(mesh, verts, factors);
  filter_region_clip_factors(ss, positions_eval, verts, factors);
  if (brush.flag & BRUSH_FRONTFACE) {
    calc_front_face(cache.view_normal, vert_normals, verts, factors);
  }

  tls.distances.reinitialize(verts.size());
  const MutableSpan<float> distances = tls.distances;
  calc_brush_distances(
      ss, positions_eval, verts, eBrushFalloffShape(brush.falloff_shape), distances);
  filter_distances_with_radius(cache.radius, distances, factors);
  apply_hardness_to_distances(cache, distances);
  calc_brush_strength_factors(cache, brush, distances, factors);

  if (cache.automasking) {
    auto_mask::calc_vert_factors(object, *cache.automasking, node, verts, factors);
  }

  calc_brush_texture_factors(ss, brush, positions_eval, verts, factors);

  PBVHVertexIter vd;
  const float bstrength = cache.bstrength;
  const OrigPositionData orig_data = orig_position_data_get_mesh(object, node);

  if (use_persistent_base) {
    BKE_pbvh_vertex_iter_begin (*ss.pbvh, &node, vd, PBVH_ITER_UNIQUE) {
      float *disp_factor = &layer_displacement_factor[vd.index];

      /* When using persistent base, the layer brush (holding Control) invert mode resets the
       * height of the layer to 0. This makes possible to clean edges of previously added layers
       * on top of the base. */
      /* The main direction of the layers is inverted using the regular brush strength with the
       * brush direction property. */
      if (cache.invert) {
        (*disp_factor) += std::abs(factors[vd.i] * bstrength * (*disp_factor)) *
                          ((*disp_factor) > 0.0f ? -1.0f : 1.0f);
      }
      else {
        (*disp_factor) += factors[vd.i] * bstrength * (1.05f - std::abs(*disp_factor));
      }
      const float clamp_mask = 1.0f - vd.mask;
      *disp_factor = std::clamp(*disp_factor, -clamp_mask, clamp_mask);

      float3 normal = persistent_base_normals[vd.index];
      normal *= brush.height;
      float3 final_co = persistent_base_positions[vd.index] + normal * *disp_factor;

      float3 vdisp = final_co - float3(vd.co);
      vdisp *= factors[vd.i];
      final_co = float3(vd.co) + vdisp;

      SCULPT_clip(sd, ss, vd.co, final_co);
    }
    BKE_pbvh_vertex_iter_end;
  }
  else {
    BKE_pbvh_vertex_iter_begin (*ss.pbvh, &node, vd, PBVH_ITER_UNIQUE) {
      const int vi = vd.index;
      float *disp_factor = &layer_displacement_factor[vi];

      (*disp_factor) += factors[vd.i] * bstrength * (1.05f - std::abs(*disp_factor));
      const float clamp_mask = 1.0f - vd.mask;
      *disp_factor = std::clamp(*disp_factor, -clamp_mask, clamp_mask);

      float3 normal = orig_data.normals[vd.i];
      normal *= brush.height;
      float3 final_co = orig_data.positions[vd.i] + normal * *disp_factor;

      float3 vdisp = final_co - float3(vd.co);
      vdisp *= factors[vd.i];
      final_co = float3(vd.co) + vdisp;

      SCULPT_clip(sd, ss, vd.co, final_co);
    }
    BKE_pbvh_vertex_iter_end;
  }
}

static void calc_grids(const Sculpt &sd,
                       const Brush &brush,
                       Object &object,
                       PBVHNode &node,
                       LocalData &tls,
                       MutableSpan<float> layer_displacement_factor)
{
  SculptSession &ss = *object.sculpt;
  const StrokeCache &cache = *ss.cache;
  SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;
  const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);

  const Span<int> grids = bke::pbvh::node_grid_indices(node);
  const int grid_verts_num = grids.size() * key.grid_area;

  tls.positions.reinitialize(grid_verts_num);
  const MutableSpan<float3> positions = tls.positions;
  gather_grids_positions(subdiv_ccg, grids, positions);

  tls.factors.reinitialize(grid_verts_num);
  const MutableSpan<float> factors = tls.factors;
  fill_factor_from_hide_and_mask(subdiv_ccg, grids, factors);
  filter_region_clip_factors(ss, positions, factors);
  if (brush.flag & BRUSH_FRONTFACE) {
    calc_front_face(cache.view_normal, subdiv_ccg, grids, factors);
  }

  tls.distances.reinitialize(grid_verts_num);
  const MutableSpan<float> distances = tls.distances;
  calc_brush_distances(ss, positions, eBrushFalloffShape(brush.falloff_shape), distances);
  filter_distances_with_radius(cache.radius, distances, factors);
  apply_hardness_to_distances(cache, distances);
  calc_brush_strength_factors(cache, brush, distances, factors);

  if (cache.automasking) {
    auto_mask::calc_grids_factors(object, *cache.automasking, node, grids, factors);
  }

  calc_brush_texture_factors(ss, brush, positions, factors);

  PBVHVertexIter vd;
  const float bstrength = cache.bstrength;
  const OrigPositionData orig_data = orig_position_data_get_grids(object, node);

  BKE_pbvh_vertex_iter_begin (*ss.pbvh, &node, vd, PBVH_ITER_UNIQUE) {
    const int vi = vd.index;
    float *disp_factor = &layer_displacement_factor[vi];

    (*disp_factor) += factors[vd.i] * bstrength * (1.05f - std::abs(*disp_factor));
    const float clamp_mask = 1.0f - vd.mask;
    *disp_factor = std::clamp(*disp_factor, -clamp_mask, clamp_mask);

    float3 normal = orig_data.normals[vd.i];
    normal *= brush.height;
    float3 final_co = orig_data.positions[vd.i] + normal * *disp_factor;

    float3 vdisp = final_co - float3(vd.co);
    vdisp *= factors[vd.i];
    final_co = float3(vd.co) + vdisp;

    SCULPT_clip(sd, ss, vd.co, final_co);
  }
  BKE_pbvh_vertex_iter_end;
}

static void calc_bmesh(const Sculpt &sd,
                       const Brush &brush,
                       Object &object,
                       PBVHNode &node,
                       LocalData &tls,
                       MutableSpan<float> layer_displacement_factor)
{
  SculptSession &ss = *object.sculpt;
  const StrokeCache &cache = *ss.cache;

  const Set<BMVert *, 0> &verts = BKE_pbvh_bmesh_node_unique_verts(&node);

  tls.positions.reinitialize(verts.size());
  const MutableSpan<float3> positions = tls.positions;
  gather_bmesh_positions(verts, positions);

  tls.factors.reinitialize(verts.size());
  const MutableSpan<float> factors = tls.factors;
  fill_factor_from_hide_and_mask(*ss.bm, verts, factors);
  filter_region_clip_factors(ss, positions, factors);
  if (brush.flag & BRUSH_FRONTFACE) {
    calc_front_face(cache.view_normal, verts, factors);
  }

  tls.distances.reinitialize(verts.size());
  const MutableSpan<float> distances = tls.distances;
  calc_brush_distances(ss, positions, eBrushFalloffShape(brush.falloff_shape), distances);
  filter_distances_with_radius(cache.radius, distances, factors);
  apply_hardness_to_distances(cache, distances);
  calc_brush_strength_factors(cache, brush, distances, factors);

  if (cache.automasking) {
    auto_mask::calc_vert_factors(object, *cache.automasking, node, verts, factors);
  }

  calc_brush_texture_factors(ss, brush, positions, factors);

  PBVHVertexIter vd;
  const float bstrength = cache.bstrength;

  Array<float3> orig_positions(verts.size());
  Array<float3> orig_normals(verts.size());
  orig_position_data_gather_bmesh(*ss.bm_log, verts, orig_positions, orig_normals);

  BKE_pbvh_vertex_iter_begin (*ss.pbvh, &node, vd, PBVH_ITER_UNIQUE) {
    const int vi = vd.index;

    float *disp_factor = &layer_displacement_factor[vi];

    (*disp_factor) += factors[vd.i] * bstrength * (1.05f - std::abs(*disp_factor));

    const float clamp_mask = 1.0f - vd.mask;
    *disp_factor = std::clamp(*disp_factor, -clamp_mask, clamp_mask);

    float3 normal = orig_normals[vd.i];
    normal *= brush.height;
    float3 final_co = orig_positions[vd.i] + normal * *disp_factor;

    float3 vdisp = final_co - float3(vd.co);
    vdisp *= factors[vd.i];
    final_co = float3(vd.co) + vdisp;

    SCULPT_clip(sd, ss, vd.co, final_co);
  }
  BKE_pbvh_vertex_iter_end;
}

}  // namespace layer_cc

void do_layer_brush(const Sculpt &sd, Object &object, Span<PBVHNode *> nodes)
{
  SculptSession &ss = *object.sculpt;
  const Brush &brush = *BKE_paint_brush_for_read(&sd.paint);

  if (ss.cache->layer_displacement_factor.is_empty()) {
    ss.cache->layer_displacement_factor = Array<float>(SCULPT_vertex_count_get(ss), 0.0f);
  }
  const MutableSpan<float> displacement = ss.cache->layer_displacement_factor;

  threading::EnumerableThreadSpecific<LocalData> all_tls;
  switch (BKE_pbvh_type(*object.sculpt->pbvh)) {
    case PBVH_FACES: {
      Mesh &mesh = *static_cast<Mesh *>(object.data);
      const PBVH &pbvh = *ss.pbvh;
      const Span<float3> positions_eval = BKE_pbvh_get_vert_positions(pbvh);
      const Span<float3> vert_normals = BKE_pbvh_get_vert_normals(pbvh);

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
                     use_persistent_base,
                     persistent_position,
                     persistent_normal,
                     object,
                     *nodes[i],
                     tls,
                     displacement);
        }
      });
      persistent_disp_attr.finish();
      break;
    }
    case PBVH_GRIDS:
      threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
        LocalData &tls = all_tls.local();
        for (const int i : range) {
          calc_grids(sd, brush, object, *nodes[i], tls, displacement);
        }
      });
      break;
    case PBVH_BMESH:
      threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
        LocalData &tls = all_tls.local();
        for (const int i : range) {
          calc_bmesh(sd, brush, object, *nodes[i], tls, displacement);
        }
      });
      break;
  }
}

}  // namespace blender::ed::sculpt_paint
