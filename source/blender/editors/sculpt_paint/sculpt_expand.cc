/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edsculpt
 */
#include "sculpt_expand.hh"

#include <cmath>

#include "MEM_guardedalloc.h"

#include "BLI_array_utils.hh"
#include "BLI_bit_vector.hh"
#include "BLI_linklist_stack.h"
#include "BLI_math_vector.hh"

#include "DNA_brush_types.h"
#include "DNA_object_types.h"

#include "BKE_attribute.hh"
#include "BKE_brush.hh"
#include "BKE_ccg.hh"
#include "BKE_colortools.hh"
#include "BKE_context.hh"
#include "BKE_image.hh"
#include "BKE_layer.hh"
#include "BKE_mesh.hh"
#include "BKE_mesh_mapping.hh"
#include "BKE_paint.hh"
#include "BKE_paint_bvh.hh"
#include "BKE_report.hh"
#include "BKE_subdiv_ccg.hh"

#include "BLT_translation.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "ED_screen.hh"
#include "ED_sculpt.hh"

#include "paint_intern.hh"
#include "paint_mask.hh"
#include "sculpt_boundary.hh"
#include "sculpt_color.hh"
#include "sculpt_face_set.hh"
#include "sculpt_flood_fill.hh"
#include "sculpt_geodesic.hh"
#include "sculpt_intern.hh"
#include "sculpt_islands.hh"
#include "sculpt_smooth.hh"
#include "sculpt_undo.hh"

#include "IMB_colormanagement.hh"
#include "IMB_imbuf.hh"

#include "bmesh.hh"

namespace blender::ed::sculpt_paint::expand {

/* Sculpt Expand. */
/* Operator for creating selections and patterns in Sculpt Mode. Expand can create masks, face sets
 * and fill vertex colors. */
/* The main functionality of the operator
 * - The operator initializes a value per vertex, called "falloff". There are multiple algorithms
 * to generate these falloff values which will create different patterns in the result when using
 * the operator. These falloff values require algorithms that rely on mesh connectivity, so they
 * are only valid on parts of the mesh that are in the same connected component as the given
 * initial vertices. If needed, these falloff values are propagated from vertex or grids into the
 * base mesh faces.
 *
 * - On each modal callback, the operator gets the active vertex and face and gets its falloff
 *   value from its precalculated falloff. This is now the active falloff value.
 * - Using the active falloff value and the settings of the expand operation (which can be modified
 *   during execution using the modal key-map), the operator loops over all elements in the mesh to
 *   check if they are enabled of not.
 * - Based on each element state after evaluating the settings, the desired mesh data (mask, face
 *   sets, colors...) is updated.
 */

/**
 * Used for defining an invalid vertex state (for example, when the cursor is not over the mesh).
 */
#define SCULPT_EXPAND_VERTEX_NONE -1

/** Used for defining an uninitialized active component index for an unused symmetry pass. */
#define EXPAND_ACTIVE_COMPONENT_NONE -1
/**
 * Defines how much each time the texture distortion is increased/decreased
 * when using the modal key-map.
 */
#define SCULPT_EXPAND_TEXTURE_DISTORTION_STEP 0.01f

/**
 * This threshold offsets the required falloff value to start a new loop. This is needed because in
 * some situations, vertices which have the same falloff value as max_falloff will start a new
 * loop, which is undesired.
 */
#define SCULPT_EXPAND_LOOP_THRESHOLD 0.00001f

/**
 * Defines how much changes in curvature in the mesh affect the falloff shape when using normal
 * falloff. This default was found experimentally and it works well in most cases, but can be
 * exposed for tweaking if needed.
 */
#define SCULPT_EXPAND_NORMALS_FALLOFF_EDGE_SENSITIVITY 300

/* Expand Modal Key-map. */
enum {
  SCULPT_EXPAND_MODAL_CONFIRM = 1,
  SCULPT_EXPAND_MODAL_CANCEL,
  SCULPT_EXPAND_MODAL_INVERT,
  SCULPT_EXPAND_MODAL_PRESERVE_TOGGLE,
  SCULPT_EXPAND_MODAL_GRADIENT_TOGGLE,
  SCULPT_EXPAND_MODAL_FALLOFF_CYCLE,
  SCULPT_EXPAND_MODAL_RECURSION_STEP_GEODESIC,
  SCULPT_EXPAND_MODAL_RECURSION_STEP_TOPOLOGY,
  SCULPT_EXPAND_MODAL_MOVE_TOGGLE,
  SCULPT_EXPAND_MODAL_FALLOFF_GEODESIC,
  SCULPT_EXPAND_MODAL_FALLOFF_TOPOLOGY,
  SCULPT_EXPAND_MODAL_FALLOFF_TOPOLOGY_DIAGONALS,
  SCULPT_EXPAND_MODAL_FALLOFF_SPHERICAL,
  SCULPT_EXPAND_MODAL_SNAP_TOGGLE,
  SCULPT_EXPAND_MODAL_LOOP_COUNT_INCREASE,
  SCULPT_EXPAND_MODAL_LOOP_COUNT_DECREASE,
  SCULPT_EXPAND_MODAL_BRUSH_GRADIENT_TOGGLE,
  SCULPT_EXPAND_MODAL_TEXTURE_DISTORTION_INCREASE,
  SCULPT_EXPAND_MODAL_TEXTURE_DISTORTION_DECREASE,
};

/* Functions for getting the state of mesh elements (vertices and base mesh faces). When the main
 * functions for getting the state of an element return true it means that data associated to that
 * element will be modified by expand. */

/**
 * Returns true if the vertex is in a connected component with correctly initialized falloff
 * values.
 */
static bool is_vert_in_active_component(const SculptSession &ss,
                                        const Cache &expand_cache,
                                        const int vert)
{
  for (int i = 0; i < EXPAND_SYMM_AREAS; i++) {
    if (islands::vert_id_get(ss, vert) == expand_cache.active_connected_islands[i]) {
      return true;
    }
  }
  return false;
}

/**
 * Returns true if the face is in a connected component with correctly initialized falloff values.
 */
static bool is_face_in_active_component(const Object &object,
                                        const OffsetIndices<int> faces,
                                        const Span<int> corner_verts,
                                        const Cache &expand_cache,
                                        const int f)
{
  const SculptSession &ss = *object.sculpt;
  switch (bke::object::pbvh_get(object)->type()) {
    case bke::pbvh::Type::Mesh:
      return is_vert_in_active_component(ss, expand_cache, corner_verts[faces[f].start()]);
    case bke::pbvh::Type::Grids:
      return is_vert_in_active_component(
          ss,
          expand_cache,
          faces[f].start() * BKE_subdiv_ccg_key_top_level(*ss.subdiv_ccg).grid_area);
    case bke::pbvh::Type::BMesh:
      return is_vert_in_active_component(
          ss, expand_cache, BM_elem_index_get(ss.bm->ftable[f]->l_first->v));
  }
  BLI_assert_unreachable();
  return false;
}

/**
 * Returns the falloff value of a vertex. This function includes texture distortion, which is not
 * precomputed into the initial falloff values.
 */
static float falloff_value_vertex_get(const SculptSession &ss,
                                      const Cache &expand_cache,
                                      const float3 &position,
                                      const int vert)
{
  if (expand_cache.texture_distortion_strength == 0.0f) {
    return expand_cache.vert_falloff[vert];
  }
  const Brush *brush = expand_cache.brush;
  const MTex *mtex = BKE_brush_mask_texture_get(brush, OB_MODE_SCULPT);
  if (!mtex->tex) {
    return expand_cache.vert_falloff[vert];
  }

  float rgba[4];
  const float avg = BKE_brush_sample_tex_3d(
      expand_cache.paint, brush, mtex, position, rgba, 0, ss.tex_pool);

  const float distortion = (avg - 0.5f) * expand_cache.texture_distortion_strength *
                           expand_cache.max_vert_falloff;
  return expand_cache.vert_falloff[vert] + distortion;
}

/**
 * Returns the maximum valid falloff value stored in the falloff array, taking the maximum possible
 * texture distortion into account.
 */
static float max_vert_falloff_get(const Cache &expand_cache)
{
  if (expand_cache.texture_distortion_strength == 0.0f) {
    return expand_cache.max_vert_falloff;
  }

  const MTex *mask_tex = BKE_brush_mask_texture_get(expand_cache.brush, OB_MODE_SCULPT);
  if (!mask_tex->tex) {
    return expand_cache.max_vert_falloff;
  }

  return expand_cache.max_vert_falloff +
         (0.5f * expand_cache.texture_distortion_strength * expand_cache.max_vert_falloff);
}

static bool vert_falloff_is_enabled(const SculptSession &ss,
                                    const Cache &expand_cache,
                                    const float3 &position,
                                    const int vert)
{
  const float max_falloff_factor = max_vert_falloff_get(expand_cache);
  const float loop_len = (max_falloff_factor / expand_cache.loop_count) +
                         SCULPT_EXPAND_LOOP_THRESHOLD;

  const float vertex_falloff_factor = falloff_value_vertex_get(ss, expand_cache, position, vert);
  const float active_factor = fmod(expand_cache.active_falloff, loop_len);
  const float falloff_factor = fmod(vertex_falloff_factor, loop_len);

  return falloff_factor < active_factor;
}

/**
 * Main function to get the state of a face for the current state and settings of a #Cache.
 * Returns true when the target data should be modified by expand.
 */
static bool face_state_get(const Object &object,
                           const OffsetIndices<int> faces,
                           const Span<int> corner_verts,
                           const Span<bool> hide_poly,
                           const Span<int> face_sets,
                           const Cache &expand_cache,
                           const int face)
{
  if (!hide_poly.is_empty() && hide_poly[face]) {
    return false;
  }

  if (!is_face_in_active_component(object, faces, corner_verts, expand_cache, face)) {
    return false;
  }

  if (expand_cache.all_enabled) {
    if (expand_cache.invert) {
      return false;
    }
    return true;
  }

  bool enabled = false;

  if (expand_cache.snap_enabled_face_sets) {
    const int face_set = expand_cache.original_face_sets[face];
    enabled = expand_cache.snap_enabled_face_sets->contains(face_set);
  }
  else {
    const float loop_len = (expand_cache.max_face_falloff / expand_cache.loop_count) +
                           SCULPT_EXPAND_LOOP_THRESHOLD;

    const float active_factor = fmod(expand_cache.active_falloff, loop_len);
    const float falloff_factor = fmod(expand_cache.face_falloff[face], loop_len);
    enabled = falloff_factor < active_factor;
  }

  if (expand_cache.falloff_type == FalloffType::ActiveFaceSet) {
    if (face_sets[face] == expand_cache.initial_active_face_set) {
      enabled = false;
    }
  }

  if (expand_cache.invert) {
    enabled = !enabled;
  }

  return enabled;
}

/**
 * For target modes that support gradients (such as sculpt masks or colors), this function returns
 * the corresponding gradient value for an enabled vertex.
 */
static float gradient_value_get(const SculptSession &ss,
                                const Cache &expand_cache,
                                const float3 &position,
                                const int vert)
{
  if (!expand_cache.falloff_gradient) {
    return 1.0f;
  }

  const float max_falloff_factor = max_vert_falloff_get(expand_cache);
  const float loop_len = (max_falloff_factor / expand_cache.loop_count) +
                         SCULPT_EXPAND_LOOP_THRESHOLD;

  const float vertex_falloff_factor = falloff_value_vertex_get(ss, expand_cache, position, vert);
  const float active_factor = fmod(expand_cache.active_falloff, loop_len);
  const float falloff_factor = fmod(vertex_falloff_factor, loop_len);

  float linear_falloff;

  if (expand_cache.invert) {
    /* Active factor is the result of a modulus operation using loop_len, so they will never be
     * equal and loop_len - active_factor should never be 0. */
    BLI_assert((loop_len - active_factor) != 0.0f);
    linear_falloff = (falloff_factor - active_factor) / (loop_len - active_factor);
  }
  else {
    linear_falloff = 1.0f - (falloff_factor / active_factor);
  }

  if (!expand_cache.brush_gradient) {
    return linear_falloff;
  }

  return BKE_brush_curve_strength(expand_cache.brush, linear_falloff, 1.0f);
}

/* Utility functions for getting all vertices state during expand. */

/**
 * Returns a bitmap indexed by vertex index which contains if the vertex was enabled or not for a
 * give expand_cache state.
 */
static BitVector<> enabled_state_to_bitmap(const Depsgraph &depsgraph,
                                           const Object &object,
                                           const Cache &expand_cache)
{
  const SculptSession &ss = *object.sculpt;
  const int totvert = SCULPT_vertex_count_get(object);
  BitVector<> enabled_verts(totvert);
  if (expand_cache.all_enabled) {
    if (!expand_cache.invert) {
      enabled_verts.fill(true);
    }
    return enabled_verts;
  }
  switch (bke::object::pbvh_get(object)->type()) {
    case bke::pbvh::Type::Mesh: {
      const Mesh &mesh = *static_cast<const Mesh *>(object.data);
      const Span<float3> positions = bke::pbvh::vert_positions_eval(depsgraph, object);
      const GroupedSpan<int> vert_to_face_map = mesh.vert_to_face_map();
      const bke::AttributeAccessor attributes = mesh.attributes();
      const VArraySpan hide_vert = *attributes.lookup<bool>(".hide_vert", bke::AttrDomain::Point);
      const VArraySpan face_sets = *attributes.lookup_or_default<int>(
          ".sculpt_face_set", bke::AttrDomain::Face, 0);
      threading::parallel_for_aligned(
          IndexRange(totvert), 1024, bits::BitsPerInt, [&](const IndexRange range) {
            for (const int vert : range) {
              if (!hide_vert.is_empty() && hide_vert[vert]) {
                continue;
              }
              if (!is_vert_in_active_component(ss, expand_cache, vert)) {
                continue;
              }
              if (expand_cache.snap) {
                const int face_set = face_set::vert_face_set_get(
                    vert_to_face_map, face_sets, vert);
                enabled_verts[vert].set(expand_cache.snap_enabled_face_sets->contains(face_set));
                continue;
              }
              enabled_verts[vert].set(
                  vert_falloff_is_enabled(ss, expand_cache, positions[vert], vert));
            }
          });
      break;
    }
    case bke::pbvh::Type::Grids: {
      const Mesh &base_mesh = *static_cast<const Mesh *>(object.data);
      const bke::AttributeAccessor attributes = base_mesh.attributes();
      const VArraySpan face_sets = *attributes.lookup_or_default<int>(
          ".sculpt_face_set", bke::AttrDomain::Face, 0);

      SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;
      const Span<int> grid_to_face_map = subdiv_ccg.grid_to_face_map;
      const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);
      const Span<float3> positions = subdiv_ccg.positions;
      BitGroupVector<> &grid_hidden = subdiv_ccg.grid_hidden;
      for (const int grid : IndexRange(subdiv_ccg.grids_num)) {
        const int start = grid * key.grid_area;
        const int face_set = face_sets[grid_to_face_map[grid]];
        BKE_subdiv_ccg_foreach_visible_grid_vert(key, grid_hidden, grid, [&](const int offset) {
          const int vert = start + offset;
          if (!is_vert_in_active_component(ss, expand_cache, vert)) {
            return;
          }
          if (expand_cache.snap) {
            enabled_verts[vert].set(expand_cache.snap_enabled_face_sets->contains(face_set));
            return;
          }
          enabled_verts[vert].set(
              vert_falloff_is_enabled(ss, expand_cache, positions[vert], vert));
        });
      }
      break;
    }
    case bke::pbvh::Type::BMesh: {
      BMesh &bm = *ss.bm;
      for (const int vert : IndexRange(totvert)) {
        const BMVert *bm_vert = BM_vert_at_index(&bm, vert);
        if (BM_elem_flag_test(bm_vert, BM_ELEM_HIDDEN)) {
          continue;
        }
        if (!is_vert_in_active_component(ss, expand_cache, vert)) {
          continue;
        }
        if (expand_cache.snap) {
          /* TODO: Support face sets for BMesh. */
          const int face_set = 0;
          enabled_verts[vert].set(expand_cache.snap_enabled_face_sets->contains(face_set));
          continue;
        }
        enabled_verts[vert].set(vert_falloff_is_enabled(ss, expand_cache, bm_vert->co, vert));
      }
      break;
    }
  }
  if (expand_cache.invert) {
    bits::invert(MutableBoundedBitSpan(enabled_verts));
  }
  return enabled_verts;
}

/**
 * Returns a bitmap indexed by vertex index which contains if the vertex is in the boundary of the
 * enabled vertices. This is defined as vertices that are enabled and at least have one connected
 * vertex that is not enabled.
 */
static IndexMask boundary_from_enabled(Object &object,
                                       const BitSpan enabled_verts,
                                       const bool use_mesh_boundary,
                                       IndexMaskMemory &memory)
{
  SculptSession &ss = *object.sculpt;
  const int totvert = SCULPT_vertex_count_get(object);

  const IndexMask enabled_mask = IndexMask::from_bits(enabled_verts, memory);

  BitVector<> boundary_verts(totvert);
  switch (bke::object::pbvh_get(object)->type()) {
    case bke::pbvh::Type::Mesh: {
      const Mesh &mesh = *static_cast<const Mesh *>(object.data);
      const OffsetIndices faces = mesh.faces();
      const Span<int> corner_verts = mesh.corner_verts();
      const GroupedSpan<int> vert_to_face_map = mesh.vert_to_face_map();
      const bke::AttributeAccessor attributes = mesh.attributes();
      const VArraySpan hide_poly = *attributes.lookup<bool>(".hide_poly", bke::AttrDomain::Face);
      return IndexMask::from_predicate(enabled_mask, GrainSize(1024), memory, [&](const int vert) {
        Vector<int> neighbors;
        for (const int neighbor : vert_neighbors_get_mesh(
                 faces, corner_verts, vert_to_face_map, hide_poly, vert, neighbors))
        {
          if (!enabled_verts[neighbor]) {
            return true;
          }
        }

        if (use_mesh_boundary &&
            boundary::vert_is_boundary(vert_to_face_map, hide_poly, ss.vertex_info.boundary, vert))
        {
          return true;
        }

        return false;
      });
    }
    case bke::pbvh::Type::Grids: {
      const Mesh &base_mesh = *static_cast<const Mesh *>(object.data);
      const OffsetIndices faces = base_mesh.faces();
      const Span<int> corner_verts = base_mesh.corner_verts();

      const SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;
      const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);
      return IndexMask::from_predicate(enabled_mask, GrainSize(1024), memory, [&](const int vert) {
        const SubdivCCGCoord coord = SubdivCCGCoord::from_index(key, vert);
        SubdivCCGNeighbors neighbors;
        BKE_subdiv_ccg_neighbor_coords_get(subdiv_ccg, coord, false, neighbors);
        for (const SubdivCCGCoord neighbor : neighbors.coords) {
          if (!enabled_verts[neighbor.to_index(key)]) {
            return true;
          }
        }

        if (use_mesh_boundary &&
            boundary::vert_is_boundary(
                faces, corner_verts, ss.vertex_info.boundary, subdiv_ccg, coord))
        {
          return true;
        }

        return false;
      });
    }
    case bke::pbvh::Type::BMesh: {
      return IndexMask::from_predicate(enabled_mask, GrainSize(1024), memory, [&](const int vert) {
        BMVert *bm_vert = BM_vert_at_index(ss.bm, vert);
        BMeshNeighborVerts neighbors;
        for (const BMVert *neighbor : vert_neighbors_get_bmesh(*bm_vert, neighbors)) {
          if (!enabled_verts[BM_elem_index_get(neighbor)]) {
            return true;
          }
        }

        if (use_mesh_boundary && BM_vert_is_boundary(bm_vert)) {
          return true;
        }

        return false;
      });
    }
  }
  BLI_assert_unreachable();
  return {};
}

static void check_topology_islands(Object &ob, FalloffType falloff_type)
{
  SculptSession &ss = *ob.sculpt;
  Cache &expand_cache = *ss.expand_cache;

  expand_cache.check_islands = ELEM(falloff_type,
                                    FalloffType::Geodesic,
                                    FalloffType::Topology,
                                    FalloffType::TopologyNormals,
                                    FalloffType::BoundaryTopology,
                                    FalloffType::Normals);

  if (expand_cache.check_islands) {
    islands::ensure_cache(ob);
  }
}

}  // namespace blender::ed::sculpt_paint::expand

namespace blender::ed::sculpt_paint {

/* Functions implementing different algorithms for initializing falloff values. */

Vector<int> find_symm_verts_mesh(const Depsgraph &depsgraph,
                                 const Object &object,
                                 const int original_vert,
                                 const float max_distance)
{
  const ePaintSymmetryFlags symm = SCULPT_mesh_symmetry_xyz_get(object);
  const bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
  const bool use_original = false;

  Vector<int> symm_verts;
  symm_verts.append(original_vert);

  const Mesh &mesh = *static_cast<const Mesh *>(object.data);
  const Span<float3> positions = bke::pbvh::vert_positions_eval(depsgraph, object);
  const bke::AttributeAccessor attributes = mesh.attributes();
  const VArraySpan hide_vert = *attributes.lookup<bool>(".hide_vert", bke::AttrDomain::Point);

  const float3 location = positions[original_vert];
  for (int symm_it = 1; symm_it <= symm; symm_it++) {
    if (!is_symmetry_iteration_valid(symm_it, symm)) {
      continue;
    }
    const float3 symm_location = symmetry_flip(location, ePaintSymmetryFlags(symm_it));
    const std::optional<int> nearest = nearest_vert_calc_mesh(
        pbvh, positions, hide_vert, symm_location, max_distance, use_original);
    if (!nearest) {
      continue;
    }
    symm_verts.append(*nearest);
  }

  std::sort(symm_verts.begin(), symm_verts.end());
  return symm_verts;
}

Vector<int> find_symm_verts_grids(const Object &object,
                                  const int original_vert,
                                  const float max_distance)
{
  const ePaintSymmetryFlags symm = SCULPT_mesh_symmetry_xyz_get(object);
  const bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
  const bool use_original = false;

  Vector<int> symm_verts;
  symm_verts.append(original_vert);

  const SculptSession &ss = *object.sculpt;
  const SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;
  const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);
  const Span<float3> positions = subdiv_ccg.positions;
  const float3 location = positions[original_vert];
  for (int symm_it = 1; symm_it <= symm; symm_it++) {
    if (!is_symmetry_iteration_valid(symm_it, symm)) {
      continue;
    }
    const float3 symm_location = symmetry_flip(location, ePaintSymmetryFlags(symm_it));
    const std::optional<SubdivCCGCoord> nearest = nearest_vert_calc_grids(
        pbvh, subdiv_ccg, symm_location, max_distance, use_original);
    if (!nearest) {
      continue;
    }
    symm_verts.append(nearest->to_index(key));
  }

  std::sort(symm_verts.begin(), symm_verts.end());
  return symm_verts;
}

Vector<int> find_symm_verts_bmesh(const Object &object,
                                  const int original_vert,
                                  const float max_distance)
{
  const ePaintSymmetryFlags symm = SCULPT_mesh_symmetry_xyz_get(object);
  const bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
  const bool use_original = false;

  Vector<int> symm_verts;
  symm_verts.append(original_vert);

  const SculptSession &ss = *object.sculpt;
  BMesh &bm = *ss.bm;
  const BMVert *original_bm_vert = BM_vert_at_index(&bm, original_vert);
  const float3 location = original_bm_vert->co;
  for (int symm_it = 1; symm_it <= symm; symm_it++) {
    if (!is_symmetry_iteration_valid(symm_it, symm)) {
      continue;
    }
    const float3 symm_location = symmetry_flip(location, ePaintSymmetryFlags(symm_it));
    const std::optional<BMVert *> nearest = nearest_vert_calc_bmesh(
        pbvh, symm_location, max_distance, use_original);
    if (!nearest) {
      continue;
    }
    symm_verts.append(BM_elem_index_get(*nearest));
  }

  std::sort(symm_verts.begin(), symm_verts.end());
  return symm_verts;
}

Vector<int> find_symm_verts(const Depsgraph &depsgraph,
                            const Object &object,
                            const int original_vert,
                            const float max_distance)
{
  const bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
  switch (pbvh.type()) {
    case bke::pbvh::Type::Mesh:
      return find_symm_verts_mesh(depsgraph, object, original_vert, max_distance);
    case bke::pbvh::Type::Grids:
      return find_symm_verts_grids(object, original_vert, max_distance);
    case bke::pbvh::Type::BMesh:
      return find_symm_verts_bmesh(object, original_vert, max_distance);
  }
  BLI_assert_unreachable();
  return {};
}

}  // namespace blender::ed::sculpt_paint

namespace blender::ed::sculpt_paint::expand {

/**
 * Geodesic: Initializes the falloff with geodesic distances from the given active vertex, taking
 * symmetry into account.
 */
static Array<float> geodesic_falloff_create(const Depsgraph &depsgraph,
                                            Object &ob,
                                            const IndexMask &initial_verts)
{
  const Mesh &mesh = *static_cast<const Mesh *>(ob.data);
  const Span<float3> vert_positions = bke::pbvh::vert_positions_eval(depsgraph, ob);
  const Span<int2> edges = mesh.edges();
  const OffsetIndices faces = mesh.faces();
  const Span<int> corner_verts = mesh.corner_verts();
  const Span<int> corner_edges = mesh.corner_edges();
  const bke::AttributeAccessor attributes = mesh.attributes();
  const VArraySpan<bool> hide_poly = *attributes.lookup<bool>(".hide_poly", bke::AttrDomain::Face);

  SculptSession &ss = *ob.sculpt;
  if (ss.edge_to_face_map.is_empty()) {
    ss.edge_to_face_map = bke::mesh::build_edge_to_face_map(
        faces, corner_edges, edges.size(), ss.edge_to_face_offsets, ss.edge_to_face_indices);
  }
  if (ss.vert_to_edge_map.is_empty()) {
    ss.vert_to_edge_map = bke::mesh::build_vert_to_edge_map(
        edges, mesh.verts_num, ss.vert_to_edge_offsets, ss.vert_to_edge_indices);
  }

  Set<int> verts;
  initial_verts.foreach_index([&](const int vert) { verts.add(vert); });

  return geodesic::distances_create(vert_positions,
                                    edges,
                                    faces,
                                    corner_verts,
                                    ss.vert_to_edge_map,
                                    ss.edge_to_face_map,
                                    hide_poly,
                                    verts,
                                    FLT_MAX);
}
static Array<float> geodesic_falloff_create(const Depsgraph &depsgraph,
                                            Object &ob,
                                            const int initial_vert)
{
  const Vector<int> symm_verts = find_symm_verts(depsgraph, ob, initial_vert);

  IndexMaskMemory memory;
  const IndexMask mask = IndexMask::from_indices(symm_verts.as_span(), memory);

  return geodesic_falloff_create(depsgraph, ob, mask);
}

/**
 * Topology: Initializes the falloff using a flood-fill operation,
 * increasing the falloff value by 1 when visiting a new vertex.
 */
static void calc_topology_falloff_from_verts(Object &ob,
                                             const IndexMask &initial_verts,
                                             MutableSpan<float> distances)
{
  SculptSession &ss = *ob.sculpt;
  const Mesh &mesh = *static_cast<const Mesh *>(ob.data);
  const GroupedSpan<int> vert_to_face_map = mesh.vert_to_face_map();
  const bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(ob);
  const int totvert = SCULPT_vertex_count_get(ob);

  switch (pbvh.type()) {
    case bke::pbvh::Type::Mesh: {
      flood_fill::FillDataMesh flood(totvert);
      initial_verts.foreach_index([&](const int vert) { flood.add_and_skip_initial(vert); });
      flood.execute(ob, vert_to_face_map, [&](const int from_vert, const int to_vert) {
        distances[to_vert] = distances[from_vert] + 1.0f;
        return true;
      });
      break;
    }
    case bke::pbvh::Type::Grids: {
      const SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;
      const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);

      flood_fill::FillDataGrids flood(totvert);
      initial_verts.foreach_index([&](const int vert) {
        const SubdivCCGCoord orig_coord = SubdivCCGCoord::from_index(key, vert);
        flood.add_and_skip_initial(orig_coord, vert);
      });
      flood.execute(
          ob,
          subdiv_ccg,
          [&](const SubdivCCGCoord from, const SubdivCCGCoord to, const bool is_duplicate) {
            const int from_vert = from.to_index(key);
            const int to_vert = to.to_index(key);
            if (is_duplicate) {
              distances[to_vert] = distances[from_vert];
            }
            else {
              distances[to_vert] = distances[from_vert] + 1.0f;
            }
            return true;
          });
      break;
    }
    case bke::pbvh::Type::BMesh: {
      BMesh &bm = *ss.bm;
      flood_fill::FillDataBMesh flood(totvert);
      initial_verts.foreach_index(
          [&](const int vert) { flood.add_and_skip_initial(BM_vert_at_index(&bm, vert), vert); });
      flood.execute(ob, [&](BMVert *from_bm_vert, BMVert *to_bm_vert) {
        const int from_vert = BM_elem_index_get(from_bm_vert);
        const int to_vert = BM_elem_index_get(to_bm_vert);
        distances[to_vert] = distances[from_vert] + 1.0f;
        return true;
      });
      break;
    }
  }
}

static Array<float> topology_falloff_create(const Depsgraph &depsgraph,
                                            Object &ob,
                                            const int initial_vert)
{
  const Vector<int> symm_verts = find_symm_verts(depsgraph, ob, initial_vert);

  IndexMaskMemory memory;
  const IndexMask mask = IndexMask::from_indices(symm_verts.as_span(), memory);

  Array<float> dists(SCULPT_vertex_count_get(ob), 0.0f);
  calc_topology_falloff_from_verts(ob, mask, dists);
  return dists;
}

/**
 * Normals: Flood-fills the mesh and reduces the falloff depending on the normal difference between
 * each vertex and the previous one.
 * This creates falloff patterns that follow and snap to the hard edges of the object.
 */
static Array<float> normals_falloff_create(const Depsgraph &depsgraph,
                                           Object &ob,
                                           const int vert,
                                           const float edge_sensitivity,
                                           const int blur_steps)
{
  SculptSession &ss = *ob.sculpt;
  const bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(ob);
  const int totvert = SCULPT_vertex_count_get(ob);
  Array<float> dists(totvert, 0.0f);
  Array<float> edge_factors(totvert, 1.0f);

  switch (pbvh.type()) {
    case bke::pbvh::Type::Mesh: {
      const Mesh &mesh = *static_cast<const Mesh *>(ob.data);
      const GroupedSpan<int> vert_to_face_map = mesh.vert_to_face_map();
      const Span<float3> vert_normals = bke::pbvh::vert_normals_eval(depsgraph, ob);

      const float3 orig_normal = vert_normals[vert];
      flood_fill::FillDataMesh flood(totvert);
      flood.add_initial(find_symm_verts(depsgraph, ob, vert));
      flood.execute(ob, vert_to_face_map, [&](const int from_vert, const int to_vert) {
        const float3 &from_normal = vert_normals[from_vert];
        const float3 &to_normal = vert_normals[to_vert];
        const float from_edge_factor = edge_factors[from_vert];
        const float dist = math::dot(orig_normal, to_normal) *
                           powf(from_edge_factor, edge_sensitivity);
        edge_factors[to_vert] = math::dot(to_normal, from_normal) * from_edge_factor;
        dists[to_vert] = std::clamp(dist, 0.0f, 1.0f);
        return true;
      });
      break;
    }
    case bke::pbvh::Type::Grids: {
      const SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;
      const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);
      const Span<float3> normals = subdiv_ccg.normals;
      const float3 orig_normal = normals[vert];
      flood_fill::FillDataGrids flood(totvert);
      flood.add_initial(key, find_symm_verts(depsgraph, ob, vert));
      flood.execute(
          ob,
          subdiv_ccg,
          [&](const SubdivCCGCoord from, const SubdivCCGCoord to, const bool is_duplicate) {
            const int from_vert = from.to_index(key);
            const int to_vert = to.to_index(key);
            if (is_duplicate) {
              edge_factors[to_vert] = edge_factors[from_vert];
              dists[to_vert] = dists[from_vert];
            }
            else {
              const float3 &from_normal = normals[from_vert];
              const float3 &to_normal = normals[to_vert];
              const float from_edge_factor = edge_factors[from_vert];
              const float dist = math::dot(orig_normal, to_normal) *
                                 powf(from_edge_factor, edge_sensitivity);
              edge_factors[to_vert] = math::dot(to_normal, from_normal) * from_edge_factor;
              dists[to_vert] = std::clamp(dist, 0.0f, 1.0f);
            }
            return true;
          });
      break;
    }
    case bke::pbvh::Type::BMesh: {
      flood_fill::FillDataBMesh flood(totvert);
      BMVert *orig_vert = BM_vert_at_index(ss.bm, vert);
      const float3 orig_normal = orig_vert->no;
      flood.add_initial(*ss.bm, find_symm_verts(depsgraph, ob, vert));
      flood.execute(ob, [&](BMVert *from_bm_vert, BMVert *to_bm_vert) {
        const float3 from_normal = from_bm_vert->no;
        const float3 to_normal = to_bm_vert->no;
        const int from_vert = BM_elem_index_get(from_bm_vert);
        const int to_vert = BM_elem_index_get(to_bm_vert);
        const float from_edge_factor = edge_factors[from_vert];
        const float dist = math::dot(orig_normal, to_normal) *
                           powf(from_edge_factor, edge_sensitivity);
        edge_factors[to_vert] = math::dot(to_normal, from_normal) * from_edge_factor;
        dists[to_vert] = std::clamp(dist, 0.0f, 1.0f);
        return true;
      });
      break;
    }
  }

  smooth::blur_geometry_data_array(ob, blur_steps, dists);

  for (int i = 0; i < totvert; i++) {
    dists[i] = 1.0 - dists[i];
  }

  return dists;
}

/**
 * Spherical: Initializes the falloff based on the distance from a vertex, taking symmetry into
 * account.
 */
static Array<float> spherical_falloff_create(const Depsgraph &depsgraph,
                                             const Object &object,
                                             const int vert)
{
  SculptSession &ss = *object.sculpt;
  const bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
  Array<float> dists(SCULPT_vertex_count_get(object));

  const Vector<int> symm_verts = find_symm_verts(depsgraph, object, vert);

  switch (pbvh.type()) {
    case bke::pbvh::Type::Mesh: {
      const Span<float3> positions = bke::pbvh::vert_positions_eval(depsgraph, object);

      Array<float3> locations(symm_verts.size());
      array_utils::gather(positions, symm_verts.as_span(), locations.as_mutable_span());

      threading::parallel_for(positions.index_range(), 1024, [&](const IndexRange range) {
        for (const int vert : range) {
          float dist = std::numeric_limits<float>::max();
          for (const float3 &location : locations) {
            dist = std::min(dist, math::distance(positions[vert], location));
          }
          dists[vert] = dist;
        }
      });
      break;
    }
    case bke::pbvh::Type::Grids: {
      SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;
      const Span<float3> positions = subdiv_ccg.positions;

      Array<float3> locations(symm_verts.size());
      for (const int i : symm_verts.index_range()) {
        locations[i] = positions[symm_verts[i]];
      }

      threading::parallel_for(positions.index_range(), 1024, [&](const IndexRange range) {
        for (const int vert : range) {
          float dist = std::numeric_limits<float>::max();
          for (const float3 &location : locations) {
            dist = std::min(dist, math::distance(positions[vert], location));
          }
          dists[vert] = dist;
        }
      });
      break;
    }
    case bke::pbvh::Type::BMesh: {
      BMesh &bm = *ss.bm;

      Array<float3> locations(symm_verts.size());
      for (const int i : symm_verts.index_range()) {
        locations[i] = BM_vert_at_index(&bm, symm_verts[i])->co;
      }

      threading::parallel_for(IndexRange(bm.totvert), 1024, [&](const IndexRange range) {
        for (const int vert : range) {
          float dist = std::numeric_limits<float>::max();
          for (const float3 &location : locations) {
            dist = std::min(dist,
                            math::distance(float3(BM_vert_at_index(&bm, vert)->co), location));
          }
          dists[vert] = dist;
        }
      });
      break;
    }
  }

  return dists;
}

/**
 * Boundary: This falloff mode uses the code from sculpt_boundary to initialize the closest mesh
 * boundary to a falloff value of 0. Then, it propagates that falloff to the rest of the mesh so it
 * stays parallel to the boundary, increasing the falloff value by 1 on each step.
 */
static Array<float> boundary_topology_falloff_create(const Depsgraph &depsgraph,
                                                     Object &ob,
                                                     const int inititial_vert)
{
  const Vector<int> symm_verts = find_symm_verts(depsgraph, ob, inititial_vert);

  BitVector<> boundary_verts(SCULPT_vertex_count_get(ob));
  for (const int vert : symm_verts) {
    if (std::unique_ptr<boundary::SculptBoundary> boundary = boundary::data_init(
            depsgraph, ob, nullptr, vert, FLT_MAX))
    {
      for (const int vert : boundary->verts) {
        boundary_verts[vert].set();
      }
    }
  }

  IndexMaskMemory memory;
  const IndexMask boundary_mask = IndexMask::from_bits(boundary_verts, memory);

  Array<float> dists(SCULPT_vertex_count_get(ob), 0.0f);
  calc_topology_falloff_from_verts(ob, boundary_mask, dists);
  return dists;
}

/**
 * Topology diagonals. This falloff is similar to topology, but it also considers the diagonals of
 * the base mesh faces when checking a vertex neighbor. For this reason, this is not implement
 * using the general flood-fill and sculpt neighbors accessors.
 */
static Array<float> diagonals_falloff_create(const Depsgraph &depsgraph,
                                             Object &ob,
                                             const int vert)
{
  const bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(ob);
  const Mesh &mesh = *static_cast<const Mesh *>(ob.data);
  const OffsetIndices<int> faces = mesh.faces();
  const Span<int> corner_verts = mesh.corner_verts();
  const GroupedSpan<int> vert_to_face_map = mesh.vert_to_face_map();
  const int totvert = SCULPT_vertex_count_get(ob);
  Array<float> dists(totvert, 0.0f);

  /* This algorithm uses mesh data (faces and loops), so this falloff type can't be initialized for
   * Multires. It also does not make sense to implement it for dyntopo as the result will be the
   * same as Topology falloff. */
  if (pbvh.type() != bke::pbvh::Type::Mesh) {
    return dists;
  }

  const Vector<int> symm_verts = find_symm_verts(depsgraph, ob, vert);

  /* Search and mask as visited the initial vertices using the enabled symmetry passes. */
  BitVector<> visited_verts(totvert);
  std::queue<int> queue;
  for (const int vert : symm_verts) {
    queue.push(vert);
    visited_verts[vert].set();
  }

  if (queue.empty()) {
    return dists;
  }

  /* Propagate the falloff increasing the value by 1 each time a new vertex is visited. */
  while (!queue.empty()) {
    const int next_vert = queue.front();
    queue.pop();

    for (const int face : vert_to_face_map[next_vert]) {
      for (const int vert : corner_verts.slice(faces[face])) {
        if (visited_verts[vert]) {
          continue;
        }
        dists[vert] = dists[next_vert] + 1.0f;
        visited_verts[vert].set();
        queue.push(vert);
      }
    }
  }

  return dists;
}

/**
 * Updates the max_falloff value for vertices in a #Cache based on the current values of
 * the falloff, skipping any invalid values initialized to FLT_MAX and not initialized components.
 */
static void update_max_vert_falloff_value(const Object &object, Cache &expand_cache)
{
  SculptSession &ss = *object.sculpt;
  expand_cache.max_vert_falloff = threading::parallel_reduce(
      IndexRange(SCULPT_vertex_count_get(object)),
      4096,
      std::numeric_limits<float>::lowest(),
      [&](const IndexRange range, float max) {
        for (const int vert : range) {
          if (expand_cache.vert_falloff[vert] == FLT_MAX) {
            continue;
          }
          if (!is_vert_in_active_component(ss, expand_cache, vert)) {
            continue;
          }
          max = std::max(max, expand_cache.vert_falloff[vert]);
        }
        return max;
      },
      [](const float a, const float b) { return std::max(a, b); });
}

/**
 * Updates the max_falloff value for faces in a Cache based on the current values of the
 * falloff, skipping any invalid values initialized to FLT_MAX and not initialized components.
 */
static void update_max_face_falloff_factor(const Object &object, Mesh &mesh, Cache &expand_cache)
{
  const OffsetIndices<int> faces = mesh.faces();
  const Span<int> corner_verts = mesh.corner_verts();
  expand_cache.max_face_falloff = threading::parallel_reduce(
      faces.index_range(),
      4096,
      std::numeric_limits<float>::lowest(),
      [&](const IndexRange range, float max) {
        for (const int face : range) {
          if (expand_cache.face_falloff[face] == FLT_MAX) {
            continue;
          }
          if (!is_face_in_active_component(object, faces, corner_verts, expand_cache, face)) {
            continue;
          }
          max = std::max(max, expand_cache.face_falloff[face]);
        }
        return max;
      },
      [](const float a, const float b) { return std::max(a, b); });
}

/**
 * Functions to get falloff values for faces from the values from the vertices. This is used for
 * expanding Face Sets. Depending on the data type of the #SculptSession, this needs to get the per
 * face falloff value from the connected vertices of each face or from the grids stored per loops
 * for each face.
 */
static void vert_to_face_falloff_grids(SculptSession &ss, Mesh *mesh, Cache &expand_cache)
{
  const OffsetIndices faces = mesh->faces();
  const CCGKey key = BKE_subdiv_ccg_key_top_level(*ss.subdiv_ccg);

  threading::parallel_for(faces.index_range(), 1024, [&](const IndexRange range) {
    for (const int face : range) {
      float accum = 0.0f;
      for (const int corner : faces[face]) {
        const int grid_loop_index = corner * key.grid_area;
        for (int g = 0; g < key.grid_area; g++) {
          accum += expand_cache.vert_falloff[grid_loop_index + g];
        }
      }
      expand_cache.face_falloff[face] = accum / (faces[face].size() * key.grid_area);
    }
  });
}

static void vert_to_face_falloff_mesh(Mesh *mesh, Cache &expand_cache)
{
  const OffsetIndices faces = mesh->faces();
  const Span<int> corner_verts = mesh->corner_verts();

  threading::parallel_for(faces.index_range(), 1024, [&](const IndexRange range) {
    for (const int face : range) {
      const Span<int> face_verts = corner_verts.slice(faces[face]);
      float accum = 0.0f;
      for (const int vert : face_verts) {
        accum += expand_cache.vert_falloff[vert];
      }
      expand_cache.face_falloff[face] = accum / face_verts.size();
    }
  });
}

/**
 * Main function to update the faces falloff from a already calculated vertex falloff.
 */
static void vert_to_face_falloff(Object &object, Mesh *mesh, Cache &expand_cache)
{
  BLI_assert(!expand_cache.vert_falloff.is_empty());

  expand_cache.face_falloff.reinitialize(mesh->faces_num);
  const bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);

  if (pbvh.type() == bke::pbvh::Type::Mesh) {
    vert_to_face_falloff_mesh(mesh, expand_cache);
  }
  else if (pbvh.type() == bke::pbvh::Type::Grids) {
    vert_to_face_falloff_grids(*object.sculpt, mesh, expand_cache);
  }
  else {
    BLI_assert(false);
  }
}

/* Recursions. These functions will generate new falloff values based on the state of the vertices
 * from the current Cache options and falloff values. */

/**
 * Geodesic recursion: Initializes falloff values using geodesic distances from the boundary of the
 * current vertices state.
 */
static void geodesics_from_state_boundary(const Depsgraph &depsgraph,
                                          Object &ob,
                                          Cache &expand_cache,
                                          const BitSpan enabled_verts)
{
  BLI_assert(bke::object::pbvh_get(ob)->type() == bke::pbvh::Type::Mesh);

  IndexMaskMemory memory;
  const IndexMask boundary_verts = boundary_from_enabled(ob, enabled_verts, false, memory);

  expand_cache.face_falloff = {};

  expand_cache.vert_falloff = geodesic_falloff_create(depsgraph, ob, boundary_verts);
}

/**
 * Topology recursion: Initializes falloff values using topology steps from the boundary of the
 * current vertices state, increasing the value by 1 each time a new vertex is visited.
 */
static void topology_from_state_boundary(Object &ob,
                                         Cache &expand_cache,
                                         const BitSpan enabled_verts)
{
  expand_cache.face_falloff = {};

  expand_cache.vert_falloff.reinitialize(SCULPT_vertex_count_get(ob));
  expand_cache.vert_falloff.fill(0);

  IndexMaskMemory memory;
  const IndexMask boundary_verts = boundary_from_enabled(ob, enabled_verts, false, memory);

  calc_topology_falloff_from_verts(ob, boundary_verts, expand_cache.vert_falloff);
}

/**
 * Main function to create a recursion step from the current #Cache state.
 */
static void resursion_step_add(const Depsgraph &depsgraph,
                               Object &ob,
                               Cache &expand_cache,
                               const RecursionType recursion_type)
{
  const bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(ob);
  if (pbvh.type() != bke::pbvh::Type::Mesh) {
    return;
  }

  const BitVector<> enabled_verts = enabled_state_to_bitmap(depsgraph, ob, expand_cache);

  /* Each time a new recursion step is created, reset the distortion strength. This is the expected
   * result from the recursion, as otherwise the new falloff will render with undesired distortion
   * from the beginning. */
  expand_cache.texture_distortion_strength = 0.0f;

  switch (recursion_type) {
    case RecursionType::Geodesic:
      geodesics_from_state_boundary(depsgraph, ob, expand_cache, enabled_verts);
      break;
    case RecursionType::Topology:
      topology_from_state_boundary(ob, expand_cache, enabled_verts);
      break;
  }

  update_max_vert_falloff_value(ob, expand_cache);
  if (expand_cache.target == TargetType::FaceSets) {
    Mesh &mesh = *static_cast<Mesh *>(ob.data);
    vert_to_face_falloff(ob, &mesh, expand_cache);
    update_max_face_falloff_factor(ob, mesh, expand_cache);
  }
}

/* Face Set Boundary falloff. */

/**
 * When internal falloff is set to true, the falloff will fill the active Face Set with a gradient,
 * otherwise the active Face Set will be filled with a constant falloff of 0.0f.
 */
static void init_from_face_set_boundary(const Depsgraph &depsgraph,
                                        Object &ob,
                                        Cache &expand_cache,
                                        const int active_face_set,
                                        const bool internal_falloff)
{
  const bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(ob);
  const int totvert = SCULPT_vertex_count_get(ob);

  Array<bool> vert_has_face_set(totvert);
  Array<bool> vert_has_unique_face_set(totvert);
  switch (pbvh.type()) {
    case bke::pbvh::Type::Mesh: {
      const Mesh &mesh = *static_cast<const Mesh *>(ob.data);
      const GroupedSpan<int> vert_to_face_map = mesh.vert_to_face_map();
      const bke::AttributeAccessor attributes = mesh.attributes();
      const VArraySpan face_sets = *attributes.lookup<int>(".sculpt_face_set",
                                                           bke::AttrDomain::Face);
      threading::parallel_for(IndexRange(totvert), 1024, [&](const IndexRange range) {
        for (const int vert : range) {
          vert_has_face_set[vert] = face_set::vert_has_face_set(
              vert_to_face_map, face_sets, vert, active_face_set);
          vert_has_unique_face_set[vert] = face_set::vert_has_unique_face_set(
              vert_to_face_map, face_sets, vert);
        }
      });
      break;
    }
    case bke::pbvh::Type::Grids: {
      const Mesh &base_mesh = *static_cast<const Mesh *>(ob.data);
      const OffsetIndices<int> faces = base_mesh.faces();
      const Span<int> corner_verts = base_mesh.corner_verts();
      const GroupedSpan<int> vert_to_face_map = base_mesh.vert_to_face_map();
      const bke::AttributeAccessor attributes = base_mesh.attributes();
      const VArraySpan face_sets = *attributes.lookup<int>(".sculpt_face_set",
                                                           bke::AttrDomain::Face);
      const SubdivCCG &subdiv_ccg = *ob.sculpt->subdiv_ccg;
      const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);
      threading::parallel_for(IndexRange(totvert), 1024, [&](const IndexRange range) {
        for (const int vert : range) {
          const SubdivCCGCoord coord = SubdivCCGCoord::from_index(key, vert);
          vert_has_face_set[vert] = face_set::vert_has_face_set(
              subdiv_ccg, face_sets, coord.grid_index, active_face_set);
          vert_has_unique_face_set[vert] = face_set::vert_has_unique_face_set(
              faces, corner_verts, vert_to_face_map, face_sets, subdiv_ccg, coord);
        }
      });
      break;
    }
    case bke::pbvh::Type::BMesh: {
      BMesh &bm = *ob.sculpt->bm;
      const int offset = CustomData_get_offset_named(&bm.pdata, CD_PROP_INT32, ".sculpt_face_set");
      BM_mesh_elem_table_ensure(&bm, BM_FACE);
      threading::parallel_for(IndexRange(totvert), 1024, [&](const IndexRange range) {
        for (const int vert : range) {
          const BMVert *bm_vert = BM_vert_at_index(&bm, vert);
          vert_has_face_set[vert] = face_set::vert_has_face_set(offset, *bm_vert, active_face_set);
          vert_has_unique_face_set[vert] = face_set::vert_has_unique_face_set(offset, *bm_vert);
        }
      });
      break;
    }
  }

  BitVector<> enabled_verts(totvert);
  for (int i = 0; i < totvert; i++) {
    if (!vert_has_unique_face_set[i]) {
      continue;
    }
    if (!vert_has_face_set[i]) {
      continue;
    }
    enabled_verts[i].set();
  }

  if (pbvh.type() == bke::pbvh::Type::Mesh) {
    geodesics_from_state_boundary(depsgraph, ob, expand_cache, enabled_verts);
  }
  else {
    topology_from_state_boundary(ob, expand_cache, enabled_verts);
  }

  if (internal_falloff) {
    for (int i = 0; i < totvert; i++) {
      if (!(vert_has_face_set[i] && vert_has_unique_face_set[i])) {
        continue;
      }
      expand_cache.vert_falloff[i] *= -1.0f;
    }

    float min_factor = FLT_MAX;
    for (int i = 0; i < totvert; i++) {
      min_factor = min_ff(expand_cache.vert_falloff[i], min_factor);
    }

    const float additional_falloff = fabsf(min_factor);
    for (int i = 0; i < totvert; i++) {
      expand_cache.vert_falloff[i] += additional_falloff;
    }
  }
  else {
    for (int i = 0; i < totvert; i++) {
      if (!vert_has_face_set[i]) {
        continue;
      }
      expand_cache.vert_falloff[i] = 0.0f;
    }
  }
}

/**
 * Main function to initialize new falloff values in a #Cache given an initial vertex and a
 * falloff type.
 */
static void calc_falloff_from_vert_and_symmetry(const Depsgraph &depsgraph,
                                                Cache &expand_cache,
                                                Object &ob,
                                                const int vert,
                                                FalloffType falloff_type)
{
  expand_cache.falloff_type = falloff_type;

  const bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(ob);
  const bool has_topology_info = pbvh.type() == bke::pbvh::Type::Mesh;

  switch (falloff_type) {
    case FalloffType::Geodesic:
      expand_cache.vert_falloff = has_topology_info ?
                                      geodesic_falloff_create(depsgraph, ob, vert) :
                                      spherical_falloff_create(depsgraph, ob, vert);
      break;
    case FalloffType::Topology:
      expand_cache.vert_falloff = topology_falloff_create(depsgraph, ob, vert);
      break;
    case FalloffType::TopologyNormals:
      expand_cache.vert_falloff = has_topology_info ?
                                      diagonals_falloff_create(depsgraph, ob, vert) :
                                      topology_falloff_create(depsgraph, ob, vert);
      break;
    case FalloffType::Normals:
      expand_cache.vert_falloff = normals_falloff_create(
          depsgraph,
          ob,
          vert,
          SCULPT_EXPAND_NORMALS_FALLOFF_EDGE_SENSITIVITY,
          expand_cache.normal_falloff_blur_steps);
      break;
    case FalloffType::Sphere:
      expand_cache.vert_falloff = spherical_falloff_create(depsgraph, ob, vert);
      break;
    case FalloffType::BoundaryTopology:
      expand_cache.vert_falloff = boundary_topology_falloff_create(depsgraph, ob, vert);
      break;
    case FalloffType::BoundaryFaceSet:
      init_from_face_set_boundary(
          depsgraph, ob, expand_cache, expand_cache.initial_active_face_set, true);
      break;
    case FalloffType::ActiveFaceSet:
      init_from_face_set_boundary(
          depsgraph, ob, expand_cache, expand_cache.initial_active_face_set, false);
      break;
  }

  /* Update max falloff values and propagate to base mesh faces if needed. */
  update_max_vert_falloff_value(ob, expand_cache);
  if (expand_cache.target == TargetType::FaceSets) {
    Mesh &mesh = *static_cast<Mesh *>(ob.data);
    vert_to_face_falloff(ob, &mesh, expand_cache);
    update_max_face_falloff_factor(ob, mesh, expand_cache);
  }
}

/**
 * Adds to the snapping Face Set `gset` all Face Sets which contain all enabled vertices for the
 * current #Cache state. This improves the usability of snapping, as already enabled
 * elements won't switch their state when toggling snapping with the modal key-map.
 */
static void snap_init_from_enabled(const Depsgraph &depsgraph,
                                   const Object &object,
                                   Cache &expand_cache)
{
  const bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
  if (pbvh.type() != bke::pbvh::Type::Mesh) {
    return;
  }
  const Mesh &mesh = *static_cast<const Mesh *>(object.data);
  const OffsetIndices<int> faces = mesh.faces();
  const Span<int> corner_verts = mesh.corner_verts();
  /* Make sure this code runs with snapping and invert disabled. This simplifies the code and
   * prevents using this function with snapping already enabled. */
  const bool prev_snap_state = expand_cache.snap;
  const bool prev_invert_state = expand_cache.invert;
  expand_cache.snap = false;
  expand_cache.invert = false;

  const BitVector<> enabled_verts = enabled_state_to_bitmap(depsgraph, object, expand_cache);

  for (const int i : faces.index_range()) {
    const int face_set = expand_cache.original_face_sets[i];
    expand_cache.snap_enabled_face_sets->add(face_set);
  }

  for (const int i : faces.index_range()) {
    const Span<int> face_verts = corner_verts.slice(faces[i]);
    const bool any_disabled = std::any_of(face_verts.begin(),
                                          face_verts.end(),
                                          [&](const int vert) { return !enabled_verts[vert]; });
    if (any_disabled) {
      const int face_set = expand_cache.original_face_sets[i];
      expand_cache.snap_enabled_face_sets->remove(face_set);
    }
  }

  expand_cache.snap = prev_snap_state;
  expand_cache.invert = prev_invert_state;
}

static void expand_cache_free(SculptSession &ss)
{
  MEM_delete<Cache>(ss.expand_cache);
  /* Needs to be set to nullptr as the paint cursor relies on checking this pointer detecting if an
   * expand operation is running. */
  ss.expand_cache = nullptr;
}

/**
 * Functions to restore the original state from the #Cache when canceling the operator.
 */
static void restore_face_set_data(Object &object, Cache &expand_cache)
{
  bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
  bke::SpanAttributeWriter<int> face_sets = face_set::ensure_face_sets_mesh(
      *static_cast<Mesh *>(object.data));
  face_sets.span.copy_from(expand_cache.original_face_sets);
  face_sets.finish();

  IndexMaskMemory memory;
  const IndexMask node_mask = bke::pbvh::all_leaf_nodes(pbvh, memory);
  pbvh.tag_face_sets_changed(node_mask);
}

static void restore_color_data(Object &ob, Cache &expand_cache)
{
  bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(ob);
  MutableSpan<bke::pbvh::MeshNode> nodes = pbvh.nodes<bke::pbvh::MeshNode>();
  Mesh &mesh = *static_cast<Mesh *>(ob.data);
  IndexMaskMemory memory;
  const IndexMask node_mask = bke::pbvh::all_leaf_nodes(pbvh, memory);

  const OffsetIndices<int> faces = mesh.faces();
  const Span<int> corner_verts = mesh.corner_verts();
  const GroupedSpan<int> vert_to_face_map = mesh.vert_to_face_map();
  bke::GSpanAttributeWriter color_attribute = color::active_color_attribute_for_write(mesh);
  node_mask.foreach_index([&](const int i) {
    for (const int vert : nodes[i].verts()) {
      color::color_vert_set(faces,
                            corner_verts,
                            vert_to_face_map,
                            color_attribute.domain,
                            vert,
                            expand_cache.original_colors[vert],
                            color_attribute.span);
    }
  });
  pbvh.tag_attribute_changed(node_mask, mesh.active_color_attribute);
  color_attribute.finish();
}

static void write_mask_data(Object &object, const Span<float> mask)
{
  SculptSession &ss = *object.sculpt;
  bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
  IndexMaskMemory memory;
  const IndexMask node_mask = bke::pbvh::all_leaf_nodes(pbvh, memory);
  switch (pbvh.type()) {
    case bke::pbvh::Type::Mesh: {
      Mesh &mesh = *static_cast<Mesh *>(object.data);
      bke::MutableAttributeAccessor attributes = mesh.attributes_for_write();
      attributes.remove(".sculpt_mask");
      attributes.add<float>(".sculpt_mask",
                            bke::AttrDomain::Point,
                            bke::AttributeInitVArray(VArray<float>::from_span(mask)));
      bke::pbvh::update_mask_mesh(mesh, node_mask, pbvh);
      break;
    }
    case bke::pbvh::Type::BMesh: {
      BMesh &bm = *ss.bm;
      const int offset = CustomData_get_offset_named(&bm.vdata, CD_PROP_FLOAT, ".sculpt_mask");
      vert_random_access_ensure(object);
      for (const int i : mask.index_range()) {
        BM_ELEM_CD_SET_FLOAT(BM_vert_at_index(&bm, i), offset, mask[i]);
      }
      bke::pbvh::update_mask_bmesh(bm, node_mask, pbvh);
      break;
    }
    case bke::pbvh::Type::Grids: {
      SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;
      subdiv_ccg.masks.as_mutable_span().copy_from(mask);
      bke::pbvh::update_mask_grids(subdiv_ccg, node_mask, pbvh);
      break;
    }
  }
  pbvh.tag_masks_changed(node_mask);
}

/* Main function to restore the original state of the data to how it was before starting the expand
 * operation. */
static void restore_original_state(bContext *C, Object &ob, Cache &expand_cache)
{
  switch (expand_cache.target) {
    case TargetType::Mask:
      write_mask_data(ob, expand_cache.original_mask);
      flush_update_step(C, UpdateType::Mask);
      flush_update_done(C, ob, UpdateType::Mask);
      SCULPT_tag_update_overlays(C);
      break;
    case TargetType::FaceSets:
      restore_face_set_data(ob, expand_cache);
      flush_update_step(C, UpdateType::FaceSet);
      flush_update_done(C, ob, UpdateType::FaceSet);
      SCULPT_tag_update_overlays(C);
      break;
    case TargetType::Colors:
      restore_color_data(ob, expand_cache);
      flush_update_step(C, UpdateType::Color);
      flush_update_done(C, ob, UpdateType::Color);
      break;
  }
}

/**
 * Cancel operator callback.
 */
static void sculpt_expand_cancel(bContext *C, wmOperator * /*op*/)
{
  Object &ob = *CTX_data_active_object(C);
  SculptSession &ss = *ob.sculpt;

  restore_original_state(C, ob, *ss.expand_cache);

  undo::push_end(ob);
  expand_cache_free(ss);

  ED_workspace_status_text(C, nullptr);
}

/* Functions to update the sculpt mesh data. */

static void calc_new_mask_mesh(const SculptSession &ss,
                               const Span<float3> positions,
                               const BitSpan enabled_verts,
                               const Span<int> verts,
                               const MutableSpan<float> mask)
{
  const Cache &expand_cache = *ss.expand_cache;

  for (const int i : verts.index_range()) {
    const int vert = verts[i];
    const bool enabled = enabled_verts[vert];

    if (expand_cache.check_islands && !is_vert_in_active_component(ss, expand_cache, vert)) {
      continue;
    }

    if (enabled) {
      mask[i] = gradient_value_get(ss, expand_cache, positions[vert], vert);
    }
    else {
      mask[i] = 0.0f;
    }

    if (expand_cache.preserve) {
      if (expand_cache.invert) {
        mask[i] = min_ff(mask[i], expand_cache.original_mask[vert]);
      }
      else {
        mask[i] = max_ff(mask[i], expand_cache.original_mask[vert]);
      }
    }
  }

  mask::clamp_mask(mask);
}

static bool update_mask_grids(const SculptSession &ss,
                              const BitSpan enabled_verts,
                              bke::pbvh::GridsNode &node,
                              SubdivCCG &subdiv_ccg)
{
  const Cache &expand_cache = *ss.expand_cache;
  const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);
  const Span<float3> positions = subdiv_ccg.positions;
  MutableSpan<float> masks = subdiv_ccg.masks;

  bool any_changed = false;
  for (const int grid : node.grids()) {
    for (const int vert : bke::ccg::grid_range(key, grid)) {
      const float initial_mask = masks[vert];

      if (expand_cache.check_islands && !is_vert_in_active_component(ss, expand_cache, vert)) {
        continue;
      }

      float new_mask;

      if (enabled_verts[vert]) {
        new_mask = gradient_value_get(ss, expand_cache, positions[vert], vert);
      }
      else {
        new_mask = 0.0f;
      }

      if (expand_cache.preserve) {
        if (expand_cache.invert) {
          new_mask = min_ff(new_mask, expand_cache.original_mask[vert]);
        }
        else {
          new_mask = max_ff(new_mask, expand_cache.original_mask[vert]);
        }
      }

      if (new_mask == initial_mask) {
        continue;
      }

      masks[vert] = clamp_f(new_mask, 0.0f, 1.0f);
      any_changed = true;
    }
  }
  if (any_changed) {
    bke::pbvh::node_update_mask_grids(key, masks, node);
  }
  return any_changed;
}

static bool update_mask_bmesh(SculptSession &ss,
                              const BitSpan enabled_verts,
                              const int mask_offset,
                              bke::pbvh::BMeshNode *node)
{
  const Cache &expand_cache = *ss.expand_cache;

  bool any_changed = false;
  for (BMVert *vert : BKE_pbvh_bmesh_node_unique_verts(node)) {
    const int vert_index = BM_elem_index_get(vert);
    const float initial_mask = BM_ELEM_CD_GET_FLOAT(vert, mask_offset);

    if (expand_cache.check_islands && !is_vert_in_active_component(ss, expand_cache, vert_index)) {
      continue;
    }

    float new_mask;

    if (enabled_verts[vert_index]) {
      new_mask = gradient_value_get(ss, expand_cache, vert->co, vert_index);
    }
    else {
      new_mask = 0.0f;
    }

    if (expand_cache.preserve) {
      if (expand_cache.invert) {
        new_mask = min_ff(new_mask, expand_cache.original_mask[BM_elem_index_get(vert)]);
      }
      else {
        new_mask = max_ff(new_mask, expand_cache.original_mask[BM_elem_index_get(vert)]);
      }
    }

    if (new_mask == initial_mask) {
      continue;
    }

    BM_ELEM_CD_SET_FLOAT(vert, mask_offset, clamp_f(new_mask, 0.0f, 1.0f));
    any_changed = true;
  }
  if (any_changed) {
    bke::pbvh::node_update_mask_bmesh(mask_offset, *node);
  }
  return any_changed;
}

/**
 * Update Face Set data. Not multi-threaded per node as nodes don't contain face arrays.
 */
static void face_sets_update(Object &object, Cache &expand_cache)
{
  Mesh &mesh = *static_cast<Mesh *>(object.data);
  bke::SpanAttributeWriter<int> face_sets = face_set::ensure_face_sets_mesh(mesh);
  const OffsetIndices<int> faces = mesh.faces();
  const Span<int> corner_verts = mesh.corner_verts();
  const bke::AttributeAccessor attributes = mesh.attributes();
  const VArraySpan<bool> hide_poly = *attributes.lookup<bool>(".hide_poly", bke::AttrDomain::Face);
  for (const int f : face_sets.span.index_range()) {
    const bool enabled = face_state_get(
        object, faces, corner_verts, hide_poly, face_sets.span, expand_cache, f);
    if (!enabled) {
      continue;
    }
    if (expand_cache.preserve) {
      face_sets.span[f] += expand_cache.next_face_set;
    }
    else {
      face_sets.span[f] = expand_cache.next_face_set;
    }
  }

  face_sets.finish();

  bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
  pbvh.tag_face_sets_changed(expand_cache.node_mask);
}

/**
 * Callback to update vertex colors per bke::pbvh::Tree node.
 */
static bool colors_update_task(const Depsgraph &depsgraph,
                               Object &object,
                               const Span<float3> vert_positions,
                               const OffsetIndices<int> faces,
                               const Span<int> corner_verts,
                               const GroupedSpan<int> vert_to_face_map,
                               const Span<bool> hide_vert,
                               const Span<float> mask,
                               bke::pbvh::MeshNode *node,
                               bke::GSpanAttributeWriter &color_attribute)
{
  const SculptSession &ss = *object.sculpt;
  const Cache &expand_cache = *ss.expand_cache;

  const BitVector<> enabled_verts = enabled_state_to_bitmap(depsgraph, object, expand_cache);

  bool any_changed = false;
  const Span<int> verts = node->verts();
  for (const int i : verts.index_range()) {
    const int vert = verts[i];
    if (!hide_vert.is_empty() && hide_vert[vert]) {
      continue;
    }

    float4 initial_color = color::color_vert_get(
        faces, corner_verts, vert_to_face_map, color_attribute.span, color_attribute.domain, vert);

    float fade;

    if (enabled_verts[vert]) {
      fade = gradient_value_get(ss, expand_cache, vert_positions[vert], vert);
    }
    else {
      fade = 0.0f;
    }

    if (!mask.is_empty()) {
      fade *= 1.0f - mask[vert];
    }
    fade = clamp_f(fade, 0.0f, 1.0f);

    float4 final_color;
    float4 final_fill_color;
    mul_v4_v4fl(final_fill_color, expand_cache.fill_color, fade);
    IMB_blend_color_float(final_color,
                          expand_cache.original_colors[vert],
                          final_fill_color,
                          IMB_BlendMode(expand_cache.blend_mode));

    if (initial_color == final_color) {
      continue;
    }

    color::color_vert_set(faces,
                          corner_verts,
                          vert_to_face_map,
                          color_attribute.domain,
                          vert,
                          final_color,
                          color_attribute.span);

    any_changed = true;
  }
  return any_changed;
}

/* Store the original mesh data state in the expand cache. */
static void original_state_store(Object &ob, Cache &expand_cache)
{
  Mesh &mesh = *static_cast<Mesh *>(ob.data);
  const int totvert = SCULPT_vertex_count_get(ob);

  face_set::create_face_sets_mesh(ob);

  /* Face Sets are always stored as they are needed for snapping. */
  expand_cache.initial_face_sets = face_set::duplicate_face_sets(mesh);
  expand_cache.original_face_sets = face_set::duplicate_face_sets(mesh);

  if (expand_cache.target == TargetType::Mask) {
    expand_cache.original_mask = mask::duplicate_mask(ob);
  }

  if (expand_cache.target == TargetType::Colors) {
    const Mesh &mesh = *static_cast<const Mesh *>(ob.data);
    const OffsetIndices<int> faces = mesh.faces();
    const Span<int> corner_verts = mesh.corner_verts();
    const GroupedSpan<int> vert_to_face_map = mesh.vert_to_face_map();
    const bke::GAttributeReader color_attribute = color::active_color_attribute(mesh);
    const GVArraySpan colors = *color_attribute;

    expand_cache.original_colors = Array<float4>(totvert);
    for (int i = 0; i < totvert; i++) {
      expand_cache.original_colors[i] = color::color_vert_get(
          faces, corner_verts, vert_to_face_map, colors, color_attribute.domain, i);
    }
  }
}

/**
 * Restore the state of the Face Sets before a new update.
 */
static void face_sets_restore(Object &object, Cache &expand_cache)
{
  Mesh &mesh = *static_cast<Mesh *>(object.data);
  const OffsetIndices<int> faces = mesh.faces();
  const Span<int> corner_verts = mesh.corner_verts();
  bke::SpanAttributeWriter<int> face_sets = face_set::ensure_face_sets_mesh(mesh);
  for (const int i : faces.index_range()) {
    if (expand_cache.original_face_sets[i] <= 0) {
      /* Do not modify hidden Face Sets, even when restoring the IDs state. */
      continue;
    }
    if (!is_face_in_active_component(object, faces, corner_verts, expand_cache, i)) {
      continue;
    }
    face_sets.span[i] = expand_cache.initial_face_sets[i];
  }
  face_sets.finish();
}

static void update_for_vert(bContext *C, Object &ob, const std::optional<int> vertex)
{
  const Depsgraph &depsgraph = *CTX_data_depsgraph_pointer(C);
  SculptSession &ss = *ob.sculpt;
  bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(ob);
  Cache &expand_cache = *ss.expand_cache;

  /* Update the active factor in the cache. */
  if (!vertex) {
    /* This means that the cursor is not over the mesh, so a valid active falloff can't be
     * determined. In this situations, don't evaluate enabled states and default all vertices in
     * connected components to enabled. */
    expand_cache.active_falloff = expand_cache.max_vert_falloff;
    expand_cache.all_enabled = true;
  }
  else {
    expand_cache.active_falloff = expand_cache.vert_falloff[*vertex];
    expand_cache.all_enabled = false;
  }

  if (expand_cache.target == TargetType::FaceSets) {
    /* Face sets needs to be restored their initial state on each iteration as the overwrite
     * existing data. */
    face_sets_restore(ob, expand_cache);
  }

  const IndexMask &node_mask = expand_cache.node_mask;

  const BitVector<> enabled_verts = enabled_state_to_bitmap(depsgraph, ob, expand_cache);

  switch (expand_cache.target) {
    case TargetType::Mask: {
      switch (pbvh.type()) {
        case bke::pbvh::Type::Mesh: {
          const Span<float3> positions = bke::pbvh::vert_positions_eval(depsgraph, ob);
          mask::update_mask_mesh(
              depsgraph, ob, node_mask, [&](const MutableSpan<float> mask, const Span<int> verts) {
                calc_new_mask_mesh(ss, positions, enabled_verts, verts, mask);
              });
          break;
        }
        case bke::pbvh::Type::Grids: {
          Array<bool> node_changed(node_mask.min_array_size(), false);

          MutableSpan<bke::pbvh::GridsNode> nodes = pbvh.nodes<bke::pbvh::GridsNode>();
          node_mask.foreach_index(GrainSize(1), [&](const int i) {
            node_changed[i] = update_mask_grids(ss, enabled_verts, nodes[i], *ss.subdiv_ccg);
          });

          IndexMaskMemory memory;
          pbvh.tag_masks_changed(IndexMask::from_bools(node_changed, memory));
          break;
        }
        case bke::pbvh::Type::BMesh: {
          const int mask_offset = CustomData_get_offset_named(
              &ss.bm->vdata, CD_PROP_FLOAT, ".sculpt_mask");
          MutableSpan<bke::pbvh::BMeshNode> nodes = pbvh.nodes<bke::pbvh::BMeshNode>();

          Array<bool> node_changed(node_mask.min_array_size(), false);
          node_mask.foreach_index(GrainSize(1), [&](const int i) {
            node_changed[i] = update_mask_bmesh(ss, enabled_verts, mask_offset, &nodes[i]);
          });

          IndexMaskMemory memory;
          pbvh.tag_masks_changed(IndexMask::from_bools(node_changed, memory));
          break;
        }
      }
      flush_update_step(C, UpdateType::Mask);
      break;
    }
    case TargetType::FaceSets:
      face_sets_update(ob, expand_cache);
      flush_update_step(C, UpdateType::FaceSet);
      break;
    case TargetType::Colors: {
      Mesh &mesh = *static_cast<Mesh *>(ob.data);
      const Span<float3> vert_positions = bke::pbvh::vert_positions_eval(depsgraph, ob);
      const OffsetIndices<int> faces = mesh.faces();
      const Span<int> corner_verts = mesh.corner_verts();
      const GroupedSpan<int> vert_to_face_map = mesh.vert_to_face_map();
      const bke::AttributeAccessor attributes = mesh.attributes();
      const VArraySpan hide_vert = *attributes.lookup<bool>(".hide_vert", bke::AttrDomain::Point);
      const VArraySpan mask = *attributes.lookup<float>(".sculpt_mask", bke::AttrDomain::Point);
      bke::GSpanAttributeWriter color_attribute = color::active_color_attribute_for_write(mesh);

      Array<bool> node_changed(node_mask.min_array_size(), false);

      MutableSpan<bke::pbvh::MeshNode> nodes = pbvh.nodes<bke::pbvh::MeshNode>();
      node_mask.foreach_index(GrainSize(1), [&](const int i) {
        node_changed[i] = colors_update_task(depsgraph,
                                             ob,
                                             vert_positions,
                                             faces,
                                             corner_verts,
                                             vert_to_face_map,
                                             hide_vert,
                                             mask,
                                             &nodes[i],
                                             color_attribute);
      });

      IndexMaskMemory memory;
      pbvh.tag_attribute_changed(IndexMask::from_bools(node_changed, memory),
                                 mesh.active_color_attribute);

      color_attribute.finish();
      flush_update_step(C, UpdateType::Color);
      break;
    }
  }
}

/**
 * Updates the #SculptSession cursor data and gets the active vertex
 * if the cursor is over the mesh.
 */
static std::optional<int> target_vert_update_and_get(bContext *C, Object &ob, const float mval[2])
{
  SculptSession &ss = *ob.sculpt;
  CursorGeometryInfo cgi;
  if (cursor_geometry_info_update(C, &cgi, mval, false)) {
    return ss.active_vert_index();
  }
  return std::nullopt;
}

/**
 * Moves the sculpt pivot to the average point of the boundary enabled vertices of the current
 * expand state. Take symmetry and active components into account.
 */
static void reposition_pivot(bContext *C, Object &ob, Cache &expand_cache)
{
  SculptSession &ss = *ob.sculpt;
  const char symm = SCULPT_mesh_symmetry_xyz_get(ob);
  const Depsgraph &depsgraph = *CTX_data_depsgraph_pointer(C);

  const bool initial_invert_state = expand_cache.invert;
  expand_cache.invert = false;
  const BitVector<> enabled_verts = enabled_state_to_bitmap(depsgraph, ob, expand_cache);

  /* For boundary topology, position the pivot using only the boundary of the enabled vertices,
   * without taking mesh boundary into account. This allows to create deformations like bending the
   * mesh from the boundary of the mask that was just created. */
  const float use_mesh_boundary = expand_cache.falloff_type != FalloffType::BoundaryTopology;

  IndexMaskMemory memory;
  const IndexMask boundary_verts = boundary_from_enabled(
      ob, enabled_verts, use_mesh_boundary, memory);

  /* Ignore invert state, as this is the expected behavior in most cases and mask are created in
   * inverted state by default. */
  expand_cache.invert = initial_invert_state;

  double3 average(0);
  int total = 0;
  switch (bke::object::pbvh_get(ob)->type()) {
    case bke::pbvh::Type::Mesh: {
      const Span<float3> positions = bke::pbvh::vert_positions_eval(depsgraph, ob);
      const float3 expand_init_co = positions[expand_cache.initial_active_vert];
      boundary_verts.foreach_index([&](const int vert) {
        if (!is_vert_in_active_component(ss, expand_cache, vert)) {
          return;
        }
        const float3 &position = positions[vert];
        if (!SCULPT_check_vertex_pivot_symmetry(position, expand_init_co, symm)) {
          return;
        }
        average += double3(position);
        total++;
      });
      break;
    }
    case bke::pbvh::Type::Grids: {
      const SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;
      const Span<float3> positions = subdiv_ccg.positions;
      const float3 expand_init_co = positions[expand_cache.initial_active_vert];
      boundary_verts.foreach_index([&](const int vert) {
        if (!is_vert_in_active_component(ss, expand_cache, vert)) {
          return;
        }
        const float3 position = positions[vert];
        if (!SCULPT_check_vertex_pivot_symmetry(position, expand_init_co, symm)) {
          return;
        }
        average += double3(position);
        total++;
      });
      break;
    }
    case bke::pbvh::Type::BMesh: {
      BMesh &bm = *ss.bm;
      const float3 expand_init_co = BM_vert_at_index(&bm, expand_cache.initial_active_vert)->co;
      boundary_verts.foreach_index([&](const int vert) {
        if (!is_vert_in_active_component(ss, expand_cache, vert)) {
          return;
        }
        const float3 position = BM_vert_at_index(&bm, vert)->co;
        if (!SCULPT_check_vertex_pivot_symmetry(position, expand_init_co, symm)) {
          return;
        }
        average += double3(position);
        total++;
      });
      break;
    }
  }

  if (total > 0) {
    ss.pivot_pos = float3(average / total);
  }

  WM_event_add_notifier(C, NC_GEOM | ND_SELECT, ob.data);
}

static void finish(bContext *C)
{
  Object &ob = *CTX_data_active_object(C);
  SculptSession &ss = *ob.sculpt;
  undo::push_end(ob);

  switch (ss.expand_cache->target) {
    case TargetType::Mask:
      flush_update_done(C, ob, UpdateType::Mask);
      break;
    case TargetType::FaceSets:
      flush_update_done(C, ob, UpdateType::FaceSet);
      break;
    case TargetType::Colors:
      flush_update_done(C, ob, UpdateType::Color);
      break;
  }

  expand_cache_free(ss);
  ED_workspace_status_text(C, nullptr);
}

/**
 * Finds and stores in the #Cache the sculpt connected component index for each symmetry
 * pass needed for expand.
 */
static void find_active_connected_components_from_vert(const Depsgraph &depsgraph,
                                                       Object &ob,
                                                       Cache &expand_cache,
                                                       const int initial_vertex)
{
  SculptSession &ss = *ob.sculpt;
  for (int i = 0; i < EXPAND_SYMM_AREAS; i++) {
    expand_cache.active_connected_islands[i] = EXPAND_ACTIVE_COMPONENT_NONE;
  }

  const ePaintSymmetryFlags symm = SCULPT_mesh_symmetry_xyz_get(ob);

  const Vector<int> symm_verts = find_symm_verts(depsgraph, ob, initial_vertex);

  int valid_index = 0;
  for (int symm_it = 0; symm_it <= symm; symm_it++) {
    if (!is_symmetry_iteration_valid(symm_it, symm)) {
      continue;
    }
    expand_cache.active_connected_islands[symm_it] = islands::vert_id_get(ss,
                                                                          symm_verts[valid_index]);
    valid_index++;
  }
}

/**
 * Stores the active vertex, Face Set and mouse coordinates in the #Cache based on the
 * current cursor position.
 */
static bool set_initial_components_for_mouse(bContext *C,
                                             Object &ob,
                                             Cache &expand_cache,
                                             const float mval[2])
{
  SculptSession &ss = *ob.sculpt;
  const Depsgraph &depsgraph = *CTX_data_depsgraph_pointer(C);

  std::optional<int> initial_vert = target_vert_update_and_get(C, ob, mval);
  if (!initial_vert) {
    /* Cursor not over the mesh, for creating valid initial falloffs, fall back to the last active
     * vertex in the sculpt session. */
    const int last_active_vert_index = ss.last_active_vert_index();
    /* It still may be the case that there is no last active vert in rare circumstances for
     * everyday usage.
     * (i.e. if the cursor has never been over the mesh at all. A solution to both this problem
     * and needing to store this data is to figure out which is the nearest vertex to the current
     * cursor position */
    if (last_active_vert_index == -1) {
      return false;
    }
    initial_vert = last_active_vert_index;
  }

  copy_v2_v2(ss.expand_cache->initial_mouse, mval);
  expand_cache.initial_active_vert = *initial_vert;
  expand_cache.initial_active_face_set = face_set::active_face_set_get(ob);

  if (expand_cache.next_face_set == SCULPT_FACE_SET_NONE) {
    /* Only set the next face set once, otherwise this ID will constantly update to a new one each
     * time this function is called for using a new initial vertex from a different cursor
     * position. */
    if (expand_cache.modify_active_face_set) {
      expand_cache.next_face_set = face_set::active_face_set_get(ob);
    }
    else {
      expand_cache.next_face_set = face_set::find_next_available_id(ob);
    }
  }

  /* The new mouse position can be over a different connected component, so this needs to be
   * updated. */
  find_active_connected_components_from_vert(depsgraph, ob, expand_cache, *initial_vert);
  return true;
}

/**
 * Displaces the initial mouse coordinates using the new mouse position to get a new active vertex.
 * After that, initializes a new falloff of the same type with the new active vertex.
 */
static void move_propagation_origin(bContext *C,
                                    Object &ob,
                                    const wmEvent *event,
                                    Cache &expand_cache)
{
  const Depsgraph &depsgraph = *CTX_data_depsgraph_pointer(C);
  const float mval_fl[2] = {float(event->mval[0]), float(event->mval[1])};
  float move_disp[2];
  sub_v2_v2v2(move_disp, mval_fl, expand_cache.initial_mouse_move);

  float new_mval[2];
  add_v2_v2v2(new_mval, move_disp, expand_cache.original_mouse_move);

  set_initial_components_for_mouse(C, ob, expand_cache, new_mval);
  calc_falloff_from_vert_and_symmetry(depsgraph,
                                      expand_cache,
                                      ob,
                                      expand_cache.initial_active_vert,
                                      expand_cache.move_preview_falloff_type);
}

/**
 * Ensures that the #SculptSession contains the required data needed for Expand.
 */
static void ensure_sculptsession_data(Object &ob)
{
  SculptSession &ss = *ob.sculpt;
  islands::ensure_cache(ob);
  vert_random_access_ensure(ob);
  boundary::ensure_boundary_info(ob);
  if (!ss.tex_pool) {
    ss.tex_pool = BKE_image_pool_new();
  }
}

/**
 * Returns the active Face Sets ID from the enabled face or grid in the #SculptSession.
 */
static int active_face_set_id_get(Object &object, Cache &expand_cache)
{
  SculptSession &ss = *object.sculpt;
  switch (bke::object::pbvh_get(object)->type()) {
    case bke::pbvh::Type::Mesh:
      if (!ss.active_face_index) {
        return SCULPT_FACE_SET_NONE;
      }
      return expand_cache.original_face_sets[*ss.active_face_index];
    case bke::pbvh::Type::Grids: {
      if (!ss.active_grid_index) {
        return SCULPT_FACE_SET_NONE;
      }
      const int face_index = BKE_subdiv_ccg_grid_to_face_index(*ss.subdiv_ccg,
                                                               *ss.active_grid_index);
      return expand_cache.original_face_sets[face_index];
    }
    case bke::pbvh::Type::BMesh: {
      /* Dyntopo does not support Face Set functionality. */
      BLI_assert(false);
    }
  }
  return SCULPT_FACE_SET_NONE;
}

static void sculpt_expand_status(bContext *C, wmOperator *op, Cache *expand_cache)
{
  WorkspaceStatus status(C);

  status.opmodal(IFACE_("Confirm"), op->type, SCULPT_EXPAND_MODAL_CONFIRM);
  status.opmodal(IFACE_("Cancel"), op->type, SCULPT_EXPAND_MODAL_CANCEL);
  status.opmodal(IFACE_("Invert"), op->type, SCULPT_EXPAND_MODAL_INVERT, expand_cache->invert);
  status.opmodal(IFACE_("Snap"), op->type, SCULPT_EXPAND_MODAL_SNAP_TOGGLE, expand_cache->snap);
  status.opmodal(IFACE_("Move"), op->type, SCULPT_EXPAND_MODAL_MOVE_TOGGLE, expand_cache->move);
  status.opmodal(
      IFACE_("Preserve"), op->type, SCULPT_EXPAND_MODAL_PRESERVE_TOGGLE, expand_cache->preserve);

  if (expand_cache->target != TargetType::FaceSets) {
    status.opmodal(IFACE_("Falloff Gradient"),
                   op->type,
                   SCULPT_EXPAND_MODAL_GRADIENT_TOGGLE,
                   expand_cache->falloff_gradient);
    status.opmodal(IFACE_("Brush Gradient"),
                   op->type,
                   SCULPT_EXPAND_MODAL_BRUSH_GRADIENT_TOGGLE,
                   expand_cache->brush_gradient);
  }

  if (ELEM(expand_cache->falloff_type,
           FalloffType::Geodesic,
           FalloffType::Topology,
           FalloffType::TopologyNormals,
           FalloffType::Sphere))
  {
    status.item(IFACE_("Falloff:"), 0);
    status.opmodal(IFACE_("Geodesic"),
                   op->type,
                   SCULPT_EXPAND_MODAL_FALLOFF_GEODESIC,
                   expand_cache->falloff_type == FalloffType::Geodesic);
    status.opmodal(IFACE_("Topology"),
                   op->type,
                   SCULPT_EXPAND_MODAL_FALLOFF_TOPOLOGY,
                   expand_cache->falloff_type == FalloffType::Topology);
    status.opmodal(IFACE_("Diagonals"),
                   op->type,
                   SCULPT_EXPAND_MODAL_FALLOFF_TOPOLOGY_DIAGONALS,
                   expand_cache->falloff_type == FalloffType::TopologyNormals);
    status.opmodal(IFACE_("Spherical"),
                   op->type,
                   SCULPT_EXPAND_MODAL_FALLOFF_SPHERICAL,
                   expand_cache->falloff_type == FalloffType::Sphere);
  }

  status.opmodal({}, op->type, SCULPT_EXPAND_MODAL_LOOP_COUNT_INCREASE);
  status.item("/", 0);
  status.separator(-1.2f);
  status.opmodal(IFACE_("Loop Count"), op->type, SCULPT_EXPAND_MODAL_LOOP_COUNT_DECREASE);

  status.opmodal(IFACE_("Geodesic Step"), op->type, SCULPT_EXPAND_MODAL_RECURSION_STEP_GEODESIC);
  status.opmodal(IFACE_("Topology Step"), op->type, SCULPT_EXPAND_MODAL_RECURSION_STEP_TOPOLOGY);

  const MTex *mask_tex = BKE_brush_mask_texture_get(expand_cache->brush, OB_MODE_SCULPT);
  if (mask_tex->tex) {
    status.opmodal({}, op->type, SCULPT_EXPAND_MODAL_TEXTURE_DISTORTION_INCREASE);
    status.opmodal(
        IFACE_("Texture Distortion"), op->type, SCULPT_EXPAND_MODAL_TEXTURE_DISTORTION_DECREASE);
  }
}

static wmOperatorStatus sculpt_expand_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  Object &ob = *CTX_data_active_object(C);
  SculptSession &ss = *ob.sculpt;

  /* Skips INBETWEEN_MOUSEMOVE events and other events that may cause unnecessary updates. */
  if (!ELEM(event->type, MOUSEMOVE, EVT_MODAL_MAP)) {
    return OPERATOR_RUNNING_MODAL;
  }

  /* Update SculptSession data. */
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  BKE_sculpt_update_object_for_edit(depsgraph, &ob, false);
  ensure_sculptsession_data(ob);

  /* Update and get the active vertex (and face) from the cursor. */
  const float mval_fl[2] = {float(event->mval[0]), float(event->mval[1])};
  const std::optional<int> target_expand_vertex = target_vert_update_and_get(C, ob, mval_fl);

  /* Handle the modal keymap state changes. */
  Cache &expand_cache = *ss.expand_cache;
  if (event->type == EVT_MODAL_MAP) {
    switch (event->val) {
      case SCULPT_EXPAND_MODAL_CANCEL: {
        sculpt_expand_cancel(C, op);
        return OPERATOR_FINISHED;
      }
      case SCULPT_EXPAND_MODAL_INVERT: {
        expand_cache.invert = !expand_cache.invert;
        break;
      }
      case SCULPT_EXPAND_MODAL_PRESERVE_TOGGLE: {
        expand_cache.preserve = !expand_cache.preserve;
        break;
      }
      case SCULPT_EXPAND_MODAL_GRADIENT_TOGGLE: {
        expand_cache.falloff_gradient = !expand_cache.falloff_gradient;
        break;
      }
      case SCULPT_EXPAND_MODAL_BRUSH_GRADIENT_TOGGLE: {
        expand_cache.brush_gradient = !expand_cache.brush_gradient;
        if (expand_cache.brush_gradient) {
          expand_cache.falloff_gradient = true;
        }
        break;
      }
      case SCULPT_EXPAND_MODAL_SNAP_TOGGLE: {
        if (expand_cache.snap) {
          expand_cache.snap = false;
          if (expand_cache.snap_enabled_face_sets) {
            expand_cache.snap_enabled_face_sets.reset();
          }
        }
        else {
          expand_cache.snap = true;
          expand_cache.snap_enabled_face_sets = std::make_unique<Set<int>>();
          snap_init_from_enabled(*depsgraph, ob, expand_cache);
        }
        break;
      }
      case SCULPT_EXPAND_MODAL_MOVE_TOGGLE: {
        if (expand_cache.move) {
          expand_cache.move = false;
          calc_falloff_from_vert_and_symmetry(*depsgraph,
                                              expand_cache,
                                              ob,
                                              expand_cache.initial_active_vert,
                                              expand_cache.move_original_falloff_type);
          break;
        }
        expand_cache.move = true;
        expand_cache.move_original_falloff_type = expand_cache.falloff_type;
        copy_v2_v2(expand_cache.initial_mouse_move, mval_fl);
        copy_v2_v2(expand_cache.original_mouse_move, expand_cache.initial_mouse);
        if (expand_cache.falloff_type == FalloffType::Geodesic &&
            SCULPT_vertex_count_get(ob) > expand_cache.max_geodesic_move_preview)
        {
          /* Set to spherical falloff for preview in high poly meshes as it is the fastest one.
           * In most cases it should match closely the preview from geodesic. */
          expand_cache.move_preview_falloff_type = FalloffType::Sphere;
        }
        else {
          expand_cache.move_preview_falloff_type = expand_cache.falloff_type;
        }
        break;
      }
      case SCULPT_EXPAND_MODAL_RECURSION_STEP_GEODESIC: {
        resursion_step_add(*depsgraph, ob, expand_cache, RecursionType::Geodesic);
        break;
      }
      case SCULPT_EXPAND_MODAL_RECURSION_STEP_TOPOLOGY: {
        resursion_step_add(*depsgraph, ob, expand_cache, RecursionType::Topology);
        break;
      }
      case SCULPT_EXPAND_MODAL_CONFIRM: {
        update_for_vert(C, ob, target_expand_vertex);

        if (expand_cache.reposition_pivot) {
          reposition_pivot(C, ob, expand_cache);
        }

        finish(C);
        return OPERATOR_FINISHED;
      }
      case SCULPT_EXPAND_MODAL_FALLOFF_GEODESIC: {
        check_topology_islands(ob, FalloffType::Geodesic);

        calc_falloff_from_vert_and_symmetry(
            *depsgraph, expand_cache, ob, expand_cache.initial_active_vert, FalloffType::Geodesic);
        break;
      }
      case SCULPT_EXPAND_MODAL_FALLOFF_TOPOLOGY: {
        check_topology_islands(ob, FalloffType::Topology);

        calc_falloff_from_vert_and_symmetry(
            *depsgraph, expand_cache, ob, expand_cache.initial_active_vert, FalloffType::Topology);
        break;
      }
      case SCULPT_EXPAND_MODAL_FALLOFF_TOPOLOGY_DIAGONALS: {
        check_topology_islands(ob, FalloffType::TopologyNormals);

        calc_falloff_from_vert_and_symmetry(*depsgraph,
                                            expand_cache,
                                            ob,
                                            expand_cache.initial_active_vert,
                                            FalloffType::TopologyNormals);
        break;
      }
      case SCULPT_EXPAND_MODAL_FALLOFF_SPHERICAL: {
        expand_cache.check_islands = false;
        calc_falloff_from_vert_and_symmetry(
            *depsgraph, expand_cache, ob, expand_cache.initial_active_vert, FalloffType::Sphere);
        break;
      }
      case SCULPT_EXPAND_MODAL_LOOP_COUNT_INCREASE: {
        expand_cache.loop_count += 1;
        break;
      }
      case SCULPT_EXPAND_MODAL_LOOP_COUNT_DECREASE: {
        expand_cache.loop_count -= 1;
        expand_cache.loop_count = max_ii(expand_cache.loop_count, 1);
        break;
      }
      case SCULPT_EXPAND_MODAL_TEXTURE_DISTORTION_INCREASE: {
        if (expand_cache.texture_distortion_strength == 0.0f) {
          const MTex *mask_tex = BKE_brush_mask_texture_get(expand_cache.brush, OB_MODE_SCULPT);
          if (mask_tex->tex == nullptr) {
            BKE_report(op->reports,
                       RPT_WARNING,
                       "Active brush does not contain any texture to distort the expand boundary");
            break;
          }
          if (mask_tex->brush_map_mode != MTEX_MAP_MODE_3D) {
            BKE_report(op->reports,
                       RPT_WARNING,
                       "Texture mapping not set to 3D, results may be unpredictable");
          }
        }
        expand_cache.texture_distortion_strength += SCULPT_EXPAND_TEXTURE_DISTORTION_STEP;
        break;
      }
      case SCULPT_EXPAND_MODAL_TEXTURE_DISTORTION_DECREASE: {
        expand_cache.texture_distortion_strength -= SCULPT_EXPAND_TEXTURE_DISTORTION_STEP;
        expand_cache.texture_distortion_strength = max_ff(expand_cache.texture_distortion_strength,
                                                          0.0f);
        break;
      }
    }
  }

  /* Handle expand origin movement if enabled. */
  if (expand_cache.move) {
    move_propagation_origin(C, ob, event, expand_cache);
  }

  /* Add new Face Sets IDs to the snapping set if enabled. */
  if (expand_cache.snap) {
    const int active_face_set_id = active_face_set_id_get(ob, expand_cache);
    /* The key may exist, in that case this does nothing. */
    expand_cache.snap_enabled_face_sets->add(active_face_set_id);
  }

  /* Update the sculpt data with the current state of the #Cache. */
  update_for_vert(C, ob, target_expand_vertex);

  sculpt_expand_status(C, op, &expand_cache);

  return OPERATOR_RUNNING_MODAL;
}

/**
 * Deletes the `delete_id` Face Set ID from the mesh Face Sets
 * and stores the result in `r_face_set`.
 * The faces that were using the `delete_id` Face Set are filled
 * using the content from their neighbors.
 */
static void delete_face_set_id(
    int *r_face_sets, Object &object, Cache &expand_cache, Mesh *mesh, const int delete_id)
{
  const GroupedSpan<int> vert_to_face_map = mesh->vert_to_face_map();
  const OffsetIndices faces = mesh->faces();
  const Span<int> corner_verts = mesh->corner_verts();

  /* Check that all the face sets IDs in the mesh are not equal to `delete_id`
   * before attempting to delete it. */
  bool all_same_id = true;
  for (const int i : faces.index_range()) {
    if (!is_face_in_active_component(object, faces, corner_verts, expand_cache, i)) {
      continue;
    }
    if (r_face_sets[i] != delete_id) {
      all_same_id = false;
      break;
    }
  }
  if (all_same_id) {
    return;
  }

  BLI_LINKSTACK_DECLARE(queue, void *);
  BLI_LINKSTACK_DECLARE(queue_next, void *);

  BLI_LINKSTACK_INIT(queue);
  BLI_LINKSTACK_INIT(queue_next);

  for (const int i : faces.index_range()) {
    if (r_face_sets[i] == delete_id) {
      BLI_LINKSTACK_PUSH(queue, POINTER_FROM_INT(i));
    }
  }

  while (BLI_LINKSTACK_SIZE(queue)) {
    bool any_updated = false;
    while (BLI_LINKSTACK_SIZE(queue)) {
      const int f_index = POINTER_AS_INT(BLI_LINKSTACK_POP(queue));
      int other_id = delete_id;
      for (const int vert : corner_verts.slice(faces[f_index])) {
        for (const int neighbor_face_index : vert_to_face_map[vert]) {
          if (expand_cache.original_face_sets[neighbor_face_index] <= 0) {
            /* Skip picking IDs from hidden Face Sets. */
            continue;
          }
          if (r_face_sets[neighbor_face_index] != delete_id) {
            other_id = r_face_sets[neighbor_face_index];
          }
        }
      }

      if (other_id != delete_id) {
        any_updated = true;
        r_face_sets[f_index] = other_id;
      }
      else {
        BLI_LINKSTACK_PUSH(queue_next, POINTER_FROM_INT(f_index));
      }
    }
    if (!any_updated) {
      /* No Face Sets where updated in this iteration, which means that no more content to keep
       * filling the faces of the deleted Face Set was found. Break to avoid entering an infinite
       * loop trying to search for those faces again. */
      break;
    }

    BLI_LINKSTACK_SWAP(queue, queue_next);
  }

  BLI_LINKSTACK_FREE(queue);
  BLI_LINKSTACK_FREE(queue_next);
}

static void cache_initial_config_set(bContext *C, wmOperator *op, Cache &expand_cache)
{
  expand_cache.normal_falloff_blur_steps = RNA_int_get(op->ptr, "normal_falloff_smooth");
  expand_cache.invert = RNA_boolean_get(op->ptr, "invert");
  expand_cache.preserve = RNA_boolean_get(op->ptr, "use_mask_preserve");
  expand_cache.auto_mask = RNA_boolean_get(op->ptr, "use_auto_mask");
  expand_cache.falloff_gradient = RNA_boolean_get(op->ptr, "use_falloff_gradient");
  expand_cache.target = TargetType(RNA_enum_get(op->ptr, "target"));
  expand_cache.modify_active_face_set = RNA_boolean_get(op->ptr, "use_modify_active");
  expand_cache.reposition_pivot = RNA_boolean_get(op->ptr, "use_reposition_pivot");
  expand_cache.max_geodesic_move_preview = RNA_int_get(op->ptr, "max_geodesic_move_preview");

  /* These can be exposed in RNA if needed. */
  expand_cache.loop_count = 1;
  expand_cache.brush_gradient = false;

  /* Texture and color data from the active Brush. */
  const Paint *paint = BKE_paint_get_active_from_context(C);
  const Sculpt &sd = *CTX_data_tool_settings(C)->sculpt;
  expand_cache.paint = paint;
  expand_cache.brush = BKE_paint_brush_for_read(&sd.paint);
  BKE_curvemapping_init(expand_cache.brush->curve_distance_falloff);
  copy_v4_fl(expand_cache.fill_color, 1.0f);
  copy_v3_v3(expand_cache.fill_color, BKE_brush_color_get(paint, expand_cache.brush));

  expand_cache.scene = CTX_data_scene(C);
  expand_cache.texture_distortion_strength = 0.0f;
  expand_cache.blend_mode = expand_cache.brush->blend;
}

/**
 * Does the undo sculpt push for the affected target data of the #Cache.
 */
static void undo_push(const Depsgraph &depsgraph, Object &ob, Cache &expand_cache)
{
  IndexMaskMemory memory;
  const IndexMask node_mask = bke::pbvh::all_leaf_nodes(*bke::object::pbvh_get(ob), memory);

  switch (expand_cache.target) {
    case TargetType::Mask:
      undo::push_nodes(depsgraph, ob, node_mask, undo::Type::Mask);
      break;
    case TargetType::FaceSets:
      undo::push_nodes(depsgraph, ob, node_mask, undo::Type::FaceSet);
      break;
    case TargetType::Colors: {
      undo::push_nodes(depsgraph, ob, node_mask, undo::Type::Color);
      break;
    }
  }
}

static bool any_nonzero_mask(const Object &object)
{
  const SculptSession &ss = *object.sculpt;
  switch (bke::object::pbvh_get(object)->type()) {
    case bke::pbvh::Type::Mesh: {
      const Mesh &mesh = *static_cast<const Mesh *>(object.data);
      const bke::AttributeAccessor attributes = mesh.attributes();
      const VArraySpan mask = *attributes.lookup<float>(".sculpt_mask");
      if (mask.is_empty()) {
        return false;
      }
      return std::any_of(
          mask.begin(), mask.end(), [&](const float value) { return value > 0.0f; });
    }
    case bke::pbvh::Type::Grids: {
      const SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;
      const Span<float> mask = subdiv_ccg.masks;
      if (mask.is_empty()) {
        return false;
      }
      return std::any_of(
          mask.begin(), mask.end(), [&](const float value) { return value > 0.0f; });
    }
    case bke::pbvh::Type::BMesh: {
      BMesh &bm = *ss.bm;
      const int offset = CustomData_get_offset_named(&bm.vdata, CD_PROP_FLOAT, ".sculpt_mask");
      if (offset == -1) {
        return false;
      }
      BMIter iter;
      BMVert *vert;
      BM_ITER_MESH (vert, &iter, &bm, BM_VERTS_OF_MESH) {
        if (BM_ELEM_CD_GET_FLOAT(vert, offset) > 0.0f) {
          return true;
        }
      }
      return false;
    }
  }
  return false;
}

static wmOperatorStatus sculpt_expand_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  const Scene &scene = *CTX_data_scene(C);
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Object &ob = *CTX_data_active_object(C);
  SculptSession &ss = *ob.sculpt;
  Mesh *mesh = static_cast<Mesh *>(ob.data);

  const View3D *v3d = CTX_wm_view3d(C);
  const Base *base = CTX_data_active_base(C);
  if (!BKE_base_is_visible(v3d, base)) {
    return OPERATOR_CANCELLED;
  }

  /* Create and configure the Expand Cache. */
  ss.expand_cache = MEM_new<Cache>(__func__);
  cache_initial_config_set(C, op, *ss.expand_cache);

  /* Update object. */
  const bool needs_colors = ss.expand_cache->target == TargetType::Colors;

  if (needs_colors) {
    /* CTX_data_ensure_evaluated_depsgraph should be used at the end to include the updates of
     * earlier steps modifying the data. */
    BKE_sculpt_color_layer_create_if_needed(&ob);
    depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  }

  if (ss.expand_cache->target == TargetType::Mask) {
    Scene &scene = *CTX_data_scene(C);
    MultiresModifierData *mmd = BKE_sculpt_multires_active(&scene, &ob);
    BKE_sculpt_mask_layers_ensure(depsgraph, CTX_data_main(C), &ob, mmd);

    if (RNA_boolean_get(op->ptr, "use_auto_mask")) {
      if (any_nonzero_mask(ob)) {
        write_mask_data(ob, Array<float>(SCULPT_vertex_count_get(ob), 1.0f));
      }
    }
  }

  BKE_sculpt_update_object_for_edit(depsgraph, &ob, needs_colors);

  /* Do nothing when the mesh has 0 vertices. */
  const int totvert = SCULPT_vertex_count_get(ob);
  if (totvert == 0) {
    expand_cache_free(ss);
    return OPERATOR_CANCELLED;
  }
  const bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(ob);

  /* Face Set operations are not supported in dyntopo. */
  if (ss.expand_cache->target == TargetType::FaceSets && pbvh.type() == bke::pbvh::Type::BMesh) {
    expand_cache_free(ss);
    return OPERATOR_CANCELLED;
  }

  ensure_sculptsession_data(ob);

  /* Set the initial element for expand from the event position. */
  const float mouse[2] = {float(event->mval[0]), float(event->mval[1])};

  /* When getting the initial active vert, in cases where the cursor is not over the mesh and
   * the mesh type has changed, we cannot proceed with the expand operator, as there is no
   * sensible last active vertex when switching between backing implementations. */
  if (!set_initial_components_for_mouse(C, ob, *ss.expand_cache, mouse)) {
    expand_cache_free(ss);
    return OPERATOR_CANCELLED;
  }

  /* Initialize undo. */
  undo::push_begin(scene, ob, op);
  undo_push(*depsgraph, ob, *ss.expand_cache);

  /* Cache bke::pbvh::Tree nodes. */
  ss.expand_cache->node_mask = bke::pbvh::all_leaf_nodes(pbvh, ss.expand_cache->node_mask_memory);

  /* Store initial state. */
  original_state_store(ob, *ss.expand_cache);

  if (ss.expand_cache->modify_active_face_set) {
    delete_face_set_id(ss.expand_cache->initial_face_sets.data(),
                       ob,
                       *ss.expand_cache,
                       mesh,
                       ss.expand_cache->next_face_set);
  }

  const int initial_vert = ss.expand_cache->initial_active_vert;

  /* Initialize the falloff. */
  FalloffType falloff_type = FalloffType(RNA_enum_get(op->ptr, "falloff_type"));

  /* When starting from a boundary vertex, set the initial falloff to boundary. */
  switch (pbvh.type()) {
    case bke::pbvh::Type::Mesh: {
      const Mesh &mesh = *static_cast<const Mesh *>(ob.data);
      const GroupedSpan<int> vert_to_face_map = mesh.vert_to_face_map();
      const bke::AttributeAccessor attributes = mesh.attributes();
      const VArraySpan hide_poly = *attributes.lookup<bool>(".hide_poly", bke::AttrDomain::Face);
      if (boundary::vert_is_boundary(
              vert_to_face_map, hide_poly, ss.vertex_info.boundary, initial_vert))
      {
        falloff_type = FalloffType::BoundaryTopology;
      }
      break;
    }
    case bke::pbvh::Type::Grids: {
      const Mesh &base_mesh = *static_cast<const Mesh *>(ob.data);
      const OffsetIndices<int> faces = base_mesh.faces();
      const Span<int> corner_verts = base_mesh.corner_verts();
      const bke::AttributeAccessor attributes = base_mesh.attributes();
      const VArraySpan face_sets = *attributes.lookup_or_default<int>(
          ".sculpt_face_set", bke::AttrDomain::Face, 0);
      const SubdivCCG &subdiv_ccg = *ob.sculpt->subdiv_ccg;
      const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);
      if (boundary::vert_is_boundary(faces,
                                     corner_verts,
                                     ss.vertex_info.boundary,
                                     subdiv_ccg,
                                     SubdivCCGCoord::from_index(key, initial_vert)))
      {
        falloff_type = FalloffType::BoundaryTopology;
      }
      break;
    }
    case bke::pbvh::Type::BMesh: {
      BMesh &bm = *ob.sculpt->bm;
      vert_random_access_ensure(ob);
      if (boundary::vert_is_boundary(BM_vert_at_index(&bm, initial_vert))) {
        falloff_type = FalloffType::BoundaryTopology;
      }
      break;
    }
  }

  calc_falloff_from_vert_and_symmetry(
      *depsgraph, *ss.expand_cache, ob, initial_vert, falloff_type);

  check_topology_islands(ob, falloff_type);

  /* Initial mesh data update, resets all target data in the sculpt mesh. */
  update_for_vert(C, ob, initial_vert);

  sculpt_expand_status(C, op, ss.expand_cache);

  WM_event_add_modal_handler(C, op);
  return OPERATOR_RUNNING_MODAL;
}

void modal_keymap(wmKeyConfig *keyconf)
{
  static const EnumPropertyItem modal_items[] = {
      {SCULPT_EXPAND_MODAL_CONFIRM, "CONFIRM", 0, "Confirm", ""},
      {SCULPT_EXPAND_MODAL_CANCEL, "CANCEL", 0, "Cancel", ""},
      {SCULPT_EXPAND_MODAL_INVERT, "INVERT", 0, "Invert", ""},
      {SCULPT_EXPAND_MODAL_PRESERVE_TOGGLE, "PRESERVE", 0, "Toggle Preserve State", ""},
      {SCULPT_EXPAND_MODAL_GRADIENT_TOGGLE, "GRADIENT", 0, "Toggle Gradient", ""},
      {SCULPT_EXPAND_MODAL_RECURSION_STEP_GEODESIC,
       "RECURSION_STEP_GEODESIC",
       0,
       "Geodesic recursion step",
       ""},
      {SCULPT_EXPAND_MODAL_RECURSION_STEP_TOPOLOGY,
       "RECURSION_STEP_TOPOLOGY",
       0,
       "Topology recursion Step",
       ""},
      {SCULPT_EXPAND_MODAL_MOVE_TOGGLE, "MOVE_TOGGLE", 0, "Move Origin", ""},
      {SCULPT_EXPAND_MODAL_FALLOFF_GEODESIC, "FALLOFF_GEODESICS", 0, "Geodesic Falloff", ""},
      {SCULPT_EXPAND_MODAL_FALLOFF_TOPOLOGY, "FALLOFF_TOPOLOGY", 0, "Topology Falloff", ""},
      {SCULPT_EXPAND_MODAL_FALLOFF_TOPOLOGY_DIAGONALS,
       "FALLOFF_TOPOLOGY_DIAGONALS",
       0,
       "Diagonals Falloff",
       ""},
      {SCULPT_EXPAND_MODAL_FALLOFF_SPHERICAL, "FALLOFF_SPHERICAL", 0, "Spherical Falloff", ""},
      {SCULPT_EXPAND_MODAL_SNAP_TOGGLE, "SNAP_TOGGLE", 0, "Snap expand to Face Sets", ""},
      {SCULPT_EXPAND_MODAL_LOOP_COUNT_INCREASE,
       "LOOP_COUNT_INCREASE",
       0,
       "Loop Count Increase",
       ""},
      {SCULPT_EXPAND_MODAL_LOOP_COUNT_DECREASE,
       "LOOP_COUNT_DECREASE",
       0,
       "Loop Count Decrease",
       ""},
      {SCULPT_EXPAND_MODAL_BRUSH_GRADIENT_TOGGLE,
       "BRUSH_GRADIENT_TOGGLE",
       0,
       "Toggle Brush Gradient",
       ""},
      {SCULPT_EXPAND_MODAL_TEXTURE_DISTORTION_INCREASE,
       "TEXTURE_DISTORTION_INCREASE",
       0,
       "Texture Distortion Increase",
       ""},
      {SCULPT_EXPAND_MODAL_TEXTURE_DISTORTION_DECREASE,
       "TEXTURE_DISTORTION_DECREASE",
       0,
       "Texture Distortion Decrease",
       ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const char *name = "Sculpt Expand Modal";
  wmKeyMap *keymap = WM_modalkeymap_find(keyconf, name);

  /* This function is called for each space-type, only needs to add map once. */
  if (keymap && keymap->modal_items) {
    return;
  }

  keymap = WM_modalkeymap_ensure(keyconf, name, modal_items);
  WM_modalkeymap_assign(keymap, "SCULPT_OT_expand");
}

void SCULPT_OT_expand(wmOperatorType *ot)
{
  ot->name = "Expand";
  ot->idname = "SCULPT_OT_expand";
  ot->description = "Generic sculpt expand operator";

  ot->invoke = sculpt_expand_invoke;
  ot->modal = sculpt_expand_modal;
  ot->cancel = sculpt_expand_cancel;
  ot->poll = SCULPT_mode_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_DEPENDS_ON_CURSOR;

  static EnumPropertyItem prop_sculpt_expand_falloff_type_items[] = {
      {int(FalloffType::Geodesic), "GEODESIC", 0, "Geodesic", ""},
      {int(FalloffType::Topology), "TOPOLOGY", 0, "Topology", ""},
      {int(FalloffType::TopologyNormals), "TOPOLOGY_DIAGONALS", 0, "Topology Diagonals", ""},
      {int(FalloffType::Normals), "NORMALS", 0, "Normals", ""},
      {int(FalloffType::Sphere), "SPHERICAL", 0, "Spherical", ""},
      {int(FalloffType::BoundaryTopology), "BOUNDARY_TOPOLOGY", 0, "Boundary Topology", ""},
      {int(FalloffType::BoundaryFaceSet), "BOUNDARY_FACE_SET", 0, "Boundary Face Set", ""},
      {int(FalloffType::ActiveFaceSet), "ACTIVE_FACE_SET", 0, "Active Face Set", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static EnumPropertyItem prop_sculpt_expand_target_type_items[] = {
      {int(TargetType::Mask), "MASK", 0, "Mask", ""},
      {int(TargetType::FaceSets), "FACE_SETS", 0, "Face Sets", ""},
      {int(TargetType::Colors), "COLOR", 0, "Color", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  RNA_def_enum(ot->srna,
               "target",
               prop_sculpt_expand_target_type_items,
               int(TargetType::Mask),
               "Data Target",
               "Data that is going to be modified in the expand operation");

  RNA_def_enum(ot->srna,
               "falloff_type",
               prop_sculpt_expand_falloff_type_items,
               int(FalloffType::Geodesic),
               "Falloff Type",
               "Initial falloff of the expand operation");

  ot->prop = RNA_def_boolean(
      ot->srna, "invert", false, "Invert", "Invert the expand active elements");
  ot->prop = RNA_def_boolean(ot->srna,
                             "use_mask_preserve",
                             false,
                             "Preserve Previous",
                             "Preserve the previous state of the target data");
  ot->prop = RNA_def_boolean(ot->srna,
                             "use_falloff_gradient",
                             false,
                             "Falloff Gradient",
                             "Expand Using a linear falloff");

  ot->prop = RNA_def_boolean(ot->srna,
                             "use_modify_active",
                             false,
                             "Modify Active",
                             "Modify the active Face Set instead of creating a new one");

  ot->prop = RNA_def_boolean(
      ot->srna,
      "use_reposition_pivot",
      true,
      "Reposition Pivot",
      "Reposition the sculpt transform pivot to the boundary of the expand active area");

  ot->prop = RNA_def_int(ot->srna,
                         "max_geodesic_move_preview",
                         10000,
                         0,
                         INT_MAX,
                         "Max Vertex Count for Geodesic Move Preview",
                         "Maximum number of vertices in the mesh for using geodesic falloff when "
                         "moving the origin of expand. If the total number of vertices is greater "
                         "than this value, the falloff will be set to spherical when moving",
                         0,
                         1000000);
  ot->prop = RNA_def_boolean(ot->srna,
                             "use_auto_mask",
                             false,
                             "Auto Create",
                             "Fill in mask if nothing is already masked");
  ot->prop = RNA_def_int(ot->srna,
                         "normal_falloff_smooth",
                         2,
                         0,
                         10,
                         "Normal Smooth",
                         "Blurring steps for normal falloff",
                         0,
                         10);
}

}  // namespace blender::ed::sculpt_paint::expand
