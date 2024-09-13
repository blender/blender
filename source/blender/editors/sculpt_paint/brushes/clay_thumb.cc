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
#include "BLI_math_geom.h"
#include "BLI_math_matrix.h"
#include "BLI_math_matrix.hh"
#include "BLI_math_rotation.h"
#include "BLI_math_vector.h"
#include "BLI_math_vector.hh"
#include "BLI_task.h"
#include "BLI_task.hh"

#include "editors/sculpt_paint/mesh_brush_common.hh"
#include "editors/sculpt_paint/sculpt_intern.hh"

namespace blender::ed::sculpt_paint {

inline namespace clay_thumb_cc {

struct LocalData {
  Vector<float3> positions;
  Vector<float> factors;
  Vector<float> distances;
  Vector<float3> translations;
};

static void calc_faces(const Depsgraph &depsgraph,
                       const Sculpt &sd,
                       const Brush &brush,
                       const float4 &plane_tilt,
                       const float strength,
                       const Span<float3> positions_eval,
                       const Span<float3> vert_normals,
                       const bke::pbvh::MeshNode &node,
                       Object &object,
                       LocalData &tls,
                       const MutableSpan<float3> positions_orig)
{
  SculptSession &ss = *object.sculpt;
  const StrokeCache &cache = *ss.cache;
  Mesh &mesh = *static_cast<Mesh *>(object.data);

  const Span<int> verts = node.verts();
  const MutableSpan positions = gather_data_mesh(positions_eval, verts, tls.positions);

  tls.factors.resize(verts.size());
  const MutableSpan<float> factors = tls.factors;
  fill_factor_from_hide_and_mask(mesh, verts, factors);
  filter_region_clip_factors(ss, positions, factors);

  if (brush.flag & BRUSH_FRONTFACE) {
    calc_front_face(cache.view_normal_symm, vert_normals, verts, factors);
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

  tls.translations.resize(verts.size());
  const MutableSpan<float3> translations = tls.translations;
  calc_translations_to_plane(positions, plane_tilt, translations);

  scale_translations(translations, factors);

  write_translations(depsgraph, sd, object, positions_eval, verts, translations, positions_orig);
}

static void calc_grids(const Depsgraph &depsgraph,
                       const Sculpt &sd,
                       const Brush &brush,
                       const float4 &plane_tilt,
                       const float strength,
                       const bke::pbvh::GridsNode &node,
                       Object &object,
                       LocalData &tls)
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

  tls.translations.resize(positions.size());
  const MutableSpan<float3> translations = tls.translations;
  calc_translations_to_plane(positions, plane_tilt, translations);

  scale_translations(translations, factors);

  clip_and_lock_translations(sd, ss, positions, translations);
  apply_translations(translations, grids, subdiv_ccg);
}

static void calc_bmesh(const Depsgraph &depsgraph,
                       const Sculpt &sd,
                       const Brush &brush,
                       const float4 &plane_tilt,
                       const float strength,
                       Object &object,
                       bke::pbvh::BMeshNode &node,
                       LocalData &tls)
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

  tls.translations.resize(verts.size());
  const MutableSpan<float3> translations = tls.translations;
  calc_translations_to_plane(positions, plane_tilt, translations);

  scale_translations(translations, factors);

  clip_and_lock_translations(sd, ss, positions, translations);
  apply_translations(translations, verts);
}

}  // namespace clay_thumb_cc

void do_clay_thumb_brush(const Depsgraph &depsgraph,
                         const Sculpt &sd,
                         Object &object,
                         const IndexMask &node_mask)
{
  const SculptSession &ss = *object.sculpt;
  bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
  const Brush &brush = *BKE_paint_brush_for_read(&sd.paint);
  const float3 &location = ss.cache->location_symm;

  /* Sampled geometry normal and area center. */
  float3 area_no_sp;
  float3 area_no;
  float3 area_co_tmp;

  float4x4 tmat;

  calc_brush_plane(depsgraph, brush, object, node_mask, area_no_sp, area_co_tmp);

  if (brush.sculpt_plane != SCULPT_DISP_DIR_AREA || (brush.flag & BRUSH_ORIGINAL_NORMAL)) {
    area_no = calc_area_normal(depsgraph, brush, object, node_mask).value_or(float3(0));
  }
  else {
    area_no = area_no_sp;
  }

  /* Delay the first daub because grab delta is not setup. */
  if (SCULPT_stroke_is_first_brush_step_of_symmetry_pass(*ss.cache)) {
    ss.cache->clay_thumb_brush.front_angle = 0.0f;
    return;
  }

  /* Simulate the clay accumulation by increasing the plane angle as more samples are added to the
   * stroke. */
  if (SCULPT_stroke_is_main_symmetry_pass(*ss.cache)) {
    ss.cache->clay_thumb_brush.front_angle += 0.8f;
    ss.cache->clay_thumb_brush.front_angle = std::clamp(
        ss.cache->clay_thumb_brush.front_angle, 0.0f, 60.0f);
  }

  if (math::is_zero(ss.cache->grab_delta_symm)) {
    return;
  }

  /* Initialize brush local-space matrix. */
  float4x4 mat = float4x4::identity();
  mat.x_axis() = math::cross(area_no, ss.cache->grab_delta_symm);
  mat.y_axis() = math::cross(area_no, mat.x_axis());
  mat.z_axis() = area_no;
  mat.location() = ss.cache->location_symm;
  normalize_m4(mat.ptr());

  /* Scale brush local space matrix. */
  float4x4 scale = math::from_scale<float4x4>(float3(ss.cache->radius));
  mul_m4_m4m4(tmat.ptr(), mat.ptr(), scale.ptr());
  invert_m4_m4(mat.ptr(), tmat.ptr());

  float clay_strength = ss.cache->bstrength * clay_thumb_get_stabilized_pressure(*ss.cache);

  float4 plane_tilt;
  float3 normal_tilt;
  float4x4 imat;

  invert_m4_m4(imat.ptr(), mat.ptr());
  rotate_v3_v3v3fl(
      normal_tilt, area_no_sp, imat[0], DEG2RADF(-ss.cache->clay_thumb_brush.front_angle));

  /* Tilted plane (front part of the brush). */
  plane_from_point_normal_v3(plane_tilt, location, normal_tilt);

  threading::EnumerableThreadSpecific<LocalData> all_tls;
  switch (pbvh.type()) {
    case bke::pbvh::Type::Mesh: {
      MutableSpan<bke::pbvh::MeshNode> nodes = pbvh.nodes<bke::pbvh::MeshNode>();
      Mesh &mesh = *static_cast<Mesh *>(object.data);
      const Span<float3> positions_eval = bke::pbvh::vert_positions_eval(depsgraph, object);
      const Span<float3> vert_normals = bke::pbvh::vert_normals_eval(depsgraph, object);
      MutableSpan<float3> positions_orig = mesh.vert_positions_for_write();
      threading::parallel_for(node_mask.index_range(), 1, [&](const IndexRange range) {
        LocalData &tls = all_tls.local();
        node_mask.slice(range).foreach_index([&](const int i) {
          calc_faces(depsgraph,
                     sd,
                     brush,
                     plane_tilt,
                     clay_strength,
                     positions_eval,
                     vert_normals,
                     nodes[i],
                     object,
                     tls,
                     positions_orig);
          bke::pbvh::update_node_bounds_mesh(positions_eval, nodes[i]);
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
          calc_grids(depsgraph, sd, brush, plane_tilt, clay_strength, nodes[i], object, tls);
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
          calc_bmesh(depsgraph, sd, brush, plane_tilt, clay_strength, object, nodes[i], tls);
          bke::pbvh::update_node_bounds_bmesh(nodes[i]);
        });
      });
      break;
    }
  }
  pbvh.tag_positions_changed(node_mask);
  bke::pbvh::flush_bounds_to_parents(pbvh);
}

float clay_thumb_get_stabilized_pressure(const StrokeCache &cache)
{
  const float pressure_sum = std::accumulate(cache.clay_thumb_brush.pressure_stabilizer.begin(),
                                             cache.clay_thumb_brush.pressure_stabilizer.end(),
                                             0.0f);
  return pressure_sum / cache.clay_thumb_brush.pressure_stabilizer.size();
}

}  // namespace blender::ed::sculpt_paint
