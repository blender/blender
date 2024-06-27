/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "editors/sculpt_paint/brushes/types.hh"

#include "DNA_brush_types.h"
#include "DNA_mesh_types.h"

#include "BKE_pbvh.hh"
#include "BKE_subdiv_ccg.hh"

#include "BLI_enumerable_thread_specific.hh"
#include "BLI_math_base.hh"
#include "BLI_math_geom.h"
#include "BLI_math_vector.hh"
#include "BLI_task.h"
#include "BLI_utildefines.h"

#include "editors/sculpt_paint/mesh_brush_common.hh"
#include "editors/sculpt_paint/sculpt_intern.hh"

namespace blender::ed::sculpt_paint {
inline namespace clay_cc {
struct LocalData {
  Vector<float> factors;
  Vector<float> distances;
  Vector<float3> translations;
};

BLI_NOINLINE static void calc_closest_to_plane(const float4 &test_plane,
                                               const Span<float3> positions,
                                               const Span<int> verts,
                                               const MutableSpan<float3> translations)
{
  /* Equivalent to #closest_to_plane_normalized_v3 */
  BLI_assert(verts.size() == translations.size());
  for (const int i : verts.index_range()) {
    const float side = plane_point_side_v3(test_plane, positions[verts[i]]);
    translations[i] = float3(test_plane) * -side;
  }
}

static void calc_faces(const Sculpt &sd,
                       const Brush &brush,
                       const float4 &test_plane,
                       const float strength,
                       const Span<float3> positions_eval,
                       const Span<float3> vert_normals,
                       const PBVHNode &node,
                       Object &object,
                       LocalData &tls,
                       const MutableSpan<float3> positions_orig)
{
  SculptSession &ss = *object.sculpt;
  const StrokeCache &cache = *ss.cache;
  Mesh &mesh = *static_cast<Mesh *>(object.data);

  const Span<int> verts = bke::pbvh::node_unique_verts(node);

  tls.factors.reinitialize(verts.size());
  const MutableSpan<float> factors = tls.factors;
  fill_factor_from_hide_and_mask(mesh, verts, factors);

  if (brush.flag & BRUSH_FRONTFACE) {
    calc_front_face(cache.view_normal, vert_normals, verts, factors);
  }

  tls.distances.reinitialize(verts.size());
  const MutableSpan<float> distances = tls.distances;
  calc_distance_falloff(
      ss, positions_eval, verts, eBrushFalloffShape(brush.falloff_shape), distances, factors);
  calc_brush_strength_factors(cache, brush, distances, factors);

  if (cache.automasking) {
    auto_mask::calc_vert_factors(object, *cache.automasking, node, verts, factors);
  }

  calc_brush_texture_factors(ss, brush, positions_eval, verts, factors);

  tls.translations.reinitialize(verts.size());
  const MutableSpan<float3> translations = tls.translations;

  calc_closest_to_plane(test_plane, positions_eval, verts, translations);
  scale_translations(translations, strength);
  scale_translations(translations, factors);

  write_translations(sd, object, positions_eval, verts, translations, positions_orig);
}

static void calc_grids(Object &object,
                       const Brush &brush,
                       const float4 &test_plane,
                       const float strength,
                       PBVHNode &node)
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

  /* TODO: Remove usage of proxies. */
  const MutableSpan<float3> proxy = BKE_pbvh_node_add_proxy(*ss.pbvh, node).co;
  int i = 0;
  for (const int grid : bke::pbvh::node_grid_indices(node)) {
    const int grid_verts_start = grid * key.grid_area;
    CCGElem *elem = grids[grid];
    for (const int j : IndexRange(key.grid_area)) {
      if (!grid_hidden.is_empty() && grid_hidden[grid][j]) {
        i++;
        continue;
      }
      const float3 &co = CCG_elem_offset_co(key, elem, j);
      if (!sculpt_brush_test_sq_fn(test, co)) {
        i++;
        continue;
      }

      float3 intr;
      closest_to_plane_normalized_v3(intr, test_plane, co);

      float3 offset;
      sub_v3_v3v3(offset, intr, co);

      auto_mask::node_update(automask_data, i);
      const float fade = SCULPT_brush_strength_factor(
          ss,
          brush,
          co,
          math::sqrt(test.dist),
          CCG_elem_offset_no(key, elem, j),
          nullptr,
          key.has_mask ? CCG_elem_offset_mask(key, elem, j) : 0.0f,
          BKE_pbvh_make_vref(grid_verts_start + j),
          thread_id,
          &automask_data);
      proxy[i] = offset * fade * strength;
      i++;
    }
  }
}

static void calc_bmesh(Object &object,
                       const Brush &brush,
                       const float4 &test_plane,
                       const float strength,
                       PBVHNode &node)
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

  /* TODO: Remove usage of proxies. */
  const MutableSpan<float3> proxy = BKE_pbvh_node_add_proxy(*ss.pbvh, node).co;
  int i = 0;
  for (BMVert *vert : BKE_pbvh_bmesh_node_unique_verts(&node)) {
    if (BM_elem_flag_test(vert, BM_ELEM_HIDDEN)) {
      i++;
      continue;
    }
    if (!sculpt_brush_test_sq_fn(test, vert->co)) {
      i++;
      continue;
    }

    float3 intr;
    closest_to_plane_normalized_v3(intr, test_plane, vert->co);

    float3 offset;
    sub_v3_v3v3(offset, intr, vert->co);

    auto_mask::node_update(automask_data, *vert);
    const float mask = mask_offset == -1 ? 0.0f : BM_ELEM_CD_GET_FLOAT(vert, mask_offset);
    const float fade = SCULPT_brush_strength_factor(ss,
                                                    brush,
                                                    vert->co,
                                                    math::sqrt(test.dist),
                                                    vert->no,
                                                    nullptr,
                                                    mask,
                                                    BKE_pbvh_make_vref(intptr_t(vert)),
                                                    thread_id,
                                                    &automask_data);
    proxy[i] = offset * fade * strength;
    i++;
  }
}

}  // namespace clay_cc
void do_clay_brush(const Sculpt &sd, Object &object, Span<PBVHNode *> nodes)
{
  SculptSession &ss = *object.sculpt;
  const Brush &brush = *BKE_paint_brush_for_read(&sd.paint);

  float3 area_no;
  float3 area_co;
  calc_brush_plane(brush, object, nodes, area_no, area_co);

  const float initial_radius = fabsf(ss.cache->initial_radius);
  const float offset = SCULPT_brush_plane_offset_get(sd, ss);

  /* This implementation skips a factor calculation as it currently has
   * no user-facing impact (i.e. is effectively a constant)
   * See: #123518 */
  float displace = fabsf(initial_radius * (0.25f + offset + 0.15f));

  const bool flip = ss.cache->bstrength < 0.0f;
  if (flip) {
    displace = -displace;
  }

  const float3 modified_area_co = ss.cache->location + (area_no * ss.cache->scale * displace);

  float4 test_plane;
  plane_from_point_normal_v3(test_plane, modified_area_co, area_no);
  BLI_ASSERT_UNIT_V3(test_plane);

  const float bstrength = fabsf(ss.cache->bstrength);
  switch (BKE_pbvh_type(*object.sculpt->pbvh)) {
    case PBVH_FACES: {
      threading::EnumerableThreadSpecific<LocalData> all_tls;
      Mesh &mesh = *static_cast<Mesh *>(object.data);
      const PBVH &pbvh = *ss.pbvh;
      const Span<float3> positions_eval = BKE_pbvh_get_vert_positions(pbvh);
      const Span<float3> vert_normals = BKE_pbvh_get_vert_normals(pbvh);
      MutableSpan<float3> positions_orig = mesh.vert_positions_for_write();
      threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
        LocalData &tls = all_tls.local();
        for (const int i : range) {
          calc_faces(sd,
                     brush,
                     test_plane,
                     bstrength,
                     positions_eval,
                     vert_normals,
                     *nodes[i],
                     object,
                     tls,
                     positions_orig);
          BKE_pbvh_node_mark_positions_update(nodes[i]);
        }
      });
      break;
    }
    case PBVH_GRIDS:
      threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
        for (const int i : range) {
          calc_grids(object, brush, test_plane, bstrength, *nodes[i]);
        }
      });
      break;
    case PBVH_BMESH:
      threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
        for (const int i : range) {
          calc_bmesh(object, brush, test_plane, bstrength, *nodes[i]);
        }
      });
      break;
  }
}
}  // namespace blender::ed::sculpt_paint
