/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "editors/sculpt_paint/brushes/types.hh"

#include "DNA_brush_types.h"
#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_colortools.hh"
#include "BKE_key.hh"
#include "BKE_mesh.hh"
#include "BKE_paint.hh"
#include "BKE_pbvh.hh"
#include "BKE_subdiv_ccg.hh"

#include "BLI_array.hh"
#include "BLI_enumerable_thread_specific.hh"
#include "BLI_math_matrix.hh"
#include "BLI_math_vector.hh"
#include "BLI_task.h"
#include "BLI_task.hh"
#include "BLI_virtual_array.hh"

#include "editors/sculpt_paint/mesh_brush_common.hh"
#include "editors/sculpt_paint/sculpt_intern.hh"

namespace blender::ed::sculpt_paint {

inline namespace smooth_cc {

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

struct LocalData {
  Vector<float> factors;
  Vector<float> distances;
  Vector<Vector<int>> vert_neighbors;
  Vector<float3> translations;
};

static float3 average_positions(const Span<float3> positions, const Span<int> indices)
{
  const float factor = math::rcp(float(indices.size()));
  float3 result(0);
  for (const int i : indices) {
    result += positions[i] * factor;
  }
  return result;
}

BLI_NOINLINE static void calc_smooth_positions_faces(const OffsetIndices<int> faces,
                                                     const Span<int> corner_verts,
                                                     const GroupedSpan<int> vert_to_face_map,
                                                     const BitSpan boundary_verts,
                                                     const Span<bool> hide_poly,
                                                     const Span<int> verts,
                                                     const Span<float3> positions,
                                                     LocalData &tls,
                                                     const MutableSpan<float3> new_positions)
{
  tls.vert_neighbors.reinitialize(verts.size());
  calc_vert_neighbors_interior(
      faces, corner_verts, vert_to_face_map, boundary_verts, hide_poly, verts, tls.vert_neighbors);
  const Span<Vector<int>> vert_neighbors = tls.vert_neighbors;

  for (const int i : verts.index_range()) {
    const Span<int> neighbors = vert_neighbors[i];
    if (neighbors.is_empty()) {
      new_positions[i] = positions[verts[i]];
    }
    else {
      new_positions[i] = average_positions(positions, neighbors);
    }
  }
}

BLI_NOINLINE static void translations_from_new_positions(const Span<float3> new_positions,
                                                         const Span<int> verts,
                                                         const Span<float3> old_positions,
                                                         const MutableSpan<float3> translations)
{
  BLI_assert(new_positions.size() == verts.size());
  for (const int i : verts.index_range()) {
    translations[i] = new_positions[i] - old_positions[verts[i]];
  }
}

BLI_NOINLINE static void apply_positions_faces(const Sculpt &sd,
                                               const Brush &brush,
                                               const Span<float3> positions_eval,
                                               const Span<float3> vert_normals,
                                               const PBVHNode &node,
                                               const float strength,
                                               Object &object,
                                               LocalData &tls,
                                               const Span<float3> new_positions,
                                               MutableSpan<float3> positions_orig)
{
  SculptSession &ss = *object.sculpt;
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
  calc_distance_falloff(
      ss, positions_eval, verts, eBrushFalloffShape(brush.falloff_shape), distances, factors);
  apply_hardness_to_distances(cache, distances);
  calc_brush_strength_factors(cache, brush, distances, factors);

  if (cache.automasking) {
    auto_mask::calc_vert_factors(object, *cache.automasking, node, verts, factors);
  }

  scale_factors(factors, strength);

  calc_brush_texture_factors(ss, brush, positions_eval, verts, factors);

  tls.translations.reinitialize(verts.size());
  const MutableSpan<float3> translations = tls.translations;
  translations_from_new_positions(new_positions, verts, positions_eval, translations);
  scale_translations(translations, factors);

  write_translations(sd, object, positions_eval, verts, translations, positions_orig);
}

BLI_NOINLINE static void do_smooth_brush_mesh(const Sculpt &sd,
                                              const Brush &brush,
                                              Object &object,
                                              Span<PBVHNode *> nodes,
                                              const float brush_strength)
{
  const SculptSession &ss = *object.sculpt;
  Mesh &mesh = *static_cast<Mesh *>(object.data);
  const OffsetIndices faces = mesh.faces();
  const Span<int> corner_verts = mesh.corner_verts();
  const bke::AttributeAccessor attributes = mesh.attributes();
  const VArraySpan hide_poly = *attributes.lookup<bool>(".hide_poly", bke::AttrDomain::Face);

  const PBVH &pbvh = *ss.pbvh;

  const Span<float3> positions_eval = BKE_pbvh_get_vert_positions(pbvh);
  const Span<float3> vert_normals = BKE_pbvh_get_vert_normals(pbvh);
  MutableSpan<float3> positions_orig = mesh.vert_positions_for_write();

  Array<int> node_vert_offset_data(nodes.size() + 1);
  for (const int i : nodes.index_range()) {
    node_vert_offset_data[i] = bke::pbvh::node_unique_verts(*nodes[i]).size();
  }
  const OffsetIndices<int> node_vert_offsets = offset_indices::accumulate_counts_to_offsets(
      node_vert_offset_data);
  Array<float3> new_positions(node_vert_offsets.total_size());

  threading::EnumerableThreadSpecific<LocalData> all_tls;
  for (const float strength : iteration_strengths(brush_strength)) {
    threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
      LocalData &tls = all_tls.local();
      for (const int i : range) {
        calc_smooth_positions_faces(faces,
                                    corner_verts,
                                    ss.vert_to_face_map,
                                    ss.vertex_info.boundary,
                                    hide_poly,
                                    bke::pbvh::node_unique_verts(*nodes[i]),
                                    positions_eval,
                                    tls,
                                    new_positions.as_mutable_span().slice(node_vert_offsets[i]));
      }
    });

    threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
      LocalData &tls = all_tls.local();
      for (const int i : range) {
        apply_positions_faces(sd,
                              brush,
                              positions_eval,
                              vert_normals,
                              *nodes[i],
                              strength,
                              object,
                              tls,
                              new_positions.as_span().slice(node_vert_offsets[i]),
                              positions_orig);
      }
    });
  }
}

static void calc_grids(const Sculpt &sd,
                       Object &object,
                       const Brush &brush,
                       const float strength,
                       const PBVHNode &node)
{
  SculptSession &ss = *object.sculpt;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, test, brush.falloff_shape);
  const int thread_id = BLI_task_parallel_thread_id(nullptr);
  auto_mask::NodeData automask_data = auto_mask::node_begin(
      object, ss.cache->automasking.get(), node);

  SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;
  const CCGKey key = *BKE_pbvh_get_grid_key(*ss.pbvh);
  const Span<CCGElem *> grids = subdiv_ccg.grids;
  const BitGroupVector<> &grid_hidden = subdiv_ccg.grid_hidden;

  for (const int grid : bke::pbvh::node_grid_indices(node)) {
    const int grid_verts_start = grid * key.grid_area;
    CCGElem *elem = grids[grid];
    for (const int j : IndexRange(key.grid_area)) {
      if (!grid_hidden.is_empty() && grid_hidden[grid][j]) {
        continue;
      }
      float3 &co = CCG_elem_offset_co(key, elem, j);
      if (!sculpt_brush_test_sq_fn(test, co)) {
        continue;
      }
      const float fade = strength * SCULPT_brush_strength_factor(
                                        ss,
                                        brush,
                                        co,
                                        sqrtf(test.dist),
                                        CCG_elem_offset_no(key, elem, j),
                                        nullptr,
                                        key.has_mask ? CCG_elem_offset_mask(key, elem, j) : 0.0f,
                                        BKE_pbvh_make_vref(grid_verts_start + j),
                                        thread_id,
                                        &automask_data);

      float3 avg = smooth::neighbor_coords_average_interior(
          ss, BKE_pbvh_make_vref(grid_verts_start + j));
      float3 final_co = co + (avg - co) * fade;
      SCULPT_clip(sd, ss, co, final_co);
    }
  }
}

static void calc_bmesh(
    const Sculpt &sd, Object &object, const Brush &brush, const float strength, PBVHNode &node)
{
  SculptSession &ss = *object.sculpt;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, test, brush.falloff_shape);
  const int thread_id = BLI_task_parallel_thread_id(nullptr);
  auto_mask::NodeData automask_data = auto_mask::node_begin(
      object, ss.cache->automasking.get(), node);

  const int mask_offset = CustomData_get_offset_named(
      &ss.bm->vdata, CD_PROP_FLOAT, ".sculpt_mask");

  for (BMVert *vert : BKE_pbvh_bmesh_node_unique_verts(&node)) {
    if (BM_elem_flag_test(vert, BM_ELEM_HIDDEN)) {
      continue;
    }
    if (!sculpt_brush_test_sq_fn(test, vert->co)) {
      continue;
    }
    auto_mask::node_update(automask_data, *vert);
    const float mask = mask_offset == -1 ? 0.0f : BM_ELEM_CD_GET_FLOAT(vert, mask_offset);
    const float fade = strength * SCULPT_brush_strength_factor(ss,
                                                               brush,
                                                               vert->co,
                                                               sqrtf(test.dist),
                                                               vert->no,
                                                               nullptr,
                                                               mask,
                                                               BKE_pbvh_make_vref(intptr_t(vert)),
                                                               thread_id,
                                                               &automask_data);

    float3 avg = smooth::neighbor_coords_average_interior(ss, BKE_pbvh_make_vref(intptr_t(vert)));
    float3 final_co = float3(vert->co) + (avg - float3(vert->co)) * fade;
    SCULPT_clip(sd, ss, vert->co, final_co);
  }
}

}  // namespace smooth_cc

void do_smooth_brush(const Sculpt &sd,
                     Object &object,
                     const Span<PBVHNode *> nodes,
                     const float brush_strength)
{
  SculptSession &ss = *object.sculpt;
  const Brush &brush = *BKE_paint_brush_for_read(&sd.paint);

  SCULPT_boundary_info_ensure(object);

  switch (BKE_pbvh_type(*object.sculpt->pbvh)) {
    case PBVH_FACES:
      do_smooth_brush_mesh(sd, brush, object, nodes, brush_strength);
      break;
    case PBVH_GRIDS:
      for (const float strength : iteration_strengths(brush_strength)) {
        threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
          for (const int i : range) {
            calc_grids(sd, object, brush, strength, *nodes[i]);
          }
        });
      }
      break;
    case PBVH_BMESH:
      BM_mesh_elem_index_ensure(ss.bm, BM_VERT);
      BM_mesh_elem_table_ensure(ss.bm, BM_VERT);
      for (const float strength : iteration_strengths(brush_strength)) {
        threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
          for (const int i : range) {
            calc_bmesh(sd, object, brush, strength, *nodes[i]);
          }
        });
      }
      break;
  }
}

}  // namespace blender::ed::sculpt_paint
