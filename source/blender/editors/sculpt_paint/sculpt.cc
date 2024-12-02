/* SPDX-FileCopyrightText: 2006 by Nicholas Bishop. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edsculpt
 * Implements the Sculpt Mode Brushes.
 */

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iostream>

#include "MEM_guardedalloc.h"

#include "CLG_log.h"

#include "BLI_array_utils.hh"
#include "BLI_atomic_disjoint_set.hh"
#include "BLI_bit_span_ops.hh"
#include "BLI_blenlib.h"
#include "BLI_dial_2d.h"
#include "BLI_enumerable_thread_specific.hh"
#include "BLI_ghash.h"
#include "BLI_math_geom.h"
#include "BLI_math_matrix.h"
#include "BLI_math_matrix.hh"
#include "BLI_math_rotation.h"
#include "BLI_math_rotation.hh"
#include "BLI_set.hh"
#include "BLI_span.hh"
#include "BLI_task.h"
#include "BLI_task.hh"
#include "BLI_utildefines.h"
#include "BLI_vector.hh"

#include "DNA_brush_types.h"
#include "DNA_customdata_types.h"
#include "DNA_key_types.h"
#include "DNA_node_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_attribute.hh"
#include "BKE_brush.hh"
#include "BKE_ccg.hh"
#include "BKE_colortools.hh"
#include "BKE_context.hh"
#include "BKE_customdata.hh"
#include "BKE_global.hh"
#include "BKE_image.hh"
#include "BKE_key.hh"
#include "BKE_layer.hh"
#include "BKE_mesh.hh"
#include "BKE_modifier.hh"
#include "BKE_multires.hh"
#include "BKE_node_runtime.hh"
#include "BKE_object.hh"
#include "BKE_object_types.hh"
#include "BKE_paint.hh"
#include "BKE_paint_bvh.hh"
#include "BKE_report.hh"
#include "BKE_subdiv_ccg.hh"
#include "BKE_subsurf.hh"
#include "BLI_math_vector.hh"

#include "NOD_texture.h"

#include "DEG_depsgraph.hh"

#include "WM_api.hh"
#include "WM_toolsystem.hh"
#include "WM_types.hh"

#include "ED_gpencil_legacy.hh"
#include "ED_paint.hh"
#include "ED_screen.hh"
#include "ED_sculpt.hh"
#include "ED_view3d.hh"

#include "paint_intern.hh"
#include "sculpt_automask.hh"
#include "sculpt_boundary.hh"
#include "sculpt_cloth.hh"
#include "sculpt_color.hh"
#include "sculpt_dyntopo.hh"
#include "sculpt_face_set.hh"
#include "sculpt_filter.hh"
#include "sculpt_hide.hh"
#include "sculpt_intern.hh"
#include "sculpt_islands.hh"
#include "sculpt_pose.hh"
#include "sculpt_undo.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "bmesh.hh"

#include "editors/sculpt_paint/brushes/types.hh"
#include "mesh_brush_common.hh"
#include "sculpt_automask.hh"

using blender::float3;
using blender::MutableSpan;
using blender::Set;
using blender::Span;
using blender::Vector;

static CLG_LogRef LOG = {"ed.sculpt_paint"};

namespace blender::ed::sculpt_paint {

float sculpt_calc_radius(const ViewContext &vc,
                         const Brush &brush,
                         const Scene &scene,
                         const float3 location)
{
  if (!BKE_brush_use_locked_size(&scene, &brush)) {
    return paint_calc_object_space_radius(vc, location, BKE_brush_size_get(&scene, &brush));
  }
  else {
    return BKE_brush_unprojected_radius_get(&scene, &brush);
  }
}

bool report_if_shape_key_is_locked(const Object &ob, ReportList *reports)
{
  SculptSession &ss = *ob.sculpt;

  if (ss.shapekey_active && (ss.shapekey_active->flag & KEYBLOCK_LOCKED_SHAPE) != 0) {
    if (reports) {
      BKE_reportf(reports, RPT_ERROR, "The active shape key of %s is locked", ob.id.name + 2);
    }
    return true;
  }

  return false;
}

}  // namespace blender::ed::sculpt_paint

void SCULPT_vertex_random_access_ensure(Object &object)
{
  SculptSession &ss = *object.sculpt;
  if (blender::bke::object::pbvh_get(object)->type() == blender::bke::pbvh::Type::BMesh) {
    BM_mesh_elem_index_ensure(ss.bm, BM_VERT);
    BM_mesh_elem_table_ensure(ss.bm, BM_VERT);
  }
}

int SCULPT_vertex_count_get(const Object &object)
{
  const SculptSession &ss = *object.sculpt;
  switch (blender::bke::object::pbvh_get(object)->type()) {
    case blender::bke::pbvh::Type::Mesh:
      BLI_assert(object.type == OB_MESH);
      return static_cast<const Mesh *>(object.data)->verts_num;
    case blender::bke::pbvh::Type::BMesh:
      return BM_mesh_elem_count(ss.bm, BM_VERT);
    case blender::bke::pbvh::Type::Grids:
      return BKE_pbvh_get_grid_num_verts(object);
  }

  return 0;
}

namespace blender::ed::sculpt_paint {

Span<float3> vert_positions_for_grab_active_get(const Depsgraph &depsgraph, const Object &object)
{
  const SculptSession &ss = *object.sculpt;
  BLI_assert(bke::object::pbvh_get(object)->type() == bke::pbvh::Type::Mesh);
  if (ss.shapekey_active) {
    /* Always grab active shape key if the sculpt happens on shapekey. */
    return bke::pbvh::vert_positions_eval(depsgraph, object);
  }
  /* Otherwise use the base mesh positions. */
  const Mesh &mesh = *static_cast<const Mesh *>(object.data);
  return mesh.vert_positions();
}

}  // namespace blender::ed::sculpt_paint

ePaintSymmetryFlags SCULPT_mesh_symmetry_xyz_get(const Object &object)
{
  const Mesh *mesh = static_cast<const Mesh *>(object.data);
  return ePaintSymmetryFlags(mesh->symmetry);
}

/* Sculpt Face Sets and Visibility. */

namespace blender::ed::sculpt_paint {

namespace face_set {

int active_face_set_get(const Object &object)
{
  const SculptSession &ss = *object.sculpt;
  switch (bke::object::pbvh_get(object)->type()) {
    case bke::pbvh::Type::Mesh: {
      const Mesh &mesh = *static_cast<const Mesh *>(object.data);
      const bke::AttributeAccessor attributes = mesh.attributes();
      const VArray face_sets = *attributes.lookup<int>(".sculpt_face_set", bke::AttrDomain::Face);
      if (!face_sets || !ss.active_face_index) {
        return SCULPT_FACE_SET_NONE;
      }
      return face_sets[*ss.active_face_index];
    }
    case bke::pbvh::Type::Grids: {
      const Mesh &mesh = *static_cast<const Mesh *>(object.data);
      const bke::AttributeAccessor attributes = mesh.attributes();
      const VArray face_sets = *attributes.lookup<int>(".sculpt_face_set", bke::AttrDomain::Face);
      if (!face_sets || !ss.active_grid_index) {
        return SCULPT_FACE_SET_NONE;
      }
      const int face_index = BKE_subdiv_ccg_grid_to_face_index(*ss.subdiv_ccg,
                                                               *ss.active_grid_index);
      return face_sets[face_index];
    }
    case bke::pbvh::Type::BMesh:
      return SCULPT_FACE_SET_NONE;
  }
  return SCULPT_FACE_SET_NONE;
}

}  // namespace face_set

namespace face_set {

int vert_face_set_get(const GroupedSpan<int> vert_to_face_map,
                      const Span<int> face_sets,
                      const int vert)
{
  int face_set = SCULPT_FACE_SET_NONE;
  for (const int face : vert_to_face_map[vert]) {
    face_set = std::max(face_sets[face], face_set);
  }
  return face_set;
}

int vert_face_set_get(const SubdivCCG &subdiv_ccg, const Span<int> face_sets, const int grid)
{
  const int face = BKE_subdiv_ccg_grid_to_face_index(subdiv_ccg, grid);
  return face_sets[face];
}

int vert_face_set_get(const int /*face_set_offset*/, const BMVert & /*vert*/)
{
  return SCULPT_FACE_SET_NONE;
}

bool vert_has_face_set(const GroupedSpan<int> vert_to_face_map,
                       const Span<int> face_sets,
                       const int vert,
                       const int face_set)
{
  if (face_sets.is_empty()) {
    return face_set == SCULPT_FACE_SET_NONE;
  }
  const Span<int> faces = vert_to_face_map[vert];
  return std::any_of(
      faces.begin(), faces.end(), [&](const int face) { return face_sets[face] == face_set; });
}

bool vert_has_face_set(const SubdivCCG &subdiv_ccg,
                       const Span<int> face_sets,
                       const int grid,
                       const int face_set)
{
  if (face_sets.is_empty()) {
    return face_set == SCULPT_FACE_SET_NONE;
  }
  const int face = BKE_subdiv_ccg_grid_to_face_index(subdiv_ccg, grid);
  return face_sets[face] == face_set;
}

bool vert_has_face_set(const int face_set_offset, const BMVert &vert, const int face_set)
{
  if (face_set_offset == -1) {
    return false;
  }
  BMIter iter;
  BMFace *face;
  BM_ITER_ELEM (face, &iter, &const_cast<BMVert &>(vert), BM_FACES_OF_VERT) {
    if (BM_ELEM_CD_GET_INT(face, face_set_offset) == face_set) {
      return true;
    }
  }
  return false;
}

bool vert_has_unique_face_set(const GroupedSpan<int> vert_to_face_map,
                              const Span<int> face_sets,
                              int vert)
{
  /* TODO: Move this check higher out of this function. */
  if (face_sets.is_empty()) {
    return true;
  }
  int face_set = -1;
  for (const int face_index : vert_to_face_map[vert]) {
    if (face_set == -1) {
      face_set = face_sets[face_index];
    }
    else {
      if (face_sets[face_index] != face_set) {
        return false;
      }
    }
  }
  return true;
}

/**
 * Checks if the face sets of the adjacent faces to the edge between \a v1 and \a v2
 * in the base mesh are equal.
 */
static bool sculpt_check_unique_face_set_for_edge_in_base_mesh(
    const GroupedSpan<int> vert_to_face_map,
    const Span<int> face_sets,
    const Span<int> corner_verts,
    const OffsetIndices<int> faces,
    int v1,
    int v2)
{
  const Span<int> vert_map = vert_to_face_map[v1];
  int p1 = -1, p2 = -1;
  for (int i = 0; i < vert_map.size(); i++) {
    const int face_i = vert_map[i];
    for (const int corner : faces[face_i]) {
      if (corner_verts[corner] == v2) {
        if (p1 == -1) {
          p1 = vert_map[i];
          break;
        }

        if (p2 == -1) {
          p2 = vert_map[i];
          break;
        }
      }
    }
  }

  if (p1 != -1 && p2 != -1) {
    return face_sets[p1] == face_sets[p2];
  }
  return true;
}

bool vert_has_unique_face_set(const OffsetIndices<int> faces,
                              const Span<int> corner_verts,
                              const GroupedSpan<int> vert_to_face_map,
                              const Span<int> face_sets,
                              const SubdivCCG &subdiv_ccg,
                              SubdivCCGCoord coord)
{
  /* TODO: Move this check higher out of this function. */
  if (face_sets.is_empty()) {
    return true;
  }
  int v1, v2;
  const SubdivCCGAdjacencyType adjacency = BKE_subdiv_ccg_coarse_mesh_adjacency_info_get(
      subdiv_ccg, coord, corner_verts, faces, v1, v2);
  switch (adjacency) {
    case SUBDIV_CCG_ADJACENT_VERTEX:
      return vert_has_unique_face_set(vert_to_face_map, face_sets, v1);
    case SUBDIV_CCG_ADJACENT_EDGE:
      return sculpt_check_unique_face_set_for_edge_in_base_mesh(
          vert_to_face_map, face_sets, corner_verts, faces, v1, v2);
    case SUBDIV_CCG_ADJACENT_NONE:
      return true;
  }
  BLI_assert_unreachable();
  return true;
}

bool vert_has_unique_face_set(const int /*face_set_offset*/, const BMVert & /*vert*/)
{
  /* TODO: Obviously not fully implemented yet. Needs to be implemented for Relax Face Sets brush
   * to work. */
  return true;
}

}  // namespace face_set

Span<BMVert *> vert_neighbors_get_bmesh(BMVert &vert, Vector<BMVert *, 64> &r_neighbors)
{
  r_neighbors.clear();
  BMIter liter;
  BMLoop *l;
  BM_ITER_ELEM (l, &liter, &vert, BM_LOOPS_OF_VERT) {
    for (BMVert *other_vert : {l->prev->v, l->next->v}) {
      if (other_vert != &vert) {
        r_neighbors.append(other_vert);
      }
    }
  }
  return r_neighbors;
}

Span<BMVert *> vert_neighbors_get_interior_bmesh(BMVert &vert, Vector<BMVert *, 64> &r_neighbors)
{
  r_neighbors.clear();
  BMIter liter;
  BMLoop *l;
  BM_ITER_ELEM (l, &liter, &vert, BM_LOOPS_OF_VERT) {
    for (BMVert *other_vert : {l->prev->v, l->next->v}) {
      if (other_vert != &vert) {
        r_neighbors.append(other_vert);
      }
    }
  }

  if (BM_vert_is_boundary(&vert)) {
    if (r_neighbors.size() == 2) {
      /* Do not include neighbors of corner vertices. */
      r_neighbors.clear();
    }
    else {
      /* Only include other boundary vertices as neighbors of boundary vertices. */
      r_neighbors.remove_if([&](const BMVert *vert) { return !BM_vert_is_boundary(vert); });
    }
  }

  return r_neighbors;
}

Span<int> vert_neighbors_get_mesh(const OffsetIndices<int> faces,
                                  const Span<int> corner_verts,
                                  const GroupedSpan<int> vert_to_face,
                                  const Span<bool> hide_poly,
                                  const int vert,
                                  Vector<int> &r_neighbors)
{
  r_neighbors.clear();

  for (const int face : vert_to_face[vert]) {
    if (!hide_poly.is_empty() && hide_poly[face]) {
      continue;
    }
    const int2 verts = bke::mesh::face_find_adjacent_verts(faces[face], corner_verts, vert);
    r_neighbors.append_non_duplicates(verts[0]);
    r_neighbors.append_non_duplicates(verts[1]);
  }

  return r_neighbors.as_span();
}

namespace boundary {

bool vert_is_boundary(const GroupedSpan<int> vert_to_face_map,
                      const Span<bool> hide_poly,
                      const BitSpan boundary,
                      const int vert)
{
  if (!hide::vert_all_faces_visible_get(hide_poly, vert_to_face_map, vert)) {
    return true;
  }
  return boundary[vert].test();
}

bool vert_is_boundary(const OffsetIndices<int> faces,
                      const Span<int> corner_verts,
                      const BitSpan boundary,
                      const SubdivCCG &subdiv_ccg,
                      const SubdivCCGCoord vert)
{
  /* TODO: Unlike the base mesh implementation this method does NOT take into account face
   * visibility. Either this should be noted as a intentional limitation or fixed. */
  int v1, v2;
  const SubdivCCGAdjacencyType adjacency = BKE_subdiv_ccg_coarse_mesh_adjacency_info_get(
      subdiv_ccg, vert, corner_verts, faces, v1, v2);
  switch (adjacency) {
    case SUBDIV_CCG_ADJACENT_VERTEX:
      return boundary[v1].test();
    case SUBDIV_CCG_ADJACENT_EDGE:
      return boundary[v1].test() && boundary[v2].test();
    case SUBDIV_CCG_ADJACENT_NONE:
      return false;
  }
  BLI_assert_unreachable();
  return false;
}

bool vert_is_boundary(BMVert *vert)
{
  /* TODO: Unlike the base mesh implementation this method does NOT take into account face
   * visibility. Either this should be noted as a intentional limitation or fixed. */
  return BM_vert_is_boundary(vert);
}

}  // namespace boundary

}  // namespace blender::ed::sculpt_paint

/* Utilities */

bool SCULPT_stroke_is_main_symmetry_pass(const blender::ed::sculpt_paint::StrokeCache &cache)
{
  return cache.mirror_symmetry_pass == 0 && cache.radial_symmetry_pass == 0 &&
         cache.tile_pass == 0;
}

bool SCULPT_stroke_is_first_brush_step(const blender::ed::sculpt_paint::StrokeCache &cache)
{
  return cache.first_time && cache.mirror_symmetry_pass == 0 && cache.radial_symmetry_pass == 0 &&
         cache.tile_pass == 0;
}

bool SCULPT_stroke_is_first_brush_step_of_symmetry_pass(
    const blender::ed::sculpt_paint::StrokeCache &cache)
{
  return cache.first_time;
}

bool SCULPT_check_vertex_pivot_symmetry(const float vco[3], const float pco[3], const char symm)
{
  bool is_in_symmetry_area = true;
  for (int i = 0; i < 3; i++) {
    char symm_it = 1 << i;
    if (symm & symm_it) {
      if (pco[i] == 0.0f) {
        if (vco[i] > 0.0f) {
          is_in_symmetry_area = false;
        }
      }
      if (vco[i] * pco[i] < 0.0f) {
        is_in_symmetry_area = false;
      }
    }
  }
  return is_in_symmetry_area;
}

void sculpt_project_v3_normal_align(const SculptSession &ss,
                                    const float normal_weight,
                                    float grab_delta[3])
{
  /* Signed to support grabbing in (to make a hole) as well as out. */
  const float len_signed = dot_v3v3(ss.cache->sculpt_normal_symm, grab_delta);

  /* This scale effectively projects the offset so dragging follows the cursor,
   * as the normal points towards the view, the scale increases. */
  float len_view_scale;
  {
    float view_aligned_normal[3];
    project_plane_v3_v3v3(
        view_aligned_normal, ss.cache->sculpt_normal_symm, ss.cache->view_normal_symm);
    len_view_scale = fabsf(dot_v3v3(view_aligned_normal, ss.cache->sculpt_normal_symm));
    len_view_scale = (len_view_scale > FLT_EPSILON) ? 1.0f / len_view_scale : 1.0f;
  }

  mul_v3_fl(grab_delta, 1.0f - normal_weight);
  madd_v3_v3fl(
      grab_delta, ss.cache->sculpt_normal_symm, (len_signed * normal_weight) * len_view_scale);
}

namespace blender::ed::sculpt_paint {

std::optional<int> nearest_vert_calc_mesh(const bke::pbvh::Tree &pbvh,
                                          const Span<float3> vert_positions,
                                          const Span<bool> hide_vert,
                                          const float3 &location,
                                          const float max_distance,
                                          const bool use_original)
{
  const float max_distance_sq = max_distance * max_distance;
  IndexMaskMemory memory;
  const IndexMask nodes_in_sphere = bke::pbvh::search_nodes(
      pbvh, memory, [&](const bke::pbvh::Node &node) {
        return node_in_sphere(node, location, max_distance_sq, use_original);
      });
  if (nodes_in_sphere.is_empty()) {
    return std::nullopt;
  }

  struct NearestData {
    int vert = -1;
    float distance_sq = std::numeric_limits<float>::max();
  };

  const Span<bke::pbvh::MeshNode> nodes = pbvh.nodes<bke::pbvh::MeshNode>();
  const NearestData nearest = threading::parallel_reduce(
      nodes_in_sphere.index_range(),
      1,
      NearestData(),
      [&](const IndexRange range, NearestData nearest) {
        nodes_in_sphere.slice(range).foreach_index([&](const int i) {
          for (const int vert : nodes[i].verts()) {
            if (!hide_vert.is_empty() && hide_vert[vert]) {
              continue;
            }
            const float distance_sq = math::distance_squared(vert_positions[vert], location);
            if (distance_sq < nearest.distance_sq) {
              nearest = {vert, distance_sq};
            }
          }
        });
        return nearest;
      },
      [](const NearestData a, const NearestData b) {
        return a.distance_sq < b.distance_sq ? a : b;
      });
  return nearest.vert;
}

std::optional<SubdivCCGCoord> nearest_vert_calc_grids(const bke::pbvh::Tree &pbvh,
                                                      const SubdivCCG &subdiv_ccg,
                                                      const float3 &location,
                                                      const float max_distance,
                                                      const bool use_original)
{
  const float max_distance_sq = max_distance * max_distance;
  IndexMaskMemory memory;
  const IndexMask nodes_in_sphere = bke::pbvh::search_nodes(
      pbvh, memory, [&](const bke::pbvh::Node &node) {
        return node_in_sphere(node, location, max_distance_sq, use_original);
      });
  if (nodes_in_sphere.is_empty()) {
    return std::nullopt;
  }

  struct NearestData {
    SubdivCCGCoord coord = {};
    float distance_sq = std::numeric_limits<float>::max();
  };

  const BitGroupVector<> grid_hidden = subdiv_ccg.grid_hidden;
  const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);
  const Span<float3> positions = subdiv_ccg.positions;

  const Span<bke::pbvh::GridsNode> nodes = pbvh.nodes<bke::pbvh::GridsNode>();
  const NearestData nearest = threading::parallel_reduce(
      nodes_in_sphere.index_range(),
      1,
      NearestData(),
      [&](const IndexRange range, NearestData nearest) {
        nodes_in_sphere.slice(range).foreach_index([&](const int i) {
          for (const int grid : nodes[i].grids()) {
            const IndexRange grid_range = bke::ccg::grid_range(key, grid);
            BKE_subdiv_ccg_foreach_visible_grid_vert(key, grid_hidden, grid, [&](const int i) {
              const float distance_sq = math::distance_squared(positions[grid_range[i]], location);
              if (distance_sq < nearest.distance_sq) {
                SubdivCCGCoord coord{};
                coord.grid_index = grid;
                coord.x = i % key.grid_size;
                coord.y = i / key.grid_size;
                nearest = {coord, distance_sq};
              }
            });
          }
        });
        return nearest;
      },
      [](const NearestData a, const NearestData b) {
        return a.distance_sq < b.distance_sq ? a : b;
      });
  return nearest.coord;
}

std::optional<BMVert *> nearest_vert_calc_bmesh(const bke::pbvh::Tree &pbvh,
                                                const float3 &location,
                                                const float max_distance,
                                                const bool use_original)
{
  const float max_distance_sq = max_distance * max_distance;
  IndexMaskMemory memory;
  const IndexMask nodes_in_sphere = bke::pbvh::search_nodes(
      pbvh, memory, [&](const bke::pbvh::Node &node) {
        return node_in_sphere(node, location, max_distance_sq, use_original);
      });
  if (nodes_in_sphere.is_empty()) {
    return std::nullopt;
  }

  struct NearestData {
    BMVert *vert = nullptr;
    float distance_sq = std::numeric_limits<float>::max();
  };

  const Span<bke::pbvh::BMeshNode> nodes = pbvh.nodes<bke::pbvh::BMeshNode>();
  const NearestData nearest = threading::parallel_reduce(
      nodes_in_sphere.index_range(),
      1,
      NearestData(),
      [&](const IndexRange range, NearestData nearest) {
        nodes_in_sphere.slice(range).foreach_index([&](const int i) {
          for (BMVert *vert :
               BKE_pbvh_bmesh_node_unique_verts(const_cast<bke::pbvh::BMeshNode *>(&nodes[i])))
          {
            if (BM_elem_flag_test(vert, BM_ELEM_HIDDEN)) {
              continue;
            }
            const float distance_sq = math::distance_squared(float3(vert->co), location);
            if (distance_sq < nearest.distance_sq) {
              nearest = {vert, distance_sq};
            }
          }
        });
        return nearest;
      },
      [](const NearestData a, const NearestData b) {
        return a.distance_sq < b.distance_sq ? a : b;
      });
  return nearest.vert;
}

}  // namespace blender::ed::sculpt_paint

bool SCULPT_is_symmetry_iteration_valid(char i, char symm)
{
  return i == 0 || (symm & i && (symm != 5 || i != 3) && (symm != 6 || !ELEM(i, 3, 5)));
}

bool SCULPT_is_vertex_inside_brush_radius_symm(const float vertex[3],
                                               const float br_co[3],
                                               float radius,
                                               char symm)
{
  for (char i = 0; i <= symm; ++i) {
    if (!SCULPT_is_symmetry_iteration_valid(i, symm)) {
      continue;
    }
    float3 location = blender::ed::sculpt_paint::symmetry_flip(br_co, ePaintSymmetryFlags(i));
    if (len_squared_v3v3(location, vertex) < radius * radius) {
      return true;
    }
  }
  return false;
}

void SCULPT_tag_update_overlays(bContext *C)
{
  ARegion *region = CTX_wm_region(C);
  ED_region_tag_redraw(region);

  Object &ob = *CTX_data_active_object(C);
  WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, &ob);

  DEG_id_tag_update(&ob.id, ID_RECALC_SHADING);

  RegionView3D *rv3d = CTX_wm_region_view3d(C);
  if (!BKE_sculptsession_use_pbvh_draw(&ob, rv3d)) {
    DEG_id_tag_update(&ob.id, ID_RECALC_GEOMETRY);
  }
}

/** \} */

namespace blender::ed::sculpt_paint {

/* -------------------------------------------------------------------- */
/** \name Brush Capabilities
 *
 * Avoid duplicate checks, internal logic only,
 * share logic with #rna_def_sculpt_capabilities where possible.
 * \{ */

static bool brush_type_needs_original(const char sculpt_brush_type)
{
  return ELEM(sculpt_brush_type,
              SCULPT_BRUSH_TYPE_GRAB,
              SCULPT_BRUSH_TYPE_ROTATE,
              SCULPT_BRUSH_TYPE_THUMB,
              SCULPT_BRUSH_TYPE_LAYER,
              SCULPT_BRUSH_TYPE_DRAW_SHARP,
              SCULPT_BRUSH_TYPE_ELASTIC_DEFORM,
              SCULPT_BRUSH_TYPE_SMOOTH,
              SCULPT_BRUSH_TYPE_BOUNDARY,
              SCULPT_BRUSH_TYPE_POSE);
}

static bool brush_uses_topology_rake(const SculptSession &ss, const Brush &brush)
{
  return SCULPT_BRUSH_TYPE_HAS_TOPOLOGY_RAKE(brush.sculpt_brush_type) &&
         (brush.topology_rake_factor > 0.0f) && (ss.bm != nullptr);
}

/**
 * Test whether the #StrokeCache.sculpt_normal needs update in #do_brush_action
 */
static int sculpt_brush_needs_normal(const SculptSession &ss, const Sculpt &sd, const Brush &brush)
{
  using namespace blender::ed::sculpt_paint;
  const MTex *mask_tex = BKE_brush_mask_texture_get(&brush, OB_MODE_SCULPT);
  return ((SCULPT_BRUSH_TYPE_HAS_NORMAL_WEIGHT(brush.sculpt_brush_type) &&
           (ss.cache->normal_weight > 0.0f)) ||
          auto_mask::needs_normal(ss, sd, &brush) ||
          ELEM(brush.sculpt_brush_type,
               SCULPT_BRUSH_TYPE_BLOB,
               SCULPT_BRUSH_TYPE_CREASE,
               SCULPT_BRUSH_TYPE_DRAW,
               SCULPT_BRUSH_TYPE_DRAW_SHARP,
               SCULPT_BRUSH_TYPE_CLOTH,
               SCULPT_BRUSH_TYPE_LAYER,
               SCULPT_BRUSH_TYPE_NUDGE,
               SCULPT_BRUSH_TYPE_ROTATE,
               SCULPT_BRUSH_TYPE_ELASTIC_DEFORM,
               SCULPT_BRUSH_TYPE_THUMB) ||

          (mask_tex->brush_map_mode == MTEX_MAP_MODE_AREA)) ||
         brush_uses_topology_rake(ss, brush) || BKE_brush_has_cube_tip(&brush, PaintMode::Sculpt);
}

static bool brush_needs_rake_rotation(const Brush &brush)
{
  return SCULPT_BRUSH_TYPE_HAS_RAKE(brush.sculpt_brush_type) && (brush.rake_factor != 0.0f);
}

/** \} */

static void rake_data_update(SculptRakeData *srd, const float co[3])
{
  float rake_dist = len_v3v3(srd->follow_co, co);
  if (rake_dist > srd->follow_dist) {
    interp_v3_v3v3(srd->follow_co, srd->follow_co, co, rake_dist - srd->follow_dist);
  }
}

/* -------------------------------------------------------------------- */
/** \name Sculpt Dynamic Topology
 * \{ */

namespace dyntopo {

bool stroke_is_dyntopo(const Object &object, const Brush &brush)
{
  const SculptSession &ss = *object.sculpt;
  const bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
  return ((pbvh.type() == bke::pbvh::Type::BMesh) &&

          (!ss.cache || (!ss.cache->alt_smooth)) &&

          /* Requires mesh restore, which doesn't work with
           * dynamic-topology. */
          !(brush.flag & BRUSH_ANCHORED) && !(brush.flag & BRUSH_DRAG_DOT) &&

          SCULPT_BRUSH_TYPE_HAS_DYNTOPO(brush.sculpt_brush_type));
}

}  // namespace dyntopo

/** \} */

/* -------------------------------------------------------------------- */
/** \name Sculpt Paint Mesh
 * \{ */

namespace undo {

static void restore_mask_from_undo_step(Object &object)
{
  SculptSession &ss = *object.sculpt;
  bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
  IndexMaskMemory memory;
  const IndexMask node_mask = bke::pbvh::all_leaf_nodes(pbvh, memory);

  Array<bool> node_changed(node_mask.min_array_size(), false);

  switch (pbvh.type()) {
    case bke::pbvh::Type::Mesh: {
      MutableSpan<bke::pbvh::MeshNode> nodes = pbvh.nodes<bke::pbvh::MeshNode>();
      Mesh &mesh = *static_cast<Mesh *>(object.data);
      bke::MutableAttributeAccessor attributes = mesh.attributes_for_write();
      bke::SpanAttributeWriter<float> mask = attributes.lookup_or_add_for_write_span<float>(
          ".sculpt_mask", bke::AttrDomain::Point);
      node_mask.foreach_index(GrainSize(1), [&](const int i) {
        if (const std::optional<Span<float>> orig_data = orig_mask_data_lookup_mesh(object,
                                                                                    nodes[i]))
        {
          const Span<int> verts = nodes[i].verts();
          scatter_data_mesh(*orig_data, verts, mask.span);
          bke::pbvh::node_update_mask_mesh(mask.span, nodes[i]);
          node_changed[i] = true;
        }
      });
      mask.finish();
      break;
    }
    case bke::pbvh::Type::BMesh: {
      MutableSpan<bke::pbvh::BMeshNode> nodes = pbvh.nodes<bke::pbvh::BMeshNode>();
      const int offset = CustomData_get_offset_named(&ss.bm->vdata, CD_PROP_FLOAT, ".sculpt_mask");
      if (offset != -1) {
        node_mask.foreach_index(GrainSize(1), [&](const int i) {
          for (BMVert *vert : BKE_pbvh_bmesh_node_unique_verts(&nodes[i])) {
            if (const float *orig_mask = BM_log_find_original_vert_mask(ss.bm_log, vert)) {
              BM_ELEM_CD_SET_FLOAT(vert, offset, *orig_mask);
              bke::pbvh::node_update_mask_bmesh(offset, nodes[i]);
              node_changed[i] = true;
            }
          }
        });
      }
      break;
    }
    case bke::pbvh::Type::Grids: {
      MutableSpan<bke::pbvh::GridsNode> nodes = pbvh.nodes<bke::pbvh::GridsNode>();
      SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;
      const BitGroupVector<> grid_hidden = subdiv_ccg.grid_hidden;
      const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);
      MutableSpan<float> masks = subdiv_ccg.masks;
      node_mask.foreach_index(GrainSize(1), [&](const int i) {
        if (const std::optional<Span<float>> orig_data = orig_mask_data_lookup_grids(object,
                                                                                     nodes[i]))
        {
          int index = 0;
          for (const int grid : nodes[i].grids()) {
            const IndexRange grid_range = bke::ccg::grid_range(key, grid);
            for (const int i : IndexRange(key.grid_area)) {
              if (grid_hidden.is_empty() || !grid_hidden[grid][i]) {
                masks[grid_range[i]] = (*orig_data)[index];
              }
              index++;
            }
          }
          bke::pbvh::node_update_mask_grids(key, masks, nodes[i]);
          node_changed[i] = true;
        }
      });
      break;
    }
  }
  pbvh.tag_masks_changed(IndexMask::from_bools(node_changed, memory));
}

static void restore_color_from_undo_step(Object &object)
{
  bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
  MutableSpan<bke::pbvh::MeshNode> nodes = pbvh.nodes<bke::pbvh::MeshNode>();
  IndexMaskMemory memory;
  const IndexMask node_mask = IndexMask::from_predicate(
      nodes.index_range(), GrainSize(64), memory, [&](const int i) {
        return orig_color_data_lookup_mesh(object, nodes[i]).has_value();
      });

  BLI_assert(pbvh.type() == bke::pbvh::Type::Mesh);
  Mesh &mesh = *static_cast<Mesh *>(object.data);
  const OffsetIndices<int> faces = mesh.faces();
  const Span<int> corner_verts = mesh.corner_verts();
  const GroupedSpan<int> vert_to_face_map = mesh.vert_to_face_map();
  bke::GSpanAttributeWriter color_attribute = color::active_color_attribute_for_write(mesh);
  node_mask.foreach_index(GrainSize(1), [&](const int i) {
    const Span<float4> orig_data = *orig_color_data_lookup_mesh(object, nodes[i]);
    const Span<int> verts = nodes[i].verts();
    for (const int i : verts.index_range()) {
      color::color_vert_set(faces,
                            corner_verts,
                            vert_to_face_map,
                            color_attribute.domain,
                            verts[i],
                            orig_data[i],
                            color_attribute.span);
    }
  });
  pbvh.tag_attribute_changed(node_mask, mesh.active_color_attribute);
  color_attribute.finish();
}

static void restore_face_set_from_undo_step(Object &object)
{
  SculptSession &ss = *object.sculpt;
  bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
  IndexMaskMemory memory;
  const IndexMask node_mask = bke::pbvh::all_leaf_nodes(pbvh, memory);

  Array<bool> node_changed(node_mask.min_array_size(), false);

  switch (pbvh.type()) {
    case bke::pbvh::Type::Mesh: {
      MutableSpan<bke::pbvh::MeshNode> nodes = pbvh.nodes<bke::pbvh::MeshNode>();
      bke::SpanAttributeWriter<int> attribute = face_set::ensure_face_sets_mesh(
          *static_cast<Mesh *>(object.data));
      node_mask.foreach_index(GrainSize(1), [&](const int i) {
        if (const std::optional<Span<int>> orig_data = orig_face_set_data_lookup_mesh(object,
                                                                                      nodes[i]))
        {
          scatter_data_mesh(*orig_data, nodes[i].faces(), attribute.span);
          node_changed[i] = true;
        }
      });
      attribute.finish();
      break;
    }
    case bke::pbvh::Type::Grids: {
      const SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;
      MutableSpan<bke::pbvh::GridsNode> nodes = pbvh.nodes<bke::pbvh::GridsNode>();
      bke::SpanAttributeWriter<int> attribute = face_set::ensure_face_sets_mesh(
          *static_cast<Mesh *>(object.data));
      threading::EnumerableThreadSpecific<Vector<int>> all_tls;
      node_mask.foreach_index(GrainSize(1), [&](const int i) {
        Vector<int> &tls = all_tls.local();
        if (const std::optional<Span<int>> orig_data = orig_face_set_data_lookup_grids(object,
                                                                                       nodes[i]))
        {
          const Span<int> faces = bke::pbvh::node_face_indices_calc_grids(
              subdiv_ccg, nodes[i], tls);
          scatter_data_mesh(*orig_data, faces, attribute.span);
          node_changed[i] = true;
        }
      });
      attribute.finish();
      break;
    }
    case bke::pbvh::Type::BMesh:
      break;
  }

  pbvh.tag_face_sets_changed(IndexMask::from_bools(node_changed, memory));
}

void restore_position_from_undo_step(const Depsgraph &depsgraph, Object &object)
{
  SculptSession &ss = *object.sculpt;
  bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
  IndexMaskMemory memory;

  switch (pbvh.type()) {
    case bke::pbvh::Type::Mesh: {
      const Span<bke::pbvh::MeshNode> nodes = pbvh.nodes<bke::pbvh::MeshNode>();
      Mesh &mesh = *static_cast<Mesh *>(object.data);
      MutableSpan positions_eval = bke::pbvh::vert_positions_eval_for_write(depsgraph, object);
      MutableSpan positions_orig = mesh.vert_positions_for_write();

      const IndexMask node_mask = IndexMask::from_predicate(
          nodes.index_range(), GrainSize(64), memory, [&](const int i) {
            return orig_position_data_lookup_mesh(object, nodes[i]).has_value();
          });

      struct LocalData {
        Vector<float3> translations;
      };

      std::optional<ShapeKeyData> shape_key_data = ShapeKeyData::from_object(object);
      const bool need_translations = !ss.deform_imats.is_empty() || shape_key_data.has_value();

      threading::EnumerableThreadSpecific<LocalData> all_tls;
      node_mask.foreach_index(GrainSize(1), [&](const int i) {
        threading::isolate_task([&] {
          LocalData &tls = all_tls.local();
          const OrigPositionData orig_data = *orig_position_data_lookup_mesh(object, nodes[i]);
          const Span<int> verts = nodes[i].verts();
          const Span<float3> undo_positions = orig_data.positions;
          if (need_translations) {
            /* Calculate translations from evaluated positions before they are changed. */
            tls.translations.resize(verts.size());
            translations_from_new_positions(
                undo_positions, verts, positions_eval, tls.translations);
          }

          scatter_data_mesh(undo_positions, verts, positions_eval);

          if (positions_eval.data() == positions_orig.data()) {
            return;
          }

          const MutableSpan<float3> translations = tls.translations;
          if (!ss.deform_imats.is_empty()) {
            apply_crazyspace_to_translations(ss.deform_imats, verts, translations);
          }

          if (shape_key_data) {
            for (MutableSpan<float3> data : shape_key_data->dependent_keys) {
              apply_translations(translations, verts, data);
            }

            if (shape_key_data->basis_key_active) {
              /* The basis key positions and the mesh positions are always kept in sync. */
              apply_translations(translations, verts, positions_orig);
            }
            apply_translations(translations, verts, shape_key_data->active_key_data);
          }
          else {
            apply_translations(translations, verts, positions_orig);
          }
        });
      });
      pbvh.tag_positions_changed(node_mask);
      break;
    }
    case bke::pbvh::Type::BMesh: {
      MutableSpan<bke::pbvh::BMeshNode> nodes = pbvh.nodes<bke::pbvh::BMeshNode>();
      if (!undo::get_bmesh_log_entry()) {
        return;
      }
      const IndexMask node_mask = bke::pbvh::all_leaf_nodes(pbvh, memory);
      node_mask.foreach_index(GrainSize(1), [&](const int i) {
        for (BMVert *vert : BKE_pbvh_bmesh_node_unique_verts(&nodes[i])) {
          if (const float *orig_co = BM_log_find_original_vert_co(ss.bm_log, vert)) {
            copy_v3_v3(vert->co, orig_co);
          }
        }
      });
      pbvh.tag_positions_changed(node_mask);
      break;
    }
    case bke::pbvh::Type::Grids: {
      const Span<bke::pbvh::GridsNode> nodes = pbvh.nodes<bke::pbvh::GridsNode>();

      const IndexMask node_mask = IndexMask::from_predicate(
          nodes.index_range(), GrainSize(64), memory, [&](const int i) {
            return orig_position_data_lookup_grids(object, nodes[i]).has_value();
          });

      SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;
      const BitGroupVector<> grid_hidden = subdiv_ccg.grid_hidden;
      const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);
      MutableSpan<float3> positions = subdiv_ccg.positions;
      node_mask.foreach_index(GrainSize(1), [&](const int i) {
        const OrigPositionData orig_data = *orig_position_data_lookup_grids(object, nodes[i]);
        int index = 0;
        for (const int grid : nodes[i].grids()) {
          const IndexRange grid_range = bke::ccg::grid_range(key, grid);
          for (const int i : IndexRange(key.grid_area)) {
            if (grid_hidden.is_empty() || !grid_hidden[grid][i]) {
              positions[grid_range[i]] = orig_data.positions[index];
            }
            index++;
          }
        }
      });
      pbvh.tag_positions_changed(node_mask);
      break;
    }
  }

  /* Update normals for potentially-changed positions. Theoretically this may be unnecessary if
   * the brush restoring to the initial state doesn't use the normals, but we have no easy way to
   * know that from here. */
  bke::pbvh::update_normals(depsgraph, object, pbvh);
}

static void restore_from_undo_step(const Depsgraph &depsgraph, const Sculpt &sd, Object &object)
{
  SculptSession &ss = *object.sculpt;
  const Brush *brush = BKE_paint_brush_for_read(&sd.paint);

  switch (brush->sculpt_brush_type) {
    case SCULPT_BRUSH_TYPE_MASK:
      restore_mask_from_undo_step(object);
      break;
    case SCULPT_BRUSH_TYPE_PAINT:
    case SCULPT_BRUSH_TYPE_SMEAR:
      restore_color_from_undo_step(object);
      break;
    case SCULPT_BRUSH_TYPE_DRAW_FACE_SETS:
      if (ss.cache->alt_smooth) {
        restore_position_from_undo_step(depsgraph, object);
      }
      else {
        restore_face_set_from_undo_step(object);
      }
      break;
    default:
      restore_position_from_undo_step(depsgraph, object);
      break;
  }
  /* Disable multi-threading when dynamic-topology is enabled. Otherwise,
   * new entries might be inserted by #undo::push_node() into the #GHash
   * used internally by #BM_log_original_vert_co() by a different thread. See #33787. */
}

}  // namespace undo

}  // namespace blender::ed::sculpt_paint

/*** BVH Tree ***/

static void extend_redraw_rect_previous(Object &ob, rcti &rect)
{
  /* Expand redraw \a rect with redraw \a rect from previous step to
   * prevent partial-redraw issues caused by fast strokes. This is
   * needed here (not in sculpt_flush_update) as it was before
   * because redraw rectangle should be the same in both of
   * optimized bke::pbvh::Tree draw function and 3d view redraw, if not -- some
   * mesh parts could disappear from screen (sergey). */
  SculptSession &ss = *ob.sculpt;

  if (!ss.cache) {
    return;
  }

  if (BLI_rcti_is_empty(&ss.cache->previous_r)) {
    return;
  }

  BLI_rcti_union(&rect, &ss.cache->previous_r);
}

bool SCULPT_get_redraw_rect(const ARegion &region,
                            const RegionView3D &rv3d,
                            const Object &ob,
                            rcti &rect)
{
  using namespace blender;
  const bke::pbvh::Tree *pbvh = bke::object::pbvh_get(ob);
  if (!pbvh) {
    return false;
  }

  const Bounds<float3> bounds = BKE_pbvh_redraw_BB(*pbvh);

  /* Convert 3D bounding box to screen space. */
  if (!paint_convert_bb_to_rect(&rect, bounds.min, bounds.max, region, rv3d, ob)) {
    return false;
  }

  return true;
}

const float *SCULPT_brush_frontface_normal_from_falloff_shape(const SculptSession &ss,
                                                              char falloff_shape)
{
  if (falloff_shape == PAINT_FALLOFF_SHAPE_SPHERE) {
    return ss.cache->sculpt_normal_symm;
  }
  BLI_assert(falloff_shape == PAINT_FALLOFF_SHAPE_TUBE);
  return ss.cache->view_normal_symm;
}

/* ===== Sculpting =====
 */

static float calc_overlap(const blender::ed::sculpt_paint::StrokeCache &cache,
                          const ePaintSymmetryFlags symm,
                          const char axis,
                          const float angle)
{
  float3 mirror = blender::ed::sculpt_paint::symmetry_flip(cache.location, symm);

  if (axis != 0) {
    float mat[3][3];
    axis_angle_to_mat3_single(mat, axis, angle);
    mul_m3_v3(mat, mirror);
  }

  const float distsq = len_squared_v3v3(mirror, cache.location);

  if (distsq <= 4.0f * (cache.radius_squared)) {
    return (2.0f * (cache.radius) - sqrtf(distsq)) / (2.0f * (cache.radius));
  }
  return 0.0f;
}

static float calc_radial_symmetry_feather(const Sculpt &sd,
                                          const blender::ed::sculpt_paint::StrokeCache &cache,
                                          const ePaintSymmetryFlags symm,
                                          const char axis)
{
  float overlap = 0.0f;

  for (int i = 1; i < sd.radial_symm[axis - 'X']; i++) {
    const float angle = 2.0f * M_PI * i / sd.radial_symm[axis - 'X'];
    overlap += calc_overlap(cache, symm, axis, angle);
  }

  return overlap;
}

static float calc_symmetry_feather(const Sculpt &sd,
                                   const blender::ed::sculpt_paint::StrokeCache &cache)
{
  if (!(sd.paint.symmetry_flags & PAINT_SYMMETRY_FEATHER)) {
    return 1.0f;
  }
  float overlap;
  const int symm = cache.symmetry;

  overlap = 0.0f;
  for (int i = 0; i <= symm; i++) {
    if (!SCULPT_is_symmetry_iteration_valid(i, symm)) {
      continue;
    }

    overlap += calc_overlap(cache, ePaintSymmetryFlags(i), 0, 0);

    overlap += calc_radial_symmetry_feather(sd, cache, ePaintSymmetryFlags(i), 'X');
    overlap += calc_radial_symmetry_feather(sd, cache, ePaintSymmetryFlags(i), 'Y');
    overlap += calc_radial_symmetry_feather(sd, cache, ePaintSymmetryFlags(i), 'Z');
  }
  return 1.0f / overlap;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Calculate Normal and Center
 *
 * Calculate geometry surrounding the brush center.
 * (optionally using original coordinates).
 *
 * Functions are:
 * - #calc_area_center
 * - #calc_area_normal
 * - #calc_area_normal_and_center
 *
 * \note These are all _very_ similar, when changing one, check others.
 * \{ */

namespace blender::ed::sculpt_paint {

struct AreaNormalCenterData {
  /* 0 = towards view, 1 = flipped */
  std::array<float3, 2> area_cos;
  std::array<int, 2> count_co;

  std::array<float3, 2> area_nos;
  std::array<int, 2> count_no;
};

static float area_normal_and_center_get_normal_radius(const SculptSession &ss, const Brush &brush)
{
  float test_radius = ss.cache ? ss.cache->radius : ss.cursor_radius;
  if (brush.ob_mode == OB_MODE_SCULPT) {
    test_radius *= brush.normal_radius_factor;
  }
  return test_radius;
}

static float area_normal_and_center_get_position_radius(const SculptSession &ss,
                                                        const Brush &brush)
{
  float test_radius = ss.cache ? ss.cache->radius : ss.cursor_radius;
  if (brush.ob_mode == OB_MODE_SCULPT) {
    /* Layer brush produces artifacts with normal and area radius */
    /* Enable area radius control only on Scrape for now */
    if (ELEM(brush.sculpt_brush_type, SCULPT_BRUSH_TYPE_SCRAPE, SCULPT_BRUSH_TYPE_FILL) &&
        brush.area_radius_factor > 0.0f)
    {
      test_radius *= brush.area_radius_factor;
      if (ss.cache && brush.flag2 & BRUSH_AREA_RADIUS_PRESSURE) {
        test_radius *= ss.cache->pressure;
      }
    }
    else {
      test_radius *= brush.normal_radius_factor;
    }
  }
  return test_radius;
}

/* Weight the normals towards the center. */
static float area_normal_calc_weight(const float distance, const float radius_inv)
{
  float p = 1.0f - (distance * radius_inv);
  return std::clamp(3.0f * p * p - 2.0f * p * p * p, 0.0f, 1.0f);
}

/* Weight the coordinates towards the center. */
static float3 area_center_calc_weighted(const float3 &test_location,
                                        const float distance,
                                        const float radius_inv,
                                        const float3 &co)
{
  /* Weight the coordinates towards the center. */
  float p = 1.0f - (distance * radius_inv);
  const float afactor = std::clamp(3.0f * p * p - 2.0f * p * p * p, 0.0f, 1.0f);

  const float3 disp = (co - test_location) * (1.0f - afactor);
  return test_location + disp;
}

static void accumulate_area_center(const float3 &test_location,
                                   const float3 &position,
                                   const float distance,
                                   const float radius_inv,
                                   const int flip_index,
                                   AreaNormalCenterData &anctd)
{
  anctd.area_cos[flip_index] += area_center_calc_weighted(
      test_location, distance, radius_inv, position);
  anctd.count_co[flip_index] += 1;
}

static void accumulate_area_normal(const float3 &normal,
                                   const float distance,
                                   const float radius_inv,
                                   const int flip_index,
                                   AreaNormalCenterData &anctd)
{
  anctd.area_nos[flip_index] += normal * area_normal_calc_weight(distance, radius_inv);
  anctd.count_no[flip_index] += 1;
}

struct SampleLocalData {
  Vector<float3> positions;
  Vector<float> distances;
};

static void calc_area_normal_and_center_node_mesh(const Object &object,
                                                  const Span<float3> vert_positions,
                                                  const Span<float3> vert_normals,
                                                  const Span<bool> hide_vert,
                                                  const Brush &brush,
                                                  const bool use_area_nos,
                                                  const bool use_area_cos,
                                                  const bke::pbvh::MeshNode &node,
                                                  SampleLocalData &tls,
                                                  AreaNormalCenterData &anctd)
{
  const SculptSession &ss = *object.sculpt;
  const float3 &location = ss.cache ? ss.cache->location_symm : ss.cursor_location;
  const float3 &view_normal = ss.cache ? ss.cache->view_normal_symm : ss.cursor_view_normal;
  const float position_radius = area_normal_and_center_get_position_radius(ss, brush);
  const float position_radius_sq = position_radius * position_radius;
  const float position_radius_inv = math::rcp(position_radius);
  const float normal_radius = area_normal_and_center_get_normal_radius(ss, brush);
  const float normal_radius_sq = normal_radius * normal_radius;
  const float normal_radius_inv = math::rcp(normal_radius);

  const Span<int> verts = node.verts();

  if (ss.cache && !ss.cache->accum) {
    if (const std::optional<OrigPositionData> orig_data = orig_position_data_lookup_mesh(object,
                                                                                         node))
    {
      const Span<float3> orig_positions = orig_data->positions;
      const Span<float3> orig_normals = orig_data->normals;

      tls.distances.reinitialize(verts.size());
      const MutableSpan<float> distances_sq = tls.distances;
      calc_brush_distances_squared(
          ss, orig_positions, eBrushFalloffShape(brush.falloff_shape), distances_sq);

      for (const int i : verts.index_range()) {
        const int vert = verts[i];
        if (!hide_vert.is_empty() && hide_vert[vert]) {
          continue;
        }
        const bool normal_test_r = use_area_nos && distances_sq[i] <= normal_radius_sq;
        const bool area_test_r = use_area_cos && distances_sq[i] <= position_radius_sq;
        if (!normal_test_r && !area_test_r) {
          continue;
        }
        const float3 &normal = orig_normals[i];
        const float distance = std::sqrt(distances_sq[i]);
        const int flip_index = math::dot(view_normal, normal) <= 0.0f;
        if (area_test_r) {
          accumulate_area_center(
              location, orig_positions[i], distance, position_radius_inv, flip_index, anctd);
        }
        if (normal_test_r) {
          accumulate_area_normal(normal, distance, normal_radius_inv, flip_index, anctd);
        }
      }
      return;
    }
  }

  tls.distances.reinitialize(verts.size());
  const MutableSpan<float> distances_sq = tls.distances;
  calc_brush_distances_squared(
      ss, vert_positions, verts, eBrushFalloffShape(brush.falloff_shape), distances_sq);

  for (const int i : verts.index_range()) {
    const int vert = verts[i];
    if (!hide_vert.is_empty() && hide_vert[vert]) {
      continue;
    }
    const bool normal_test_r = distances_sq[i] <= normal_radius_sq;
    const bool area_test_r = distances_sq[i] <= position_radius_sq;
    if (!normal_test_r && !area_test_r) {
      continue;
    }
    const float3 &normal = vert_normals[vert];
    const float distance = std::sqrt(distances_sq[i]);
    const int flip_index = math::dot(view_normal, normal) <= 0.0f;
    if (area_test_r) {
      accumulate_area_center(
          location, vert_positions[vert], distance, position_radius_inv, flip_index, anctd);
    }
    if (normal_test_r) {
      accumulate_area_normal(normal, distance, normal_radius_inv, flip_index, anctd);
    }
  }
}

static void calc_area_normal_and_center_node_grids(const Object &object,
                                                   const Brush &brush,
                                                   const bool use_area_nos,
                                                   const bool use_area_cos,
                                                   const bke::pbvh::GridsNode &node,
                                                   SampleLocalData &tls,
                                                   AreaNormalCenterData &anctd)
{
  const SculptSession &ss = *object.sculpt;
  const float3 &location = ss.cache ? ss.cache->location_symm : ss.cursor_location;
  const float3 &view_normal = ss.cache ? ss.cache->view_normal_symm : ss.cursor_view_normal;
  const float position_radius = area_normal_and_center_get_position_radius(ss, brush);
  const float position_radius_sq = position_radius * position_radius;
  const float position_radius_inv = math::rcp(position_radius);
  const float normal_radius = area_normal_and_center_get_normal_radius(ss, brush);
  const float normal_radius_sq = normal_radius * normal_radius;
  const float normal_radius_inv = math::rcp(normal_radius);

  const SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;
  const CCGKey key = BKE_subdiv_ccg_key_top_level(*ss.subdiv_ccg);
  const Span<float3> normals = subdiv_ccg.normals;
  const BitGroupVector<> &grid_hidden = subdiv_ccg.grid_hidden;
  const Span<int> grids = node.grids();

  if (ss.cache && !ss.cache->accum) {
    if (const std::optional<OrigPositionData> orig_data = orig_position_data_lookup_grids(object,
                                                                                          node))
    {
      const Span<float3> orig_positions = orig_data->positions;
      const Span<float3> orig_normals = orig_data->normals;

      tls.distances.reinitialize(orig_positions.size());
      const MutableSpan<float> distances_sq = tls.distances;
      calc_brush_distances_squared(
          ss, orig_positions, eBrushFalloffShape(brush.falloff_shape), distances_sq);

      for (const int i : grids.index_range()) {
        const IndexRange grid_range_node = bke::ccg::grid_range(key, i);
        const int grid = grids[i];
        for (const int offset : IndexRange(key.grid_area)) {
          if (!grid_hidden.is_empty() && grid_hidden[grid][offset]) {
            continue;
          }
          const int node_vert = grid_range_node[offset];

          const bool normal_test_r = use_area_nos && distances_sq[node_vert] <= normal_radius_sq;
          const bool area_test_r = use_area_cos && distances_sq[node_vert] <= position_radius_sq;
          if (!normal_test_r && !area_test_r) {
            continue;
          }
          const float3 &normal = orig_normals[node_vert];
          const float distance = std::sqrt(distances_sq[node_vert]);
          const int flip_index = math::dot(view_normal, normal) <= 0.0f;
          if (area_test_r) {
            accumulate_area_center(location,
                                   orig_positions[node_vert],
                                   distance,
                                   position_radius_inv,
                                   flip_index,
                                   anctd);
          }
          if (normal_test_r) {
            accumulate_area_normal(normal, distance, normal_radius_inv, flip_index, anctd);
          }
        }
      }
      return;
    }
  }

  const Span<float3> positions = gather_grids_positions(subdiv_ccg, grids, tls.positions);
  tls.distances.reinitialize(positions.size());
  const MutableSpan<float> distances_sq = tls.distances;
  calc_brush_distances_squared(
      ss, positions, eBrushFalloffShape(brush.falloff_shape), distances_sq);

  for (const int i : grids.index_range()) {
    const IndexRange grid_range_node = bke::ccg::grid_range(key, i);
    const int grid = grids[i];
    const IndexRange grid_range = bke::ccg::grid_range(key, grid);
    for (const int offset : IndexRange(key.grid_area)) {
      if (!grid_hidden.is_empty() && grid_hidden[grid][offset]) {
        continue;
      }
      const int node_vert = grid_range_node[offset];
      const int vert = grid_range[offset];

      const bool normal_test_r = use_area_nos && distances_sq[node_vert] <= normal_radius_sq;
      const bool area_test_r = use_area_cos && distances_sq[node_vert] <= position_radius_sq;
      if (!normal_test_r && !area_test_r) {
        continue;
      }
      const float3 &normal = normals[vert];
      const float distance = std::sqrt(distances_sq[node_vert]);
      const int flip_index = math::dot(view_normal, normal) <= 0.0f;
      if (area_test_r) {
        accumulate_area_center(
            location, positions[node_vert], distance, position_radius_inv, flip_index, anctd);
      }
      if (normal_test_r) {
        accumulate_area_normal(normal, distance, normal_radius_inv, flip_index, anctd);
      }
    }
  }
}

static void calc_area_normal_and_center_node_bmesh(const Object &object,
                                                   const Brush &brush,
                                                   const bool use_area_nos,
                                                   const bool use_area_cos,
                                                   const bool has_bm_orco,
                                                   const bke::pbvh::BMeshNode &node,
                                                   SampleLocalData &tls,
                                                   AreaNormalCenterData &anctd)
{
  const SculptSession &ss = *object.sculpt;
  const float3 &location = ss.cache ? ss.cache->location_symm : ss.cursor_location;
  const float3 &view_normal = ss.cache ? ss.cache->view_normal_symm : ss.cursor_view_normal;
  const float position_radius = area_normal_and_center_get_position_radius(ss, brush);
  const float position_radius_sq = position_radius * position_radius;
  const float position_radius_inv = math::rcp(position_radius);
  const float normal_radius = area_normal_and_center_get_normal_radius(ss, brush);
  const float normal_radius_sq = normal_radius * normal_radius;
  const float normal_radius_inv = math::rcp(normal_radius);

  bool use_original = false;
  if (ss.cache && !ss.cache->accum) {
    use_original = undo::get_bmesh_log_entry() != nullptr;
  }

  /* When the mesh is edited we can't rely on original coords
   * (original mesh may not even have verts in brush radius). */
  if (use_original && has_bm_orco) {
    Span<float3> orig_positions;
    Span<int3> orig_tris;
    BKE_pbvh_node_get_bm_orco_data(node, orig_positions, orig_tris);

    tls.positions.resize(orig_tris.size());
    const MutableSpan<float3> positions = tls.positions;
    for (const int i : orig_tris.index_range()) {
      const float *co_tri[3] = {
          orig_positions[orig_tris[i][0]],
          orig_positions[orig_tris[i][1]],
          orig_positions[orig_tris[i][2]],
      };
      closest_on_tri_to_point_v3(positions[i], location, UNPACK3(co_tri));
    }

    tls.distances.reinitialize(positions.size());
    const MutableSpan<float> distances_sq = tls.distances;
    calc_brush_distances_squared(
        ss, positions, eBrushFalloffShape(brush.falloff_shape), distances_sq);

    for (const int i : orig_tris.index_range()) {
      const bool normal_test_r = use_area_nos && distances_sq[i] <= normal_radius_sq;
      const bool area_test_r = use_area_cos && distances_sq[i] <= position_radius_sq;
      if (!normal_test_r && !area_test_r) {
        continue;
      }
      const float3 normal = math::normal_tri(float3(orig_positions[orig_tris[i][0]]),
                                             float3(orig_positions[orig_tris[i][1]]),
                                             float3(orig_positions[orig_tris[i][2]]));

      const float distance = std::sqrt(distances_sq[i]);
      const int flip_index = math::dot(view_normal, normal) <= 0.0f;
      if (area_test_r) {
        accumulate_area_center(
            location, positions[i], distance, position_radius_inv, flip_index, anctd);
      }
      if (normal_test_r) {
        accumulate_area_normal(normal, distance, normal_radius_inv, flip_index, anctd);
      }
    }
    return;
  }

  const Set<BMVert *, 0> &verts = BKE_pbvh_bmesh_node_unique_verts(
      &const_cast<bke::pbvh::BMeshNode &>(node));
  if (use_original) {
    tls.positions.resize(verts.size());
    const MutableSpan<float3> positions = tls.positions;
    Array<float3> normals(verts.size());
    orig_position_data_gather_bmesh(*ss.bm_log, verts, positions, normals);

    tls.distances.reinitialize(positions.size());
    const MutableSpan<float> distances_sq = tls.distances;
    calc_brush_distances_squared(
        ss, positions, eBrushFalloffShape(brush.falloff_shape), distances_sq);

    int i = 0;
    for (BMVert *vert : verts) {
      if (BM_elem_flag_test(vert, BM_ELEM_HIDDEN)) {
        i++;
        continue;
      }
      const bool normal_test_r = use_area_nos && distances_sq[i] <= normal_radius_sq;
      const bool area_test_r = use_area_cos && distances_sq[i] <= position_radius_sq;
      if (!normal_test_r && !area_test_r) {
        i++;
        continue;
      }
      const float3 &normal = normals[i];
      const float distance = std::sqrt(distances_sq[i]);
      const int flip_index = math::dot(view_normal, normal) <= 0.0f;
      if (area_test_r) {
        accumulate_area_center(
            location, positions[i], distance, position_radius_inv, flip_index, anctd);
      }
      if (normal_test_r) {
        accumulate_area_normal(normal, distance, normal_radius_inv, flip_index, anctd);
      }
      i++;
    }
    return;
  }

  const Span<float3> positions = gather_bmesh_positions(verts, tls.positions);

  tls.distances.reinitialize(positions.size());
  const MutableSpan<float> distances_sq = tls.distances;
  calc_brush_distances_squared(
      ss, positions, eBrushFalloffShape(brush.falloff_shape), distances_sq);

  int i = 0;
  for (BMVert *vert : verts) {
    if (BM_elem_flag_test(vert, BM_ELEM_HIDDEN)) {
      i++;
      continue;
    }
    const bool normal_test_r = use_area_nos && distances_sq[i] <= normal_radius_sq;
    const bool area_test_r = use_area_cos && distances_sq[i] <= position_radius_sq;
    if (!normal_test_r && !area_test_r) {
      i++;
      continue;
    }
    const float3 &normal = vert->no;
    const float distance = std::sqrt(distances_sq[i]);
    const int flip_index = math::dot(view_normal, normal) <= 0.0f;
    if (area_test_r) {
      accumulate_area_center(
          location, positions[i], distance, position_radius_inv, flip_index, anctd);
    }
    if (normal_test_r) {
      accumulate_area_normal(normal, distance, normal_radius_inv, flip_index, anctd);
    }
    i++;
  }
}

static AreaNormalCenterData calc_area_normal_and_center_reduce(const AreaNormalCenterData &a,
                                                               const AreaNormalCenterData &b)
{
  AreaNormalCenterData joined{};

  joined.area_cos[0] = a.area_cos[0] + b.area_cos[0];
  joined.area_cos[1] = a.area_cos[1] + b.area_cos[1];
  joined.count_co[0] = a.count_co[0] + b.count_co[0];
  joined.count_co[1] = a.count_co[1] + b.count_co[1];

  joined.area_nos[0] = a.area_nos[0] + b.area_nos[0];
  joined.area_nos[1] = a.area_nos[1] + b.area_nos[1];
  joined.count_no[0] = a.count_no[0] + b.count_no[0];
  joined.count_no[1] = a.count_no[1] + b.count_no[1];

  return joined;
}

void calc_area_center(const Depsgraph &depsgraph,
                      const Brush &brush,
                      const Object &ob,
                      const IndexMask &node_mask,
                      float r_area_co[3])
{
  const bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(ob);
  const SculptSession &ss = *ob.sculpt;
  int n;

  AreaNormalCenterData anctd;
  threading::EnumerableThreadSpecific<SampleLocalData> all_tls;
  switch (pbvh.type()) {
    case bke::pbvh::Type::Mesh: {
      const Mesh &mesh = *static_cast<const Mesh *>(ob.data);
      const Span<float3> vert_positions = bke::pbvh::vert_positions_eval(depsgraph, ob);
      const Span<float3> vert_normals = bke::pbvh::vert_normals_eval(depsgraph, ob);
      const bke::AttributeAccessor attributes = mesh.attributes();
      const VArraySpan hide_vert = *attributes.lookup<bool>(".hide_vert", bke::AttrDomain::Point);

      const Span<bke::pbvh::MeshNode> nodes = pbvh.nodes<bke::pbvh::MeshNode>();
      anctd = threading::parallel_reduce(
          node_mask.index_range(),
          1,
          AreaNormalCenterData{},
          [&](const IndexRange range, AreaNormalCenterData anctd) {
            SampleLocalData &tls = all_tls.local();
            node_mask.slice(range).foreach_index([&](const int i) {
              calc_area_normal_and_center_node_mesh(ob,
                                                    vert_positions,
                                                    vert_normals,
                                                    hide_vert,
                                                    brush,
                                                    false,
                                                    true,
                                                    nodes[i],
                                                    tls,
                                                    anctd);
            });
            return anctd;
          },
          calc_area_normal_and_center_reduce);
      break;
    }
    case bke::pbvh::Type::BMesh: {
      const bool has_bm_orco = ss.bm && dyntopo::stroke_is_dyntopo(ob, brush);

      const Span<bke::pbvh::BMeshNode> nodes = pbvh.nodes<bke::pbvh::BMeshNode>();
      anctd = threading::parallel_reduce(
          node_mask.index_range(),
          1,
          AreaNormalCenterData{},
          [&](const IndexRange range, AreaNormalCenterData anctd) {
            SampleLocalData &tls = all_tls.local();
            node_mask.slice(range).foreach_index([&](const int i) {
              calc_area_normal_and_center_node_bmesh(
                  ob, brush, false, true, has_bm_orco, nodes[i], tls, anctd);
            });
            return anctd;
          },
          calc_area_normal_and_center_reduce);
      break;
    }
    case bke::pbvh::Type::Grids: {
      const Span<bke::pbvh::GridsNode> nodes = pbvh.nodes<bke::pbvh::GridsNode>();
      anctd = threading::parallel_reduce(
          node_mask.index_range(),
          1,
          AreaNormalCenterData{},
          [&](const IndexRange range, AreaNormalCenterData anctd) {
            SampleLocalData &tls = all_tls.local();
            node_mask.slice(range).foreach_index([&](const int i) {
              calc_area_normal_and_center_node_grids(ob, brush, false, true, nodes[i], tls, anctd);
            });
            return anctd;
          },
          calc_area_normal_and_center_reduce);
      break;
    }
  }

  /* For flatten center. */
  for (n = 0; n < anctd.area_cos.size(); n++) {
    if (anctd.count_co[n] == 0) {
      continue;
    }

    mul_v3_v3fl(r_area_co, anctd.area_cos[n], 1.0f / anctd.count_co[n]);
    break;
  }

  if (n == 2) {
    zero_v3(r_area_co);
  }

  if (anctd.count_co[0] == 0 && anctd.count_co[1] == 0) {
    if (ss.cache) {
      copy_v3_v3(r_area_co, ss.cache->location_symm);
    }
  }
}

std::optional<float3> calc_area_normal(const Depsgraph &depsgraph,
                                       const Brush &brush,
                                       const Object &ob,
                                       const IndexMask &node_mask)
{
  SculptSession &ss = *ob.sculpt;
  const bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(ob);

  AreaNormalCenterData anctd;
  threading::EnumerableThreadSpecific<SampleLocalData> all_tls;
  switch (pbvh.type()) {
    case bke::pbvh::Type::Mesh: {
      const Mesh &mesh = *static_cast<const Mesh *>(ob.data);
      const Span<float3> vert_positions = bke::pbvh::vert_positions_eval(depsgraph, ob);
      const Span<float3> vert_normals = bke::pbvh::vert_normals_eval(depsgraph, ob);
      const bke::AttributeAccessor attributes = mesh.attributes();
      const VArraySpan hide_vert = *attributes.lookup<bool>(".hide_vert", bke::AttrDomain::Point);

      const Span<bke::pbvh::MeshNode> nodes = pbvh.nodes<bke::pbvh::MeshNode>();
      anctd = threading::parallel_reduce(
          node_mask.index_range(),
          1,
          AreaNormalCenterData{},
          [&](const IndexRange range, AreaNormalCenterData anctd) {
            SampleLocalData &tls = all_tls.local();
            node_mask.slice(range).foreach_index([&](const int i) {
              calc_area_normal_and_center_node_mesh(ob,
                                                    vert_positions,
                                                    vert_normals,
                                                    hide_vert,
                                                    brush,
                                                    true,
                                                    false,
                                                    nodes[i],
                                                    tls,
                                                    anctd);
            });
            return anctd;
          },
          calc_area_normal_and_center_reduce);
      break;
    }
    case bke::pbvh::Type::BMesh: {
      const bool has_bm_orco = ss.bm && dyntopo::stroke_is_dyntopo(ob, brush);

      const Span<bke::pbvh::BMeshNode> nodes = pbvh.nodes<bke::pbvh::BMeshNode>();
      anctd = threading::parallel_reduce(
          node_mask.index_range(),
          1,
          AreaNormalCenterData{},
          [&](const IndexRange range, AreaNormalCenterData anctd) {
            SampleLocalData &tls = all_tls.local();
            node_mask.slice(range).foreach_index([&](const int i) {
              calc_area_normal_and_center_node_bmesh(
                  ob,
                  brush,
                  true,
                  false,
                  has_bm_orco,
                  static_cast<const blender::bke::pbvh::BMeshNode &>(nodes[i]),
                  tls,
                  anctd);
            });
            return anctd;
          },
          calc_area_normal_and_center_reduce);
      break;
    }
    case bke::pbvh::Type::Grids: {
      const Span<bke::pbvh::GridsNode> nodes = pbvh.nodes<bke::pbvh::GridsNode>();
      anctd = threading::parallel_reduce(
          node_mask.index_range(),
          1,
          AreaNormalCenterData{},
          [&](const IndexRange range, AreaNormalCenterData anctd) {
            SampleLocalData &tls = all_tls.local();
            node_mask.slice(range).foreach_index([&](const int i) {
              calc_area_normal_and_center_node_grids(ob, brush, true, false, nodes[i], tls, anctd);
            });
            return anctd;
          },
          calc_area_normal_and_center_reduce);
      break;
    }
  }

  for (const int i : {0, 1}) {
    if (anctd.count_no[i] != 0) {
      if (!math::is_zero(anctd.area_nos[i])) {
        return math::normalize(anctd.area_nos[i]);
      }
    }
  }
  return std::nullopt;
}

void calc_area_normal_and_center(const Depsgraph &depsgraph,
                                 const Brush &brush,
                                 const Object &ob,
                                 const IndexMask &node_mask,
                                 float r_area_no[3],
                                 float r_area_co[3])
{
  SculptSession &ss = *ob.sculpt;
  const bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(ob);
  int n;

  AreaNormalCenterData anctd;
  threading::EnumerableThreadSpecific<SampleLocalData> all_tls;
  switch (pbvh.type()) {
    case bke::pbvh::Type::Mesh: {
      const Mesh &mesh = *static_cast<const Mesh *>(ob.data);
      const Span<float3> vert_positions = bke::pbvh::vert_positions_eval(depsgraph, ob);
      const Span<float3> vert_normals = bke::pbvh::vert_normals_eval(depsgraph, ob);
      const bke::AttributeAccessor attributes = mesh.attributes();
      const VArraySpan hide_vert = *attributes.lookup<bool>(".hide_vert", bke::AttrDomain::Point);

      const Span<bke::pbvh::MeshNode> nodes = pbvh.nodes<bke::pbvh::MeshNode>();
      anctd = threading::parallel_reduce(
          node_mask.index_range(),
          1,
          AreaNormalCenterData{},
          [&](const IndexRange range, AreaNormalCenterData anctd) {
            SampleLocalData &tls = all_tls.local();
            node_mask.slice(range).foreach_index([&](const int i) {
              calc_area_normal_and_center_node_mesh(ob,
                                                    vert_positions,
                                                    vert_normals,
                                                    hide_vert,
                                                    brush,
                                                    true,
                                                    true,
                                                    nodes[i],
                                                    tls,
                                                    anctd);
            });
            return anctd;
          },
          calc_area_normal_and_center_reduce);
      break;
    }
    case bke::pbvh::Type::BMesh: {
      const bool has_bm_orco = ss.bm && dyntopo::stroke_is_dyntopo(ob, brush);

      const Span<bke::pbvh::BMeshNode> nodes = pbvh.nodes<bke::pbvh::BMeshNode>();
      anctd = threading::parallel_reduce(
          node_mask.index_range(),
          1,
          AreaNormalCenterData{},
          [&](const IndexRange range, AreaNormalCenterData anctd) {
            SampleLocalData &tls = all_tls.local();
            node_mask.slice(range).foreach_index([&](const int i) {
              calc_area_normal_and_center_node_bmesh(
                  ob, brush, true, true, has_bm_orco, nodes[i], tls, anctd);
            });
            return anctd;
          },
          calc_area_normal_and_center_reduce);
      break;
    }
    case bke::pbvh::Type::Grids: {
      const Span<bke::pbvh::GridsNode> nodes = pbvh.nodes<bke::pbvh::GridsNode>();
      anctd = threading::parallel_reduce(
          node_mask.index_range(),
          1,
          AreaNormalCenterData{},
          [&](const IndexRange range, AreaNormalCenterData anctd) {
            SampleLocalData &tls = all_tls.local();
            node_mask.slice(range).foreach_index([&](const int i) {
              calc_area_normal_and_center_node_grids(ob, brush, true, true, nodes[i], tls, anctd);
            });
            return anctd;
          },
          calc_area_normal_and_center_reduce);
      break;
    }
  }

  /* For flatten center. */
  for (n = 0; n < anctd.area_cos.size(); n++) {
    if (anctd.count_co[n] == 0) {
      continue;
    }

    mul_v3_v3fl(r_area_co, anctd.area_cos[n], 1.0f / anctd.count_co[n]);
    break;
  }

  if (n == 2) {
    zero_v3(r_area_co);
  }

  if (anctd.count_co[0] == 0 && anctd.count_co[1] == 0) {
    if (ss.cache) {
      copy_v3_v3(r_area_co, ss.cache->location_symm);
    }
  }

  /* For area normal. */
  for (n = 0; n < anctd.area_nos.size(); n++) {
    if (normalize_v3_v3(r_area_no, anctd.area_nos[n]) != 0.0f) {
      break;
    }
  }
}

}  // namespace blender::ed::sculpt_paint

/** \} */

/* -------------------------------------------------------------------- */
/** \name Generic Brush Utilities
 * \{ */

/**
 * Calculates the sign of the direction of the brush stroke, typically indicates whether the stroke
 * will deform a surface inwards or outwards along the brush normal.
 */
static float brush_flip(const Brush &brush, const blender::ed::sculpt_paint::StrokeCache &cache)
{
  if (brush.flag & BRUSH_INVERT_TO_SCRAPE_FILL) {
    return 1.0f;
  }

  const float dir = (brush.flag & BRUSH_DIR_IN) ? -1.0f : 1.0f;
  const float pen_flip = cache.pen_flip ? -1.0f : 1.0f;
  const float invert = cache.invert ? -1.0f : 1.0f;

  return dir * pen_flip * invert;
}

/**
 * Return modified brush strength. Includes the direction of the brush, positive
 * values pull vertices, negative values push. Uses tablet pressure and a
 * special multiplier found experimentally to scale the strength factor.
 */
static float brush_strength(const Sculpt &sd,
                            const blender::ed::sculpt_paint::StrokeCache &cache,
                            const float feather,
                            const UnifiedPaintSettings &ups,
                            const PaintModeSettings & /*paint_mode_settings*/)
{
  const Scene *scene = cache.vc->scene;
  const Brush &brush = *BKE_paint_brush_for_read(&sd.paint);

  /* Primary strength input; square it to make lower values more sensitive. */
  const float root_alpha = BKE_brush_alpha_get(scene, &brush);
  const float alpha = root_alpha * root_alpha;
  const float pressure = BKE_brush_use_alpha_pressure(&brush) ? cache.pressure : 1.0f;
  float overlap = ups.overlap_factor;
  /* Spacing is integer percentage of radius, divide by 50 to get
   * normalized diameter. */

  const float flip = brush_flip(brush, cache);

  /* Pressure final value after being tweaked depending on the brush. */
  float final_pressure;

  switch (brush.sculpt_brush_type) {
    case SCULPT_BRUSH_TYPE_CLAY:
      final_pressure = pow4f(pressure);
      overlap = (1.0f + overlap) / 2.0f;
      return 0.25f * alpha * flip * final_pressure * overlap * feather;
    case SCULPT_BRUSH_TYPE_DRAW:
    case SCULPT_BRUSH_TYPE_DRAW_SHARP:
    case SCULPT_BRUSH_TYPE_LAYER:
      return alpha * flip * pressure * overlap * feather;
    case SCULPT_BRUSH_TYPE_DISPLACEMENT_ERASER:
      return alpha * pressure * overlap * feather;
    case SCULPT_BRUSH_TYPE_CLOTH:
      if (brush.cloth_deform_type == BRUSH_CLOTH_DEFORM_GRAB) {
        /* Grab deform uses the same falloff as a regular grab brush. */
        return root_alpha * feather;
      }
      else if (brush.cloth_deform_type == BRUSH_CLOTH_DEFORM_SNAKE_HOOK) {
        return root_alpha * feather * pressure * overlap;
      }
      else if (brush.cloth_deform_type == BRUSH_CLOTH_DEFORM_EXPAND) {
        /* Expand is more sensible to strength as it keeps expanding the cloth when sculpting over
         * the same vertices. */
        return 0.1f * alpha * flip * pressure * overlap * feather;
      }
      else {
        /* Multiply by 10 by default to get a larger range of strength depending on the size of the
         * brush and object. */
        return 10.0f * alpha * flip * pressure * overlap * feather;
      }
    case SCULPT_BRUSH_TYPE_DRAW_FACE_SETS:
      return alpha * pressure * overlap * feather;
    case SCULPT_BRUSH_TYPE_SLIDE_RELAX:
      return alpha * pressure * overlap * feather * 2.0f;
    case SCULPT_BRUSH_TYPE_PAINT:
      final_pressure = pressure * pressure;
      return final_pressure * overlap * feather;
    case SCULPT_BRUSH_TYPE_SMEAR:
    case SCULPT_BRUSH_TYPE_DISPLACEMENT_SMEAR:
      return alpha * pressure * overlap * feather;
    case SCULPT_BRUSH_TYPE_CLAY_STRIPS:
      /* Clay Strips needs less strength to compensate the curve. */
      final_pressure = powf(pressure, 1.5f);
      return alpha * flip * final_pressure * overlap * feather * 0.3f;
    case SCULPT_BRUSH_TYPE_CLAY_THUMB:
      final_pressure = pressure * pressure;
      return alpha * flip * final_pressure * overlap * feather * 1.3f;

    case SCULPT_BRUSH_TYPE_MASK:
      overlap = (1.0f + overlap) / 2.0f;
      switch ((BrushMaskTool)brush.mask_tool) {
        case BRUSH_MASK_DRAW:
          return alpha * flip * pressure * overlap * feather;
        case BRUSH_MASK_SMOOTH:
          return alpha * pressure * feather;
      }
      break;
    case SCULPT_BRUSH_TYPE_CREASE:
    case SCULPT_BRUSH_TYPE_BLOB:
      return alpha * flip * pressure * overlap * feather;

    case SCULPT_BRUSH_TYPE_INFLATE:
      if (flip > 0.0f) {
        return 0.250f * alpha * flip * pressure * overlap * feather;
      }
      else {
        return 0.125f * alpha * flip * pressure * overlap * feather;
      }

    case SCULPT_BRUSH_TYPE_MULTIPLANE_SCRAPE:
      overlap = (1.0f + overlap) / 2.0f;
      return alpha * flip * pressure * overlap * feather;

    case SCULPT_BRUSH_TYPE_FILL:
    case SCULPT_BRUSH_TYPE_SCRAPE:
    case SCULPT_BRUSH_TYPE_FLATTEN:
      if (flip > 0.0f) {
        overlap = (1.0f + overlap) / 2.0f;
        return alpha * flip * pressure * overlap * feather;
      }
      else {
        /* Reduce strength for DEEPEN, PEAKS, and CONTRAST. */
        return 0.5f * alpha * flip * pressure * overlap * feather;
      }

    case SCULPT_BRUSH_TYPE_SMOOTH:
      return flip * alpha * pressure * feather;

    case SCULPT_BRUSH_TYPE_PINCH:
      if (flip > 0.0f) {
        return alpha * flip * pressure * overlap * feather;
      }
      else {
        return 0.25f * alpha * flip * pressure * overlap * feather;
      }

    case SCULPT_BRUSH_TYPE_NUDGE:
      overlap = (1.0f + overlap) / 2.0f;
      return alpha * pressure * overlap * feather;

    case SCULPT_BRUSH_TYPE_THUMB:
      return alpha * pressure * feather;

    case SCULPT_BRUSH_TYPE_SNAKE_HOOK:
      return root_alpha * feather;

    case SCULPT_BRUSH_TYPE_GRAB:
      return root_alpha * feather;

    case SCULPT_BRUSH_TYPE_ROTATE:
      return alpha * pressure * feather;

    case SCULPT_BRUSH_TYPE_ELASTIC_DEFORM:
    case SCULPT_BRUSH_TYPE_POSE:
    case SCULPT_BRUSH_TYPE_BOUNDARY:
      return root_alpha * feather;
    case SCULPT_BRUSH_TYPE_SIMPLIFY:
      /* The Dyntopo Density brush does not use a normal brush workflow to calculate the effect,
       * and this strength value is unused. */
      return 0.0f;
  }
  BLI_assert_unreachable();
  return 0.0f;
}

void sculpt_apply_texture(const SculptSession &ss,
                          const Brush &brush,
                          const float brush_point[3],
                          const int thread_id,
                          float *r_value,
                          float r_rgba[4])
{
  const blender::ed::sculpt_paint::StrokeCache &cache = *ss.cache;
  const Scene *scene = cache.vc->scene;
  const MTex *mtex = BKE_brush_mask_texture_get(&brush, OB_MODE_SCULPT);

  if (!mtex->tex) {
    *r_value = 1.0f;
    copy_v4_fl(r_rgba, 1.0f);
    return;
  }

  float point[3];
  sub_v3_v3v3(point, brush_point, cache.plane_offset);

  if (mtex->brush_map_mode == MTEX_MAP_MODE_3D) {
    /* Get strength by feeding the vertex location directly into a texture. */
    *r_value = BKE_brush_sample_tex_3d(scene, &brush, mtex, point, r_rgba, 0, ss.tex_pool);
  }
  else {
    /* If the active area is being applied for symmetry, flip it
     * across the symmetry axis and rotate it back to the original
     * position in order to project it. This insures that the
     * brush texture will be oriented correctly. */
    if (cache.radial_symmetry_pass) {
      mul_m4_v3(cache.symm_rot_mat_inv.ptr(), point);
    }
    float3 symm_point = blender::ed::sculpt_paint::symmetry_flip(point,
                                                                 cache.mirror_symmetry_pass);

    /* Still no symmetry supported for other paint modes.
     * Sculpt does it DIY. */
    if (mtex->brush_map_mode == MTEX_MAP_MODE_AREA) {
      /* Similar to fixed mode, but projects from brush angle
       * rather than view direction. */

      mul_m4_v3(cache.brush_local_mat.ptr(), symm_point);

      float x = symm_point[0];
      float y = symm_point[1];

      x *= mtex->size[0];
      y *= mtex->size[1];

      x += mtex->ofs[0];
      y += mtex->ofs[1];

      paint_get_tex_pixel(mtex, x, y, ss.tex_pool, thread_id, r_value, r_rgba);

      add_v3_fl(r_rgba, brush.texture_sample_bias);  // v3 -> Ignore alpha
      *r_value -= brush.texture_sample_bias;
    }
    else {
      const blender::float2 point_2d = ED_view3d_project_float_v2_m4(
          cache.vc->region, symm_point, cache.projection_mat);
      const float point_3d[3] = {point_2d[0], point_2d[1], 0.0f};
      *r_value = BKE_brush_sample_tex_3d(scene, &brush, mtex, point_3d, r_rgba, 0, ss.tex_pool);
    }
  }
}

void SCULPT_calc_vertex_displacement(const SculptSession &ss,
                                     const Brush &brush,
                                     float rgba[3],
                                     float r_offset[3])
{
  mul_v3_fl(rgba, ss.cache->bstrength);
  /* Handle brush inversion */
  if (ss.cache->bstrength < 0) {
    rgba[0] *= -1;
    rgba[1] *= -1;
  }

  /* Apply texture size */
  for (int i = 0; i < 3; ++i) {
    rgba[i] *= blender::math::safe_divide(1.0f, pow2f(brush.mtex.size[i]));
  }

  /* Transform vector to object space */
  mul_mat3_m4_v3(ss.cache->brush_local_mat_inv.ptr(), rgba);

  /* Handle symmetry */
  if (ss.cache->radial_symmetry_pass) {
    mul_m4_v3(ss.cache->symm_rot_mat.ptr(), rgba);
  }
  copy_v3_v3(r_offset,
             blender::ed::sculpt_paint::symmetry_flip(rgba, ss.cache->mirror_symmetry_pass));
}

namespace blender::ed::sculpt_paint {

bool node_fully_masked_or_hidden(const bke::pbvh::Node &node)
{
  if (BKE_pbvh_node_fully_hidden_get(node)) {
    return true;
  }
  if (BKE_pbvh_node_fully_masked_get(node)) {
    return true;
  }
  return false;
}

bool node_in_sphere(const bke::pbvh::Node &node,
                    const float3 &location,
                    const float radius_sq,
                    const bool original)
{
  const Bounds<float3> bounds = original ? BKE_pbvh_node_get_original_BB(&node) :
                                           bke::pbvh::node_bounds(node);
  const float3 nearest = math::clamp(location, bounds.min, bounds.max);
  return math::distance_squared(location, nearest) < radius_sq;
}

bool node_in_cylinder(const DistRayAABB_Precalc &ray_dist_precalc,
                      const bke::pbvh::Node &node,
                      const float radius_sq,
                      const bool original)
{
  const Bounds<float3> bounds = (original) ? BKE_pbvh_node_get_original_BB(&node) :
                                             bke::pbvh::node_bounds(node);

  float dummy_co[3], dummy_depth;
  const float dist_sq = dist_squared_ray_to_aabb_v3(
      &ray_dist_precalc, bounds.min, bounds.max, dummy_co, &dummy_depth);

  /* TODO: Solve issues and enable distance check. */
  return dist_sq < radius_sq || true;
}

static IndexMask pbvh_gather_cursor_update(Object &ob, bool use_original, IndexMaskMemory &memory)
{
  SculptSession &ss = *ob.sculpt;
  const bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(ob);
  const float3 center = ss.cache ? ss.cache->location_symm : ss.cursor_location;
  return bke::pbvh::search_nodes(pbvh, memory, [&](const bke::pbvh::Node &node) {
    return node_in_sphere(node, center, ss.cursor_radius, use_original);
  });
}

/** \return All nodes that are potentially within the cursor or brush's area of influence. */
static IndexMask pbvh_gather_generic(
    Object &ob, const Brush &brush, bool use_original, float radius_scale, IndexMaskMemory &memory)
{
  SculptSession &ss = *ob.sculpt;
  const bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(ob);

  const float3 center = ss.cache->location_symm;
  const float radius_sq = math::square(ss.cache->radius * radius_scale);
  const bool ignore_ineffective = brush.sculpt_brush_type != SCULPT_BRUSH_TYPE_MASK;
  switch (brush.falloff_shape) {
    case PAINT_FALLOFF_SHAPE_SPHERE: {
      return bke::pbvh::search_nodes(pbvh, memory, [&](const bke::pbvh::Node &node) {
        if (ignore_ineffective && node_fully_masked_or_hidden(node)) {
          return false;
        }
        return node_in_sphere(node, center, radius_sq, use_original);
      });
    }

    case PAINT_FALLOFF_SHAPE_TUBE: {
      const DistRayAABB_Precalc ray_dist_precalc = dist_squared_ray_to_aabb_v3_precalc(
          center, ss.cache->view_normal_symm);
      return bke::pbvh::search_nodes(pbvh, memory, [&](const bke::pbvh::Node &node) {
        if (ignore_ineffective && node_fully_masked_or_hidden(node)) {
          return false;
        }
        return node_in_cylinder(ray_dist_precalc, node, radius_sq, use_original);
      });
    }
  }

  return {};
}

static IndexMask pbvh_gather_texpaint(Object &ob,
                                      const Brush &brush,
                                      const bool use_original,
                                      const float radius_scale,
                                      IndexMaskMemory &memory)
{
  return pbvh_gather_generic(ob, brush, use_original, radius_scale, memory);
}

/* Calculate primary direction of movement for many brushes. */
static float3 calc_sculpt_normal(const Depsgraph &depsgraph,
                                 const Sculpt &sd,
                                 Object &ob,
                                 const IndexMask &node_mask)
{
  const Brush &brush = *BKE_paint_brush_for_read(&sd.paint);
  const SculptSession &ss = *ob.sculpt;
  switch (brush.sculpt_plane) {
    case SCULPT_DISP_DIR_AREA:
      return calc_area_normal(depsgraph, brush, ob, node_mask).value_or(float3(0));
    case SCULPT_DISP_DIR_VIEW:
      return ss.cache->view_normal;
    case SCULPT_DISP_DIR_X:
      return float3(1, 0, 0);
    case SCULPT_DISP_DIR_Y:
      return float3(0, 1, 0);
    case SCULPT_DISP_DIR_Z:
      return float3(0, 0, 1);
  }
  BLI_assert_unreachable();
  return {};
}

static void update_sculpt_normal(const Depsgraph &depsgraph,
                                 const Sculpt &sd,
                                 Object &ob,
                                 const IndexMask &node_mask)
{
  const Brush &brush = *BKE_paint_brush_for_read(&sd.paint);
  StrokeCache &cache = *ob.sculpt->cache;
  /* Grab brush does not update the sculpt normal during a stroke. */
  const bool update_normal = !(brush.flag & BRUSH_ORIGINAL_NORMAL) &&
                             !(brush.sculpt_brush_type == SCULPT_BRUSH_TYPE_GRAB) &&
                             !(brush.sculpt_brush_type == SCULPT_BRUSH_TYPE_THUMB &&
                               !(brush.flag & BRUSH_ANCHORED)) &&
                             !(brush.sculpt_brush_type == SCULPT_BRUSH_TYPE_ELASTIC_DEFORM) &&
                             !(brush.sculpt_brush_type == SCULPT_BRUSH_TYPE_SNAKE_HOOK &&
                               cache.normal_weight > 0.0f);

  if (cache.mirror_symmetry_pass == 0 && cache.radial_symmetry_pass == 0 &&
      (SCULPT_stroke_is_first_brush_step_of_symmetry_pass(cache) || update_normal))
  {
    cache.sculpt_normal = calc_sculpt_normal(depsgraph, sd, ob, node_mask);
    if (brush.falloff_shape == PAINT_FALLOFF_SHAPE_TUBE) {
      project_plane_v3_v3v3(cache.sculpt_normal, cache.sculpt_normal, cache.view_normal_symm);
      normalize_v3(cache.sculpt_normal);
    }
    copy_v3_v3(cache.sculpt_normal_symm, cache.sculpt_normal);
  }
  else {
    cache.sculpt_normal_symm = symmetry_flip(cache.sculpt_normal, cache.mirror_symmetry_pass);
    mul_m4_v3(cache.symm_rot_mat.ptr(), cache.sculpt_normal_symm);
  }
}

static void calc_local_from_screen(const ViewContext &vc,
                                   const float center[3],
                                   const float screen_dir[2],
                                   float r_local_dir[3])
{
  Object &ob = *vc.obact;
  float loc[3];

  mul_v3_m4v3(loc, ob.object_to_world().ptr(), center);
  const float zfac = ED_view3d_calc_zfac(vc.rv3d, loc);

  ED_view3d_win_to_delta(vc.region, screen_dir, zfac, r_local_dir);
  normalize_v3(r_local_dir);

  add_v3_v3(r_local_dir, ob.loc);
  mul_m4_v3(ob.world_to_object().ptr(), r_local_dir);
}

static void calc_brush_local_mat(const float rotation,
                                 const Object &ob,
                                 float local_mat[4][4],
                                 float local_mat_inv[4][4])
{
  const StrokeCache *cache = ob.sculpt->cache;
  float tmat[4][4];
  float mat[4][4];
  float scale[4][4];
  float angle, v[3];

  /* Ensure `ob.world_to_object` is up to date. */
  invert_m4_m4(ob.runtime->world_to_object.ptr(), ob.object_to_world().ptr());

  /* Initialize last column of matrix. */
  mat[0][3] = 0.0f;
  mat[1][3] = 0.0f;
  mat[2][3] = 0.0f;
  mat[3][3] = 1.0f;

  /* Read rotation (user angle, rake, etc.) to find the view's movement direction (negative X of
   * the brush). */
  angle = rotation + cache->special_rotation;
  /* By convention, motion direction points down the brush's Y axis, the angle represents the X
   * axis, normal is a 90 deg CCW rotation of the motion direction. */
  float motion_normal_screen[2];
  motion_normal_screen[0] = cosf(angle);
  motion_normal_screen[1] = sinf(angle);
  /* Convert view's brush transverse direction to object-space,
   * i.e. the normal of the plane described by the motion */
  float motion_normal_local[3];
  calc_local_from_screen(
      *cache->vc, cache->location_symm, motion_normal_screen, motion_normal_local);

  /* Calculate the movement direction for the local matrix.
   * Note that there is a deliberate prioritization here: Our calculations are
   * designed such that the _motion vector_ gets projected into the tangent space;
   * in most cases this will be more intuitive than projecting the transverse
   * direction (which is orthogonal to the motion direction and therefore less
   * apparent to the user).
   * The Y-axis of the brush-local frame has to lie in the intersection of the tangent plane
   * and the motion plane. */

  cross_v3_v3v3(v, cache->sculpt_normal, motion_normal_local);
  normalize_v3_v3(mat[1], v);

  /* Get other axes. */
  cross_v3_v3v3(mat[0], mat[1], cache->sculpt_normal);
  copy_v3_v3(mat[2], cache->sculpt_normal);

  /* Set location. */
  copy_v3_v3(mat[3], cache->location_symm);

  /* Scale by brush radius. */
  float radius = cache->radius;

  normalize_m4(mat);
  scale_m4_fl(scale, radius);
  mul_m4_m4m4(tmat, mat, scale);

  /* Return tmat as is (for converting from local area coords to model-space coords). */
  copy_m4_m4(local_mat_inv, tmat);
  /* Return inverse (for converting from model-space coords to local area coords). */
  invert_m4_m4(local_mat, tmat);
}

}  // namespace blender::ed::sculpt_paint

#define SCULPT_TILT_SENSITIVITY 0.7f
void SCULPT_tilt_apply_to_normal(float r_normal[3],
                                 blender::ed::sculpt_paint::StrokeCache *cache,
                                 const float tilt_strength)
{
  if (!U.experimental.use_sculpt_tools_tilt) {
    return;
  }
  const float rot_max = M_PI_2 * tilt_strength * SCULPT_TILT_SENSITIVITY;
  mul_v3_mat3_m4v3(r_normal, cache->vc->obact->object_to_world().ptr(), r_normal);
  float normal_tilt_y[3];
  rotate_v3_v3v3fl(normal_tilt_y, r_normal, cache->vc->rv3d->viewinv[0], cache->tilt.y * rot_max);
  float normal_tilt_xy[3];
  rotate_v3_v3v3fl(
      normal_tilt_xy, normal_tilt_y, cache->vc->rv3d->viewinv[1], cache->tilt.x * rot_max);
  mul_v3_mat3_m4v3(r_normal, cache->vc->obact->world_to_object().ptr(), normal_tilt_xy);
  normalize_v3(r_normal);
}

void SCULPT_tilt_effective_normal_get(const SculptSession &ss, const Brush &brush, float r_no[3])
{
  copy_v3_v3(r_no, ss.cache->sculpt_normal_symm);
  SCULPT_tilt_apply_to_normal(r_no, ss.cache, brush.tilt_strength_factor);
}

static void update_brush_local_mat(const Sculpt &sd, Object &ob)
{
  using namespace blender::ed::sculpt_paint;
  StrokeCache *cache = ob.sculpt->cache;

  if (cache->mirror_symmetry_pass == 0 && cache->radial_symmetry_pass == 0) {
    const Brush *brush = BKE_paint_brush_for_read(&sd.paint);
    const MTex *mask_tex = BKE_brush_mask_texture_get(brush, OB_MODE_SCULPT);
    calc_brush_local_mat(
        mask_tex->rot, ob, cache->brush_local_mat.ptr(), cache->brush_local_mat_inv.ptr());
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Texture painting
 * \{ */

static bool sculpt_needs_pbvh_pixels(PaintModeSettings &paint_mode_settings,
                                     const Brush &brush,
                                     Object &ob)
{
  if (brush.sculpt_brush_type == SCULPT_BRUSH_TYPE_PAINT &&
      U.experimental.use_sculpt_texture_paint)
  {
    Image *image;
    ImageUser *image_user;
    return SCULPT_paint_image_canvas_get(paint_mode_settings, ob, &image, &image_user);
  }

  return false;
}

static void sculpt_pbvh_update_pixels(const Depsgraph &depsgraph,
                                      PaintModeSettings &paint_mode_settings,
                                      Object &ob)
{
  using namespace blender;
  BLI_assert(ob.type == OB_MESH);

  Image *image;
  ImageUser *image_user;
  if (!SCULPT_paint_image_canvas_get(paint_mode_settings, ob, &image, &image_user)) {
    return;
  }

  bke::pbvh::build_pixels(depsgraph, ob, *image, *image_user);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Generic Brush Plane & Symmetry Utilities
 * \{ */

struct SculptRaycastData {
  Object *object;
  SculptSession *ss;
  const float *ray_start;
  const float *ray_normal;
  bool hit;
  float depth;
  bool original;
  Span<blender::float3> vert_positions;
  blender::OffsetIndices<int> faces;
  Span<int> corner_verts;
  Span<blender::int3> corner_tris;
  blender::VArraySpan<bool> hide_poly;

  const SubdivCCG *subdiv_ccg;

  ActiveVert active_vertex = {};
  float3 face_normal;

  int active_face_grid_index;

  IsectRayPrecalc isect_precalc;
};

struct SculptFindNearestToRayData {
  Object *object;
  SculptSession *ss;
  const float *ray_start, *ray_normal;
  bool hit;
  float depth;
  float dist_sq_to_ray;
  bool original;
  Span<float3> vert_positions;
  blender::OffsetIndices<int> faces;
  Span<int> corner_verts;
  Span<blender::int3> corner_tris;
  blender::VArraySpan<bool> hide_poly;

  const SubdivCCG *subdiv_ccg;
};

ePaintSymmetryAreas SCULPT_get_vertex_symm_area(const float co[3])
{
  ePaintSymmetryAreas symm_area = ePaintSymmetryAreas(PAINT_SYMM_AREA_DEFAULT);
  if (co[0] < 0.0f) {
    symm_area |= PAINT_SYMM_AREA_X;
  }
  if (co[1] < 0.0f) {
    symm_area |= PAINT_SYMM_AREA_Y;
  }
  if (co[2] < 0.0f) {
    symm_area |= PAINT_SYMM_AREA_Z;
  }
  return symm_area;
}

static void flip_qt_qt(float out[4], const float in[4], const ePaintSymmetryFlags symm)
{
  float axis[3], angle;

  quat_to_axis_angle(axis, &angle, in);
  normalize_v3(axis);

  if (symm & PAINT_SYMM_X) {
    axis[0] *= -1.0f;
    angle *= -1.0f;
  }
  if (symm & PAINT_SYMM_Y) {
    axis[1] *= -1.0f;
    angle *= -1.0f;
  }
  if (symm & PAINT_SYMM_Z) {
    axis[2] *= -1.0f;
    angle *= -1.0f;
  }

  axis_angle_normalized_to_quat(out, axis, angle);
}

static void flip_qt(float quat[4], const ePaintSymmetryFlags symm)
{
  flip_qt_qt(quat, quat, symm);
}

float3 SCULPT_flip_v3_by_symm_area(const float3 &vector,
                                   const ePaintSymmetryFlags symm,
                                   const ePaintSymmetryAreas symmarea,
                                   const float3 &pivot)
{
  float3 result = vector;
  for (int i = 0; i < 3; i++) {
    ePaintSymmetryFlags symm_it = ePaintSymmetryFlags(1 << i);
    if (!(symm & symm_it)) {
      continue;
    }
    if (symmarea & symm_it) {
      result = blender::ed::sculpt_paint::symmetry_flip(result, symm_it);
    }
    if (pivot[i] < 0.0f) {
      result = blender::ed::sculpt_paint::symmetry_flip(result, symm_it);
    }
  }
  return result;
}

void SCULPT_flip_quat_by_symm_area(float quat[4],
                                   const ePaintSymmetryFlags symm,
                                   const ePaintSymmetryAreas symmarea,
                                   const float pivot[3])
{
  for (int i = 0; i < 3; i++) {
    ePaintSymmetryFlags symm_it = ePaintSymmetryFlags(1 << i);
    if (!(symm & symm_it)) {
      continue;
    }
    if (symmarea & symm_it) {
      flip_qt(quat, symm_it);
    }
    if (pivot[i] < 0.0f) {
      flip_qt(quat, symm_it);
    }
  }
}

bool SCULPT_brush_type_needs_all_pbvh_nodes(const Brush &brush)
{
  if (brush.sculpt_brush_type == SCULPT_BRUSH_TYPE_ELASTIC_DEFORM) {
    /* Elastic deformations in any brush need all nodes to avoid artifacts as the effect
     * of the Kelvinlet is not constrained by the radius. */
    return true;
  }

  if (brush.sculpt_brush_type == SCULPT_BRUSH_TYPE_POSE) {
    /* Pose needs all nodes because it applies all symmetry iterations at the same time
     * and the IK chain can grow to any area of the model. */
    /* TODO: This can be optimized by filtering the nodes after calculating the chain. */
    return true;
  }

  if (brush.sculpt_brush_type == SCULPT_BRUSH_TYPE_BOUNDARY) {
    /* Boundary needs all nodes because it is not possible to know where the boundary
     * deformation is going to be propagated before calculating it. */
    /* TODO: after calculating the boundary info in the first iteration, it should be
     * possible to get the nodes that have vertices included in any boundary deformation
     * and cache them. */
    return true;
  }

  if (brush.sculpt_brush_type == SCULPT_BRUSH_TYPE_SNAKE_HOOK &&
      brush.snake_hook_deform_type == BRUSH_SNAKE_HOOK_DEFORM_ELASTIC)
  {
    /* Snake hook in elastic deform type has same requirements as the elastic deform brush. */
    return true;
  }
  return false;
}

namespace blender::ed::sculpt_paint {

void calc_brush_plane(const Depsgraph &depsgraph,
                      const Brush &brush,
                      Object &ob,
                      const IndexMask &node_mask,
                      float3 &r_area_no,
                      float3 &r_area_co)
{
  const SculptSession &ss = *ob.sculpt;

  zero_v3(r_area_co);
  zero_v3(r_area_no);

  if (SCULPT_stroke_is_main_symmetry_pass(*ss.cache) &&
      (SCULPT_stroke_is_first_brush_step_of_symmetry_pass(*ss.cache) ||
       !(brush.flag & BRUSH_ORIGINAL_PLANE) || !(brush.flag & BRUSH_ORIGINAL_NORMAL)))
  {
    switch (brush.sculpt_plane) {
      case SCULPT_DISP_DIR_VIEW:
        copy_v3_v3(r_area_no, ss.cache->view_normal);
        break;

      case SCULPT_DISP_DIR_X:
        ARRAY_SET_ITEMS(r_area_no, 1.0f, 0.0f, 0.0f);
        break;

      case SCULPT_DISP_DIR_Y:
        ARRAY_SET_ITEMS(r_area_no, 0.0f, 1.0f, 0.0f);
        break;

      case SCULPT_DISP_DIR_Z:
        ARRAY_SET_ITEMS(r_area_no, 0.0f, 0.0f, 1.0f);
        break;

      case SCULPT_DISP_DIR_AREA:
        calc_area_normal_and_center(depsgraph, brush, ob, node_mask, r_area_no, r_area_co);
        if (brush.falloff_shape == PAINT_FALLOFF_SHAPE_TUBE) {
          project_plane_v3_v3v3(r_area_no, r_area_no, ss.cache->view_normal_symm);
          normalize_v3(r_area_no);
        }
        break;
    }

    /* For flatten center. */
    /* Flatten center has not been calculated yet if we are not using the area normal. */
    if (brush.sculpt_plane != SCULPT_DISP_DIR_AREA) {
      calc_area_center(depsgraph, brush, ob, node_mask, r_area_co);
    }

    /* For area normal. */
    if (!SCULPT_stroke_is_first_brush_step_of_symmetry_pass(*ss.cache) &&
        (brush.flag & BRUSH_ORIGINAL_NORMAL))
    {
      copy_v3_v3(r_area_no, ss.cache->sculpt_normal);
    }
    else {
      copy_v3_v3(ss.cache->sculpt_normal, r_area_no);
    }

    /* For flatten center. */
    if (!SCULPT_stroke_is_first_brush_step_of_symmetry_pass(*ss.cache) &&
        (brush.flag & BRUSH_ORIGINAL_PLANE))
    {
      copy_v3_v3(r_area_co, ss.cache->last_center);
    }
    else {
      copy_v3_v3(ss.cache->last_center, r_area_co);
    }
  }
  else {
    /* For area normal. */
    copy_v3_v3(r_area_no, ss.cache->sculpt_normal);

    /* For flatten center. */
    copy_v3_v3(r_area_co, ss.cache->last_center);

    /* For area normal. */
    r_area_no = symmetry_flip(r_area_no, ss.cache->mirror_symmetry_pass);

    /* For flatten center. */
    r_area_co = symmetry_flip(r_area_co, ss.cache->mirror_symmetry_pass);

    /* For area normal. */
    mul_m4_v3(ss.cache->symm_rot_mat.ptr(), r_area_no);

    /* For flatten center. */
    mul_m4_v3(ss.cache->symm_rot_mat.ptr(), r_area_co);

    /* Shift the plane for the current tile. */
    add_v3_v3(r_area_co, ss.cache->plane_offset);
  }
}

}  // namespace blender::ed::sculpt_paint

float SCULPT_brush_plane_offset_get(const Sculpt &sd, const SculptSession &ss)
{
  const Brush &brush = *BKE_paint_brush_for_read(&sd.paint);

  float rv = brush.plane_offset;

  if (brush.flag & BRUSH_OFFSET_PRESSURE) {
    rv *= ss.cache->pressure;
  }

  return rv;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Sculpt Brush Utilities
 * \{ */

namespace blender::ed::sculpt_paint {

static void dynamic_topology_update(const Depsgraph &depsgraph,
                                    const Scene & /*scene*/,
                                    const Sculpt &sd,
                                    Object &ob,
                                    const Brush &brush,
                                    UnifiedPaintSettings & /*ups*/,
                                    PaintModeSettings & /*paint_mode_settings*/)
{
  SculptSession &ss = *ob.sculpt;
  bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(ob);

  /* Build a list of all nodes that are potentially within the brush's area of influence. */
  const bool use_original = brush_type_needs_original(brush.sculpt_brush_type) ? true :
                                                                                 !ss.cache->accum;
  const float radius_scale = 1.25f;

  IndexMaskMemory memory;
  const IndexMask node_mask = pbvh_gather_generic(ob, brush, use_original, radius_scale, memory);
  if (node_mask.is_empty()) {
    return;
  }

  MutableSpan<bke::pbvh::BMeshNode> nodes = pbvh.nodes<bke::pbvh::BMeshNode>();

  /* Free index based vertex info as it will become invalid after modifying the topology during the
   * stroke. */
  ss.vertex_info.boundary.clear();

  PBVHTopologyUpdateMode mode = PBVHTopologyUpdateMode(0);
  float location[3];

  if (!(sd.flags & SCULPT_DYNTOPO_DETAIL_MANUAL)) {
    if (sd.flags & SCULPT_DYNTOPO_SUBDIVIDE) {
      mode |= PBVH_Subdivide;
    }

    if ((sd.flags & SCULPT_DYNTOPO_COLLAPSE) ||
        (brush.sculpt_brush_type == SCULPT_BRUSH_TYPE_SIMPLIFY))
    {
      mode |= PBVH_Collapse;
    }
  }

  if (brush.sculpt_brush_type == SCULPT_BRUSH_TYPE_MASK) {
    undo::push_nodes(depsgraph, ob, node_mask, undo::Type::Mask);
  }
  else {
    undo::push_nodes(depsgraph, ob, node_mask, undo::Type::Position);
  }
  pbvh.tag_positions_changed(node_mask);
  pbvh.tag_topology_changed(node_mask);
  node_mask.foreach_index([&](const int i) { BKE_pbvh_node_mark_topology_update(nodes[i]); });
  node_mask.foreach_index(GrainSize(1), [&](const int i) {
    BKE_pbvh_bmesh_node_save_orig(ss.bm, ss.bm_log, &nodes[i], false);
  });

  float max_edge_len;
  if (sd.flags & (SCULPT_DYNTOPO_DETAIL_CONSTANT | SCULPT_DYNTOPO_DETAIL_MANUAL)) {
    max_edge_len = dyntopo::detail_size::constant_to_detail_size(sd.constant_detail, ob);
  }
  else if (sd.flags & SCULPT_DYNTOPO_DETAIL_BRUSH) {
    max_edge_len = dyntopo::detail_size::brush_to_detail_size(sd.detail_percent, ss.cache->radius);
  }
  else {
    max_edge_len = dyntopo::detail_size::relative_to_detail_size(
        sd.detail_size, ss.cache->radius, ss.cache->dyntopo_pixel_radius, U.pixelsize);
  }
  const float min_edge_len = max_edge_len * dyntopo::detail_size::EDGE_LENGTH_MIN_FACTOR;

  bke::pbvh::bmesh_update_topology(*ss.bm,
                                   pbvh,
                                   *ss.bm_log,
                                   mode,
                                   min_edge_len,
                                   max_edge_len,
                                   ss.cache->location_symm,
                                   ss.cache->view_normal_symm,
                                   ss.cache->radius,
                                   (brush.flag & BRUSH_FRONTFACE) != 0,
                                   (brush.falloff_shape != PAINT_FALLOFF_SHAPE_SPHERE));

  /* Update average stroke position. */
  copy_v3_v3(location, ss.cache->location);
  mul_m4_v3(ob.object_to_world().ptr(), location);
}

static void push_undo_nodes(const Depsgraph &depsgraph,
                            Object &ob,
                            const Brush &brush,
                            const IndexMask &node_mask)
{
  SculptSession &ss = *ob.sculpt;
  bool need_coords = ss.cache->supports_gravity;

  if (brush.sculpt_brush_type == SCULPT_BRUSH_TYPE_DRAW_FACE_SETS) {
    /* Draw face sets in smooth mode moves the vertices. */
    if (ss.cache->alt_smooth) {
      need_coords = true;
    }
    else {
      undo::push_nodes(depsgraph, ob, node_mask, undo::Type::FaceSet);
    }
  }
  else if (brush.sculpt_brush_type == SCULPT_BRUSH_TYPE_MASK) {
    undo::push_nodes(depsgraph, ob, node_mask, undo::Type::Mask);
  }
  else if (SCULPT_brush_type_is_paint(brush.sculpt_brush_type)) {
    undo::push_nodes(depsgraph, ob, node_mask, undo::Type::Color);
  }
  else {
    need_coords = true;
  }

  if (need_coords) {
    undo::push_nodes(depsgraph, ob, node_mask, undo::Type::Position);
  }
}

static void do_brush_action(const Depsgraph &depsgraph,
                            const Scene &scene,
                            const Sculpt &sd,
                            Object &ob,
                            const Brush &brush,
                            UnifiedPaintSettings &ups,
                            PaintModeSettings &paint_mode_settings)
{
  SculptSession &ss = *ob.sculpt;
  bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(ob);
  IndexMaskMemory memory;
  IndexMask node_mask, texnode_mask;

  const bool use_original = brush_type_needs_original(brush.sculpt_brush_type) ? true :
                                                                                 !ss.cache->accum;
  const bool use_pixels = sculpt_needs_pbvh_pixels(paint_mode_settings, brush, ob);

  if (sculpt_needs_pbvh_pixels(paint_mode_settings, brush, ob)) {
    sculpt_pbvh_update_pixels(depsgraph, paint_mode_settings, ob);

    texnode_mask = pbvh_gather_texpaint(ob, brush, use_original, 1.0f, memory);

    if (texnode_mask.is_empty()) {
      return;
    }
  }

  /* Build a list of all nodes that are potentially within the brush's area of influence */

  if (SCULPT_brush_type_needs_all_pbvh_nodes(brush)) {
    /* These brushes need to update all nodes as they are not constrained by the brush radius */
    node_mask = bke::pbvh::all_leaf_nodes(pbvh, memory);
  }
  else if (brush.sculpt_brush_type == SCULPT_BRUSH_TYPE_CLOTH) {
    node_mask = cloth::brush_affected_nodes_gather(ob, brush, memory);
  }
  else {
    float radius_scale = 1.0f;

    /* Corners of square brushes can go outside the brush radius. */
    if (BKE_brush_has_cube_tip(&brush, PaintMode::Sculpt)) {
      radius_scale = M_SQRT2;
    }

    /* With these options enabled not all required nodes are inside the original brush radius, so
     * the brush can produce artifacts in some situations. */
    if (brush.sculpt_brush_type == SCULPT_BRUSH_TYPE_DRAW && brush.flag & BRUSH_ORIGINAL_NORMAL) {
      radius_scale = 2.0f;
    }
    node_mask = pbvh_gather_generic(ob, brush, use_original, radius_scale, memory);
  }

  /* Draw Face Sets in draw mode makes a single undo push, in alt-smooth mode deforms the
   * vertices and uses regular coords undo. */
  /* It also assigns the paint_face_set here as it needs to be done regardless of the stroke type
   * and the number of nodes under the brush influence. */
  if (brush.sculpt_brush_type == SCULPT_BRUSH_TYPE_DRAW_FACE_SETS &&
      SCULPT_stroke_is_first_brush_step(*ss.cache) && !ss.cache->alt_smooth)
  {
    if (ss.cache->invert) {
      /* When inverting the brush, pick the paint face mask ID from the mesh. */
      ss.cache->paint_face_set = face_set::active_face_set_get(ob);
    }
    else {
      /* By default create a new Face Sets. */
      ss.cache->paint_face_set = face_set::find_next_available_id(ob);
    }
  }

  /* For anchored brushes with spherical falloff, we start off with zero radius, thus we have no
   * bke::pbvh::Tree nodes on the first brush step. */
  if (!node_mask.is_empty() ||
      ((brush.falloff_shape == PAINT_FALLOFF_SHAPE_SPHERE) && (brush.flag & BRUSH_ANCHORED)))
  {
    if (SCULPT_stroke_is_first_brush_step(*ss.cache)) {
      /* Initialize auto-masking cache. */
      if (auto_mask::is_enabled(sd, ob, &brush)) {
        ss.cache->automasking = auto_mask::cache_init(depsgraph, sd, &brush, ob);
      }
      /* Initialize surface smooth cache. */
      if ((brush.sculpt_brush_type == SCULPT_BRUSH_TYPE_SMOOTH) &&
          (brush.smooth_deform_type == BRUSH_SMOOTH_DEFORM_SURFACE))
      {
        BLI_assert(ss.cache->surface_smooth_laplacian_disp.is_empty());
        ss.cache->surface_smooth_laplacian_disp = Array<float3>(SCULPT_vertex_count_get(ob),
                                                                float3(0));
      }
    }
  }

  /* Only act if some verts are inside the brush area. */
  if (node_mask.is_empty()) {
    return;
  }
  float location[3];

  if (!use_pixels) {
    push_undo_nodes(depsgraph, ob, brush, node_mask);
  }

  if (sculpt_brush_needs_normal(ss, sd, brush)) {
    update_sculpt_normal(depsgraph, sd, ob, node_mask);
  }

  update_brush_local_mat(sd, ob);

  if (brush.sculpt_brush_type == SCULPT_BRUSH_TYPE_POSE &&
      SCULPT_stroke_is_first_brush_step(*ss.cache))
  {
    pose::pose_brush_init(depsgraph, ob, ss, brush);
  }

  if (brush.deform_target == BRUSH_DEFORM_TARGET_CLOTH_SIM) {
    if (!ss.cache->cloth_sim) {
      ss.cache->cloth_sim = cloth::brush_simulation_create(
          depsgraph, ob, 1.0f, 0.0f, 0.0f, false, true);
    }
    cloth::brush_store_simulation_state(depsgraph, ob, *ss.cache->cloth_sim);
    cloth::ensure_nodes_constraints(sd,
                                    ob,
                                    node_mask,
                                    *ss.cache->cloth_sim,
                                    ss.cache->location_symm,
                                    std::numeric_limits<float>::max());
  }

  bool invert = ss.cache->pen_flip || ss.cache->invert;
  if (brush.flag & BRUSH_DIR_IN) {
    invert = !invert;
  }

  /* Apply one type of brush action. */
  switch (brush.sculpt_brush_type) {
    case SCULPT_BRUSH_TYPE_DRAW: {
      const bool use_vector_displacement = (brush.flag2 & BRUSH_USE_COLOR_AS_DISPLACEMENT &&
                                            (brush.mtex.brush_map_mode == MTEX_MAP_MODE_AREA));
      if (use_vector_displacement) {
        do_draw_vector_displacement_brush(depsgraph, sd, ob, node_mask);
      }
      else {
        do_draw_brush(depsgraph, sd, ob, node_mask);
      }
      break;
    }
    case SCULPT_BRUSH_TYPE_SMOOTH:
      if (brush.smooth_deform_type == BRUSH_SMOOTH_DEFORM_LAPLACIAN) {
        /* NOTE: The enhance brush needs to initialize its state on the first brush step. The
         * stroke strength can become 0 during the stroke, but it can not change sign (the sign is
         * determined in the beginning of the stroke. So here it is important to not switch to
         * enhance brush in the middle of the stroke. */
        if (ss.cache->initial_direction_flipped) {
          /* Invert mode, intensify details. */
          do_enhance_details_brush(depsgraph, sd, ob, node_mask);
        }
        else {
          do_smooth_brush(
              depsgraph, sd, ob, node_mask, std::clamp(ss.cache->bstrength, 0.0f, 1.0f));
        }
      }
      else if (brush.smooth_deform_type == BRUSH_SMOOTH_DEFORM_SURFACE) {
        do_surface_smooth_brush(depsgraph, sd, ob, node_mask);
      }
      break;
    case SCULPT_BRUSH_TYPE_CREASE:
      do_crease_brush(depsgraph, scene, sd, ob, node_mask);
      break;
    case SCULPT_BRUSH_TYPE_BLOB:
      do_blob_brush(depsgraph, scene, sd, ob, node_mask);
      break;
    case SCULPT_BRUSH_TYPE_PINCH:
      do_pinch_brush(depsgraph, sd, ob, node_mask);
      break;
    case SCULPT_BRUSH_TYPE_INFLATE:
      do_inflate_brush(depsgraph, sd, ob, node_mask);
      break;
    case SCULPT_BRUSH_TYPE_GRAB:
      do_grab_brush(depsgraph, sd, ob, node_mask);
      break;
    case SCULPT_BRUSH_TYPE_ROTATE:
      do_rotate_brush(depsgraph, sd, ob, node_mask);
      break;
    case SCULPT_BRUSH_TYPE_SNAKE_HOOK:
      do_snake_hook_brush(depsgraph, sd, ob, node_mask);
      break;
    case SCULPT_BRUSH_TYPE_NUDGE:
      do_nudge_brush(depsgraph, sd, ob, node_mask);
      break;
    case SCULPT_BRUSH_TYPE_THUMB:
      do_thumb_brush(depsgraph, sd, ob, node_mask);
      break;
    case SCULPT_BRUSH_TYPE_LAYER:
      do_layer_brush(depsgraph, sd, ob, node_mask);
      break;
    case SCULPT_BRUSH_TYPE_FLATTEN:
      do_flatten_brush(depsgraph, sd, ob, node_mask);
      break;
    case SCULPT_BRUSH_TYPE_CLAY:
      do_clay_brush(depsgraph, sd, ob, node_mask);
      break;
    case SCULPT_BRUSH_TYPE_CLAY_STRIPS:
      do_clay_strips_brush(depsgraph, sd, ob, node_mask);
      break;
    case SCULPT_BRUSH_TYPE_MULTIPLANE_SCRAPE:
      do_multiplane_scrape_brush(depsgraph, sd, ob, node_mask);
      break;
    case SCULPT_BRUSH_TYPE_CLAY_THUMB:
      do_clay_thumb_brush(depsgraph, sd, ob, node_mask);
      break;
    case SCULPT_BRUSH_TYPE_FILL:
      if (invert && brush.flag & BRUSH_INVERT_TO_SCRAPE_FILL) {
        do_scrape_brush(depsgraph, sd, ob, node_mask);
      }
      else {
        do_fill_brush(depsgraph, sd, ob, node_mask);
      }
      break;
    case SCULPT_BRUSH_TYPE_SCRAPE:
      if (invert && brush.flag & BRUSH_INVERT_TO_SCRAPE_FILL) {
        do_fill_brush(depsgraph, sd, ob, node_mask);
      }
      else {
        do_scrape_brush(depsgraph, sd, ob, node_mask);
      }
      break;
    case SCULPT_BRUSH_TYPE_MASK:
      switch ((BrushMaskTool)brush.mask_tool) {
        case BRUSH_MASK_DRAW:
          do_mask_brush(depsgraph, sd, ob, node_mask);
          break;
        case BRUSH_MASK_SMOOTH:
          do_smooth_mask_brush(depsgraph, sd, ob, node_mask, ss.cache->bstrength);
          break;
      }
      break;
    case SCULPT_BRUSH_TYPE_POSE:
      pose::do_pose_brush(depsgraph, sd, ob, node_mask);
      break;
    case SCULPT_BRUSH_TYPE_DRAW_SHARP:
      do_draw_sharp_brush(depsgraph, sd, ob, node_mask);
      break;
    case SCULPT_BRUSH_TYPE_ELASTIC_DEFORM:
      do_elastic_deform_brush(depsgraph, sd, ob, node_mask);
      break;
    case SCULPT_BRUSH_TYPE_SLIDE_RELAX:
      if (ss.cache->alt_smooth) {
        do_topology_relax_brush(depsgraph, sd, ob, node_mask);
      }
      else {
        do_topology_slide_brush(depsgraph, sd, ob, node_mask);
      }
      break;
    case SCULPT_BRUSH_TYPE_BOUNDARY:
      boundary::do_boundary_brush(depsgraph, sd, ob, node_mask);
      break;
    case SCULPT_BRUSH_TYPE_CLOTH:
      cloth::do_cloth_brush(depsgraph, sd, ob, node_mask);
      break;
    case SCULPT_BRUSH_TYPE_DRAW_FACE_SETS:
      if (!ss.cache->alt_smooth) {
        do_draw_face_sets_brush(depsgraph, sd, ob, node_mask);
      }
      else {
        do_relax_face_sets_brush(depsgraph, sd, ob, node_mask);
      }
      break;
    case SCULPT_BRUSH_TYPE_DISPLACEMENT_ERASER:
      do_displacement_eraser_brush(depsgraph, sd, ob, node_mask);
      break;
    case SCULPT_BRUSH_TYPE_DISPLACEMENT_SMEAR:
      do_displacement_smear_brush(depsgraph, sd, ob, node_mask);
      break;
    case SCULPT_BRUSH_TYPE_PAINT:
      color::do_paint_brush(
          scene, depsgraph, paint_mode_settings, sd, ob, node_mask, texnode_mask);
      break;
    case SCULPT_BRUSH_TYPE_SMEAR:
      color::do_smear_brush(depsgraph, sd, ob, node_mask);
      break;
  }

  if (!ELEM(brush.sculpt_brush_type, SCULPT_BRUSH_TYPE_SMOOTH, SCULPT_BRUSH_TYPE_MASK) &&
      brush.autosmooth_factor > 0)
  {
    if (brush.flag & BRUSH_INVERSE_SMOOTH_PRESSURE) {
      do_smooth_brush(
          depsgraph, sd, ob, node_mask, brush.autosmooth_factor * (1.0f - ss.cache->pressure));
    }
    else {
      do_smooth_brush(depsgraph, sd, ob, node_mask, brush.autosmooth_factor);
    }
  }

  if (brush_uses_topology_rake(ss, brush)) {
    do_bmesh_topology_rake_brush(depsgraph, sd, ob, node_mask, brush.topology_rake_factor);
  }

  /* The cloth brush adds the gravity as a regular force and it is processed in the solver. */
  if (ss.cache->supports_gravity && !ELEM(brush.sculpt_brush_type,
                                          SCULPT_BRUSH_TYPE_CLOTH,
                                          SCULPT_BRUSH_TYPE_DRAW_FACE_SETS,
                                          SCULPT_BRUSH_TYPE_BOUNDARY))
  {
    do_gravity_brush(depsgraph, sd, ob, node_mask);
  }

  if (brush.deform_target == BRUSH_DEFORM_TARGET_CLOTH_SIM) {
    if (SCULPT_stroke_is_main_symmetry_pass(*ss.cache)) {
      cloth::sim_activate_nodes(ob, *ss.cache->cloth_sim, node_mask);
      cloth::do_simulation_step(depsgraph, sd, ob, *ss.cache->cloth_sim, node_mask);
    }
  }

  /* Update average stroke position. */
  copy_v3_v3(location, ss.cache->location);
  mul_m4_v3(ob.object_to_world().ptr(), location);

  add_v3_v3(ups.average_stroke_accum, location);
  ups.average_stroke_counter++;
  /* Update last stroke position. */
  ups.last_stroke_valid = true;
}

}  // namespace blender::ed::sculpt_paint

void SCULPT_cache_calc_brushdata_symm(blender::ed::sculpt_paint::StrokeCache &cache,
                                      const ePaintSymmetryFlags symm,
                                      const char axis,
                                      const float angle)
{
  using namespace blender;
  cache.location_symm = ed::sculpt_paint::symmetry_flip(cache.location, symm);
  cache.last_location_symm = ed::sculpt_paint::symmetry_flip(cache.last_location, symm);
  cache.grab_delta_symm = ed::sculpt_paint::symmetry_flip(cache.grab_delta, symm);
  cache.view_normal_symm = ed::sculpt_paint::symmetry_flip(cache.view_normal, symm);

  cache.initial_location_symm = ed::sculpt_paint::symmetry_flip(cache.initial_location, symm);
  cache.initial_normal_symm = ed::sculpt_paint::symmetry_flip(cache.initial_normal, symm);

  /* XXX This reduces the length of the grab delta if it approaches the line of symmetry
   * XXX However, a different approach appears to be needed. */
#if 0
  if (sd->paint.symmetry_flags & PAINT_SYMMETRY_FEATHER) {
    float frac = 1.0f / max_overlap_count(sd);
    float reduce = (feather - frac) / (1.0f - frac);

    printf("feather: %f frac: %f reduce: %f\n", feather, frac, reduce);

    if (frac < 1.0f) {
      mul_v3_fl(cache.grab_delta_symmetry, reduce);
    }
  }
#endif

  cache.symm_rot_mat = float4x4::identity();
  cache.symm_rot_mat_inv = float4x4::identity();
  zero_v3(cache.plane_offset);

  /* Expects XYZ. */
  if (axis) {
    rotate_m4(cache.symm_rot_mat.ptr(), axis, angle);
    rotate_m4(cache.symm_rot_mat_inv.ptr(), axis, -angle);
  }

  mul_m4_v3(cache.symm_rot_mat.ptr(), cache.location_symm);
  mul_m4_v3(cache.symm_rot_mat.ptr(), cache.grab_delta_symm);

  if (cache.supports_gravity) {
    cache.gravity_direction_symm = ed::sculpt_paint::symmetry_flip(cache.gravity_direction, symm);
    mul_m4_v3(cache.symm_rot_mat.ptr(), cache.gravity_direction_symm);
  }

  if (cache.rake_rotation) {
    float4 new_quat;
    float4 existing(cache.rake_rotation->w,
                    cache.rake_rotation->x,
                    cache.rake_rotation->y,
                    cache.rake_rotation->z);
    flip_qt_qt(new_quat, existing, symm);
    cache.rake_rotation_symm = math::Quaternion(new_quat);
  }
}

namespace blender::ed::sculpt_paint {

using BrushActionFunc = void (*)(const Depsgraph &depsgraph,
                                 const Scene &scene,
                                 const Sculpt &sd,
                                 Object &ob,
                                 const Brush &brush,
                                 UnifiedPaintSettings &ups,
                                 PaintModeSettings &paint_mode_settings);

static void do_tiled(const Depsgraph &depsgraph,
                     const Scene &scene,
                     const Sculpt &sd,
                     Object &ob,
                     const Brush &brush,
                     UnifiedPaintSettings &ups,
                     PaintModeSettings &paint_mode_settings,
                     const BrushActionFunc action)
{
  SculptSession &ss = *ob.sculpt;
  StrokeCache *cache = ss.cache;
  const float radius = cache->radius;
  const Bounds<float3> bb = *BKE_object_boundbox_get(&ob);
  const float *bbMin = bb.min;
  const float *bbMax = bb.max;
  const float *step = sd.paint.tile_offset;

  /* These are integer locations, for real location: multiply with step and add orgLoc.
   * So 0,0,0 is at orgLoc. */
  int start[3];
  int end[3];
  int cur[3];

  /* Position of the "prototype" stroke for tiling. */
  float orgLoc[3];
  float original_initial_location[3];
  copy_v3_v3(orgLoc, cache->location_symm);
  copy_v3_v3(original_initial_location, cache->initial_location_symm);

  for (int dim = 0; dim < 3; dim++) {
    if ((sd.paint.symmetry_flags & (PAINT_TILE_X << dim)) && step[dim] > 0) {
      start[dim] = (bbMin[dim] - orgLoc[dim] - radius) / step[dim];
      end[dim] = (bbMax[dim] - orgLoc[dim] + radius) / step[dim];
    }
    else {
      start[dim] = end[dim] = 0;
    }
  }

  /* First do the "un-tiled" position to initialize the stroke for this location. */
  cache->tile_pass = 0;
  action(depsgraph, scene, sd, ob, brush, ups, paint_mode_settings);

  /* Now do it for all the tiles. */
  copy_v3_v3_int(cur, start);
  for (cur[0] = start[0]; cur[0] <= end[0]; cur[0]++) {
    for (cur[1] = start[1]; cur[1] <= end[1]; cur[1]++) {
      for (cur[2] = start[2]; cur[2] <= end[2]; cur[2]++) {
        if (!cur[0] && !cur[1] && !cur[2]) {
          /* Skip tile at orgLoc, this was already handled before all others. */
          continue;
        }

        ++cache->tile_pass;

        for (int dim = 0; dim < 3; dim++) {
          cache->location_symm[dim] = cur[dim] * step[dim] + orgLoc[dim];
          cache->plane_offset[dim] = cur[dim] * step[dim];
          cache->initial_location_symm[dim] = cur[dim] * step[dim] +
                                              original_initial_location[dim];
        }
        action(depsgraph, scene, sd, ob, brush, ups, paint_mode_settings);
      }
    }
  }
}

static void do_radial_symmetry(const Depsgraph &depsgraph,
                               const Scene &scene,
                               const Sculpt &sd,
                               Object &ob,
                               const Brush &brush,
                               UnifiedPaintSettings &ups,
                               PaintModeSettings &paint_mode_settings,
                               const BrushActionFunc action,
                               const ePaintSymmetryFlags symm,
                               const int axis,
                               const float /*feather*/)
{
  SculptSession &ss = *ob.sculpt;

  for (int i = 1; i < sd.radial_symm[axis - 'X']; i++) {
    const float angle = 2.0f * M_PI * i / sd.radial_symm[axis - 'X'];
    ss.cache->radial_symmetry_pass = i;
    SCULPT_cache_calc_brushdata_symm(*ss.cache, symm, axis, angle);
    do_tiled(depsgraph, scene, sd, ob, brush, ups, paint_mode_settings, action);
  }
}

/**
 * Noise texture gives different values for the same input coord; this
 * can tear a multi-resolution mesh during sculpting so do a stitch in this case.
 */
static void sculpt_fix_noise_tear(const Sculpt &sd, Object &ob)
{
  SculptSession &ss = *ob.sculpt;
  const Brush &brush = *BKE_paint_brush_for_read(&sd.paint);
  const MTex *mtex = BKE_brush_mask_texture_get(&brush, OB_MODE_SCULPT);

  if (ss.multires.active && mtex->tex && mtex->tex->type == TEX_NOISE) {
    multires_stitch_grids(&ob);
  }
}

static void do_symmetrical_brush_actions(const Depsgraph &depsgraph,
                                         const Scene &scene,
                                         const Sculpt &sd,
                                         Object &ob,
                                         const BrushActionFunc action,
                                         UnifiedPaintSettings &ups,
                                         PaintModeSettings &paint_mode_settings)
{
  const Brush &brush = *BKE_paint_brush_for_read(&sd.paint);
  SculptSession &ss = *ob.sculpt;
  StrokeCache &cache = *ss.cache;
  const char symm = SCULPT_mesh_symmetry_xyz_get(ob);

  float feather = calc_symmetry_feather(sd, *ss.cache);

  cache.bstrength = brush_strength(sd, cache, feather, ups, paint_mode_settings);
  cache.symmetry = symm;

  /* `symm` is a bit combination of XYZ -
   * 1 is mirror X; 2 is Y; 3 is XY; 4 is Z; 5 is XZ; 6 is YZ; 7 is XYZ */
  for (int i = 0; i <= symm; i++) {
    if (!SCULPT_is_symmetry_iteration_valid(i, symm)) {
      continue;
    }
    const ePaintSymmetryFlags symm = ePaintSymmetryFlags(i);
    cache.mirror_symmetry_pass = symm;
    cache.radial_symmetry_pass = 0;

    SCULPT_cache_calc_brushdata_symm(cache, symm, 0, 0);
    do_tiled(depsgraph, scene, sd, ob, brush, ups, paint_mode_settings, action);

    do_radial_symmetry(
        depsgraph, scene, sd, ob, brush, ups, paint_mode_settings, action, symm, 'X', feather);
    do_radial_symmetry(
        depsgraph, scene, sd, ob, brush, ups, paint_mode_settings, action, symm, 'Y', feather);
    do_radial_symmetry(
        depsgraph, scene, sd, ob, brush, ups, paint_mode_settings, action, symm, 'Z', feather);
  }
}

}  // namespace blender::ed::sculpt_paint

bool SCULPT_mode_poll(bContext *C)
{
  Object *ob = CTX_data_active_object(C);
  return ob && ob->mode & OB_MODE_SCULPT;
}

bool SCULPT_mode_poll_view3d(bContext *C)
{
  using namespace blender::ed::sculpt_paint;
  return (SCULPT_mode_poll(C) && CTX_wm_region_view3d(C) && !ED_gpencil_session_active());
}

bool SCULPT_poll(bContext *C)
{
  using namespace blender::ed::sculpt_paint;
  return SCULPT_mode_poll(C) && blender::ed::sculpt_paint::paint_brush_tool_poll(C);
}

/**
 * While most non-brush tools in sculpt mode do not use the brush cursor, the trim tools
 * and the filter tools are expected to have the cursor visible so that some functionality is
 * easier to visually estimate.
 *
 * See: #122856
 */
static bool is_brush_related_tool(bContext *C)
{
  Paint *paint = BKE_paint_get_active_from_context(C);
  Object *ob = CTX_data_active_object(C);
  ScrArea *area = CTX_wm_area(C);
  ARegion *region = CTX_wm_region(C);

  if (paint && ob && BKE_paint_brush(paint) &&
      (area && ELEM(area->spacetype, SPACE_VIEW3D, SPACE_IMAGE)) &&
      (region && region->regiontype == RGN_TYPE_WINDOW))
  {
    bToolRef *tref = area->runtime.tool;
    if (tref && tref->runtime && tref->runtime->keymap[0]) {
      std::array<wmOperatorType *, 7> trim_operators = {
          WM_operatortype_find("SCULPT_OT_trim_box_gesture", false),
          WM_operatortype_find("SCULPT_OT_trim_lasso_gesture", false),
          WM_operatortype_find("SCULPT_OT_trim_line_gesture", false),
          WM_operatortype_find("SCULPT_OT_trim_polyline_gesture", false),
          WM_operatortype_find("SCULPT_OT_mesh_filter", false),
          WM_operatortype_find("SCULPT_OT_cloth_filter", false),
          WM_operatortype_find("SCULPT_OT_color_filter", false),
      };

      return std::any_of(trim_operators.begin(), trim_operators.end(), [tref](wmOperatorType *ot) {
        PointerRNA ptr;
        return WM_toolsystem_ref_properties_get_from_operator(tref, ot, &ptr);
      });
    }
  }
  return false;
}

bool SCULPT_brush_cursor_poll(bContext *C)
{
  using namespace blender::ed::sculpt_paint;
  return SCULPT_mode_poll(C) && (paint_brush_cursor_poll(C) || is_brush_related_tool(C));
}

static const char *sculpt_brush_type_name(const Sculpt &sd)
{
  const Brush &brush = *BKE_paint_brush_for_read(&sd.paint);

  switch (eBrushSculptType(brush.sculpt_brush_type)) {
    case SCULPT_BRUSH_TYPE_DRAW:
      return "Draw Brush";
    case SCULPT_BRUSH_TYPE_SMOOTH:
      return "Smooth Brush";
    case SCULPT_BRUSH_TYPE_CREASE:
      return "Crease Brush";
    case SCULPT_BRUSH_TYPE_BLOB:
      return "Blob Brush";
    case SCULPT_BRUSH_TYPE_PINCH:
      return "Pinch Brush";
    case SCULPT_BRUSH_TYPE_INFLATE:
      return "Inflate Brush";
    case SCULPT_BRUSH_TYPE_GRAB:
      return "Grab Brush";
    case SCULPT_BRUSH_TYPE_NUDGE:
      return "Nudge Brush";
    case SCULPT_BRUSH_TYPE_THUMB:
      return "Thumb Brush";
    case SCULPT_BRUSH_TYPE_LAYER:
      return "Layer Brush";
    case SCULPT_BRUSH_TYPE_FLATTEN:
      return "Flatten Brush";
    case SCULPT_BRUSH_TYPE_CLAY:
      return "Clay Brush";
    case SCULPT_BRUSH_TYPE_CLAY_STRIPS:
      return "Clay Strips Brush";
    case SCULPT_BRUSH_TYPE_CLAY_THUMB:
      return "Clay Thumb Brush";
    case SCULPT_BRUSH_TYPE_FILL:
      return "Fill Brush";
    case SCULPT_BRUSH_TYPE_SCRAPE:
      return "Scrape Brush";
    case SCULPT_BRUSH_TYPE_SNAKE_HOOK:
      return "Snake Hook Brush";
    case SCULPT_BRUSH_TYPE_ROTATE:
      return "Rotate Brush";
    case SCULPT_BRUSH_TYPE_MASK:
      return "Mask Brush";
    case SCULPT_BRUSH_TYPE_SIMPLIFY:
      return "Simplify Brush";
    case SCULPT_BRUSH_TYPE_DRAW_SHARP:
      return "Draw Sharp Brush";
    case SCULPT_BRUSH_TYPE_ELASTIC_DEFORM:
      return "Elastic Deform Brush";
    case SCULPT_BRUSH_TYPE_POSE:
      return "Pose Brush";
    case SCULPT_BRUSH_TYPE_MULTIPLANE_SCRAPE:
      return "Multi-plane Scrape Brush";
    case SCULPT_BRUSH_TYPE_SLIDE_RELAX:
      return "Slide/Relax Brush";
    case SCULPT_BRUSH_TYPE_BOUNDARY:
      return "Boundary Brush";
    case SCULPT_BRUSH_TYPE_CLOTH:
      return "Cloth Brush";
    case SCULPT_BRUSH_TYPE_DRAW_FACE_SETS:
      return "Draw Face Sets";
    case SCULPT_BRUSH_TYPE_DISPLACEMENT_ERASER:
      return "Multires Displacement Eraser";
    case SCULPT_BRUSH_TYPE_DISPLACEMENT_SMEAR:
      return "Multires Displacement Smear";
    case SCULPT_BRUSH_TYPE_PAINT:
      return "Paint Brush";
    case SCULPT_BRUSH_TYPE_SMEAR:
      return "Smear Brush";
  }

  return "Sculpting";
}

namespace blender::ed::sculpt_paint {

StrokeCache::~StrokeCache()
{
  MEM_SAFE_FREE(this->dial);
}

}  // namespace blender::ed::sculpt_paint

enum class StrokeFlags : uint8_t {
  ClipX = 1,
  ClipY = 2,
  ClipZ = 4,
};

namespace blender::ed::sculpt_paint {

/* Initialize mirror modifier clipping. */
static void sculpt_init_mirror_clipping(const Object &ob, const SculptSession &ss)
{
  ss.cache->mirror_modifier_clip.mat = float4x4::identity();

  LISTBASE_FOREACH (ModifierData *, md, &ob.modifiers) {
    if (!(md->type == eModifierType_Mirror && (md->mode & eModifierMode_Realtime))) {
      continue;
    }
    MirrorModifierData *mmd = (MirrorModifierData *)md;

    if (!(mmd->flag & MOD_MIR_CLIPPING)) {
      continue;
    }
    /* Check each axis for mirroring. */
    for (int i = 0; i < 3; i++) {
      if (!(mmd->flag & (MOD_MIR_AXIS_X << i))) {
        continue;
      }
      /* Enable sculpt clipping. */
      ss.cache->mirror_modifier_clip.flag |= uint8_t(StrokeFlags::ClipX) << i;

      /* Update the clip tolerance. */
      ss.cache->mirror_modifier_clip.tolerance[i] = std::max(
          mmd->tolerance, ss.cache->mirror_modifier_clip.tolerance[i]);

      /* Store matrix for mirror object clipping. */
      if (mmd->mirror_ob) {
        const float4x4 mirror_ob_inv = math::invert(mmd->mirror_ob->object_to_world());
        mul_m4_m4m4(ss.cache->mirror_modifier_clip.mat.ptr(),
                    mirror_ob_inv.ptr(),
                    ob.object_to_world().ptr());
      }
    }
  }
  ss.cache->mirror_modifier_clip.mat_inv = math::invert(ss.cache->mirror_modifier_clip.mat);
}

static void smooth_brush_toggle_on(const bContext *C, Paint *paint, StrokeCache *cache)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  Brush *cur_brush = BKE_paint_brush(paint);

  if (cur_brush->sculpt_brush_type == SCULPT_BRUSH_TYPE_MASK) {
    cache->saved_mask_brush_tool = cur_brush->mask_tool;
    cur_brush->mask_tool = BRUSH_MASK_SMOOTH;
    return;
  }

  if (ELEM(cur_brush->sculpt_brush_type,
           SCULPT_BRUSH_TYPE_SLIDE_RELAX,
           SCULPT_BRUSH_TYPE_DRAW_FACE_SETS,
           SCULPT_BRUSH_TYPE_PAINT,
           SCULPT_BRUSH_TYPE_SMEAR))
  {
    /* Do nothing, this brush has its own smooth mode. */
    return;
  }

  /* Switch to the smooth brush if possible. */
  BKE_paint_brush_set_essentials(bmain, paint, "Smooth");
  Brush *smooth_brush = BKE_paint_brush(paint);

  if (!smooth_brush) {
    BKE_paint_brush_set(paint, cur_brush);
    CLOG_WARN(&LOG, "Switching to the smooth brush not possible, corresponding brush not");
    cache->saved_active_brush = nullptr;
    return;
  }

  int cur_brush_size = BKE_brush_size_get(scene, cur_brush);

  cache->saved_active_brush = cur_brush;

  cache->saved_smooth_size = BKE_brush_size_get(scene, smooth_brush);
  BKE_brush_size_set(scene, smooth_brush, cur_brush_size);
  BKE_curvemapping_init(smooth_brush->curve);
}

static void smooth_brush_toggle_off(const bContext *C, Paint *paint, StrokeCache *cache)
{
  Brush &brush = *BKE_paint_brush(paint);

  if (brush.sculpt_brush_type == SCULPT_BRUSH_TYPE_MASK) {
    brush.mask_tool = cache->saved_mask_brush_tool;
    return;
  }

  if (ELEM(brush.sculpt_brush_type,
           SCULPT_BRUSH_TYPE_SLIDE_RELAX,
           SCULPT_BRUSH_TYPE_DRAW_FACE_SETS,
           SCULPT_BRUSH_TYPE_PAINT,
           SCULPT_BRUSH_TYPE_SMEAR))
  {
    /* Do nothing. */
    return;
  }

  /* If saved_active_brush is not set, brush was not switched/affected in
   * smooth_brush_toggle_on(). */
  if (cache->saved_active_brush) {
    Scene *scene = CTX_data_scene(C);
    BKE_brush_size_set(scene, &brush, cache->saved_smooth_size);
    BKE_paint_brush_set(paint, cache->saved_active_brush);
    cache->saved_active_brush = nullptr;
  }
}

/* Initialize the stroke cache invariants from operator properties. */
static void sculpt_update_cache_invariants(
    bContext *C, Sculpt &sd, SculptSession &ss, wmOperator *op, const float mval[2])
{
  StrokeCache *cache = MEM_new<StrokeCache>(__func__);
  ToolSettings *tool_settings = CTX_data_tool_settings(C);
  UnifiedPaintSettings *ups = &tool_settings->unified_paint_settings;
  const Brush *brush = BKE_paint_brush_for_read(&sd.paint);
  ViewContext *vc = paint_stroke_view_context(static_cast<PaintStroke *>(op->customdata));
  Object &ob = *CTX_data_active_object(C);
  float mat[3][3];
  float viewDir[3] = {0.0f, 0.0f, 1.0f};
  float max_scale;
  int mode;

  ss.cache = cache;

  /* Set scaling adjustment. */
  max_scale = 0.0f;
  for (int i = 0; i < 3; i++) {
    max_scale = max_ff(max_scale, fabsf(ob.scale[i]));
  }
  cache->scale[0] = max_scale / ob.scale[0];
  cache->scale[1] = max_scale / ob.scale[1];
  cache->scale[2] = max_scale / ob.scale[2];

  cache->plane_trim_squared = brush->plane_trim * brush->plane_trim;

  cache->mirror_modifier_clip.flag = 0;

  sculpt_init_mirror_clipping(ob, ss);

  /* Initial mouse location. */
  if (mval) {
    copy_v2_v2(cache->initial_mouse, mval);
  }
  else {
    zero_v2(cache->initial_mouse);
  }

  copy_v3_v3(cache->initial_location_symm, ss.cursor_location);
  copy_v3_v3(cache->initial_location, ss.cursor_location);

  copy_v3_v3(cache->initial_normal_symm, ss.cursor_normal);
  copy_v3_v3(cache->initial_normal, ss.cursor_normal);

  mode = RNA_enum_get(op->ptr, "mode");
  cache->pen_flip = RNA_boolean_get(op->ptr, "pen_flip");
  cache->invert = mode == BRUSH_STROKE_INVERT;
  cache->alt_smooth = mode == BRUSH_STROKE_SMOOTH;
  cache->normal_weight = brush->normal_weight;

  /* Interpret invert as following normal, for grab brushes. */
  if (SCULPT_BRUSH_TYPE_HAS_NORMAL_WEIGHT(brush->sculpt_brush_type)) {
    if (cache->invert) {
      cache->invert = false;
      cache->normal_weight = (cache->normal_weight == 0.0f);
    }
  }

  /* Not very nice, but with current events system implementation
   * we can't handle brush appearance inversion hotkey separately (sergey). */
  if (cache->invert) {
    ups->draw_inverted = true;
  }
  else {
    ups->draw_inverted = false;
  }

  /* Alt-Smooth. */
  if (cache->alt_smooth) {
    smooth_brush_toggle_on(C, &sd.paint, cache);
    /* Refresh the brush pointer in case we switched brush in the toggle function. */
    brush = BKE_paint_brush(&sd.paint);
  }

  copy_v2_v2(cache->mouse, cache->initial_mouse);
  copy_v2_v2(cache->mouse_event, cache->initial_mouse);
  copy_v2_v2(ups->tex_mouse, cache->initial_mouse);

  cache->initial_direction_flipped = brush_flip(*brush, *cache) < 0.0f;

  /* Truly temporary data that isn't stored in properties. */
  cache->vc = vc;
  cache->brush = brush;

  /* Cache projection matrix. */
  cache->projection_mat = ED_view3d_ob_project_mat_get(cache->vc->rv3d, &ob);

  invert_m4_m4(ob.runtime->world_to_object.ptr(), ob.object_to_world().ptr());
  copy_m3_m4(mat, cache->vc->rv3d->viewinv);
  mul_m3_v3(mat, viewDir);
  copy_m3_m4(mat, ob.world_to_object().ptr());
  mul_m3_v3(mat, viewDir);
  normalize_v3_v3(cache->view_normal, viewDir);

  cache->supports_gravity = (!ELEM(brush->sculpt_brush_type,
                                   SCULPT_BRUSH_TYPE_MASK,
                                   SCULPT_BRUSH_TYPE_SMOOTH,
                                   SCULPT_BRUSH_TYPE_SIMPLIFY,
                                   SCULPT_BRUSH_TYPE_DISPLACEMENT_SMEAR,
                                   SCULPT_BRUSH_TYPE_DISPLACEMENT_ERASER) &&
                             (sd.gravity_factor > 0.0f));
  /* Get gravity vector in world space. */
  if (cache->supports_gravity) {
    if (sd.gravity_object) {
      Object *gravity_object = sd.gravity_object;

      copy_v3_v3(cache->gravity_direction, gravity_object->object_to_world().ptr()[2]);
    }
    else {
      cache->gravity_direction[0] = cache->gravity_direction[1] = 0.0f;
      cache->gravity_direction[2] = 1.0f;
    }

    /* Transform to sculpted object space. */
    mul_m3_v3(mat, cache->gravity_direction);
    normalize_v3(cache->gravity_direction);
  }

  cache->accum = true;

  /* Make copies of the mesh vertex locations and normals for some brushes. */
  if (brush->flag & BRUSH_ANCHORED) {
    cache->accum = false;
  }

  /* Draw sharp does not need the original coordinates to produce the accumulate effect, so it
   * should work the opposite way. */
  if (brush->sculpt_brush_type == SCULPT_BRUSH_TYPE_DRAW_SHARP) {
    cache->accum = false;
  }

  if (SCULPT_BRUSH_TYPE_HAS_ACCUMULATE(brush->sculpt_brush_type)) {
    if (!(brush->flag & BRUSH_ACCUMULATE)) {
      cache->accum = false;
      if (brush->sculpt_brush_type == SCULPT_BRUSH_TYPE_DRAW_SHARP) {
        cache->accum = true;
      }
    }
  }

  /* Original coordinates require the sculpt undo system, which isn't used
   * for image brushes. It's also not necessary, just disable it. */
  if (brush && brush->sculpt_brush_type == SCULPT_BRUSH_TYPE_PAINT &&
      SCULPT_use_image_paint_brush(tool_settings->paint_mode, ob))
  {
    cache->accum = true;
  }

  cache->first_time = true;

#define PIXEL_INPUT_THRESHHOLD 5
  if (brush->sculpt_brush_type == SCULPT_BRUSH_TYPE_ROTATE) {
    cache->dial = BLI_dial_init(cache->initial_mouse, PIXEL_INPUT_THRESHHOLD);
  }

#undef PIXEL_INPUT_THRESHHOLD
}

static float brush_dynamic_size_get(const Brush &brush,
                                    const StrokeCache &cache,
                                    float initial_size)
{
  switch (brush.sculpt_brush_type) {
    case SCULPT_BRUSH_TYPE_CLAY:
      return max_ff(initial_size * 0.20f, initial_size * pow3f(cache.pressure));
    case SCULPT_BRUSH_TYPE_CLAY_STRIPS:
      return max_ff(initial_size * 0.30f, initial_size * powf(cache.pressure, 1.5f));
    case SCULPT_BRUSH_TYPE_CLAY_THUMB: {
      float clay_stabilized_pressure = clay_thumb_get_stabilized_pressure(cache);
      return initial_size * clay_stabilized_pressure;
    }
    default:
      return initial_size * cache.pressure;
  }
}

/* In these brushes the grab delta is calculated always from the initial stroke location, which is
 * generally used to create grab deformations. */
static bool need_delta_from_anchored_origin(const Brush &brush)
{
  if (brush.sculpt_brush_type == SCULPT_BRUSH_TYPE_SMEAR && (brush.flag & BRUSH_ANCHORED)) {
    return true;
  }

  if (ELEM(brush.sculpt_brush_type,
           SCULPT_BRUSH_TYPE_GRAB,
           SCULPT_BRUSH_TYPE_POSE,
           SCULPT_BRUSH_TYPE_BOUNDARY,
           SCULPT_BRUSH_TYPE_THUMB,
           SCULPT_BRUSH_TYPE_ELASTIC_DEFORM))
  {
    return true;
  }
  if (brush.sculpt_brush_type == SCULPT_BRUSH_TYPE_CLOTH &&
      brush.cloth_deform_type == BRUSH_CLOTH_DEFORM_GRAB)
  {
    return true;
  }
  return false;
}

/* In these brushes the grab delta is calculated from the previous stroke location, which is used
 * to calculate to orientate the brush tip and deformation towards the stroke direction. */
static bool need_delta_for_tip_orientation(const Brush &brush)
{
  if (brush.sculpt_brush_type == SCULPT_BRUSH_TYPE_CLOTH) {
    return brush.cloth_deform_type != BRUSH_CLOTH_DEFORM_GRAB;
  }
  return ELEM(brush.sculpt_brush_type,
              SCULPT_BRUSH_TYPE_CLAY_STRIPS,
              SCULPT_BRUSH_TYPE_PINCH,
              SCULPT_BRUSH_TYPE_MULTIPLANE_SCRAPE,
              SCULPT_BRUSH_TYPE_CLAY_THUMB,
              SCULPT_BRUSH_TYPE_NUDGE,
              SCULPT_BRUSH_TYPE_SNAKE_HOOK);
}

static void brush_delta_update(const Depsgraph &depsgraph,
                               UnifiedPaintSettings &ups,
                               const Object &ob,
                               const Brush &brush)
{
  SculptSession &ss = *ob.sculpt;
  const bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(ob);
  StrokeCache *cache = ss.cache;
  const float mval[2] = {
      cache->mouse_event[0],
      cache->mouse_event[1],
  };
  int brush_type = brush.sculpt_brush_type;

  if (!ELEM(brush_type,
            SCULPT_BRUSH_TYPE_PAINT,
            SCULPT_BRUSH_TYPE_GRAB,
            SCULPT_BRUSH_TYPE_ELASTIC_DEFORM,
            SCULPT_BRUSH_TYPE_CLOTH,
            SCULPT_BRUSH_TYPE_NUDGE,
            SCULPT_BRUSH_TYPE_CLAY_STRIPS,
            SCULPT_BRUSH_TYPE_PINCH,
            SCULPT_BRUSH_TYPE_MULTIPLANE_SCRAPE,
            SCULPT_BRUSH_TYPE_CLAY_THUMB,
            SCULPT_BRUSH_TYPE_SNAKE_HOOK,
            SCULPT_BRUSH_TYPE_POSE,
            SCULPT_BRUSH_TYPE_BOUNDARY,
            SCULPT_BRUSH_TYPE_SMEAR,
            SCULPT_BRUSH_TYPE_THUMB) &&
      !brush_uses_topology_rake(ss, brush))
  {
    return;
  }
  float grab_location[3], imat[4][4], delta[3], loc[3];

  if (SCULPT_stroke_is_first_brush_step_of_symmetry_pass(*ss.cache)) {
    if (brush_type == SCULPT_BRUSH_TYPE_GRAB && brush.flag & BRUSH_GRAB_ACTIVE_VERTEX) {
      if (pbvh.type() == bke::pbvh::Type::Mesh) {
        const Span<float3> positions = vert_positions_for_grab_active_get(depsgraph, ob);
        cache->orig_grab_location = positions[std::get<int>(ss.active_vert())];
      }
      else {
        cache->orig_grab_location = ss.active_vert_position(depsgraph, ob);
      }
    }
    else {
      copy_v3_v3(cache->orig_grab_location, cache->location);
    }
  }
  else if (brush_type == SCULPT_BRUSH_TYPE_SNAKE_HOOK ||
           (brush_type == SCULPT_BRUSH_TYPE_CLOTH &&
            brush.cloth_deform_type == BRUSH_CLOTH_DEFORM_SNAKE_HOOK))
  {
    add_v3_v3(cache->location, cache->grab_delta);
  }

  /* Compute 3d coordinate at same z from original location + mval. */
  mul_v3_m4v3(loc, ob.object_to_world().ptr(), cache->orig_grab_location);
  ED_view3d_win_to_3d(cache->vc->v3d, cache->vc->region, loc, mval, grab_location);

  /* Compute delta to move verts by. */
  if (!SCULPT_stroke_is_first_brush_step_of_symmetry_pass(*ss.cache)) {
    if (need_delta_from_anchored_origin(brush)) {
      sub_v3_v3v3(delta, grab_location, cache->old_grab_location);
      invert_m4_m4(imat, ob.object_to_world().ptr());
      mul_mat3_m4_v3(imat, delta);
      add_v3_v3(cache->grab_delta, delta);
    }
    else if (need_delta_for_tip_orientation(brush)) {
      if (brush.flag & BRUSH_ANCHORED) {
        float orig[3];
        mul_v3_m4v3(orig, ob.object_to_world().ptr(), cache->orig_grab_location);
        sub_v3_v3v3(cache->grab_delta, grab_location, orig);
      }
      else {
        sub_v3_v3v3(cache->grab_delta, grab_location, cache->old_grab_location);
      }
      invert_m4_m4(imat, ob.object_to_world().ptr());
      mul_mat3_m4_v3(imat, cache->grab_delta);
    }
    else {
      /* Use for 'Brush.topology_rake_factor'. */
      sub_v3_v3v3(cache->grab_delta, grab_location, cache->old_grab_location);
    }
  }
  else {
    zero_v3(cache->grab_delta);
  }

  if (brush.falloff_shape == PAINT_FALLOFF_SHAPE_TUBE) {
    project_plane_v3_v3v3(cache->grab_delta, cache->grab_delta, ss.cache->view_normal);
  }

  copy_v3_v3(cache->old_grab_location, grab_location);

  if (need_delta_from_anchored_origin(brush)) {
    /* Location stays the same for finding vertices in brush radius. */
    copy_v3_v3(cache->location, cache->orig_grab_location);

    ups.draw_anchored = true;
    copy_v2_v2(ups.anchored_initial_mouse, cache->initial_mouse);
    ups.anchored_size = ups.pixel_radius;
  }

  /* Handle 'rake' */
  cache->rake_rotation = std::nullopt;
  cache->rake_rotation_symm = std::nullopt;
  invert_m4_m4(imat, ob.object_to_world().ptr());
  mul_mat3_m4_v3(imat, grab_location);

  if (SCULPT_stroke_is_first_brush_step_of_symmetry_pass(*ss.cache)) {
    copy_v3_v3(cache->rake_data.follow_co, grab_location);
  }

  if (!brush_needs_rake_rotation(brush)) {
    return;
  }
  cache->rake_data.follow_dist = cache->radius * SCULPT_RAKE_BRUSH_FACTOR;

  if (!is_zero_v3(cache->grab_delta)) {
    const float eps = 0.00001f;

    float v1[3], v2[3];

    copy_v3_v3(v1, cache->rake_data.follow_co);
    copy_v3_v3(v2, cache->rake_data.follow_co);
    sub_v3_v3(v2, cache->grab_delta);

    sub_v3_v3(v1, grab_location);
    sub_v3_v3(v2, grab_location);

    if ((normalize_v3(v2) > eps) && (normalize_v3(v1) > eps) && (len_squared_v3v3(v1, v2) > eps)) {
      const float rake_dist_sq = len_squared_v3v3(cache->rake_data.follow_co, grab_location);
      const float rake_fade = (rake_dist_sq > square_f(cache->rake_data.follow_dist)) ?
                                  1.0f :
                                  sqrtf(rake_dist_sq) / cache->rake_data.follow_dist;

      const math::AxisAngle between_vecs(v1, v2);
      const math::AxisAngle rotated(between_vecs.axis(),
                                    between_vecs.angle() * brush.rake_factor * rake_fade);
      cache->rake_rotation = math::to_quaternion(rotated);
    }
  }
  rake_data_update(&cache->rake_data, grab_location);
}

static void cache_paint_invariants_update(StrokeCache &cache, const Brush &brush)
{
  cache.hardness = brush.hardness;
  if (brush.paint_flags & BRUSH_PAINT_HARDNESS_PRESSURE) {
    cache.hardness *= brush.paint_flags & BRUSH_PAINT_HARDNESS_PRESSURE_INVERT ?
                          1.0f - cache.pressure :
                          cache.pressure;
  }

  cache.paint_brush.flow = brush.flow;
  if (brush.paint_flags & BRUSH_PAINT_FLOW_PRESSURE) {
    cache.paint_brush.flow *= brush.paint_flags & BRUSH_PAINT_FLOW_PRESSURE_INVERT ?
                                  1.0f - cache.pressure :
                                  cache.pressure;
  }

  cache.paint_brush.wet_mix = brush.wet_mix;
  if (brush.paint_flags & BRUSH_PAINT_WET_MIX_PRESSURE) {
    cache.paint_brush.wet_mix *= brush.paint_flags & BRUSH_PAINT_WET_MIX_PRESSURE_INVERT ?
                                     1.0f - cache.pressure :
                                     cache.pressure;

    /* This makes wet mix more sensible in higher values, which allows to create brushes that have
     * a wider pressure range were they only blend colors without applying too much of the brush
     * color. */
    cache.paint_brush.wet_mix = 1.0f - pow2f(1.0f - cache.paint_brush.wet_mix);
  }

  cache.paint_brush.wet_persistence = brush.wet_persistence;
  if (brush.paint_flags & BRUSH_PAINT_WET_PERSISTENCE_PRESSURE) {
    cache.paint_brush.wet_persistence = brush.paint_flags &
                                                BRUSH_PAINT_WET_PERSISTENCE_PRESSURE_INVERT ?
                                            1.0f - cache.pressure :
                                            cache.pressure;
  }

  cache.paint_brush.density = brush.density;
  if (brush.paint_flags & BRUSH_PAINT_DENSITY_PRESSURE) {
    cache.paint_brush.density = brush.paint_flags & BRUSH_PAINT_DENSITY_PRESSURE_INVERT ?
                                    1.0f - cache.pressure :
                                    cache.pressure;
  }
}

/* Initialize the stroke cache variants from operator properties. */
static void sculpt_update_cache_variants(bContext *C, Sculpt &sd, Object &ob, PointerRNA *ptr)
{
  Scene &scene = *CTX_data_scene(C);
  const Depsgraph &depsgraph = *CTX_data_depsgraph_pointer(C);
  UnifiedPaintSettings &ups = scene.toolsettings->unified_paint_settings;
  SculptSession &ss = *ob.sculpt;
  StrokeCache &cache = *ss.cache;
  Brush &brush = *BKE_paint_brush(&sd.paint);

  if (SCULPT_stroke_is_first_brush_step_of_symmetry_pass(cache) ||
      !((brush.flag & BRUSH_ANCHORED) ||
        (brush.sculpt_brush_type == SCULPT_BRUSH_TYPE_SNAKE_HOOK) ||
        (brush.sculpt_brush_type == SCULPT_BRUSH_TYPE_ROTATE) ||
        cloth::is_cloth_deform_brush(brush)))
  {
    RNA_float_get_array(ptr, "location", cache.location);
  }

  RNA_float_get_array(ptr, "mouse", cache.mouse);
  RNA_float_get_array(ptr, "mouse_event", cache.mouse_event);

  /* XXX: Use pressure value from first brush step for brushes which don't support strokes (grab,
   * thumb). They depends on initial state and brush coord/pressure/etc.
   * It's more an events design issue, which doesn't split coordinate/pressure/angle changing
   * events. We should avoid this after events system re-design. */
  if (paint_supports_dynamic_size(brush, PaintMode::Sculpt) || cache.first_time) {
    cache.pressure = RNA_float_get(ptr, "pressure");
  }

  cache.tilt = {RNA_float_get(ptr, "x_tilt"), RNA_float_get(ptr, "y_tilt")};

  /* Truly temporary data that isn't stored in properties. */
  if (SCULPT_stroke_is_first_brush_step_of_symmetry_pass(*ss.cache)) {
    cache.initial_radius = sculpt_calc_radius(*cache.vc, brush, scene, cache.location);

    if (!BKE_brush_use_locked_size(&scene, &brush)) {
      BKE_brush_unprojected_radius_set(&scene, &brush, cache.initial_radius);
    }
  }

  /* Clay stabilized pressure. */
  if (brush.sculpt_brush_type == SCULPT_BRUSH_TYPE_CLAY_THUMB) {
    if (SCULPT_stroke_is_first_brush_step_of_symmetry_pass(*ss.cache)) {
      ss.cache->clay_thumb_brush.pressure_stabilizer.fill(0.0f);
      ss.cache->clay_thumb_brush.stabilizer_index = 0;
    }
    else {
      cache.clay_thumb_brush.pressure_stabilizer[cache.clay_thumb_brush.stabilizer_index] =
          cache.pressure;
      cache.clay_thumb_brush.stabilizer_index += 1;
      if (cache.clay_thumb_brush.stabilizer_index >=
          ss.cache->clay_thumb_brush.pressure_stabilizer.size())
      {
        cache.clay_thumb_brush.stabilizer_index = 0;
      }
    }
  }

  if (BKE_brush_use_size_pressure(&brush) && paint_supports_dynamic_size(brush, PaintMode::Sculpt))
  {
    cache.radius = brush_dynamic_size_get(brush, cache, cache.initial_radius);
    cache.dyntopo_pixel_radius = brush_dynamic_size_get(brush, cache, ups.initial_pixel_radius);
  }
  else {
    cache.radius = cache.initial_radius;
    cache.dyntopo_pixel_radius = ups.initial_pixel_radius;
  }

  cache_paint_invariants_update(cache, brush);

  cache.radius_squared = cache.radius * cache.radius;

  if (brush.flag & BRUSH_ANCHORED) {
    /* True location has been calculated as part of the stroke system already here. */
    if (brush.flag & BRUSH_EDGE_TO_EDGE) {
      RNA_float_get_array(ptr, "location", cache.location);
    }

    cache.radius = paint_calc_object_space_radius(*cache.vc, cache.location, ups.pixel_radius);
    cache.radius_squared = cache.radius * cache.radius;
  }

  brush_delta_update(depsgraph, ups, ob, brush);

  if (brush.sculpt_brush_type == SCULPT_BRUSH_TYPE_ROTATE) {
    cache.vertex_rotation = -BLI_dial_angle(cache.dial, cache.mouse) * cache.bstrength;

    ups.draw_anchored = true;
    copy_v2_v2(ups.anchored_initial_mouse, cache.initial_mouse);
    ups.anchored_size = ups.pixel_radius;
  }

  cache.special_rotation = ups.brush_rotation;

  cache.iteration_count++;
}

/* Returns true if any of the smoothing modes are active (currently
 * one of smooth brush, autosmooth, mask smooth, or shift-key
 * smooth). */
static bool sculpt_needs_connectivity_info(const Sculpt &sd,
                                           const Brush &brush,
                                           const Object &object,
                                           int stroke_mode)
{
  SculptSession &ss = *object.sculpt;
  const bke::pbvh::Tree *pbvh = bke::object::pbvh_get(object);
  if (pbvh && auto_mask::is_enabled(sd, object, &brush)) {
    return true;
  }
  return ((stroke_mode == BRUSH_STROKE_SMOOTH) || (ss.cache && ss.cache->alt_smooth) ||
          (brush.sculpt_brush_type == SCULPT_BRUSH_TYPE_SMOOTH) || (brush.autosmooth_factor > 0) ||
          ((brush.sculpt_brush_type == SCULPT_BRUSH_TYPE_MASK) &&
           (brush.mask_tool == BRUSH_MASK_SMOOTH)) ||
          (brush.sculpt_brush_type == SCULPT_BRUSH_TYPE_POSE) ||
          (brush.sculpt_brush_type == SCULPT_BRUSH_TYPE_BOUNDARY) ||
          (brush.sculpt_brush_type == SCULPT_BRUSH_TYPE_SLIDE_RELAX) ||
          SCULPT_brush_type_is_paint(brush.sculpt_brush_type) ||
          (brush.sculpt_brush_type == SCULPT_BRUSH_TYPE_CLOTH) ||
          (brush.sculpt_brush_type == SCULPT_BRUSH_TYPE_SMEAR) ||
          (brush.sculpt_brush_type == SCULPT_BRUSH_TYPE_DRAW_FACE_SETS) ||
          (brush.sculpt_brush_type == SCULPT_BRUSH_TYPE_DISPLACEMENT_SMEAR) ||
          (brush.sculpt_brush_type == SCULPT_BRUSH_TYPE_PAINT));
}

}  // namespace blender::ed::sculpt_paint

void SCULPT_stroke_modifiers_check(const bContext *C, Object &ob, const Brush &brush)
{
  using namespace blender::ed::sculpt_paint;
  SculptSession &ss = *ob.sculpt;
  RegionView3D *rv3d = CTX_wm_region_view3d(C);
  const Sculpt &sd = *CTX_data_tool_settings(C)->sculpt;

  bool need_pmap = sculpt_needs_connectivity_info(sd, brush, ob, 0);
  if (ss.shapekey_active || ss.deform_modifiers_active ||
      (!BKE_sculptsession_use_pbvh_draw(&ob, rv3d) && need_pmap))
  {
    Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
    BKE_sculpt_update_object_for_edit(
        depsgraph, &ob, SCULPT_brush_type_is_paint(brush.sculpt_brush_type));
  }
}

static void sculpt_raycast_cb(blender::bke::pbvh::Node &node, SculptRaycastData &srd, float *tmin)
{
  using namespace blender;
  using namespace blender::ed::sculpt_paint;
  if (BKE_pbvh_node_get_tmin(&node) >= *tmin) {
    return;
  }

  bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(*srd.object);
  bool use_origco = false;
  Span<float3> origco;
  if (srd.original && srd.ss->cache) {
    switch (pbvh.type()) {
      case bke::pbvh::Type::Mesh:
        if (const std::optional<OrigPositionData> orig_data =
                orig_position_data_lookup_mesh_all_verts(
                    *srd.object, static_cast<const bke::pbvh::MeshNode &>(node)))
        {
          use_origco = true;
          origco = orig_data->positions;
        }
        break;
      case bke::pbvh::Type::Grids:
        if (const std::optional<OrigPositionData> orig_data = orig_position_data_lookup_grids(
                *srd.object, static_cast<const bke::pbvh::GridsNode &>(node)))
        {
          use_origco = true;
          origco = orig_data->positions;
        }
        break;
      case bke::pbvh::Type::BMesh:
        use_origco = true;
        break;
    }
  }

  if (node.flag_ & bke::pbvh::Node::FullyHidden) {
    return;
  }

  bool hit = false;
  switch (pbvh.type()) {
    case bke::pbvh::Type::Mesh: {
      int mesh_active_vert;
      hit = bke::pbvh::node_raycast_mesh(static_cast<bke::pbvh::MeshNode &>(node),
                                         origco,
                                         srd.vert_positions,
                                         srd.faces,
                                         srd.corner_verts,
                                         srd.corner_tris,
                                         srd.hide_poly,
                                         srd.ray_start,
                                         srd.ray_normal,
                                         &srd.isect_precalc,
                                         &srd.depth,
                                         mesh_active_vert,
                                         srd.active_face_grid_index,
                                         srd.face_normal);
      if (hit) {
        srd.active_vertex = mesh_active_vert;
      }
      break;
    }
    case bke::pbvh::Type::Grids: {
      SubdivCCGCoord grids_active_vert;
      hit = bke::pbvh::node_raycast_grids(*srd.subdiv_ccg,
                                          static_cast<bke::pbvh::GridsNode &>(node),
                                          origco,
                                          srd.ray_start,
                                          srd.ray_normal,
                                          &srd.isect_precalc,
                                          &srd.depth,
                                          grids_active_vert,
                                          srd.active_face_grid_index,
                                          srd.face_normal);
      if (hit) {
        srd.active_vertex = grids_active_vert.to_index(
            BKE_subdiv_ccg_key_top_level(*srd.subdiv_ccg));
      }
      break;
    }
    case bke::pbvh::Type::BMesh: {
      BMVert *bmesh_active_vert;
      hit = bke::pbvh::node_raycast_bmesh(static_cast<bke::pbvh::BMeshNode &>(node),
                                          srd.ray_start,
                                          srd.ray_normal,
                                          &srd.isect_precalc,
                                          &srd.depth,
                                          use_origco,
                                          &bmesh_active_vert,
                                          srd.face_normal);
      if (hit) {
        srd.active_vertex = bmesh_active_vert;
      }
      break;
    }
  }

  if (hit) {
    srd.hit = true;
    *tmin = srd.depth;
  }
}

static void sculpt_find_nearest_to_ray_cb(blender::bke::pbvh::Node &node,
                                          SculptFindNearestToRayData &srd,
                                          float *tmin)
{
  using namespace blender;
  using namespace blender::ed::sculpt_paint;
  if (BKE_pbvh_node_get_tmin(&node) >= *tmin) {
    return;
  }
  bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(*srd.object);
  bool use_origco = false;
  Span<float3> origco;
  if (srd.original && srd.ss->cache) {
    switch (pbvh.type()) {
      case bke::pbvh::Type::Mesh:
        if (const std::optional<OrigPositionData> orig_data =
                orig_position_data_lookup_mesh_all_verts(
                    *srd.object, static_cast<const bke::pbvh::MeshNode &>(node)))
        {
          use_origco = true;
          origco = orig_data->positions;
        }
        break;
      case bke::pbvh::Type::Grids:
        if (const std::optional<OrigPositionData> orig_data = orig_position_data_lookup_grids(
                *srd.object, static_cast<const bke::pbvh::GridsNode &>(node)))
        {
          use_origco = true;
          origco = orig_data->positions;
        }

        break;
      case bke::pbvh::Type::BMesh:
        use_origco = true;
        break;
    }
  }

  if (bke::pbvh::find_nearest_to_ray_node(pbvh,
                                          node,
                                          origco,
                                          use_origco,
                                          srd.vert_positions,
                                          srd.faces,
                                          srd.corner_verts,
                                          srd.corner_tris,
                                          srd.hide_poly,
                                          srd.subdiv_ccg,
                                          srd.ray_start,
                                          srd.ray_normal,
                                          &srd.depth,
                                          &srd.dist_sq_to_ray))
  {
    srd.hit = true;
    *tmin = srd.dist_sq_to_ray;
  }
}

float SCULPT_raycast_init(ViewContext *vc,
                          const float mval[2],
                          float ray_start[3],
                          float ray_end[3],
                          float ray_normal[3],
                          bool original)
{
  using namespace blender;
  float obimat[4][4];
  float dist;
  Object &ob = *vc->obact;
  RegionView3D *rv3d = vc->rv3d;
  View3D *v3d = vc->v3d;

  /* TODO: what if the segment is totally clipped? (return == 0). */
  ED_view3d_win_to_segment_clipped(
      vc->depsgraph, vc->region, vc->v3d, mval, ray_start, ray_end, true);

  invert_m4_m4(obimat, ob.object_to_world().ptr());
  mul_m4_v3(obimat, ray_start);
  mul_m4_v3(obimat, ray_end);

  sub_v3_v3v3(ray_normal, ray_end, ray_start);
  dist = normalize_v3(ray_normal);

  /* If the ray is clipped, don't adjust its start/end. */
  if ((rv3d->is_persp == false) && !RV3D_CLIPPING_ENABLED(v3d, rv3d)) {
    /* Get the view origin without the addition
     * of -ray_normal * clip_start that
     * ED_view3d_win_to_segment_clipped gave us.
     * This is necessary to avoid floating point overflow.
     */
    ED_view3d_win_to_origin(vc->region, mval, ray_start);
    mul_m4_v3(obimat, ray_start);

    bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(ob);
    bke::pbvh::clip_ray_ortho(pbvh, original, ray_start, ray_end, ray_normal);

    dist = len_v3v3(ray_start, ray_end);
  }

  return dist;
}

bool SCULPT_cursor_geometry_info_update(bContext *C,
                                        SculptCursorGeometryInfo *out,
                                        const float mval[2],
                                        bool use_sampled_normal)
{
  using namespace blender;
  using namespace blender::ed::sculpt_paint;
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  Scene *scene = CTX_data_scene(C);
  const Brush &brush = *BKE_paint_brush_for_read(BKE_paint_get_active_from_context(C));
  float ray_start[3], ray_end[3], ray_normal[3], depth, mat[3][3];
  float viewDir[3] = {0.0f, 0.0f, 1.0f};
  bool original = false;

  ViewContext vc = ED_view3d_viewcontext_init(C, depsgraph);

  Object &ob = *vc.obact;
  SculptSession &ss = *ob.sculpt;

  const View3D *v3d = CTX_wm_view3d(C);
  const Base *base = CTX_data_active_base(C);

  bke::pbvh::Tree *pbvh = bke::object::pbvh_get(ob);

  if (!pbvh || !vc.rv3d || !BKE_base_is_visible(v3d, base)) {
    zero_v3(out->location);
    zero_v3(out->normal);
    zero_v3(out->active_vertex_co);
    ss.clear_active_vert(false);
    return false;
  }

  /* bke::pbvh::Tree raycast to get active vertex and face normal. */
  depth = SCULPT_raycast_init(&vc, mval, ray_start, ray_end, ray_normal, original);
  SCULPT_stroke_modifiers_check(C, ob, brush);

  SculptRaycastData srd{};
  srd.original = original;
  srd.object = &ob;
  srd.ss = ob.sculpt;
  srd.hit = false;
  if (pbvh->type() == bke::pbvh::Type::Mesh) {
    const Mesh &mesh = *static_cast<const Mesh *>(ob.data);
    srd.vert_positions = bke::pbvh::vert_positions_eval(*depsgraph, ob);
    srd.faces = mesh.faces();
    srd.corner_verts = mesh.corner_verts();
    srd.corner_tris = mesh.corner_tris();
    const bke::AttributeAccessor attributes = mesh.attributes();
    srd.hide_poly = *attributes.lookup<bool>(".hide_poly", bke::AttrDomain::Face);
  }
  else if (pbvh->type() == bke::pbvh::Type::Grids) {
    srd.subdiv_ccg = ss.subdiv_ccg;
  }
  SCULPT_vertex_random_access_ensure(ob);
  srd.ray_start = ray_start;
  srd.ray_normal = ray_normal;
  srd.depth = depth;

  isect_ray_tri_watertight_v3_precalc(&srd.isect_precalc, ray_normal);
  bke::pbvh::raycast(
      *pbvh,
      [&](bke::pbvh::Node &node, float *tmin) { sculpt_raycast_cb(node, srd, tmin); },
      ray_start,
      ray_normal,
      srd.original);

  /* Cursor is not over the mesh, return default values. */
  if (!srd.hit) {
    zero_v3(out->location);
    zero_v3(out->normal);
    zero_v3(out->active_vertex_co);
    ss.clear_active_vert(true);
    return false;
  }

  /* Update the active vertex of the SculptSession. */
  ss.set_active_vert(srd.active_vertex);
  out->active_vertex_co = ss.active_vert_position(*depsgraph, ob);

  switch (pbvh->type()) {
    case bke::pbvh::Type::Mesh:
      ss.active_face_index = srd.active_face_grid_index;
      ss.active_grid_index = std::nullopt;
      break;
    case bke::pbvh::Type::Grids:
      ss.active_face_index = std::nullopt;
      ss.active_grid_index = srd.active_face_grid_index;
      break;
    case bke::pbvh::Type::BMesh:
      ss.active_face_index = std::nullopt;
      ss.active_grid_index = std::nullopt;
      break;
  }

  copy_v3_v3(out->location, ray_normal);
  mul_v3_fl(out->location, srd.depth);
  add_v3_v3(out->location, ray_start);

  /* Option to return the face normal directly for performance o accuracy reasons. */
  if (!use_sampled_normal) {
    copy_v3_v3(out->normal, srd.face_normal);
    return srd.hit;
  }

  /* Sampled normal calculation. */
  float radius;

  /* Update cursor data in SculptSession. */
  invert_m4_m4(ob.runtime->world_to_object.ptr(), ob.object_to_world().ptr());
  copy_m3_m4(mat, vc.rv3d->viewinv);
  mul_m3_v3(mat, viewDir);
  copy_m3_m4(mat, ob.world_to_object().ptr());
  mul_m3_v3(mat, viewDir);
  normalize_v3_v3(ss.cursor_view_normal, viewDir);
  copy_v3_v3(ss.cursor_normal, srd.face_normal);
  copy_v3_v3(ss.cursor_location, out->location);
  ss.rv3d = vc.rv3d;
  ss.v3d = vc.v3d;

  if (!BKE_brush_use_locked_size(scene, &brush)) {
    radius = paint_calc_object_space_radius(vc, out->location, BKE_brush_size_get(scene, &brush));
  }
  else {
    radius = BKE_brush_unprojected_radius_get(scene, &brush);
  }
  ss.cursor_radius = radius;

  IndexMaskMemory memory;
  const IndexMask node_mask = pbvh_gather_cursor_update(ob, original, memory);

  /* In case there are no nodes under the cursor, return the face normal. */
  if (node_mask.is_empty()) {
    copy_v3_v3(out->normal, srd.face_normal);
    return true;
  }

  bke::pbvh::update_normals(*depsgraph, ob, *pbvh);

  /* Calculate the sampled normal. */
  if (const std::optional<float3> sampled_normal = calc_area_normal(
          *depsgraph, brush, ob, node_mask))
  {
    copy_v3_v3(out->normal, *sampled_normal);
    copy_v3_v3(ss.cursor_sampled_normal, *sampled_normal);
  }
  else {
    /* Use face normal when there are no vertices to sample inside the cursor radius. */
    copy_v3_v3(out->normal, srd.face_normal);
  }
  return true;
}

bool SCULPT_stroke_get_location(bContext *C,
                                float out[3],
                                const float mval[2],
                                bool force_original)
{
  const Brush *brush = BKE_paint_brush(BKE_paint_get_active_from_context(C));
  bool check_closest = brush->falloff_shape == PAINT_FALLOFF_SHAPE_TUBE;

  return SCULPT_stroke_get_location_ex(C, out, mval, force_original, check_closest, true);
}

bool SCULPT_stroke_get_location_ex(bContext *C,
                                   float out[3],
                                   const float mval[2],
                                   bool force_original,
                                   bool check_closest,
                                   bool limit_closest_radius)
{
  using namespace blender;
  using namespace blender::ed::sculpt_paint;
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  float ray_start[3], ray_end[3], ray_normal[3], depth;

  ViewContext vc = ED_view3d_viewcontext_init(C, depsgraph);

  Object &ob = *vc.obact;

  SculptSession &ss = *ob.sculpt;
  StrokeCache *cache = ss.cache;
  bool original = force_original || ((cache) ? !cache->accum : false);

  const Brush &brush = *BKE_paint_brush(BKE_paint_get_active_from_context(C));

  SCULPT_stroke_modifiers_check(C, ob, brush);

  depth = SCULPT_raycast_init(&vc, mval, ray_start, ray_end, ray_normal, original);

  bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(ob);
  if (pbvh.type() == bke::pbvh::Type::BMesh) {
    BM_mesh_elem_table_ensure(ss.bm, BM_VERT);
    BM_mesh_elem_index_ensure(ss.bm, BM_VERT);
  }

  bool hit = false;
  {
    SculptRaycastData srd;
    srd.object = &ob;
    srd.ss = ob.sculpt;
    srd.ray_start = ray_start;
    srd.ray_normal = ray_normal;
    srd.hit = false;
    if (pbvh.type() == bke::pbvh::Type::Mesh) {
      const Mesh &mesh = *static_cast<const Mesh *>(ob.data);
      srd.vert_positions = bke::pbvh::vert_positions_eval(*depsgraph, ob);
      srd.faces = mesh.faces();
      srd.corner_verts = mesh.corner_verts();
      srd.corner_tris = mesh.corner_tris();
      const bke::AttributeAccessor attributes = mesh.attributes();
      srd.hide_poly = *attributes.lookup<bool>(".hide_poly", bke::AttrDomain::Face);
    }
    else if (pbvh.type() == bke::pbvh::Type::Grids) {
      srd.subdiv_ccg = ss.subdiv_ccg;
    }
    SCULPT_vertex_random_access_ensure(ob);
    srd.depth = depth;
    srd.original = original;
    isect_ray_tri_watertight_v3_precalc(&srd.isect_precalc, ray_normal);

    bke::pbvh::raycast(
        pbvh,
        [&](bke::pbvh::Node &node, float *tmin) { sculpt_raycast_cb(node, srd, tmin); },
        ray_start,
        ray_normal,
        srd.original);
    if (srd.hit) {
      hit = true;
      copy_v3_v3(out, ray_normal);
      mul_v3_fl(out, srd.depth);
      add_v3_v3(out, ray_start);
    }
  }

  if (hit || !check_closest) {
    return hit;
  }

  SculptFindNearestToRayData srd{};
  srd.original = original;
  srd.object = &ob;
  srd.ss = ob.sculpt;
  srd.hit = false;
  if (pbvh.type() == bke::pbvh::Type::Mesh) {
    const Mesh &mesh = *static_cast<const Mesh *>(ob.data);
    srd.vert_positions = bke::pbvh::vert_positions_eval(*depsgraph, ob);
    srd.faces = mesh.faces();
    srd.corner_verts = mesh.corner_verts();
    srd.corner_tris = mesh.corner_tris();
    const bke::AttributeAccessor attributes = mesh.attributes();
    srd.hide_poly = *attributes.lookup<bool>(".hide_poly", bke::AttrDomain::Face);
  }
  else if (pbvh.type() == bke::pbvh::Type::Grids) {
    srd.subdiv_ccg = ss.subdiv_ccg;
  }
  srd.ray_start = ray_start;
  srd.ray_normal = ray_normal;
  srd.depth = std::numeric_limits<float>::max();
  srd.dist_sq_to_ray = std::numeric_limits<float>::max();

  bke::pbvh::find_nearest_to_ray(
      pbvh,
      [&](bke::pbvh::Node &node, float *tmin) { sculpt_find_nearest_to_ray_cb(node, srd, tmin); },
      ray_start,
      ray_normal,
      srd.original);
  if (srd.hit && srd.dist_sq_to_ray) {
    hit = true;
    copy_v3_v3(out, ray_normal);
    mul_v3_fl(out, srd.depth);
    add_v3_v3(out, ray_start);
  }

  float closest_radius_sq = std::numeric_limits<float>::max();
  if (limit_closest_radius) {
    closest_radius_sq = sculpt_calc_radius(vc, brush, *CTX_data_scene(C), out);
    closest_radius_sq *= closest_radius_sq;
  }

  return hit && srd.dist_sq_to_ray < closest_radius_sq;
}

static void brush_init_tex(const Sculpt &sd, SculptSession &ss)
{
  const Brush *brush = BKE_paint_brush_for_read(&sd.paint);
  const MTex *mask_tex = BKE_brush_mask_texture_get(brush, OB_MODE_SCULPT);

  /* Init mtex nodes. */
  if (mask_tex->tex && mask_tex->tex->nodetree) {
    /* Has internal flag to detect it only does it once. */
    ntreeTexBeginExecTree(mask_tex->tex->nodetree);
  }

  if (ss.tex_pool == nullptr) {
    ss.tex_pool = BKE_image_pool_new();
  }
}

static void brush_stroke_init(bContext *C)
{
  Object &ob = *CTX_data_active_object(C);
  ToolSettings *tool_settings = CTX_data_tool_settings(C);
  const Sculpt &sd = *tool_settings->sculpt;
  SculptSession &ss = *CTX_data_active_object(C)->sculpt;
  const Brush *brush = BKE_paint_brush_for_read(&sd.paint);

  if (!G.background) {
    view3d_operator_needs_opengl(C);
  }
  brush_init_tex(sd, ss);

  const bool needs_colors = SCULPT_brush_type_is_paint(brush->sculpt_brush_type) &&
                            !SCULPT_use_image_paint_brush(tool_settings->paint_mode, ob);

  if (needs_colors) {
    BKE_sculpt_color_layer_create_if_needed(&ob);
  }

  /* CTX_data_ensure_evaluated_depsgraph should be used at the end to include the updates of
   * earlier steps modifying the data. */
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  BKE_sculpt_update_object_for_edit(
      depsgraph, &ob, SCULPT_brush_type_is_paint(brush->sculpt_brush_type));

  ED_image_paint_brush_type_update_sticky_shading_color(C, &ob);
}

static void restore_from_undo_step_if_necessary(const Depsgraph &depsgraph,
                                                const Sculpt &sd,
                                                Object &ob)
{
  using namespace blender::ed::sculpt_paint;
  SculptSession &ss = *ob.sculpt;
  const Brush *brush = BKE_paint_brush_for_read(&sd.paint);

  /* Brushes that use original coordinates and need a "restore" step. This has to happen separately
   * rather than in the brush deformation calculation because that is called once for each symmetry
   * pass, potentially within the same BVH node.
   *
   * NOTE: Despite the Cloth and Boundary brush using original coordinates, the brushes do not
   * expect this restoration to happen on every stroke step. Performing this restoration causes
   * issues with the cloth simulation mode for those brushes.
   */
  if (ELEM(brush->sculpt_brush_type,
           SCULPT_BRUSH_TYPE_ELASTIC_DEFORM,
           SCULPT_BRUSH_TYPE_GRAB,
           SCULPT_BRUSH_TYPE_THUMB,
           SCULPT_BRUSH_TYPE_ROTATE))
  {
    undo::restore_from_undo_step(depsgraph, sd, ob);
    return;
  }

  /* For the cloth brush it makes more sense to not restore the mesh state to keep running the
   * simulation from the previous state. */
  if (brush->sculpt_brush_type == SCULPT_BRUSH_TYPE_CLOTH) {
    return;
  }

  /* Restore the mesh before continuing with anchored stroke. */
  if ((brush->flag & BRUSH_ANCHORED) ||
      (ELEM(brush->sculpt_brush_type, SCULPT_BRUSH_TYPE_GRAB, SCULPT_BRUSH_TYPE_ELASTIC_DEFORM) &&
       BKE_brush_use_size_pressure(brush)) ||
      (brush->flag & BRUSH_DRAG_DOT))
  {

    undo::restore_from_undo_step(depsgraph, sd, ob);

    if (ss.cache) {
      /* Temporary data within the StrokeCache that is usually cleared at the end of the stroke
       * needs to be invalidated here so that the brushes do not accumulate and apply extra data.
       * See #129069. */
      ss.cache->layer_displacement_factor = {};
      ss.cache->paint_brush.mix_colors = {};
    }
  }
}

namespace blender::ed::sculpt_paint {

void flush_update_step(bContext *C, UpdateType update_type)
{
  Depsgraph &depsgraph = *CTX_data_depsgraph_pointer(C);
  Object &ob = *CTX_data_active_object(C);
  SculptSession &ss = *ob.sculpt;
  ARegion &region = *CTX_wm_region(C);
  MultiresModifierData *mmd = ss.multires.modifier;
  RegionView3D *rv3d = CTX_wm_region_view3d(C);
  Mesh *mesh = static_cast<Mesh *>(ob.data);

  bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(ob);

  const bool use_pbvh_draw = BKE_sculptsession_use_pbvh_draw(&ob, rv3d);

  if (rv3d) {
    /* Mark for faster 3D viewport redraws. */
    rv3d->rflag |= RV3D_PAINTING;
  }

  if (mmd != nullptr) {
    multires_mark_as_modified(&depsgraph, &ob, MULTIRES_COORDS_MODIFIED);
  }

  if ((update_type == UpdateType::Image) != 0) {
    ED_region_tag_redraw(&region);
    if (update_type == UpdateType::Image) {
      /* Early exit when only need to update the images. We don't want to tag any geometry updates
       * that would rebuild the bke::pbvh::Tree. */
      return;
    }
  }

  DEG_id_tag_update(&ob.id, ID_RECALC_SHADING);

  /* Only current viewport matters, slower update for all viewports will
   * be done in sculpt_flush_update_done. */
  if (!use_pbvh_draw) {
    /* Slow update with full dependency graph update and all that comes with it.
     * Needed when there are modifiers or full shading in the 3D viewport. */
    DEG_id_tag_update(&ob.id, ID_RECALC_GEOMETRY);
    ED_region_tag_redraw(&region);
  }
  else {
    /* Fast path where we just update the BVH nodes that changed, and redraw
     * only the part of the 3D viewport where changes happened. */
    rcti r;

    RegionView3D *rv3d = CTX_wm_region_view3d(C);
    if (rv3d && SCULPT_get_redraw_rect(region, *rv3d, ob, r)) {
      if (ss.cache) {
        ss.cache->current_r = r;
      }

      /* previous is not set in the current cache else
       * the partial rect will always grow */
      extend_redraw_rect_previous(ob, r);

      r.xmin += region.winrct.xmin - 2;
      r.xmax += region.winrct.xmin + 2;
      r.ymin += region.winrct.ymin - 2;
      r.ymax += region.winrct.ymin + 2;
      ED_region_tag_redraw_partial(&region, &r, true);
    }
  }

  if (update_type == UpdateType::Position && !ss.shapekey_active) {
    if (pbvh.type() == bke::pbvh::Type::Mesh) {
      /* Various operations inside sculpt mode can cause either the #MeshRuntimeData or the entire
       * Mesh to be changed (e.g. Undoing the very first operation after opening a file, performing
       * remesh, etc).
       *
       * This isn't an ideal fix for the core issue here, but to mitigate the drastic performance
       * falloff, we refreeze the cache before we do any operation that would tag this runtime
       * cache as dirty.
       *
       * See #130636.
       */
      if (!mesh->runtime->corner_tris_cache.frozen) {
        mesh->runtime->corner_tris_cache.freeze();
      }

      /* Updating mesh positions without marking caches dirty is generally not good, but since
       * sculpt mode has special requirements and is expected to have sole ownership of the mesh it
       * modifies, it's generally okay. */
      if (use_pbvh_draw) {
        /* When drawing from bke::pbvh::Tree is used, vertex and face normals are updated
         * later in #bke::pbvh::update_normals. However, we update the mesh's bounds eagerly here
         * since they are trivial to access from the bke::pbvh::Tree. Updating the
         * object's evaluated geometry bounding box is necessary because sculpt strokes don't cause
         * an object reevaluation. */
        mesh->tag_positions_changed_no_normals();
        /* Sculpt mode does not use or recalculate face corner normals, so they are cleared. */
        mesh->runtime->corner_normals_cache.tag_dirty();
      }
      else {
        /* Drawing happens from the modifier stack evaluation result.
         * Tag both coordinates and normals as modified, as both needed for proper drawing and the
         * modifier stack is not guaranteed to tag normals for update. */
        mesh->tag_positions_changed();
      }

      mesh->bounds_set_eager(bke::pbvh::bounds_get(pbvh));
      if (ob.runtime->bounds_eval) {
        ob.runtime->bounds_eval = mesh->bounds_min_max();
      }
    }
  }
}

void flush_update_done(const bContext *C, Object &ob, UpdateType update_type)
{
  /* After we are done drawing the stroke, check if we need to do a more
   * expensive depsgraph tag to update geometry. */
  wmWindowManager *wm = CTX_wm_manager(C);
  RegionView3D *current_rv3d = CTX_wm_region_view3d(C);
  SculptSession &ss = *ob.sculpt;
  Mesh *mesh = static_cast<Mesh *>(ob.data);

  /* Always needed for linked duplicates. */
  bool need_tag = (ID_REAL_USERS(&mesh->id) > 1);

  if (current_rv3d) {
    current_rv3d->rflag &= ~RV3D_PAINTING;
  }

  LISTBASE_FOREACH (wmWindow *, win, &wm->windows) {
    bScreen *screen = WM_window_get_active_screen(win);
    LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
      SpaceLink *sl = static_cast<SpaceLink *>(area->spacedata.first);
      if (sl->spacetype != SPACE_VIEW3D) {
        continue;
      }

      /* Tag all 3D viewports for redraw now that we are done. Others
       * viewports did not get a full redraw, and anti-aliasing for the
       * current viewport was deactivated. */
      LISTBASE_FOREACH (ARegion *, region, &area->regionbase) {
        if (region->regiontype == RGN_TYPE_WINDOW) {
          RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);
          if (rv3d != current_rv3d) {
            need_tag |= !BKE_sculptsession_use_pbvh_draw(&ob, rv3d);
          }

          ED_region_tag_redraw(region);
        }
      }
    }

    if (update_type == UpdateType::Image) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        SpaceLink *sl = static_cast<SpaceLink *>(area->spacedata.first);
        if (sl->spacetype != SPACE_IMAGE) {
          continue;
        }
        ED_area_tag_redraw_regiontype(area, RGN_TYPE_WINDOW);
      }
    }
  }

  bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(ob);

  if (update_type == UpdateType::Position) {
    bke::pbvh::store_bounds_orig(pbvh);

    /* Coordinates were modified, so fake neighbors are not longer valid. */
    SCULPT_fake_neighbors_free(ob);
  }

  if (update_type == UpdateType::Position) {
    if (pbvh.type() == bke::pbvh::Type::BMesh) {
      BKE_pbvh_bmesh_after_stroke(*ss.bm, pbvh);
    }
  }

  if (need_tag) {
    DEG_id_tag_update(&ob.id, ID_RECALC_GEOMETRY);
  }
}

}  // namespace blender::ed::sculpt_paint

/* Returns whether the mouse/stylus is over the mesh (1)
 * or over the background (0). */
static bool over_mesh(bContext *C, wmOperator * /*op*/, const float mval[2])
{
  float co_dummy[3];
  Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);

  bool check_closest = brush->falloff_shape == PAINT_FALLOFF_SHAPE_TUBE;

  return SCULPT_stroke_get_location_ex(C, co_dummy, mval, false, check_closest, true);
}

static void stroke_undo_begin(const bContext *C, wmOperator *op)
{
  using namespace blender::ed::sculpt_paint;
  const Scene &scene = *CTX_data_scene(C);
  Object &ob = *CTX_data_active_object(C);
  const Sculpt &sd = *CTX_data_tool_settings(C)->sculpt;
  const Brush *brush = BKE_paint_brush_for_read(&sd.paint);
  ToolSettings *tool_settings = CTX_data_tool_settings(C);

  /* Setup the correct undo system. Image painting and sculpting are mutual exclusive.
   * Color attributes are part of the sculpting undo system. */
  if (brush && brush->sculpt_brush_type == SCULPT_BRUSH_TYPE_PAINT &&
      SCULPT_use_image_paint_brush(tool_settings->paint_mode, ob))
  {
    ED_image_undo_push_begin(op->type->name, PaintMode::Sculpt);
  }
  else {
    undo::push_begin_ex(scene, ob, sculpt_brush_type_name(sd));
  }
}

static void stroke_undo_end(const bContext *C, Brush *brush)
{
  using namespace blender::ed::sculpt_paint;
  Object &ob = *CTX_data_active_object(C);
  ToolSettings *tool_settings = CTX_data_tool_settings(C);

  if (brush && brush->sculpt_brush_type == SCULPT_BRUSH_TYPE_PAINT &&
      SCULPT_use_image_paint_brush(tool_settings->paint_mode, ob))
  {
    ED_image_undo_push_end();
  }
  else {
    undo::push_end(ob);
  }
}

bool SCULPT_handles_colors_report(const Object &object, ReportList *reports)
{
  switch (blender::bke::object::pbvh_get(object)->type()) {
    case blender::bke::pbvh::Type::Mesh:
      return true;
    case blender::bke::pbvh::Type::BMesh:
      BKE_report(reports, RPT_ERROR, "Not supported in dynamic topology mode");
      return false;
    case blender::bke::pbvh::Type::Grids:
      BKE_report(reports, RPT_ERROR, "Not supported in multiresolution mode");
      return false;
  }
  BLI_assert_unreachable();
  return false;
}

namespace blender::ed::sculpt_paint {

static bool stroke_test_start(bContext *C, wmOperator *op, const float mval[2])
{
  /* Don't start the stroke until `mval` goes over the mesh.
   * NOTE: `mval` will only be null when re-executing the saved stroke.
   * We have exception for 'exec' strokes since they may not set `mval`,
   * only 'location', see: #52195. */
  if (((op->flag & OP_IS_INVOKE) == 0) || (mval == nullptr) || over_mesh(C, op, mval)) {
    Object &ob = *CTX_data_active_object(C);
    SculptSession &ss = *ob.sculpt;
    Sculpt &sd = *CTX_data_tool_settings(C)->sculpt;
    Brush *brush = BKE_paint_brush(&sd.paint);
    ToolSettings *tool_settings = CTX_data_tool_settings(C);

    /* NOTE: This should be removed when paint mode is available. Paint mode can force based on the
     * canvas it is painting on. (ref. use_sculpt_texture_paint). */
    if (brush && SCULPT_brush_type_is_paint(brush->sculpt_brush_type) &&
        !SCULPT_use_image_paint_brush(tool_settings->paint_mode, ob))
    {
      View3D *v3d = CTX_wm_view3d(C);
      if (v3d->shading.type == OB_SOLID) {
        v3d->shading.color_type = V3D_SHADING_VERTEX_COLOR;
      }
    }

    ED_view3d_init_mats_rv3d(&ob, CTX_wm_region_view3d(C));

    sculpt_update_cache_invariants(C, sd, ss, op, mval);

    SculptCursorGeometryInfo sgi;
    SCULPT_cursor_geometry_info_update(C, &sgi, mval, false);

    stroke_undo_begin(C, op);

    return true;
  }
  return false;
}

static void stroke_update_step(bContext *C,
                               wmOperator * /*op*/,
                               PaintStroke *stroke,
                               PointerRNA *itemptr)
{
  UnifiedPaintSettings &ups = CTX_data_tool_settings(C)->unified_paint_settings;
  const Scene &scene = *CTX_data_scene(C);
  const Depsgraph &depsgraph = *CTX_data_depsgraph_pointer(C);
  Sculpt &sd = *CTX_data_tool_settings(C)->sculpt;
  Object &ob = *CTX_data_active_object(C);
  SculptSession &ss = *ob.sculpt;
  const Brush &brush = *BKE_paint_brush_for_read(&sd.paint);
  ToolSettings &tool_settings = *CTX_data_tool_settings(C);
  StrokeCache *cache = ss.cache;
  cache->stroke_distance = paint_stroke_distance_get(stroke);

  SCULPT_stroke_modifiers_check(C, ob, brush);
  sculpt_update_cache_variants(C, sd, ob, itemptr);
  restore_from_undo_step_if_necessary(depsgraph, sd, ob);

  if (dyntopo::stroke_is_dyntopo(ob, brush)) {
    do_symmetrical_brush_actions(
        depsgraph, scene, sd, ob, dynamic_topology_update, ups, tool_settings.paint_mode);
  }

  do_symmetrical_brush_actions(
      depsgraph, scene, sd, ob, do_brush_action, ups, tool_settings.paint_mode);

  /* Hack to fix noise texture tearing mesh. */
  sculpt_fix_noise_tear(sd, ob);

  ss.cache->first_time = false;
  copy_v3_v3(ss.cache->last_location, ss.cache->location);

  /* Cleanup. */
  if (brush.sculpt_brush_type == SCULPT_BRUSH_TYPE_MASK) {
    flush_update_step(C, UpdateType::Mask);
  }
  else if (SCULPT_brush_type_is_paint(brush.sculpt_brush_type)) {
    if (SCULPT_use_image_paint_brush(tool_settings.paint_mode, ob)) {
      flush_update_step(C, UpdateType::Image);
    }
    else {
      flush_update_step(C, UpdateType::Color);
    }
  }
  else {
    flush_update_step(C, UpdateType::Position);
  }
}

static void brush_exit_tex(Sculpt &sd)
{
  Brush *brush = BKE_paint_brush(&sd.paint);
  const MTex *mask_tex = BKE_brush_mask_texture_get(brush, OB_MODE_SCULPT);

  if (mask_tex->tex && mask_tex->tex->nodetree) {
    ntreeTexEndExecTree(mask_tex->tex->nodetree->runtime->execdata);
  }
}

static void stroke_done(const bContext *C, PaintStroke * /*stroke*/)
{
  Object &ob = *CTX_data_active_object(C);
  SculptSession &ss = *ob.sculpt;
  Sculpt &sd = *CTX_data_tool_settings(C)->sculpt;
  ToolSettings *tool_settings = CTX_data_tool_settings(C);

  /* Finished. */
  if (!ss.cache) {
    brush_exit_tex(sd);
    return;
  }
  UnifiedPaintSettings *ups = &CTX_data_tool_settings(C)->unified_paint_settings;
  Brush *brush = BKE_paint_brush(&sd.paint);
  BLI_assert(brush == ss.cache->brush); /* const, so we shouldn't change. */
  ups->draw_inverted = false;

  SCULPT_stroke_modifiers_check(C, ob, *brush);

  /* Alt-Smooth. */
  if (ss.cache->alt_smooth) {
    smooth_brush_toggle_off(C, &sd.paint, ss.cache);
    /* Refresh the brush pointer in case we switched brush in the toggle function. */
    brush = BKE_paint_brush(&sd.paint);
  }

  MEM_delete(ss.cache);
  ss.cache = nullptr;

  stroke_undo_end(C, brush);

  if (brush->sculpt_brush_type == SCULPT_BRUSH_TYPE_MASK) {
    flush_update_done(C, ob, UpdateType::Mask);
  }
  else if (brush->sculpt_brush_type == SCULPT_BRUSH_TYPE_PAINT) {
    if (SCULPT_use_image_paint_brush(tool_settings->paint_mode, ob)) {
      flush_update_done(C, ob, UpdateType::Image);
    }
    else {
      flush_update_done(C, ob, UpdateType::Color);
    }
  }
  else {
    flush_update_done(C, ob, UpdateType::Position);
  }

  WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, &ob);
  brush_exit_tex(sd);
}

static int sculpt_brush_stroke_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  PaintStroke *stroke;
  int ignore_background_click;
  int retval;
  Object &ob = *CTX_data_active_object(C);
  Scene &scene = *CTX_data_scene(C);
  const View3D *v3d = CTX_wm_view3d(C);
  const Base *base = CTX_data_active_base(C);
  /* Test that ob is visible; otherwise we won't be able to get evaluated data
   * from the depsgraph. We do this here instead of SCULPT_mode_poll
   * to avoid falling through to the translate operator in the
   * global view3d keymap. */
  if (!BKE_base_is_visible(v3d, base)) {
    return OPERATOR_CANCELLED;
  }

  brush_stroke_init(C);

  Sculpt &sd = *CTX_data_tool_settings(C)->sculpt;
  Brush &brush = *BKE_paint_brush(&sd.paint);

  if (SCULPT_brush_type_is_paint(brush.sculpt_brush_type) &&
      !SCULPT_handles_colors_report(ob, op->reports))
  {
    return OPERATOR_CANCELLED;
  }
  if (SCULPT_brush_type_is_mask(brush.sculpt_brush_type)) {
    MultiresModifierData *mmd = BKE_sculpt_multires_active(&scene, &ob);
    BKE_sculpt_mask_layers_ensure(CTX_data_depsgraph_pointer(C), CTX_data_main(C), &ob, mmd);
  }
  if (!SCULPT_brush_type_is_attribute_only(brush.sculpt_brush_type) &&
      report_if_shape_key_is_locked(ob, op->reports))
  {
    return OPERATOR_CANCELLED;
  }
  if (ELEM(brush.sculpt_brush_type,
           SCULPT_BRUSH_TYPE_DISPLACEMENT_SMEAR,
           SCULPT_BRUSH_TYPE_DISPLACEMENT_ERASER))
  {
    const blender::bke::pbvh::Tree *pbvh = blender::bke::object::pbvh_get(ob);
    if (!pbvh || pbvh->type() != bke::pbvh::Type::Grids) {
      BKE_report(op->reports, RPT_ERROR, "Only supported in multiresolution mode");
      return OPERATOR_CANCELLED;
    }
  }

  stroke = paint_stroke_new(C,
                            op,
                            SCULPT_stroke_get_location,
                            stroke_test_start,
                            stroke_update_step,
                            nullptr,
                            stroke_done,
                            event->type);

  op->customdata = stroke;

  /* For tablet rotation. */
  ignore_background_click = RNA_boolean_get(op->ptr, "ignore_background_click");
  const float mval[2] = {float(event->mval[0]), float(event->mval[1])};
  if (ignore_background_click && !over_mesh(C, op, mval)) {
    paint_stroke_free(C, op, static_cast<PaintStroke *>(op->customdata));
    return OPERATOR_PASS_THROUGH;
  }

  retval = op->type->modal(C, op, event);
  if (ELEM(retval, OPERATOR_FINISHED, OPERATOR_CANCELLED)) {
    paint_stroke_free(C, op, static_cast<PaintStroke *>(op->customdata));
    return retval;
  }
  /* Add modal handler. */
  WM_event_add_modal_handler(C, op);

  OPERATOR_RETVAL_CHECK(retval);
  BLI_assert(retval == OPERATOR_RUNNING_MODAL);

  return OPERATOR_RUNNING_MODAL;
}

static int sculpt_brush_stroke_exec(bContext *C, wmOperator *op)
{
  brush_stroke_init(C);

  op->customdata = paint_stroke_new(C,
                                    op,
                                    SCULPT_stroke_get_location,
                                    stroke_test_start,
                                    stroke_update_step,
                                    nullptr,
                                    stroke_done,
                                    0);

  /* Frees op->customdata. */
  paint_stroke_exec(C, op, static_cast<PaintStroke *>(op->customdata));

  return OPERATOR_FINISHED;
}

static void sculpt_brush_stroke_cancel(bContext *C, wmOperator *op)
{
  using namespace blender::ed::sculpt_paint;
  const Depsgraph &depsgraph = *CTX_data_depsgraph_pointer(C);
  Object &ob = *CTX_data_active_object(C);
  SculptSession &ss = *ob.sculpt;
  Sculpt &sd = *CTX_data_tool_settings(C)->sculpt;
  const Brush &brush = *BKE_paint_brush_for_read(&sd.paint);

  /* XXX Canceling strokes that way does not work with dynamic topology,
   *     user will have to do real undo for now. See #46456. */
  if (ss.cache && !dyntopo::stroke_is_dyntopo(ob, brush)) {
    undo::restore_from_undo_step(depsgraph, sd, ob);
  }

  paint_stroke_cancel(C, op, static_cast<PaintStroke *>(op->customdata));

  MEM_delete(ss.cache);
  ss.cache = nullptr;

  brush_exit_tex(sd);
}

static int brush_stroke_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  return paint_stroke_modal(C, op, event, (PaintStroke **)&op->customdata);
}

static void redo_empty_ui(bContext * /*C*/, wmOperator * /*op*/) {}

void SCULPT_OT_brush_stroke(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Sculpt";
  ot->idname = "SCULPT_OT_brush_stroke";
  ot->description = "Sculpt a stroke into the geometry";

  /* API callbacks. */
  ot->invoke = sculpt_brush_stroke_invoke;
  ot->modal = brush_stroke_modal;
  ot->exec = sculpt_brush_stroke_exec;
  ot->poll = SCULPT_poll;
  ot->cancel = sculpt_brush_stroke_cancel;
  ot->ui = redo_empty_ui;

  /* Flags (sculpt does its own undo? (ton)). */
  ot->flag = OPTYPE_BLOCKING;

  /* Properties. */

  paint_stroke_operator_properties(ot);

  RNA_def_boolean(ot->srna,
                  "ignore_background_click",
                  false,
                  "Ignore Background Click",
                  "Clicks on the background do not start the stroke");
}

/* Fake Neighbors. */

static void fake_neighbor_init(Object &object, const float max_dist)
{
  SculptSession &ss = *object.sculpt;
  const int totvert = SCULPT_vertex_count_get(object);
  ss.fake_neighbors.fake_neighbor_index = Array<int>(totvert, FAKE_NEIGHBOR_NONE);
  ss.fake_neighbors.current_max_distance = max_dist;
}

static void pose_fake_neighbors_free(SculptSession &ss)
{
  ss.fake_neighbors.fake_neighbor_index = {};
}

struct NearestVertData {
  int vert = -1;
  float distance_sq = std::numeric_limits<float>::max();

  static NearestVertData join(const NearestVertData &a, const NearestVertData &b)
  {
    NearestVertData joined = a;
    if (joined.vert == -1) {
      joined.vert = b.vert;
      joined.distance_sq = b.distance_sq;
    }
    else if (b.distance_sq < joined.distance_sq) {
      joined.vert = b.vert;
      joined.distance_sq = b.distance_sq;
    }
    return joined;
  }
};

static void fake_neighbor_search_mesh(const SculptSession &ss,
                                      const Span<float3> vert_positions,
                                      const Span<bool> hide_vert,
                                      const float3 &location,
                                      const float max_distance_sq,
                                      const int island_id,
                                      const bke::pbvh::MeshNode &node,
                                      NearestVertData &nvtd)
{
  for (const int vert : node.verts()) {
    if (!hide_vert.is_empty() && hide_vert[vert]) {
      continue;
    }
    if (ss.fake_neighbors.fake_neighbor_index[vert] != FAKE_NEIGHBOR_NONE) {
      continue;
    }
    if (islands::vert_id_get(ss, vert) == island_id) {
      continue;
    }
    const float distance_sq = math::distance_squared(vert_positions[vert], location);
    if (distance_sq < max_distance_sq && distance_sq < nvtd.distance_sq) {
      nvtd.vert = vert;
      nvtd.distance_sq = distance_sq;
    }
  }
}

static void fake_neighbor_search_grids(const SculptSession &ss,
                                       const CCGKey &key,
                                       const Span<float3> positions,
                                       const BitGroupVector<> &grid_hidden,
                                       const float3 &location,
                                       const float max_distance_sq,
                                       const int island_id,
                                       const bke::pbvh::GridsNode &node,
                                       NearestVertData &nvtd)
{
  for (const int grid : node.grids()) {
    const IndexRange grid_range = bke::ccg::grid_range(key, grid);
    BKE_subdiv_ccg_foreach_visible_grid_vert(key, grid_hidden, grid, [&](const int offset) {
      const int vert = grid_range[offset];
      if (ss.fake_neighbors.fake_neighbor_index[vert] != FAKE_NEIGHBOR_NONE) {
        return;
      }
      if (islands::vert_id_get(ss, vert) == island_id) {
        return;
      }
      const float distance_sq = math::distance_squared(positions[vert], location);
      if (distance_sq < max_distance_sq && distance_sq < nvtd.distance_sq) {
        nvtd.vert = vert;
        nvtd.distance_sq = distance_sq;
      }
    });
  }
}

static void fake_neighbor_search_bmesh(const SculptSession &ss,
                                       const float3 &location,
                                       const float max_distance_sq,
                                       const int island_id,
                                       const bke::pbvh::BMeshNode &node,
                                       NearestVertData &nvtd)
{
  for (const BMVert *bm_vert :
       BKE_pbvh_bmesh_node_unique_verts(const_cast<bke::pbvh::BMeshNode *>(&node)))
  {
    if (BM_elem_flag_test(bm_vert, BM_ELEM_HIDDEN)) {
      continue;
    }
    const int vert = BM_elem_index_get(bm_vert);
    if (ss.fake_neighbors.fake_neighbor_index[vert] != FAKE_NEIGHBOR_NONE) {
      continue;
    }
    if (islands::vert_id_get(ss, vert) == island_id) {
      continue;
    }
    const float distance_sq = math::distance_squared(float3(bm_vert->co), location);
    if (distance_sq < max_distance_sq && distance_sq < nvtd.distance_sq) {
      nvtd.vert = vert;
      nvtd.distance_sq = distance_sq;
    }
  }
}

static void fake_neighbor_search(const Depsgraph &depsgraph,
                                 const Object &ob,
                                 const float max_distance_sq,
                                 MutableSpan<int> fake_neighbors)
{
  /* NOTE: This algorithm is extremely slow, it has O(n^2) runtime for the entire mesh. This looks
   * like the "closest pair of points" problem which should have far better solutions. */
  SculptSession &ss = *ob.sculpt;
  const bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(ob);

  switch (pbvh.type()) {
    case bke::pbvh::Type::Mesh: {
      const Mesh &mesh = *static_cast<const Mesh *>(ob.data);
      const Span<float3> vert_positions = bke::pbvh::vert_positions_eval(depsgraph, ob);
      const bke::AttributeAccessor attributes = mesh.attributes();
      const VArraySpan<bool> hide_vert = *attributes.lookup<bool>(".hide_vert",
                                                                  bke::AttrDomain::Point);
      for (const int vert : vert_positions.index_range()) {
        if (fake_neighbors[vert] != FAKE_NEIGHBOR_NONE) {
          continue;
        }
        const int island_id = islands::vert_id_get(ss, vert);
        const float3 &location = vert_positions[vert];

        IndexMaskMemory memory;
        const IndexMask nodes_in_sphere = bke::pbvh::search_nodes(
            pbvh, memory, [&](const bke::pbvh::Node &node) {
              return node_in_sphere(node, location, max_distance_sq, false);
            });
        if (nodes_in_sphere.is_empty()) {
          continue;
        }
        const Span<bke::pbvh::MeshNode> nodes = pbvh.nodes<bke::pbvh::MeshNode>();
        const NearestVertData nvtd = threading::parallel_reduce(
            nodes_in_sphere.index_range(),
            1,
            NearestVertData(),
            [&](const IndexRange range, NearestVertData nvtd) {
              nodes_in_sphere.slice(range).foreach_index([&](const int i) {
                fake_neighbor_search_mesh(ss,
                                          vert_positions,
                                          hide_vert,
                                          location,
                                          max_distance_sq,
                                          island_id,
                                          nodes[i],
                                          nvtd);
              });
              return nvtd;
            },
            NearestVertData::join);
        if (nvtd.vert == -1) {
          continue;
        }
        fake_neighbors[vert] = nvtd.vert;
        fake_neighbors[nvtd.vert] = vert;
      }
      break;
    }
    case bke::pbvh::Type::Grids: {
      const SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;
      const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);
      const Span<float3> positions = subdiv_ccg.positions;
      const BitGroupVector<> grid_hidden = subdiv_ccg.grid_hidden;
      for (const int vert : positions.index_range()) {
        if (fake_neighbors[vert] != FAKE_NEIGHBOR_NONE) {
          continue;
        }
        const int island_id = islands::vert_id_get(ss, vert);
        const float3 &location = positions[vert];
        IndexMaskMemory memory;
        const IndexMask nodes_in_sphere = bke::pbvh::search_nodes(
            pbvh, memory, [&](const bke::pbvh::Node &node) {
              return node_in_sphere(node, location, max_distance_sq, false);
            });
        if (nodes_in_sphere.is_empty()) {
          continue;
        }
        const Span<bke::pbvh::GridsNode> nodes = pbvh.nodes<bke::pbvh::GridsNode>();
        const NearestVertData nvtd = threading::parallel_reduce(
            nodes_in_sphere.index_range(),
            1,
            NearestVertData(),
            [&](const IndexRange range, NearestVertData nvtd) {
              nodes_in_sphere.slice(range).foreach_index([&](const int i) {
                fake_neighbor_search_grids(ss,
                                           key,
                                           positions,
                                           grid_hidden,
                                           location,
                                           max_distance_sq,
                                           island_id,
                                           nodes[i],
                                           nvtd);
              });
              return nvtd;
            },
            NearestVertData::join);
        if (nvtd.vert == -1) {
          continue;
        }
        fake_neighbors[vert] = nvtd.vert;
        fake_neighbors[nvtd.vert] = vert;
      }
      break;
    }
    case bke::pbvh::Type::BMesh: {
      const BMesh &bm = *ss.bm;
      for (const int vert : IndexRange(bm.totvert)) {
        if (fake_neighbors[vert] != FAKE_NEIGHBOR_NONE) {
          continue;
        }
        const int island_id = islands::vert_id_get(ss, vert);
        const float3 &location = BM_vert_at_index(&const_cast<BMesh &>(bm), vert)->co;
        IndexMaskMemory memory;
        const IndexMask nodes_in_sphere = bke::pbvh::search_nodes(
            pbvh, memory, [&](const bke::pbvh::Node &node) {
              return node_in_sphere(node, location, max_distance_sq, false);
            });
        if (nodes_in_sphere.is_empty()) {
          continue;
        }
        const Span<bke::pbvh::BMeshNode> nodes = pbvh.nodes<bke::pbvh::BMeshNode>();
        const NearestVertData nvtd = threading::parallel_reduce(
            nodes_in_sphere.index_range(),
            1,
            NearestVertData(),
            [&](const IndexRange range, NearestVertData nvtd) {
              nodes_in_sphere.slice(range).foreach_index([&](const int i) {
                fake_neighbor_search_bmesh(
                    ss, location, max_distance_sq, island_id, nodes[i], nvtd);
              });
              return nvtd;
            },
            NearestVertData::join);
        if (nvtd.vert == -1) {
          continue;
        }
        fake_neighbors[vert] = nvtd.vert;
        fake_neighbors[nvtd.vert] = vert;
      }
      break;
    }
  }
}

}  // namespace blender::ed::sculpt_paint

namespace blender::ed::sculpt_paint::boundary {

void ensure_boundary_info(Object &object)
{
  SculptSession &ss = *object.sculpt;
  if (!ss.vertex_info.boundary.is_empty()) {
    return;
  }

  Mesh *base_mesh = BKE_mesh_from_object(&object);

  ss.vertex_info.boundary.resize(base_mesh->verts_num);
  Array<int> adjacent_faces_edge_count(base_mesh->edges_num, 0);
  array_utils::count_indices(base_mesh->corner_edges(), adjacent_faces_edge_count);

  const Span<int2> edges = base_mesh->edges();
  for (const int e : edges.index_range()) {
    if (adjacent_faces_edge_count[e] < 2) {
      const int2 &edge = edges[e];
      ss.vertex_info.boundary[edge[0]].set();
      ss.vertex_info.boundary[edge[1]].set();
    }
  }
}

}  // namespace blender::ed::sculpt_paint::boundary

Span<int> SCULPT_fake_neighbors_ensure(const Depsgraph &depsgraph,
                                       Object &ob,
                                       const float max_dist)
{
  using namespace blender::ed::sculpt_paint;
  SculptSession &ss = *ob.sculpt;

  /* Fake neighbors were already initialized with the same distance, so no need to be
   * recalculated. */
  if (!ss.fake_neighbors.fake_neighbor_index.is_empty() &&
      ss.fake_neighbors.current_max_distance == max_dist)
  {
    return ss.fake_neighbors.fake_neighbor_index;
  }

  islands::ensure_cache(ob);
  fake_neighbor_init(ob, max_dist);
  fake_neighbor_search(depsgraph, ob, max_dist * max_dist, ss.fake_neighbors.fake_neighbor_index);

  return ss.fake_neighbors.fake_neighbor_index;
}

void SCULPT_fake_neighbors_free(Object &ob)
{
  using namespace blender::ed::sculpt_paint;
  SculptSession &ss = *ob.sculpt;
  pose_fake_neighbors_free(ss);
}

bool SCULPT_vertex_is_occluded(const Depsgraph &depsgraph,
                               const Object &object,
                               const float3 &position,
                               bool original)
{
  using namespace blender;
  SculptSession &ss = *object.sculpt;
  float ray_start[3], ray_end[3], ray_normal[3];

  ViewContext *vc = ss.cache ? ss.cache->vc : &ss.filter_cache->vc;

  const blender::float2 mouse = ED_view3d_project_float_v2_m4(
      vc->region, position, ss.cache ? ss.cache->projection_mat : ss.filter_cache->viewmat);

  int depth = SCULPT_raycast_init(vc, mouse, ray_end, ray_start, ray_normal, original);

  negate_v3(ray_normal);

  copy_v3_v3(ray_start, position);
  madd_v3_v3fl(ray_start, ray_normal, 0.002);

  bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(const_cast<Object &>(object));

  SculptRaycastData srd = {nullptr};
  srd.original = original;
  srd.object = &const_cast<Object &>(object);
  srd.ss = &ss;
  srd.hit = false;
  srd.ray_start = ray_start;
  srd.ray_normal = ray_normal;
  srd.depth = depth;
  if (pbvh.type() == bke::pbvh::Type::Mesh) {
    const Mesh &mesh = *static_cast<const Mesh *>(object.data);
    srd.vert_positions = bke::pbvh::vert_positions_eval(depsgraph, object);
    srd.faces = mesh.faces();
    srd.corner_verts = mesh.corner_verts();
    srd.corner_tris = mesh.corner_tris();
  }
  else if (pbvh.type() == bke::pbvh::Type::Grids) {
    srd.subdiv_ccg = ss.subdiv_ccg;
  }
  SCULPT_vertex_random_access_ensure(const_cast<Object &>(object));

  isect_ray_tri_watertight_v3_precalc(&srd.isect_precalc, ray_normal);
  bke::pbvh::raycast(
      pbvh,
      [&](bke::pbvh::Node &node, float *tmin) { sculpt_raycast_cb(node, srd, tmin); },
      ray_start,
      ray_normal,
      srd.original);

  return srd.hit;
}

namespace blender::ed::sculpt_paint::islands {

int vert_id_get(const SculptSession &ss, const int vert)
{
  BLI_assert(ss.topology_island_cache);
  if (!ss.topology_island_cache) {
    /* The cache should be calculated whenever it's necessary.
     * Still avoid crashing in release builds though. */
    return 0;
  }
  const SculptTopologyIslandCache &cache = *ss.topology_island_cache;
  if (!cache.vert_island_ids.is_empty()) {
    return cache.vert_island_ids[vert];
  }
  return 0;
}

void invalidate(SculptSession &ss)
{
  ss.topology_island_cache.reset();
}

static SculptTopologyIslandCache vert_disjoint_set_to_islands(const AtomicDisjointSet &vert_sets,
                                                              const int verts_num)
{
  Array<int> island_indices(verts_num);
  const int islands_num = vert_sets.calc_reduced_ids(island_indices);
  if (islands_num == 1) {
    return {};
  }

  Array<uint8_t> island_ids(island_indices.size());
  threading::parallel_for(island_ids.index_range(), 4096, [&](const IndexRange range) {
    for (const int i : range) {
      island_ids[i] = uint8_t(island_indices[i]);
    }
  });

  SculptTopologyIslandCache cache;
  cache.vert_island_ids = std::move(island_ids);
  return cache;
}

static SculptTopologyIslandCache calc_topology_islands_mesh(const Mesh &mesh)
{
  const OffsetIndices<int> faces = mesh.faces();
  const Span<int> corner_verts = mesh.corner_verts();
  const bke::AttributeAccessor attributes = mesh.attributes();
  const VArraySpan<bool> hide_poly = *attributes.lookup<bool>(".hide_poly", bke::AttrDomain::Face);
  IndexMaskMemory memory;
  const IndexMask visible_faces = hide_poly.is_empty() ?
                                      IndexMask(faces.size()) :
                                      IndexMask::from_bools_inverse(
                                          faces.index_range(), hide_poly, memory);

  AtomicDisjointSet disjoint_set(mesh.verts_num);
  visible_faces.foreach_index(GrainSize(1024), [&](const int face) {
    const Span<int> face_verts = corner_verts.slice(faces[face]);
    for (const int i : face_verts.index_range().drop_front(1)) {
      disjoint_set.join(face_verts.first(), face_verts[i]);
    }
  });
  return vert_disjoint_set_to_islands(disjoint_set, mesh.verts_num);
}

/**
 * \todo Take grid face visibility into account.
 */
static SculptTopologyIslandCache calc_topology_islands_grids(const Object &object)
{
  const SculptSession &ss = *object.sculpt;
  const SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;
  const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);
  AtomicDisjointSet disjoint_set(subdiv_ccg.positions.size());
  threading::parallel_for(IndexRange(subdiv_ccg.grids_num), 512, [&](const IndexRange range) {
    for (const int grid : range) {
      SubdivCCGNeighbors neighbors;
      for (const short y : IndexRange(key.grid_size)) {
        for (const short x : IndexRange(key.grid_size)) {
          const SubdivCCGCoord coord{grid, x, y};
          SubdivCCGNeighbors neighbors;
          BKE_subdiv_ccg_neighbor_coords_get(subdiv_ccg, coord, true, neighbors);
          for (const SubdivCCGCoord neighbor : neighbors.coords) {
            disjoint_set.join(coord.to_index(key), neighbor.to_index(key));
          }
        }
      }
    }
  });

  return vert_disjoint_set_to_islands(disjoint_set, subdiv_ccg.positions.size());
}

static SculptTopologyIslandCache calc_topology_islands_bmesh(const Object &object)
{
  const SculptSession &ss = *object.sculpt;
  const bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
  const Span<bke::pbvh::BMeshNode> nodes = pbvh.nodes<bke::pbvh::BMeshNode>();
  BMesh &bm = *ss.bm;
  BM_mesh_elem_index_ensure(&bm, BM_VERT);

  IndexMaskMemory memory;
  const IndexMask node_mask = bke::pbvh::all_leaf_nodes(pbvh, memory);
  AtomicDisjointSet disjoint_set(bm.totvert);
  node_mask.foreach_index(GrainSize(1), [&](const int i) {
    for (const BMFace *face :
         BKE_pbvh_bmesh_node_faces(&const_cast<bke::pbvh::BMeshNode &>(nodes[i])))
    {
      if (BM_elem_flag_test(face, BM_ELEM_HIDDEN)) {
        continue;
      }
      disjoint_set.join(BM_elem_index_get(face->l_first->v),
                        BM_elem_index_get(face->l_first->next->v));
      disjoint_set.join(BM_elem_index_get(face->l_first->v),
                        BM_elem_index_get(face->l_first->next->next->v));
    }
  });

  return vert_disjoint_set_to_islands(disjoint_set, bm.totvert);
}

static SculptTopologyIslandCache calculate_cache(const Object &object)
{
  switch (bke::object::pbvh_get(object)->type()) {
    case bke::pbvh::Type::Mesh:
      return calc_topology_islands_mesh(*static_cast<const Mesh *>(object.data));
    case bke::pbvh::Type::Grids:
      return calc_topology_islands_grids(object);
    case bke::pbvh::Type::BMesh:
      return calc_topology_islands_bmesh(object);
  }
  BLI_assert_unreachable();
  return {};
}

void ensure_cache(Object &object)
{
  SculptSession &ss = *object.sculpt;
  if (ss.topology_island_cache) {
    return;
  }
  ss.topology_island_cache = std::make_unique<SculptTopologyIslandCache>(calculate_cache(object));
}

}  // namespace blender::ed::sculpt_paint::islands

void SCULPT_cube_tip_init(const Sculpt & /*sd*/,
                          const Object &ob,
                          const Brush &brush,
                          float mat[4][4])
{
  using namespace blender::ed::sculpt_paint;
  SculptSession &ss = *ob.sculpt;
  float scale[4][4];
  float tmat[4][4];
  float unused[4][4];

  zero_m4(mat);
  calc_brush_local_mat(0.0, ob, unused, mat);

  /* NOTE: we ignore the radius scaling done inside of calc_brush_local_mat to
   * duplicate prior behavior.
   *
   * TODO: try disabling this and check that all edge cases work properly.
   */
  normalize_m4(mat);

  scale_m4_fl(scale, ss.cache->radius);
  mul_m4_m4m4(tmat, mat, scale);
  mul_v3_fl(tmat[1], brush.tip_scale_x);
  invert_m4_m4(mat, tmat);
}
/** \} */

namespace blender::ed::sculpt_paint {

void gather_bmesh_positions(const Set<BMVert *, 0> &verts, const MutableSpan<float3> positions)
{
  BLI_assert(verts.size() == positions.size());

  int i = 0;
  for (const BMVert *vert : verts) {
    positions[i] = vert->co;
    i++;
  }
}

void gather_grids_normals(const SubdivCCG &subdiv_ccg,
                          const Span<int> grids,
                          const MutableSpan<float3> normals)
{
  gather_data_grids(subdiv_ccg, subdiv_ccg.normals.as_span(), grids, normals);
}

void gather_bmesh_normals(const Set<BMVert *, 0> &verts, const MutableSpan<float3> normals)
{
  int i = 0;
  for (const BMVert *vert : verts) {
    normals[i] = vert->no;
    i++;
  }
}

template<typename T>
void gather_data_mesh(const Span<T> src, const Span<int> indices, const MutableSpan<T> dst)
{
  BLI_assert(indices.size() == dst.size());

  for (const int i : indices.index_range()) {
    dst[i] = src[indices[i]];
  }
}

template<typename T>
void gather_data_grids(const SubdivCCG &subdiv_ccg,
                       const Span<T> src,
                       const Span<int> grids,
                       const MutableSpan<T> node_data)
{
  const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);
  BLI_assert(grids.size() * key.grid_area == node_data.size());

  for (const int i : grids.index_range()) {
    const IndexRange grids_range = bke::ccg::grid_range(key, grids[i]);
    const IndexRange node_range = bke::ccg::grid_range(key, i);
    node_data.slice(node_range).copy_from(src.slice(grids_range));
  }
}

template<typename T>
void gather_data_bmesh(const Span<T> src,
                       const Set<BMVert *, 0> &verts,
                       const MutableSpan<T> node_data)
{
  BLI_assert(verts.size() == node_data.size());

  int i = 0;
  for (const BMVert *vert : verts) {
    node_data[i] = src[BM_elem_index_get(vert)];
    i++;
  }
}

template<typename T>
void scatter_data_mesh(const Span<T> src, const Span<int> indices, const MutableSpan<T> dst)
{
  BLI_assert(indices.size() == src.size());

  for (const int i : indices.index_range()) {
    dst[indices[i]] = src[i];
  }
}

template<typename T>
void scatter_data_grids(const SubdivCCG &subdiv_ccg,
                        const Span<T> node_data,
                        const Span<int> grids,
                        const MutableSpan<T> dst)
{
  const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);
  BLI_assert(grids.size() * key.grid_area == node_data.size());

  for (const int i : grids.index_range()) {
    const IndexRange grids_range = bke::ccg::grid_range(key, grids[i]);
    const IndexRange node_range = bke::ccg::grid_range(key, i);
    dst.slice(grids_range).copy_from(node_data.slice(node_range));
  }
}

template<typename T>
void scatter_data_bmesh(const Span<T> node_data,
                        const Set<BMVert *, 0> &verts,
                        const MutableSpan<T> dst)
{
  BLI_assert(verts.size() == node_data.size());

  int i = 0;
  for (const BMVert *vert : verts) {
    dst[BM_elem_index_get(vert)] = node_data[i];
    i++;
  }
}

template void gather_data_mesh<bool>(Span<bool>, Span<int>, MutableSpan<bool>);
template void gather_data_mesh<int>(Span<int>, Span<int>, MutableSpan<int>);
template void gather_data_mesh<float>(Span<float>, Span<int>, MutableSpan<float>);
template void gather_data_mesh<float3>(Span<float3>, Span<int>, MutableSpan<float3>);
template void gather_data_mesh<float4>(Span<float4>, Span<int>, MutableSpan<float4>);
template void gather_data_grids<int>(const SubdivCCG &, Span<int>, Span<int>, MutableSpan<int>);
template void gather_data_grids<float>(const SubdivCCG &,
                                       Span<float>,
                                       Span<int>,
                                       MutableSpan<float>);
template void gather_data_grids<float3>(const SubdivCCG &,
                                        Span<float3>,
                                        Span<int>,
                                        MutableSpan<float3>);
template void gather_data_bmesh<int>(Span<int>, const Set<BMVert *, 0> &, MutableSpan<int>);
template void gather_data_bmesh<float>(Span<float>, const Set<BMVert *, 0> &, MutableSpan<float>);
template void gather_data_bmesh<float3>(Span<float3>,
                                        const Set<BMVert *, 0> &,
                                        MutableSpan<float3>);

template void scatter_data_mesh<bool>(Span<bool>, Span<int>, MutableSpan<bool>);
template void scatter_data_mesh<int>(Span<int>, Span<int>, MutableSpan<int>);
template void scatter_data_mesh<float>(Span<float>, Span<int>, MutableSpan<float>);
template void scatter_data_mesh<float3>(Span<float3>, Span<int>, MutableSpan<float3>);
template void scatter_data_mesh<float4>(Span<float4>, Span<int>, MutableSpan<float4>);
template void scatter_data_grids<float>(const SubdivCCG &,
                                        Span<float>,
                                        Span<int>,
                                        MutableSpan<float>);
template void scatter_data_grids<float3>(const SubdivCCG &,
                                         Span<float3>,
                                         Span<int>,
                                         MutableSpan<float3>);
template void scatter_data_bmesh<float>(Span<float>, const Set<BMVert *, 0> &, MutableSpan<float>);
template void scatter_data_bmesh<float3>(Span<float3>,
                                         const Set<BMVert *, 0> &,
                                         MutableSpan<float3>);

void calc_factors_common_mesh_indexed(const Depsgraph &depsgraph,
                                      const Brush &brush,
                                      const Object &object,
                                      const MeshAttributeData &attribute_data,
                                      const Span<float3> vert_positions,
                                      const Span<float3> vert_normals,
                                      const bke::pbvh::MeshNode &node,
                                      Vector<float> &r_factors,
                                      Vector<float> &r_distances)
{
  const SculptSession &ss = *object.sculpt;
  const StrokeCache &cache = *ss.cache;

  const Span<int> verts = node.verts();

  r_factors.resize(verts.size());
  const MutableSpan<float> factors = r_factors;
  fill_factor_from_hide_and_mask(attribute_data.hide_vert, attribute_data.mask, verts, factors);
  filter_region_clip_factors(ss, vert_positions, verts, factors);
  if (brush.flag & BRUSH_FRONTFACE) {
    calc_front_face(cache.view_normal_symm, vert_normals, verts, factors);
  }

  r_distances.resize(verts.size());
  const MutableSpan<float> distances = r_distances;
  calc_brush_distances(
      ss, vert_positions, verts, eBrushFalloffShape(brush.falloff_shape), distances);
  filter_distances_with_radius(cache.radius, distances, factors);
  apply_hardness_to_distances(cache, distances);
  calc_brush_strength_factors(cache, brush, distances, factors);

  auto_mask::calc_vert_factors(depsgraph, object, cache.automasking.get(), node, verts, factors);

  calc_brush_texture_factors(ss, brush, vert_positions, verts, factors);
}

void calc_factors_common_mesh(const Depsgraph &depsgraph,
                              const Brush &brush,
                              const Object &object,
                              const MeshAttributeData &attribute_data,
                              const Span<float3> positions,
                              const Span<float3> vert_normals,
                              const bke::pbvh::MeshNode &node,
                              Vector<float> &r_factors,
                              Vector<float> &r_distances)
{
  const SculptSession &ss = *object.sculpt;
  const StrokeCache &cache = *ss.cache;

  const Span<int> verts = node.verts();

  r_factors.resize(verts.size());
  const MutableSpan<float> factors = r_factors;
  fill_factor_from_hide_and_mask(attribute_data.hide_vert, attribute_data.mask, verts, factors);
  filter_region_clip_factors(ss, positions, factors);
  if (brush.flag & BRUSH_FRONTFACE) {
    calc_front_face(cache.view_normal_symm, vert_normals, verts, factors);
  }

  r_distances.resize(verts.size());
  const MutableSpan<float> distances = r_distances;
  calc_brush_distances(ss, positions, eBrushFalloffShape(brush.falloff_shape), distances);
  filter_distances_with_radius(cache.radius, distances, factors);
  apply_hardness_to_distances(cache, distances);
  calc_brush_strength_factors(cache, brush, distances, factors);

  auto_mask::calc_vert_factors(depsgraph, object, cache.automasking.get(), node, verts, factors);

  calc_brush_texture_factors(ss, brush, positions, factors);
}

void calc_factors_common_grids(const Depsgraph &depsgraph,
                               const Brush &brush,
                               const Object &object,
                               const Span<float3> positions,
                               const bke::pbvh::GridsNode &node,
                               Vector<float> &r_factors,
                               Vector<float> &r_distances)
{
  const SculptSession &ss = *object.sculpt;
  const StrokeCache &cache = *ss.cache;
  const SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;

  const Span<int> grids = node.grids();

  r_factors.resize(positions.size());
  const MutableSpan<float> factors = r_factors;
  fill_factor_from_hide_and_mask(subdiv_ccg, grids, factors);
  filter_region_clip_factors(ss, positions, factors);
  if (brush.flag & BRUSH_FRONTFACE) {
    calc_front_face(cache.view_normal_symm, subdiv_ccg, grids, factors);
  }

  r_distances.resize(positions.size());
  const MutableSpan<float> distances = r_distances;
  calc_brush_distances(ss, positions, eBrushFalloffShape(brush.falloff_shape), distances);
  filter_distances_with_radius(cache.radius, distances, factors);
  apply_hardness_to_distances(cache, distances);
  calc_brush_strength_factors(cache, brush, distances, factors);

  auto_mask::calc_grids_factors(depsgraph, object, cache.automasking.get(), node, grids, factors);

  calc_brush_texture_factors(ss, brush, positions, factors);
}

void calc_factors_common_bmesh(const Depsgraph &depsgraph,
                               const Brush &brush,
                               const Object &object,
                               const Span<float3> positions,
                               bke::pbvh::BMeshNode &node,
                               Vector<float> &r_factors,
                               Vector<float> &r_distances)
{
  const SculptSession &ss = *object.sculpt;
  const StrokeCache &cache = *ss.cache;

  const Set<BMVert *, 0> &verts = BKE_pbvh_bmesh_node_unique_verts(&node);

  r_factors.resize(verts.size());
  const MutableSpan<float> factors = r_factors;
  fill_factor_from_hide_and_mask(*ss.bm, verts, factors);
  filter_region_clip_factors(ss, positions, factors);
  if (brush.flag & BRUSH_FRONTFACE) {
    calc_front_face(cache.view_normal_symm, verts, factors);
  }

  r_distances.resize(verts.size());
  const MutableSpan<float> distances = r_distances;
  calc_brush_distances(ss, positions, eBrushFalloffShape(brush.falloff_shape), distances);
  filter_distances_with_radius(cache.radius, distances, factors);
  apply_hardness_to_distances(cache, distances);
  calc_brush_strength_factors(cache, brush, distances, factors);

  auto_mask::calc_vert_factors(depsgraph, object, cache.automasking.get(), node, verts, factors);

  calc_brush_texture_factors(ss, brush, positions, factors);
}

void calc_factors_common_from_orig_data_mesh(const Depsgraph &depsgraph,
                                             const Brush &brush,
                                             const Object &object,
                                             const MeshAttributeData &attribute_data,
                                             const Span<float3> positions,
                                             const Span<float3> normals,
                                             const bke::pbvh::MeshNode &node,
                                             Vector<float> &r_factors,
                                             Vector<float> &r_distances)
{
  const SculptSession &ss = *object.sculpt;
  const StrokeCache &cache = *ss.cache;

  const Span<int> verts = node.verts();

  r_factors.resize(verts.size());
  const MutableSpan<float> factors = r_factors;
  fill_factor_from_hide_and_mask(attribute_data.hide_vert, attribute_data.mask, verts, factors);
  filter_region_clip_factors(ss, positions, factors);

  if (brush.flag & BRUSH_FRONTFACE) {
    calc_front_face(cache.view_normal_symm, normals, factors);
  }

  r_distances.resize(verts.size());
  const MutableSpan<float> distances = r_distances;
  calc_brush_distances(ss, positions, eBrushFalloffShape(brush.falloff_shape), distances);
  filter_distances_with_radius(cache.radius, distances, factors);
  apply_hardness_to_distances(cache, distances);
  calc_brush_strength_factors(cache, brush, distances, factors);

  auto_mask::calc_vert_factors(depsgraph, object, cache.automasking.get(), node, verts, factors);

  calc_brush_texture_factors(ss, brush, positions, factors);
}

void calc_factors_common_from_orig_data_grids(const Depsgraph &depsgraph,
                                              const Brush &brush,
                                              const Object &object,
                                              const Span<float3> positions,
                                              const Span<float3> normals,
                                              const bke::pbvh::GridsNode &node,
                                              Vector<float> &r_factors,
                                              Vector<float> &r_distances)
{
  SculptSession &ss = *object.sculpt;
  const StrokeCache &cache = *ss.cache;
  SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;

  const Span<int> grids = node.grids();

  r_factors.resize(positions.size());
  const MutableSpan<float> factors = r_factors;
  fill_factor_from_hide_and_mask(subdiv_ccg, grids, factors);
  filter_region_clip_factors(ss, positions, factors);
  if (brush.flag & BRUSH_FRONTFACE) {
    calc_front_face(cache.view_normal_symm, normals, grids, factors);
  }

  r_distances.resize(positions.size());
  const MutableSpan<float> distances = r_distances;
  calc_brush_distances(ss, positions, eBrushFalloffShape(brush.falloff_shape), distances);
  filter_distances_with_radius(cache.radius, distances, factors);
  apply_hardness_to_distances(cache, distances);
  calc_brush_strength_factors(cache, brush, distances, factors);

  auto_mask::calc_grids_factors(depsgraph, object, cache.automasking.get(), node, grids, factors);

  calc_brush_texture_factors(ss, brush, positions, factors);
}

void calc_factors_common_from_orig_data_bmesh(const Depsgraph &depsgraph,
                                              const Brush &brush,
                                              const Object &object,
                                              const Span<float3> positions,
                                              const Span<float3> normals,
                                              bke::pbvh::BMeshNode &node,
                                              Vector<float> &r_factors,
                                              Vector<float> &r_distances)
{
  SculptSession &ss = *object.sculpt;
  const StrokeCache &cache = *ss.cache;

  const Set<BMVert *, 0> &verts = BKE_pbvh_bmesh_node_unique_verts(&node);

  r_factors.resize(verts.size());
  const MutableSpan<float> factors = r_factors;
  fill_factor_from_hide_and_mask(*ss.bm, verts, factors);
  filter_region_clip_factors(ss, positions, factors);
  if (brush.flag & BRUSH_FRONTFACE) {
    calc_front_face(cache.view_normal_symm, normals, factors);
  }

  r_distances.resize(verts.size());
  const MutableSpan<float> distances = r_distances;
  calc_brush_distances(ss, positions, eBrushFalloffShape(brush.falloff_shape), distances);
  filter_distances_with_radius(cache.radius, distances, factors);
  apply_hardness_to_distances(cache, distances);
  calc_brush_strength_factors(cache, brush, distances, factors);

  auto_mask::calc_vert_factors(depsgraph, object, cache.automasking.get(), node, verts, factors);

  calc_brush_texture_factors(ss, brush, positions, factors);
}

void fill_factor_from_hide(const Span<bool> hide_vert,
                           const Span<int> verts,
                           const MutableSpan<float> r_factors)
{
  BLI_assert(verts.size() == r_factors.size());

  if (!hide_vert.is_empty()) {
    for (const int i : verts.index_range()) {
      r_factors[i] = hide_vert[verts[i]] ? 0.0f : 1.0f;
    }
  }
  else {
    r_factors.fill(1.0f);
  }
}

void fill_factor_from_hide(const SubdivCCG &subdiv_ccg,
                           const Span<int> grids,
                           const MutableSpan<float> r_factors)
{
  const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);
  BLI_assert(grids.size() * key.grid_area == r_factors.size());

  const BitGroupVector<> &grid_hidden = subdiv_ccg.grid_hidden;
  if (grid_hidden.is_empty()) {
    r_factors.fill(1.0f);
    return;
  }
  for (const int i : grids.index_range()) {
    const BitSpan hidden = grid_hidden[grids[i]];
    const int start = i * key.grid_area;
    for (const int offset : IndexRange(key.grid_area)) {
      r_factors[start + offset] = hidden[offset] ? 0.0f : 1.0f;
    }
  }
}

void fill_factor_from_hide(const Set<BMVert *, 0> &verts, const MutableSpan<float> r_factors)
{
  BLI_assert(verts.size() == r_factors.size());

  int i = 0;
  for (const BMVert *vert : verts) {
    r_factors[i] = BM_elem_flag_test_bool(vert, BM_ELEM_HIDDEN) ? 0.0f : 1.0f;
    i++;
  }
}

void fill_factor_from_hide_and_mask(const Span<bool> hide_vert,
                                    const Span<float> mask,
                                    const Span<int> verts,
                                    const MutableSpan<float> r_factors)
{
  BLI_assert(verts.size() == r_factors.size());

  if (!mask.is_empty()) {
    for (const int i : verts.index_range()) {
      r_factors[i] = 1.0f - mask[verts[i]];
    }
  }
  else {
    r_factors.fill(1.0f);
  }

  if (!hide_vert.is_empty()) {
    for (const int i : verts.index_range()) {
      if (hide_vert[verts[i]]) {
        r_factors[i] = 0.0f;
      }
    }
  }
}

void fill_factor_from_hide_and_mask(const BMesh &bm,
                                    const Set<BMVert *, 0> &verts,
                                    const MutableSpan<float> r_factors)
{
  BLI_assert(verts.size() == r_factors.size());

  /* TODO: Avoid overhead of accessing attributes for every bke::pbvh::Tree node. */
  const int mask_offset = CustomData_get_offset_named(&bm.vdata, CD_PROP_FLOAT, ".sculpt_mask");
  int i = 0;
  for (const BMVert *vert : verts) {
    r_factors[i] = (mask_offset == -1) ? 1.0f : 1.0f - BM_ELEM_CD_GET_FLOAT(vert, mask_offset);
    if (BM_elem_flag_test(vert, BM_ELEM_HIDDEN)) {
      r_factors[i] = 0.0f;
    }
    i++;
  }
}

void fill_factor_from_hide_and_mask(const SubdivCCG &subdiv_ccg,
                                    const Span<int> grids,
                                    const MutableSpan<float> r_factors)
{
  const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);
  BLI_assert(grids.size() * key.grid_area == r_factors.size());

  if (!subdiv_ccg.masks.is_empty()) {
    const Span<float> masks = subdiv_ccg.masks;
    for (const int i : grids.index_range()) {
      const Span src = masks.slice(bke::ccg::grid_range(key, grids[i]));
      MutableSpan dst = r_factors.slice(bke::ccg::grid_range(key, i));
      for (const int offset : dst.index_range()) {
        dst[offset] = 1.0f - src[offset];
      }
    }
  }
  else {
    r_factors.fill(1.0f);
  }

  const BitGroupVector<> &grid_hidden = subdiv_ccg.grid_hidden;
  if (!grid_hidden.is_empty()) {
    for (const int i : grids.index_range()) {
      const BitSpan hidden = grid_hidden[grids[i]];
      const int start = i * key.grid_area;
      for (const int offset : IndexRange(key.grid_area)) {
        if (hidden[offset]) {
          r_factors[start + offset] = 0.0f;
        }
      }
    }
  }
}

void calc_front_face(const float3 &view_normal,
                     const Span<float3> vert_normals,
                     const Span<int> verts,
                     const MutableSpan<float> factors)
{
  BLI_assert(verts.size() == factors.size());

  for (const int i : verts.index_range()) {
    const float dot = math::dot(view_normal, vert_normals[verts[i]]);
    factors[i] *= std::max(dot, 0.0f);
  }
}

void calc_front_face(const float3 &view_normal,
                     const Span<float3> normals,
                     const MutableSpan<float> factors)
{
  BLI_assert(normals.size() == factors.size());

  for (const int i : normals.index_range()) {
    const float dot = math::dot(view_normal, normals[i]);
    factors[i] *= std::max(dot, 0.0f);
  }
}
void calc_front_face(const float3 &view_normal,
                     const SubdivCCG &subdiv_ccg,
                     const Span<int> grids,
                     const MutableSpan<float> factors)
{
  const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);
  const Span<float3> normals = subdiv_ccg.normals;
  BLI_assert(grids.size() * key.grid_area == factors.size());

  for (const int i : grids.index_range()) {
    const Span<float3> grid_normals = normals.slice(bke::ccg::grid_range(key, grids[i]));
    MutableSpan<float> grid_factors = factors.slice(bke::ccg::grid_range(key, i));
    for (const int offset : grid_factors.index_range()) {
      const float dot = math::dot(view_normal, grid_normals[offset]);
      grid_factors[offset] *= std::max(dot, 0.0f);
    }
  }
}

void calc_front_face(const float3 &view_normal,
                     const Set<BMVert *, 0> &verts,
                     const MutableSpan<float> factors)
{
  BLI_assert(verts.size() == factors.size());

  int i = 0;
  for (const BMVert *vert : verts) {
    const float dot = math::dot(view_normal, float3(vert->no));
    factors[i] *= std::max(dot, 0.0f);
    i++;
  }
}

void calc_front_face(const float3 &view_normal,
                     const Set<BMFace *, 0> &faces,
                     const MutableSpan<float> factors)
{
  BLI_assert(faces.size() == factors.size());

  int i = 0;
  for (const BMFace *face : faces) {
    const float dot = math::dot(view_normal, float3(face->no));
    factors[i] *= std::max(dot, 0.0f);
    i++;
  }
}

void filter_region_clip_factors(const SculptSession &ss,
                                const Span<float3> positions,
                                const Span<int> verts,
                                const MutableSpan<float> factors)
{
  BLI_assert(verts.size() == factors.size());

  const RegionView3D *rv3d = ss.cache ? ss.cache->vc->rv3d : ss.rv3d;
  const View3D *v3d = ss.cache ? ss.cache->vc->v3d : ss.v3d;
  if (!RV3D_CLIPPING_ENABLED(v3d, rv3d)) {
    return;
  }

  const ePaintSymmetryFlags mirror_symmetry_pass = ss.cache ? ss.cache->mirror_symmetry_pass :
                                                              ePaintSymmetryFlags(0);
  const int radial_symmetry_pass = ss.cache ? ss.cache->radial_symmetry_pass : 0;
  const float4x4 symm_rot_mat_inv = ss.cache ? ss.cache->symm_rot_mat_inv : float4x4::identity();
  for (const int i : verts.index_range()) {
    float3 symm_co = symmetry_flip(positions[verts[i]], mirror_symmetry_pass);
    if (radial_symmetry_pass) {
      symm_co = math::transform_point(symm_rot_mat_inv, symm_co);
    }
    if (ED_view3d_clipping_test(rv3d, symm_co, true)) {
      factors[i] = 0.0f;
    }
  }
}

void filter_region_clip_factors(const SculptSession &ss,
                                const Span<float3> positions,
                                const MutableSpan<float> factors)
{
  BLI_assert(positions.size() == factors.size());

  const RegionView3D *rv3d = ss.cache ? ss.cache->vc->rv3d : ss.rv3d;
  const View3D *v3d = ss.cache ? ss.cache->vc->v3d : ss.v3d;
  if (!RV3D_CLIPPING_ENABLED(v3d, rv3d)) {
    return;
  }

  const ePaintSymmetryFlags mirror_symmetry_pass = ss.cache ? ss.cache->mirror_symmetry_pass :
                                                              ePaintSymmetryFlags(0);
  const int radial_symmetry_pass = ss.cache ? ss.cache->radial_symmetry_pass : 0;
  const float4x4 symm_rot_mat_inv = ss.cache ? ss.cache->symm_rot_mat_inv : float4x4::identity();
  for (const int i : positions.index_range()) {
    float3 symm_co = symmetry_flip(positions[i], mirror_symmetry_pass);
    if (radial_symmetry_pass) {
      symm_co = math::transform_point(symm_rot_mat_inv, symm_co);
    }
    if (ED_view3d_clipping_test(rv3d, symm_co, true)) {
      factors[i] = 0.0f;
    }
  }
}

void calc_brush_distances_squared(const SculptSession &ss,
                                  const Span<float3> positions,
                                  const Span<int> verts,
                                  const eBrushFalloffShape falloff_shape,
                                  const MutableSpan<float> r_distances)
{
  BLI_assert(verts.size() == r_distances.size());

  const float3 &test_location = ss.cache ? ss.cache->location_symm : ss.cursor_location;
  if (falloff_shape == PAINT_FALLOFF_SHAPE_TUBE && (ss.cache || ss.filter_cache)) {
    /* The tube falloff shape requires the cached view normal. */
    const float3 &view_normal = ss.cache ? ss.cache->view_normal_symm :
                                           ss.filter_cache->view_normal;
    float4 test_plane;
    plane_from_point_normal_v3(test_plane, test_location, view_normal);
    for (const int i : verts.index_range()) {
      float3 projected;
      closest_to_plane_normalized_v3(projected, test_plane, positions[verts[i]]);
      r_distances[i] = math::distance_squared(projected, test_location);
    }
  }
  else {
    for (const int i : verts.index_range()) {
      r_distances[i] = math::distance_squared(test_location, positions[verts[i]]);
    }
  }
}

void calc_brush_distances(const SculptSession &ss,
                          const Span<float3> positions,
                          const Span<int> verts,
                          const eBrushFalloffShape falloff_shape,
                          const MutableSpan<float> r_distances)
{
  calc_brush_distances_squared(ss, positions, verts, falloff_shape, r_distances);
  for (float &value : r_distances) {
    value = std::sqrt(value);
  }
}

void calc_brush_distances_squared(const SculptSession &ss,
                                  const Span<float3> positions,
                                  const eBrushFalloffShape falloff_shape,
                                  const MutableSpan<float> r_distances)
{
  BLI_assert(positions.size() == r_distances.size());

  const float3 &test_location = ss.cache ? ss.cache->location_symm : ss.cursor_location;
  if (falloff_shape == PAINT_FALLOFF_SHAPE_TUBE && (ss.cache || ss.filter_cache)) {
    /* The tube falloff shape requires the cached view normal. */
    const float3 &view_normal = ss.cache ? ss.cache->view_normal_symm :
                                           ss.filter_cache->view_normal;
    float4 test_plane;
    plane_from_point_normal_v3(test_plane, test_location, view_normal);
    for (const int i : positions.index_range()) {
      float3 projected;
      closest_to_plane_normalized_v3(projected, test_plane, positions[i]);
      r_distances[i] = math::distance_squared(projected, test_location);
    }
  }
  else {
    for (const int i : positions.index_range()) {
      r_distances[i] = math::distance_squared(test_location, positions[i]);
    }
  }
}

void calc_brush_distances(const SculptSession &ss,
                          const Span<float3> positions,
                          const eBrushFalloffShape falloff_shape,
                          const MutableSpan<float> r_distances)
{
  calc_brush_distances_squared(ss, positions, falloff_shape, r_distances);
  for (float &value : r_distances) {
    value = std::sqrt(value);
  }
}

void filter_distances_with_radius(const float radius,
                                  const Span<float> distances,
                                  const MutableSpan<float> factors)
{
  for (const int i : distances.index_range()) {
    if (distances[i] >= radius) {
      factors[i] = 0.0f;
    }
  }
}

void calc_brush_cube_distances(const Brush &brush,
                               const float4x4 &mat,
                               const Span<float3> positions,
                               const Span<int> verts,
                               const MutableSpan<float> r_distances,
                               const MutableSpan<float> factors)
{
  BLI_assert(verts.size() == factors.size());
  BLI_assert(verts.size() == r_distances.size());

  const float roundness = brush.tip_roundness;
  const float hardness = 1.0f - roundness;
  for (const int i : verts.index_range()) {
    if (factors[i] == 0.0f) {
      r_distances[i] = std::numeric_limits<float>::max();
      continue;
    }
    const float3 local = math::abs(math::transform_point(mat, positions[verts[i]]));

    if (!(local.x <= 1.0f && local.y <= 1.0f && local.z <= 1.0f)) {
      factors[i] = 0.0f;
      r_distances[i] = std::numeric_limits<float>::max();
      continue;
    }
    if (std::min(local.x, local.y) > hardness) {
      /* Corner, distance to the center of the corner circle. */
      r_distances[i] = math::distance(float2(hardness), float2(local)) / roundness;
      continue;
    }
    if (std::max(local.x, local.y) > hardness) {
      /* Side, distance to the square XY axis. */
      r_distances[i] = (std::max(local.x, local.y) - hardness) / roundness;
      continue;
    }

    /* Inside the square, constant distance. */
    r_distances[i] = 0.0f;
  }
}

void calc_brush_cube_distances(const Brush &brush,
                               const float4x4 &mat,
                               const Span<float3> positions,
                               const MutableSpan<float> r_distances,
                               const MutableSpan<float> factors)
{
  BLI_assert(positions.size() == factors.size());
  BLI_assert(positions.size() == r_distances.size());

  const float roundness = brush.tip_roundness;
  const float hardness = 1.0f - roundness;
  for (const int i : positions.index_range()) {
    if (factors[i] == 0.0f) {
      r_distances[i] = std::numeric_limits<float>::max();
      continue;
    }
    const float3 local = math::abs(math::transform_point(mat, positions[i]));

    if (!(local.x <= 1.0f && local.y <= 1.0f && local.z <= 1.0f)) {
      factors[i] = 0.0f;
      r_distances[i] = std::numeric_limits<float>::max();
      continue;
    }
    if (std::min(local.x, local.y) > hardness) {
      /* Corner, distance to the center of the corner circle. */
      r_distances[i] = math::distance(float2(hardness), float2(local)) / roundness;
      continue;
    }
    if (std::max(local.x, local.y) > hardness) {
      /* Side, distance to the square XY axis. */
      r_distances[i] = (std::max(local.x, local.y) - hardness) / roundness;
      continue;
    }

    /* Inside the square, constant distance. */
    r_distances[i] = 0.0f;
  }
}

void apply_hardness_to_distances(const float radius,
                                 const float hardness,
                                 const MutableSpan<float> distances)
{
  if (hardness == 0.0f) {
    return;
  }
  const float threshold = hardness * radius;
  if (hardness == 1.0f) {
    for (const int i : distances.index_range()) {
      distances[i] = distances[i] < threshold ? 0.0f : radius;
    }
    return;
  }
  const float radius_inv = math::rcp(radius);
  const float hardness_inv_rcp = math::rcp(1.0f - hardness);
  for (const int i : distances.index_range()) {
    if (distances[i] < threshold) {
      distances[i] = 0.0f;
    }
    else {
      const float radius_factor = (distances[i] * radius_inv - hardness) * hardness_inv_rcp;
      distances[i] = radius_factor * radius;
    }
  }
}

void calc_brush_strength_factors(const StrokeCache &cache,
                                 const Brush &brush,
                                 const Span<float> distances,
                                 const MutableSpan<float> factors)
{
  BKE_brush_calc_curve_factors(
      eBrushCurvePreset(brush.curve_preset), brush.curve, distances, cache.radius, factors);
}

void calc_brush_texture_factors(const SculptSession &ss,
                                const Brush &brush,
                                const Span<float3> vert_positions,
                                const Span<int> verts,
                                const MutableSpan<float> factors)
{
  BLI_assert(verts.size() == factors.size());

  const int thread_id = BLI_task_parallel_thread_id(nullptr);
  const MTex *mtex = BKE_brush_mask_texture_get(&brush, OB_MODE_SCULPT);
  if (!mtex->tex) {
    return;
  }

  for (const int i : verts.index_range()) {
    float texture_value;
    float4 texture_rgba;
    /* NOTE: This is not a thread-safe call. */
    sculpt_apply_texture(
        ss, brush, vert_positions[verts[i]], thread_id, &texture_value, texture_rgba);

    factors[i] *= texture_value;
  }
}

void calc_brush_texture_factors(const SculptSession &ss,
                                const Brush &brush,
                                const Span<float3> positions,
                                const MutableSpan<float> factors)
{
  BLI_assert(positions.size() == factors.size());

  const int thread_id = BLI_task_parallel_thread_id(nullptr);
  const MTex *mtex = BKE_brush_mask_texture_get(&brush, OB_MODE_SCULPT);
  if (!mtex->tex) {
    return;
  }

  for (const int i : positions.index_range()) {
    float texture_value;
    float4 texture_rgba;
    /* NOTE: This is not a thread-safe call. */
    sculpt_apply_texture(ss, brush, positions[i], thread_id, &texture_value, texture_rgba);

    factors[i] *= texture_value;
  }
}

void reset_translations_to_original(const MutableSpan<float3> translations,
                                    const Span<float3> positions,
                                    const Span<float3> orig_positions)
{
  BLI_assert(translations.size() == orig_positions.size());
  BLI_assert(translations.size() == positions.size());
  for (const int i : translations.index_range()) {
    const float3 prev_translation = positions[i] - orig_positions[i];
    translations[i] -= prev_translation;
  }
}

void apply_translations(const Span<float3> translations,
                        const Span<int> verts,
                        const MutableSpan<float3> positions)
{
  BLI_assert(verts.size() == translations.size());

  for (const int i : verts.index_range()) {
    const int vert = verts[i];
    positions[vert] += translations[i];
  }
}

void apply_translations(const Span<float3> translations,
                        const Span<int> grids,
                        SubdivCCG &subdiv_ccg)
{
  const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);
  MutableSpan<float3> positions = subdiv_ccg.positions;
  BLI_assert(grids.size() * key.grid_area == translations.size());

  for (const int i : grids.index_range()) {
    const Span<float3> grid_translations = translations.slice(bke::ccg::grid_range(key, i));
    MutableSpan<float3> grid_positions = positions.slice(bke::ccg::grid_range(key, grids[i]));
    for (const int offset : grid_positions.index_range()) {
      grid_positions[offset] += grid_translations[offset];
    }
  }
}

void apply_translations(const Span<float3> translations, const Set<BMVert *, 0> &verts)
{
  BLI_assert(verts.size() == translations.size());

  int i = 0;
  for (BMVert *vert : verts) {
    add_v3_v3(vert->co, translations[i]);
    i++;
  }
}

void project_translations(const MutableSpan<float3> translations, const float3 &plane)
{
  /* Equivalent to #project_plane_v3_v3v3. */
  const float len_sq = math::length_squared(plane);
  if (len_sq < std::numeric_limits<float>::epsilon()) {
    return;
  }
  const float dot_factor = -math::rcp(len_sq);
  for (const int i : translations.index_range()) {
    translations[i] += plane * math::dot(translations[i], plane) * dot_factor;
  }
}

void apply_crazyspace_to_translations(const Span<float3x3> deform_imats,
                                      const Span<int> verts,
                                      const MutableSpan<float3> translations)
{
  BLI_assert(verts.size() == translations.size());

  for (const int i : verts.index_range()) {
    translations[i] = math::transform_point(deform_imats[verts[i]], translations[i]);
  }
}

void clip_and_lock_translations(const Sculpt &sd,
                                const SculptSession &ss,
                                const Span<float3> positions,
                                const Span<int> verts,
                                const MutableSpan<float3> translations)
{
  BLI_assert(verts.size() == translations.size());

  const StrokeCache *cache = ss.cache;
  if (!cache) {
    return;
  }
  for (const int axis : IndexRange(3)) {
    if (sd.flags & (SCULPT_LOCK_X << axis)) {
      for (float3 &translation : translations) {
        translation[axis] = 0.0f;
      }
      continue;
    }

    if (!(cache->mirror_modifier_clip.flag & (uint8_t(StrokeFlags::ClipX) << axis))) {
      continue;
    }

    const float4x4 mirror(cache->mirror_modifier_clip.mat);
    const float4x4 mirror_inverse(cache->mirror_modifier_clip.mat_inv);
    for (const int i : verts.index_range()) {
      const int vert = verts[i];

      /* Transform into the space of the mirror plane, check translations, then transform back. */
      float3 co_mirror = math::transform_point(mirror, positions[vert]);
      if (math::abs(co_mirror[axis]) > cache->mirror_modifier_clip.tolerance[axis]) {
        continue;
      }
      /* Clear the translation in the local space of the mirror object. */
      co_mirror[axis] = 0.0f;
      const float3 co_local = math::transform_point(mirror_inverse, co_mirror);
      translations[i][axis] = co_local[axis] - positions[vert][axis];
    }
  }
}

void clip_and_lock_translations(const Sculpt &sd,
                                const SculptSession &ss,
                                const Span<float3> positions,
                                const MutableSpan<float3> translations)
{
  BLI_assert(positions.size() == translations.size());

  const StrokeCache *cache = ss.cache;
  if (!cache) {
    return;
  }
  for (const int axis : IndexRange(3)) {
    if (sd.flags & (SCULPT_LOCK_X << axis)) {
      for (float3 &translation : translations) {
        translation[axis] = 0.0f;
      }
      continue;
    }

    if (!(cache->mirror_modifier_clip.flag & (uint8_t(StrokeFlags::ClipX) << axis))) {
      continue;
    }

    const float4x4 mirror(cache->mirror_modifier_clip.mat);
    const float4x4 mirror_inverse(cache->mirror_modifier_clip.mat_inv);
    for (const int i : positions.index_range()) {
      /* Transform into the space of the mirror plane, check translations, then transform back. */
      float3 co_mirror = math::transform_point(mirror, positions[i]);
      if (math::abs(co_mirror[axis]) > cache->mirror_modifier_clip.tolerance[axis]) {
        continue;
      }
      /* Clear the translation in the local space of the mirror object. */
      co_mirror[axis] = 0.0f;
      const float3 co_local = math::transform_point(mirror_inverse, co_mirror);
      translations[i][axis] = co_local[axis] - positions[i][axis];
    }
  }
}

std::optional<ShapeKeyData> ShapeKeyData::from_object(Object &object)
{
  Mesh &mesh = *static_cast<Mesh *>(object.data);
  Key *keys = mesh.key;
  if (!keys) {
    return std::nullopt;
  }
  const int active_index = object.shapenr - 1;
  const KeyBlock *active_key = BKE_keyblock_find_by_index(keys, active_index);
  if (!active_key) {
    return std::nullopt;
  }
  ShapeKeyData data;
  data.active_key_data = {static_cast<float3 *>(active_key->data), active_key->totelem};
  data.basis_key_active = active_key == keys->refkey;
  if (const std::optional<Array<bool>> dependent = BKE_keyblock_get_dependent_keys(keys,
                                                                                   active_index))
  {
    int i;
    LISTBASE_FOREACH_INDEX (KeyBlock *, other_key, &keys->block, i) {
      if ((other_key != active_key) && (*dependent)[i]) {
        data.dependent_keys.append({static_cast<float3 *>(other_key->data), other_key->totelem});
      }
    }
  }
  return data;
}

PositionDeformData::PositionDeformData(const Depsgraph &depsgraph, Object &object_orig)
{
  Mesh &mesh = *static_cast<Mesh *>(object_orig.data);
  this->eval = bke::pbvh::vert_positions_eval(depsgraph, object_orig);

  if (!object_orig.sculpt->deform_imats.is_empty()) {
    deform_imats_ = object_orig.sculpt->deform_imats;
  }
  orig_ = mesh.vert_positions_for_write();

  MutableSpan eval_mut = bke::pbvh::vert_positions_eval_for_write(depsgraph, object_orig);
  if (eval_mut.data() != orig_.data()) {
    eval_mut_ = eval_mut;
  }

  shape_key_data_ = ShapeKeyData::from_object(object_orig);
}

void PositionDeformData::deform(MutableSpan<float3> translations, const Span<int> verts) const
{
  if (eval_mut_) {
    /* Apply translations to the evaluated mesh. This is necessary because multiple brush
     * evaluations can happen in between object reevaluations (otherwise just deforming the
     * original positions would be enough). */
    apply_translations(translations, verts, *eval_mut_);
  }

  if (deform_imats_) {
    /* Apply the reverse procedural deformation, since subsequent translation happens to the state
     * from "before" deforming modifiers. */
    apply_crazyspace_to_translations(*deform_imats_, verts, translations);
  }

  if (shape_key_data_) {
    if (!shape_key_data_->dependent_keys.is_empty()) {
      for (MutableSpan<float3> data : shape_key_data_->dependent_keys) {
        apply_translations(translations, verts, data);
      }
    }

    if (shape_key_data_->basis_key_active) {
      /* The basis key positions and the mesh positions are always kept in sync. */
      apply_translations(translations, verts, orig_);
    }
    apply_translations(translations, verts, shape_key_data_->active_key_data);
  }
  else {
    apply_translations(translations, verts, orig_);
  }
}

void scale_translations(const MutableSpan<float3> translations, const Span<float> factors)
{
  for (const int i : translations.index_range()) {
    translations[i] *= factors[i];
  }
}

void scale_translations(const MutableSpan<float3> translations, const float factor)
{
  if (factor == 1.0f) {
    return;
  }
  for (const int i : translations.index_range()) {
    translations[i] *= factor;
  }
}

void scale_factors(const MutableSpan<float> factors, const float strength)
{
  if (strength == 1.0f) {
    return;
  }
  for (float &factor : factors) {
    factor *= strength;
  }
}

void scale_factors(const MutableSpan<float> factors, const Span<float> strengths)
{
  BLI_assert(factors.size() == strengths.size());

  for (const int i : factors.index_range()) {
    factors[i] *= strengths[i];
  }
}

void translations_from_offset_and_factors(const float3 &offset,
                                          const Span<float> factors,
                                          const MutableSpan<float3> r_translations)
{
  BLI_assert(r_translations.size() == factors.size());

  for (const int i : factors.index_range()) {
    r_translations[i] = offset * factors[i];
  }
}

void translations_from_new_positions(const Span<float3> new_positions,
                                     const Span<int> verts,
                                     const Span<float3> old_positions,
                                     const MutableSpan<float3> translations)
{
  BLI_assert(new_positions.size() == verts.size());
  for (const int i : verts.index_range()) {
    translations[i] = new_positions[i] - old_positions[verts[i]];
  }
}

void translations_from_new_positions(const Span<float3> new_positions,
                                     const Span<float3> old_positions,
                                     const MutableSpan<float3> translations)
{
  BLI_assert(new_positions.size() == old_positions.size());
  for (const int i : new_positions.index_range()) {
    translations[i] = new_positions[i] - old_positions[i];
  }
}

void transform_positions(const Span<float3> src,
                         const float4x4 &transform,
                         const MutableSpan<float3> dst)
{
  BLI_assert(src.size() == dst.size());

  for (const int i : src.index_range()) {
    dst[i] = math::transform_point(transform, src[i]);
  }
}

void transform_positions(const float4x4 &transform, const MutableSpan<float3> positions)
{
  for (const int i : positions.index_range()) {
    positions[i] = math::transform_point(transform, positions[i]);
  }
}

OffsetIndices<int> create_node_vert_offsets(const Span<bke::pbvh::MeshNode> nodes,
                                            const IndexMask &node_mask,
                                            Array<int> &node_data)
{
  node_data.reinitialize(node_mask.size() + 1);
  node_mask.foreach_index(
      [&](const int i, const int pos) { node_data[pos] = nodes[i].verts().size(); });
  return offset_indices::accumulate_counts_to_offsets(node_data);
}

OffsetIndices<int> create_node_vert_offsets(const CCGKey &key,
                                            const Span<bke::pbvh::GridsNode> nodes,
                                            const IndexMask &node_mask,
                                            Array<int> &node_data)
{
  node_data.reinitialize(node_mask.size() + 1);
  node_mask.foreach_index([&](const int i, const int pos) {
    node_data[pos] = nodes[i].grids().size() * key.grid_area;
  });
  return offset_indices::accumulate_counts_to_offsets(node_data);
}

OffsetIndices<int> create_node_vert_offsets_bmesh(const Span<bke::pbvh::BMeshNode> nodes,
                                                  const IndexMask &node_mask,
                                                  Array<int> &node_data)
{
  node_data.reinitialize(node_mask.size() + 1);
  node_mask.foreach_index([&](const int i, const int pos) {
    node_data[pos] =
        BKE_pbvh_bmesh_node_unique_verts(const_cast<bke::pbvh::BMeshNode *>(&nodes[i])).size();
  });
  return offset_indices::accumulate_counts_to_offsets(node_data);
}

void calc_vert_neighbors(const OffsetIndices<int> faces,
                         const Span<int> corner_verts,
                         const GroupedSpan<int> vert_to_face,
                         const Span<bool> hide_poly,
                         const Span<int> verts,
                         const MutableSpan<Vector<int>> result)
{
  BLI_assert(result.size() == verts.size());
  BLI_assert(corner_verts.size() == faces.total_size());
  for (const int i : verts.index_range()) {
    vert_neighbors_get_mesh(faces, corner_verts, vert_to_face, hide_poly, verts[i], result[i]);
  }
}

void calc_vert_neighbors(const SubdivCCG &subdiv_ccg,
                         const Span<int> grids,
                         const MutableSpan<Vector<SubdivCCGCoord>> result)
{
  const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);
  SubdivCCGNeighbors neighbors;
  BLI_assert(result.size() == grids.size() * key.grid_area);
  for (const int i : grids.index_range()) {
    const int grid = grids[i];
    const int node_verts_start = i * key.grid_area;

    for (const int y : IndexRange(key.grid_size)) {
      for (const int x : IndexRange(key.grid_size)) {
        const int offset = CCG_grid_xy_to_index(key.grid_size, x, y);
        const int node_vert_index = node_verts_start + offset;

        SubdivCCGCoord coord{};
        coord.grid_index = grid;
        coord.x = x;
        coord.y = y;

        BKE_subdiv_ccg_neighbor_coords_get(subdiv_ccg, coord, false, neighbors);

        result[node_vert_index] = Vector<SubdivCCGCoord>(neighbors.coords.as_span());
      }
    }
  }
}
void calc_vert_neighbors(Set<BMVert *, 0> verts, const MutableSpan<Vector<BMVert *>> result)
{
  BLI_assert(verts.size() == result.size());

  int i = 0;
  Vector<BMVert *, 64> neighbor_data;
  for (BMVert *vert : verts) {
    Span<BMVert *> neighbors = vert_neighbors_get_bmesh(*vert, neighbor_data);
    result[i] = Vector<BMVert *>(neighbors);
    i++;
  }
}

void calc_vert_neighbors_interior(const OffsetIndices<int> faces,
                                  const Span<int> corner_verts,
                                  const GroupedSpan<int> vert_to_face,
                                  const BitSpan boundary_verts,
                                  const Span<bool> hide_poly,
                                  const Span<int> verts,
                                  const MutableSpan<Vector<int>> result)
{
  BLI_assert(result.size() == verts.size());
  BLI_assert(corner_verts.size() == faces.total_size());

  for (const int i : verts.index_range()) {
    const int vert = verts[i];
    Vector<int> &neighbors = result[i];
    vert_neighbors_get_mesh(faces, corner_verts, vert_to_face, hide_poly, verts[i], neighbors);

    if (boundary_verts[vert]) {
      if (neighbors.size() == 2) {
        /* Do not include neighbors of corner vertices. */
        neighbors.clear();
      }
      else {
        /* Only include other boundary vertices as neighbors of boundary vertices. */
        neighbors.remove_if([&](const int vert) { return !boundary_verts[vert]; });
      }
    }
  }
}

void calc_vert_neighbors_interior(const OffsetIndices<int> faces,
                                  const Span<int> corner_verts,
                                  const BitSpan boundary_verts,
                                  const SubdivCCG &subdiv_ccg,
                                  const Span<int> grids,
                                  const MutableSpan<Vector<SubdivCCGCoord>> result)
{
  const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);

  BLI_assert(grids.size() * key.grid_area == result.size());

  for (const int i : grids.index_range()) {
    const int grid = grids[i];
    const int node_verts_start = i * key.grid_area;

    /* TODO: This loop could be optimized in the future by skipping unnecessary logic for
     * non-boundary grid vertices. */
    for (const int y : IndexRange(key.grid_size)) {
      for (const int x : IndexRange(key.grid_size)) {
        const int offset = CCG_grid_xy_to_index(key.grid_size, x, y);
        const int node_vert_index = node_verts_start + offset;

        SubdivCCGCoord coord{};
        coord.grid_index = grid;
        coord.x = x;
        coord.y = y;

        SubdivCCGNeighbors neighbors;
        BKE_subdiv_ccg_neighbor_coords_get(subdiv_ccg, coord, false, neighbors);

        if (BKE_subdiv_ccg_coord_is_mesh_boundary(
                faces, corner_verts, boundary_verts, subdiv_ccg, coord))
        {
          if (neighbors.coords.size() == 2) {
            /* Do not include neighbors of corner vertices. */
            neighbors.coords.clear();
          }
          else {
            /* Only include other boundary vertices as neighbors of boundary vertices. */
            neighbors.coords.remove_if([&](const SubdivCCGCoord coord) {
              return !BKE_subdiv_ccg_coord_is_mesh_boundary(
                  faces, corner_verts, boundary_verts, subdiv_ccg, coord);
            });
          }
        }
        result[node_vert_index] = neighbors.coords;
      }
    }
  }
}

void calc_vert_neighbors_interior(const Set<BMVert *, 0> &verts,
                                  MutableSpan<Vector<BMVert *>> result)
{
  BLI_assert(verts.size() == result.size());
  Vector<BMVert *, 64> neighbor_data;

  int i = 0;
  for (BMVert *vert : verts) {
    vert_neighbors_get_interior_bmesh(*vert, neighbor_data);
    result[i] = neighbor_data;
    i++;
  }
}

void calc_translations_to_plane(const Span<float3> vert_positions,
                                const Span<int> verts,
                                const float4 &plane,
                                const MutableSpan<float3> translations)
{
  for (const int i : verts.index_range()) {
    const float3 &position = vert_positions[verts[i]];
    float3 closest;
    closest_to_plane_normalized_v3(closest, plane, position);
    translations[i] = closest - position;
  }
}

void calc_translations_to_plane(const Span<float3> positions,
                                const float4 &plane,
                                const MutableSpan<float3> translations)
{
  for (const int i : positions.index_range()) {
    const float3 &position = positions[i];
    float3 closest;
    closest_to_plane_normalized_v3(closest, plane, position);
    translations[i] = closest - position;
  }
}

void filter_verts_outside_symmetry_area(const Span<float3> positions,
                                        const float3 &pivot,
                                        const ePaintSymmetryFlags symm,
                                        const MutableSpan<float> factors)
{
  BLI_assert(positions.size() == factors.size());

  for (const int i : positions.index_range()) {
    if (!SCULPT_check_vertex_pivot_symmetry(positions[i], pivot, symm)) {
      factors[i] = 0.0f;
    }
  }
}

void filter_plane_trim_limit_factors(const Brush &brush,
                                     const StrokeCache &cache,
                                     const Span<float3> translations,
                                     const MutableSpan<float> factors)
{
  if (!(brush.flag & BRUSH_PLANE_TRIM)) {
    return;
  }
  const float threshold = cache.radius_squared * cache.plane_trim_squared;
  for (const int i : translations.index_range()) {
    if (math::length_squared(translations[i]) > threshold) {
      factors[i] = 0.0f;
    }
  }
}

void filter_below_plane_factors(const Span<float3> vert_positions,
                                const Span<int> verts,
                                const float4 &plane,
                                const MutableSpan<float> factors)
{
  for (const int i : verts.index_range()) {
    if (plane_point_side_v3(plane, vert_positions[verts[i]]) <= 0.0f) {
      factors[i] = 0.0f;
    }
  }
}

void filter_below_plane_factors(const Span<float3> positions,
                                const float4 &plane,
                                const MutableSpan<float> factors)
{
  for (const int i : positions.index_range()) {
    if (plane_point_side_v3(plane, positions[i]) <= 0.0f) {
      factors[i] = 0.0f;
    }
  }
}

void filter_above_plane_factors(const Span<float3> vert_positions,
                                const Span<int> verts,
                                const float4 &plane,
                                const MutableSpan<float> factors)
{
  for (const int i : verts.index_range()) {
    if (plane_point_side_v3(plane, vert_positions[verts[i]]) > 0.0f) {
      factors[i] = 0.0f;
    }
  }
}

void filter_above_plane_factors(const Span<float3> positions,
                                const float4 &plane,
                                const MutableSpan<float> factors)
{
  for (const int i : positions.index_range()) {
    if (plane_point_side_v3(plane, positions[i]) > 0.0f) {
      factors[i] = 0.0f;
    }
  }
}

}  // namespace blender::ed::sculpt_paint
