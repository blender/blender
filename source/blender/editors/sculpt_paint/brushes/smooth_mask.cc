/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "editors/sculpt_paint/brushes/types.hh"

#include "DNA_brush_types.h"

#include "BKE_attribute.hh"
#include "BKE_mesh.hh"
#include "BKE_subdiv_ccg.hh"

#include "BLI_array_utils.hh"
#include "BLI_enumerable_thread_specific.hh"
#include "BLI_math_base.hh"
#include "BLI_task.h"

#include "editors/sculpt_paint/mesh_brush_common.hh"
#include "editors/sculpt_paint/sculpt_intern.hh"

namespace blender::ed::sculpt_paint {

inline namespace smooth_mask_cc {

struct LocalData {
  Vector<float> factors;
  Vector<float> distances;
  Vector<Vector<int>> vert_neighbors;
  Vector<float> masks;
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

static float average_masks(const Span<float> masks, const Span<int> indices)
{
  const float factor = math::rcp(float(indices.size()));
  float result = 0;
  for (const int i : indices) {
    result += masks[i] * factor;
  }
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
  tls.vert_neighbors.reinitialize(verts.size());
  calc_vert_neighbors(faces, corner_verts, vert_to_face_map, hide_poly, verts, tls.vert_neighbors);
  const Span<Vector<int>> vert_neighbors = tls.vert_neighbors;

  for (const int i : verts.index_range()) {
    const Span<int> neighbors = vert_neighbors[i];
    if (neighbors.is_empty()) {
      new_masks[i] = masks[verts[i]];
    }
    else {
      new_masks[i] = average_masks(masks, neighbors);
    }
  }
}

static void calc_mask(const Span<float> mask_averages,
                      const Span<float> factors,
                      const MutableSpan<float> masks)
{
  BLI_assert(mask_averages.size() == factors.size());
  BLI_assert(mask_averages.size() == masks.size());

  for (const int i : masks.index_range()) {
    masks[i] += (mask_averages[i] - masks[i]) * factors[i];
  }
}

static void clamp_mask(const MutableSpan<float> masks)
{
  for (float &mask : masks) {
    mask = std::clamp(mask, 0.0f, 1.0f);
  }
}

static void apply_masks_faces(const Brush &brush,
                              const Span<float3> positions_eval,
                              const Span<float3> vert_normals,
                              const PBVHNode &node,
                              const float strength,
                              Object &object,
                              LocalData &tls,
                              const Span<float> mask_averages,
                              MutableSpan<float> mask)
{
  SculptSession &ss = *object.sculpt;
  const StrokeCache &cache = *ss.cache;
  const Mesh &mesh = *static_cast<Mesh *>(object.data);

  const Span<int> verts = bke::pbvh::node_unique_verts(node);

  tls.factors.reinitialize(verts.size());
  const MutableSpan<float> factors = tls.factors;
  fill_factor_from_hide(mesh, verts, factors);

  if (brush.flag & BRUSH_FRONTFACE) {
    calc_front_face(cache.view_normal, vert_normals, verts, factors);
  }

  tls.distances.reinitialize(verts.size());
  const MutableSpan<float> distances = tls.distances;
  calc_distance_falloff(
      ss, positions_eval, verts, eBrushFalloffShape(brush.falloff_shape), distances, factors);
  calc_brush_strength_factors(cache, brush, distances, factors);

  if (ss.cache->automasking) {
    auto_mask::calc_vert_factors(object, *ss.cache->automasking, node, verts, factors);
  }

  scale_factors(factors, strength);

  calc_brush_texture_factors(ss, brush, positions_eval, verts, factors);

  tls.masks.reinitialize(verts.size());
  const MutableSpan<float> new_masks = tls.masks;
  array_utils::gather(mask.as_span(), verts, new_masks);

  calc_mask(mask_averages, factors, new_masks);
  clamp_mask(new_masks);

  array_utils::scatter(new_masks.as_span(), verts, mask);
}

static void do_smooth_brush_mesh(const Brush &brush,
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

  Array<int> node_vert_offset_data;
  OffsetIndices node_vert_offsets = create_node_vert_offsets(nodes, node_vert_offset_data);
  Array<float> new_masks(node_vert_offsets.total_size());

  bke::MutableAttributeAccessor write_attributes = mesh.attributes_for_write();

  bke::SpanAttributeWriter<float> mask = write_attributes.lookup_for_write_span<float>(
      ".sculpt_mask");

  threading::EnumerableThreadSpecific<LocalData> all_tls;
  for (const float strength : iteration_strengths(brush_strength)) {
    threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
      LocalData &tls = all_tls.local();
      for (const int i : range) {
        calc_smooth_masks_faces(faces,
                                corner_verts,
                                ss.vert_to_face_map,
                                hide_poly,
                                bke::pbvh::node_unique_verts(*nodes[i]),
                                mask.span.as_span(),
                                tls,
                                new_masks.as_mutable_span().slice(node_vert_offsets[i]));
      }
    });

    threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
      threading::isolate_task([&]() {
        LocalData &tls = all_tls.local();
        for (const int i : range) {
          apply_masks_faces(brush,
                            positions_eval,
                            vert_normals,
                            *nodes[i],
                            strength,
                            object,
                            tls,
                            new_masks.as_span().slice(node_vert_offsets[i]),
                            mask.span);
        }
      });
    });
  }
  mask.finish();
}

static float calc_new_mask(float neighbor_average, float current_mask, float fade, float strength)
{
  float delta = (neighbor_average - current_mask) * fade * strength;
  float new_mask = current_mask + delta;
  return std::clamp(new_mask, 0.0f, 1.0f);
}

static float neighbor_mask_average_grids(
    const SubdivCCG &subdiv_ccg, const CCGKey &key, int grid_index, int x, int y)
{
  SubdivCCGCoord coord{};
  coord.grid_index = grid_index;
  coord.x = x;
  coord.y = y;

  SubdivCCGNeighbors neighbors;
  BKE_subdiv_ccg_neighbor_coords_get(subdiv_ccg, coord, false, neighbors);

  float sum = 0.0f;
  for (const SubdivCCGCoord neighbor : neighbors.coords) {
    sum += CCG_grid_elem_mask(key, subdiv_ccg.grids[neighbor.grid_index], neighbor.x, neighbor.y);
  }
  return sum / neighbors.coords.size();
}

static void calc_grids(Object &object,
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

  int i = 0;
  for (const int grid : bke::pbvh::node_grid_indices(node)) {
    const int grid_verts_start = grid * key.grid_area;
    CCGElem *elem = grids[grid];
    for (const int y : IndexRange(key.grid_size)) {
      for (const int x : IndexRange(key.grid_size)) {
        const int offset = CCG_grid_xy_to_index(key.grid_size, x, y);
        if (!grid_hidden.is_empty() && grid_hidden[grid][offset]) {
          i++;
          continue;
        }
        if (!sculpt_brush_test_sq_fn(test, CCG_elem_offset_co(key, elem, offset))) {
          i++;
          continue;
        }
        auto_mask::node_update(automask_data, i);
        const float fade = SCULPT_brush_strength_factor(
            ss,
            brush,
            CCG_elem_offset_co(key, elem, offset),
            math::sqrt(test.dist),
            CCG_elem_offset_no(key, elem, offset),
            nullptr,
            0.0f,
            BKE_pbvh_make_vref(grid_verts_start + offset),
            thread_id,
            &automask_data);

        const float new_mask = calc_new_mask(
            neighbor_mask_average_grids(*ss.subdiv_ccg, key, grid, x, y),
            CCG_elem_offset_mask(key, elem, offset),
            fade,
            strength);
        CCG_elem_offset_mask(key, elem, offset) = new_mask;
        i++;
      }
    }
  }
}

static float neighbor_mask_average_bmesh(BMVert &vert, const int mask_offset)
{

  float sum = 0.0f;
  Vector<BMVert *, 64> neighbors;
  for (BMVert *neighbor : vert_neighbors_get_bmesh(vert, neighbors)) {
    sum += BM_ELEM_CD_GET_FLOAT(neighbor, mask_offset);
  }
  return sum / neighbors.size();
}

static void calc_bmesh(Object &object, const Brush &brush, const float strength, PBVHNode &node)
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
    const float fade = SCULPT_brush_strength_factor(ss,
                                                    brush,
                                                    vert->co,
                                                    math::sqrt(test.dist),
                                                    vert->no,
                                                    nullptr,
                                                    0.0f,
                                                    BKE_pbvh_make_vref(intptr_t(vert)),
                                                    thread_id,
                                                    &automask_data);
    const float new_mask = calc_new_mask(
        neighbor_mask_average_bmesh(*vert, mask_offset), mask, fade, strength);
    BM_ELEM_CD_SET_FLOAT(vert, mask_offset, new_mask);
  }
}

}  // namespace smooth_mask_cc

void do_smooth_mask_brush(const Sculpt &sd,
                          Object &object,
                          Span<PBVHNode *> nodes,
                          float brush_strength)
{
  SculptSession &ss = *object.sculpt;
  const Brush &brush = *BKE_paint_brush_for_read(&sd.paint);
  SCULPT_boundary_info_ensure(object);
  switch (BKE_pbvh_type(*object.sculpt->pbvh)) {
    case PBVH_FACES:
      do_smooth_brush_mesh(brush, object, nodes, brush_strength);
      break;
    case PBVH_GRIDS:
      for (const float strength : iteration_strengths(brush_strength)) {
        threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
          for (const int i : range) {
            calc_grids(object, brush, strength, *nodes[i]);
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
            calc_bmesh(object, brush, strength, *nodes[i]);
          }
        });
      }
      break;
  }
}
}  // namespace blender::ed::sculpt_paint
