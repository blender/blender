/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "editors/sculpt_paint/brushes/types.hh"

#include "BKE_mesh.hh"
#include "BKE_paint.hh"
#include "BKE_paint_bvh.hh"
#include "BKE_subdiv_ccg.hh"
#include "DNA_brush_types.h"
#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BLI_enumerable_thread_specific.hh"
#include "BLI_math_matrix.h"
#include "BLI_math_vector.hh"
#include "BLI_task.hh"

#include "editors/sculpt_paint/mesh_brush_common.hh"
#include "editors/sculpt_paint/sculpt_automask.hh"
#include "editors/sculpt_paint/sculpt_intern.hh"

#include "bmesh.hh"

namespace blender::ed::sculpt_paint {

inline namespace pinch_cc {

struct LocalData {
  Vector<float3> positions;
  Vector<float> factors;
  Vector<float> distances;
  Vector<float3> translations;
};

BLI_NOINLINE static void calc_translations(const Span<float3> positions,
                                           const float3 &location,
                                           const std::array<float3, 2> &stroke_xz,
                                           const MutableSpan<float3> translations)
{
  BLI_assert(positions.size() == translations.size());

  for (const int i : positions.index_range()) {
    /* Calculate displacement from the vertex to the brush center. */
    const float3 disp_center = location - positions[i];

    /* Project the displacement into the X vector (aligned to the stroke). */
    const float3 x_disp = stroke_xz[0] * math::dot(disp_center, stroke_xz[0]);

    /* Project the displacement into the Z vector (aligned to the surface normal). */
    const float3 z_disp = stroke_xz[1] * math::dot(disp_center, stroke_xz[1]);

    /* Add the two projected vectors to calculate the final displacement.
     * The Y component is removed. */
    translations[i] = x_disp + z_disp;
  }
}

static void calc_faces(const Depsgraph &depsgraph,
                       const Sculpt &sd,
                       const Brush &brush,
                       const std::array<float3, 2> &stroke_xz,
                       const float strength,
                       const MeshAttributeData &attribute_data,
                       const Span<float3> vert_normals,
                       const bke::pbvh::MeshNode &node,
                       Object &object,
                       LocalData &tls,
                       const PositionDeformData &position_data)
{
  SculptSession &ss = *object.sculpt;
  const StrokeCache &cache = *ss.cache;

  const Span<int> verts = node.verts();
  const MutableSpan positions = gather_data_mesh(position_data.eval, verts, tls.positions);

  calc_factors_common_mesh(depsgraph,
                           brush,
                           object,
                           attribute_data,
                           positions,
                           vert_normals,
                           node,
                           tls.factors,
                           tls.distances);

  scale_factors(tls.factors, strength);

  tls.translations.resize(verts.size());
  const MutableSpan<float3> translations = tls.translations;
  calc_translations(positions, cache.location_symm, stroke_xz, translations);
  if (brush.falloff_shape == PAINT_FALLOFF_SHAPE_TUBE) {
    project_translations(translations, cache.view_normal_symm);
  }

  scale_translations(translations, tls.factors);

  clip_and_lock_translations(sd, ss, position_data.eval, verts, translations);
  position_data.deform(translations, verts);
}

static void calc_grids(const Depsgraph &depsgraph,
                       const Sculpt &sd,
                       Object &object,
                       const Brush &brush,
                       const std::array<float3, 2> &stroke_xz,
                       const float strength,
                       const bke::pbvh::GridsNode &node,
                       LocalData &tls)
{
  SculptSession &ss = *object.sculpt;
  const StrokeCache &cache = *ss.cache;
  SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;

  const Span<int> grids = node.grids();
  const MutableSpan positions = gather_grids_positions(subdiv_ccg, grids, tls.positions);

  calc_factors_common_grids(depsgraph, brush, object, positions, node, tls.factors, tls.distances);

  scale_factors(tls.factors, strength);

  tls.translations.resize(positions.size());
  const MutableSpan<float3> translations = tls.translations;
  calc_translations(positions, cache.location_symm, stroke_xz, translations);
  if (brush.falloff_shape == PAINT_FALLOFF_SHAPE_TUBE) {
    project_translations(translations, cache.view_normal_symm);
  }

  scale_translations(translations, tls.factors);

  clip_and_lock_translations(sd, ss, positions, translations);
  apply_translations(translations, grids, subdiv_ccg);
}

static void calc_bmesh(const Depsgraph &depsgraph,
                       const Sculpt &sd,
                       Object &object,
                       const Brush &brush,
                       const std::array<float3, 2> &stroke_xz,
                       const float strength,
                       bke::pbvh::BMeshNode &node,
                       LocalData &tls)
{
  SculptSession &ss = *object.sculpt;
  const StrokeCache &cache = *ss.cache;

  const Set<BMVert *, 0> &verts = BKE_pbvh_bmesh_node_unique_verts(&node);

  const MutableSpan positions = gather_bmesh_positions(verts, tls.positions);

  calc_factors_common_bmesh(depsgraph, brush, object, positions, node, tls.factors, tls.distances);

  scale_factors(tls.factors, strength);

  tls.translations.resize(verts.size());
  const MutableSpan<float3> translations = tls.translations;
  calc_translations(positions, cache.location_symm, stroke_xz, translations);
  if (brush.falloff_shape == PAINT_FALLOFF_SHAPE_TUBE) {
    project_translations(translations, cache.view_normal_symm);
  }

  scale_translations(translations, tls.factors);

  clip_and_lock_translations(sd, ss, positions, translations);
  apply_translations(translations, verts);
}

}  // namespace pinch_cc

void do_pinch_brush(const Depsgraph &depsgraph,
                    const Sculpt &sd,
                    Object &object,
                    const IndexMask &node_mask)
{
  const SculptSession &ss = *object.sculpt;
  bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
  const Brush &brush = *BKE_paint_brush_for_read(&sd.paint);

  float3 area_no;
  float3 area_co;
  calc_brush_plane(depsgraph, brush, object, node_mask, area_no, area_co);

  /* delay the first daub because grab delta is not setup */
  if (SCULPT_stroke_is_first_brush_step_of_symmetry_pass(*ss.cache)) {
    return;
  }

  if (math::is_zero(ss.cache->grab_delta_symm)) {
    return;
  }

  /* Initialize `mat`. */
  float4x4 mat = float4x4::identity();
  mat.x_axis() = math::cross(area_no, ss.cache->grab_delta_symm);
  mat.y_axis() = math::cross(area_no, mat.x_axis());
  mat.z_axis() = area_no;
  mat.location() = ss.cache->location_symm;
  normalize_m4(mat.ptr());

  const std::array<float3, 2> stroke_xz{math::normalize(mat.x_axis()),
                                        math::normalize(mat.z_axis())};

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
                   stroke_xz,
                   ss.cache->bstrength,
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
        calc_grids(depsgraph, sd, object, brush, stroke_xz, ss.cache->bstrength, nodes[i], tls);
        bke::pbvh::update_node_bounds_grids(subdiv_ccg.grid_area, positions, nodes[i]);
      });
      break;
    }
    case bke::pbvh::Type::BMesh: {
      MutableSpan<bke::pbvh::BMeshNode> nodes = pbvh.nodes<bke::pbvh::BMeshNode>();
      node_mask.foreach_index(GrainSize(1), [&](const int i) {
        LocalData &tls = all_tls.local();
        calc_bmesh(depsgraph, sd, object, brush, stroke_xz, ss.cache->bstrength, nodes[i], tls);
        bke::pbvh::update_node_bounds_bmesh(nodes[i]);
      });
      break;
    }
  }
  pbvh.tag_positions_changed(node_mask);
  pbvh.flush_bounds_to_parents();
}

}  // namespace blender::ed::sculpt_paint
