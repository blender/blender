/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "editors/sculpt_paint/brushes/types.hh"

#include "DNA_brush_types.h"
#include "DNA_mesh_types.h"

#include "BKE_paint_bvh.hh"
#include "BKE_subdiv_ccg.hh"

#include "BLI_enumerable_thread_specific.hh"
#include "BLI_math_geom.h"

#include "editors/sculpt_paint/mesh_brush_common.hh"
#include "editors/sculpt_paint/sculpt_automask.hh"
#include "editors/sculpt_paint/sculpt_intern.hh"

#include "bmesh.hh"

namespace blender::ed::sculpt_paint {
inline namespace clay_cc {
struct LocalData {
  Vector<float3> positions;
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

BLI_NOINLINE static void calc_closest_to_plane(const float4 &test_plane,
                                               const Span<float3> positions,
                                               const MutableSpan<float3> translations)
{
  /* Equivalent to #closest_to_plane_normalized_v3 */
  BLI_assert(positions.size() == translations.size());
  for (const int i : positions.index_range()) {
    const float side = plane_point_side_v3(test_plane, positions[i]);
    translations[i] = float3(test_plane) * -side;
  }
}

static void calc_faces(const Depsgraph &depsgraph,
                       const Sculpt &sd,
                       const Brush &brush,
                       const float4 &test_plane,
                       const float strength,
                       const MeshAttributeData &attribute_data,
                       const Span<float3> vert_normals,
                       const bke::pbvh::MeshNode &node,
                       Object &object,
                       LocalData &tls,
                       const PositionDeformData &position_data)
{
  SculptSession &ss = *object.sculpt;

  const Span<int> verts = node.verts();

  calc_factors_common_mesh_indexed(depsgraph,
                                   brush,
                                   object,
                                   attribute_data,
                                   position_data.eval,
                                   vert_normals,
                                   node,
                                   tls.factors,
                                   tls.distances);

  tls.translations.resize(verts.size());
  const MutableSpan<float3> translations = tls.translations;

  calc_closest_to_plane(test_plane, position_data.eval, verts, translations);
  scale_translations(translations, strength);
  scale_translations(translations, tls.factors);

  clip_and_lock_translations(sd, ss, position_data.eval, verts, translations);
  position_data.deform(translations, verts);
}

static void calc_grids(const Depsgraph &depsgraph,
                       const Sculpt &sd,
                       Object &object,
                       const Brush &brush,
                       const float4 &test_plane,
                       const float strength,
                       bke::pbvh::GridsNode &node,
                       LocalData &tls)
{
  SculptSession &ss = *object.sculpt;
  SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;

  const Span<int> grids = node.grids();
  const MutableSpan positions = gather_grids_positions(subdiv_ccg, grids, tls.positions);

  calc_factors_common_grids(depsgraph, brush, object, positions, node, tls.factors, tls.distances);

  tls.translations.resize(positions.size());
  const MutableSpan<float3> translations = tls.translations;

  calc_closest_to_plane(test_plane, positions, translations);
  scale_translations(translations, strength);
  scale_translations(translations, tls.factors);

  clip_and_lock_translations(sd, ss, positions, translations);
  apply_translations(translations, grids, subdiv_ccg);
}

static void calc_bmesh(const Depsgraph &depsgraph,
                       const Sculpt &sd,
                       Object &object,
                       const Brush &brush,
                       const float4 &test_plane,
                       const float strength,
                       bke::pbvh::BMeshNode &node,
                       LocalData &tls)
{
  SculptSession &ss = *object.sculpt;

  const Set<BMVert *, 0> &verts = BKE_pbvh_bmesh_node_unique_verts(&node);
  const MutableSpan positions = gather_bmesh_positions(verts, tls.positions);

  calc_factors_common_bmesh(depsgraph, brush, object, positions, node, tls.factors, tls.distances);

  tls.translations.resize(verts.size());
  const MutableSpan<float3> translations = tls.translations;

  calc_closest_to_plane(test_plane, positions, translations);
  scale_translations(translations, strength);
  scale_translations(translations, tls.factors);

  clip_and_lock_translations(sd, ss, positions, translations);
  apply_translations(translations, verts);
}

}  // namespace clay_cc

void do_clay_brush(const Depsgraph &depsgraph,
                   const Sculpt &sd,
                   Object &object,
                   const IndexMask &node_mask)
{
  SculptSession &ss = *object.sculpt;
  bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
  const Brush &brush = *BKE_paint_brush_for_read(&sd.paint);

  float3 area_no;
  float3 area_co;
  calc_brush_plane(depsgraph, brush, object, node_mask, area_no, area_co);

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

  const float3 modified_area_co = ss.cache->location_symm + (area_no * ss.cache->scale * displace);

  float4 test_plane;
  plane_from_point_normal_v3(test_plane, modified_area_co, area_no);
  BLI_ASSERT_UNIT_V3(test_plane);

  const float bstrength = fabsf(ss.cache->bstrength);
  threading::EnumerableThreadSpecific<LocalData> all_tls;
  switch (pbvh.type()) {
    case bke::pbvh::Type::Mesh: {
      const Mesh &mesh = *static_cast<Mesh *>(object.data);
      const MeshAttributeData attribute_data(mesh);
      const PositionDeformData position_data(depsgraph, object);
      const Span<float3> vert_normals = bke::pbvh::vert_normals_eval(depsgraph, object);
      MutableSpan<bke::pbvh::MeshNode> nodes = pbvh.nodes<bke::pbvh::MeshNode>();
      node_mask.foreach_index(GrainSize(1), [&](const int i) {
        LocalData &tls = all_tls.local();
        calc_faces(depsgraph,
                   sd,
                   brush,
                   test_plane,
                   bstrength,
                   attribute_data,
                   vert_normals,
                   nodes[i],
                   object,
                   tls,
                   position_data);
        bke::pbvh::update_node_bounds_mesh(position_data.eval, nodes[i]);
      });
      break;
    }
    case bke::pbvh::Type::Grids: {
      SubdivCCG &subdiv_ccg = *object.sculpt->subdiv_ccg;
      MutableSpan<float3> positions = subdiv_ccg.positions;
      MutableSpan<bke::pbvh::GridsNode> nodes = pbvh.nodes<bke::pbvh::GridsNode>();
      node_mask.foreach_index(GrainSize(1), [&](const int i) {
        LocalData &tls = all_tls.local();
        calc_grids(depsgraph, sd, object, brush, test_plane, bstrength, nodes[i], tls);
        bke::pbvh::update_node_bounds_grids(subdiv_ccg.grid_area, positions, nodes[i]);
      });
      break;
    }
    case bke::pbvh::Type::BMesh: {
      MutableSpan<bke::pbvh::BMeshNode> nodes = pbvh.nodes<bke::pbvh::BMeshNode>();
      node_mask.foreach_index(GrainSize(1), [&](const int i) {
        LocalData &tls = all_tls.local();
        calc_bmesh(depsgraph, sd, object, brush, test_plane, bstrength, nodes[i], tls);
        bke::pbvh::update_node_bounds_bmesh(nodes[i]);
      });
      break;
    }
  }
  pbvh.tag_positions_changed(node_mask);
  pbvh.flush_bounds_to_parents();
}

}  // namespace blender::ed::sculpt_paint
