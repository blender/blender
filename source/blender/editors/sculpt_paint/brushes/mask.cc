/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "editors/sculpt_paint/brushes/types.hh"

#include "DNA_brush_types.h"
#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_pbvh.hh"
#include "BKE_subdiv_ccg.hh"

#include "BLI_array.hh"
#include "BLI_array_utils.hh"
#include "BLI_enumerable_thread_specific.hh"
#include "BLI_math_vector.hh"
#include "BLI_span.hh"
#include "BLI_task.h"

#include "editors/sculpt_paint/mesh_brush_common.hh"
#include "editors/sculpt_paint/sculpt_intern.hh"

namespace blender::ed::sculpt_paint {
inline namespace mask_cc {

struct LocalData {
  Vector<float> factors;
  Vector<float> distances;
  Vector<float> current_masks;
  Vector<float> new_masks;
};

static void invert_mask(const MutableSpan<float> masks)
{
  for (float &mask : masks) {
    mask = 1.0f - mask;
  }
}

static void apply_factors(const float strength,
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

static void clamp_mask(const MutableSpan<float> masks)
{
  for (float &mask : masks) {
    mask = std::clamp(mask, 0.0f, 1.0f);
  }
}

static void calc_faces(const Brush &brush,
                       const float strength,
                       const Span<float3> positions,
                       const Span<float3> vert_normals,
                       const PBVHNode &node,
                       Object &object,
                       Mesh &mesh,
                       LocalData &tls,
                       const MutableSpan<float> mask)
{
  SculptSession &ss = *object.sculpt;
  const StrokeCache &cache = *ss.cache;

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
      ss, positions, verts, eBrushFalloffShape(brush.falloff_shape), distances, factors);
  calc_brush_strength_factors(cache, brush, distances, factors);

  if (cache.automasking) {
    auto_mask::calc_vert_factors(object, *cache.automasking, node, verts, factors);
  }

  calc_brush_texture_factors(ss, brush, positions, verts, factors);

  tls.new_masks.reinitialize(verts.size());
  const MutableSpan<float> new_masks = tls.new_masks;
  array_utils::gather(mask.as_span(), verts, new_masks);

  tls.current_masks = tls.new_masks;
  const MutableSpan<float> current_masks = tls.current_masks;
  if (strength > 0.0f) {
    invert_mask(current_masks);
  }
  apply_factors(strength, current_masks, factors, new_masks);
  clamp_mask(new_masks);

  array_utils::scatter(new_masks.as_span(), verts, mask);
}

static float calc_new_mask(const float mask, const float factor, const float strength)
{
  const float modified_value = strength > 0.0f ? (1.0f - mask) : mask;
  const float result = mask + factor * strength * modified_value;
  return std::clamp(result, 0.0f, 1.0f);
}

static void calc_grids(Object &object, const Brush &brush, const float strength, PBVHNode &node)
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
    for (const int j : IndexRange(key.grid_area)) {
      if (!grid_hidden.is_empty() && grid_hidden[grid][j]) {
        i++;
        continue;
      }
      if (!sculpt_brush_test_sq_fn(test, CCG_elem_offset_co(key, elem, j))) {
        i++;
        continue;
      }
      auto_mask::node_update(automask_data, i);
      const float fade = SCULPT_brush_strength_factor(ss,
                                                      brush,
                                                      CCG_elem_offset_co(key, elem, j),
                                                      math::sqrt(test.dist),
                                                      CCG_elem_offset_no(key, elem, j),
                                                      nullptr,
                                                      0.0f,
                                                      BKE_pbvh_make_vref(grid_verts_start + j),
                                                      thread_id,
                                                      &automask_data);

      const float current_mask = key.has_mask ? CCG_elem_offset_mask(key, elem, j) : 0.0f;
      const float new_mask = calc_new_mask(current_mask, fade, strength);
      CCG_elem_offset_mask(key, elem, j) = new_mask;
      i++;
    }
  }
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
    const float new_mask = calc_new_mask(mask, fade, strength);
    BM_ELEM_CD_SET_FLOAT(vert, mask_offset, new_mask);
  }
}
}  // namespace mask_cc

void do_mask_brush(const Sculpt &sd, Object &object, Span<PBVHNode *> nodes)
{
  SculptSession &ss = *object.sculpt;
  const Brush &brush = *BKE_paint_brush_for_read(&sd.paint);
  const float bstrength = ss.cache->bstrength;

  switch (BKE_pbvh_type(*ss.pbvh)) {
    case PBVH_FACES: {
      threading::EnumerableThreadSpecific<LocalData> all_tls;
      Mesh &mesh = *static_cast<Mesh *>(object.data);

      const PBVH &pbvh = *ss.pbvh;
      const Span<float3> positions = BKE_pbvh_get_vert_positions(pbvh);
      const Span<float3> vert_normals = BKE_pbvh_get_vert_normals(pbvh);

      bke::MutableAttributeAccessor attributes = mesh.attributes_for_write();

      bke::SpanAttributeWriter<float> mask = attributes.lookup_or_add_for_write_span<float>(
          ".sculpt_mask", bke::AttrDomain::Point);

      threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
        threading::isolate_task([&]() {
          LocalData &tls = all_tls.local();
          for (const int i : range) {
            calc_faces(brush,
                       bstrength,
                       positions,
                       vert_normals,
                       *nodes[i],
                       object,
                       mesh,
                       tls,
                       mask.span);
          }
        });
      });
      mask.finish();
      break;
    }
    case PBVH_GRIDS: {
      threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
        for (const int i : range) {
          calc_grids(object, brush, bstrength, *nodes[i]);
        }
      });
      break;
    }
    case PBVH_BMESH: {
      threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
        for (const int i : range) {
          calc_bmesh(object, brush, bstrength, *nodes[i]);
        }
      });
      break;
    }
  }
}
}  // namespace blender::ed::sculpt_paint
