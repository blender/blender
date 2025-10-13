/* SPDX-FileCopyrightText: 2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edsculpt
 */
#include "sculpt_cloth.hh"

#include "MEM_guardedalloc.h"

#include "BLI_array_utils.hh"
#include "BLI_enumerable_thread_specific.hh"
#include "BLI_math_geom.h"
#include "BLI_math_matrix.h"
#include "BLI_math_matrix.hh"
#include "BLI_math_rotation.h"
#include "BLI_math_vector.hh"
#include "BLI_ordered_edge.hh"
#include "BLI_utildefines.h"
#include "BLI_vector.hh"

#include "BLT_translation.hh"

#include "DNA_brush_types.h"
#include "DNA_customdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_brush.hh"
#include "BKE_ccg.hh"
#include "BKE_collision.h"
#include "BKE_context.hh"
#include "BKE_layer.hh"
#include "BKE_mesh.hh"
#include "BKE_modifier.hh"
#include "BKE_paint.hh"
#include "BKE_paint_bvh.hh"
#include "BKE_subdiv_ccg.hh"

#include "DEG_depsgraph_query.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "ED_sculpt.hh"

#include "mesh_brush_common.hh"
#include "sculpt_automask.hh"
#include "sculpt_face_set.hh"
#include "sculpt_filter.hh"
#include "sculpt_hide.hh"
#include "sculpt_intern.hh"
#include "sculpt_undo.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "GPU_immediate.hh"
#include "GPU_immediate_util.hh"
#include "GPU_matrix.hh"
#include "GPU_state.hh"

#include "bmesh.hh"

#include <cmath>
#include <cstring>

namespace blender::ed::sculpt_paint::cloth {

static MutableSpan<int> calc_vert_indices_grids(const CCGKey &key,
                                                const Span<int> grids,
                                                Vector<int> &indices)
{
  const int grid_verts_num = grids.size() * key.grid_area;
  indices.resize(grid_verts_num);
  for (const int i : grids.index_range()) {
    array_utils::fill_index_range(
        indices.as_mutable_span().slice(i * key.grid_area, key.grid_area),
        grids[i] * key.grid_area);
  }
  return indices;
}

static MutableSpan<int> calc_vert_indices_bmesh(const Set<BMVert *, 0> &verts,
                                                Vector<int> &indices)
{
  indices.resize(verts.size());
  int i = 0;
  for (const BMVert *vert : verts) {
    indices[i] = BM_elem_index_get(vert);
    i++;
  }
  return indices;
}

static MutableSpan<int> calc_visible_vert_indices_grids(const CCGKey &key,
                                                        const BitGroupVector<> &grid_hidden,
                                                        const Span<int> grids,
                                                        Vector<int> &indices)
{
  if (grid_hidden.is_empty()) {
    return calc_vert_indices_grids(key, grids, indices);
  }
  const int grid_verts_num = grids.size() * key.grid_area;
  indices.reserve(grid_verts_num);
  for (const int i : grids.index_range()) {
    const int start = grids[i] * key.grid_area;
    bits::foreach_0_index(grid_hidden[grids[i]],
                          [&](const int offset) { indices.append(start + offset); });
  }
  return indices;
}

static MutableSpan<int> calc_visible_vert_indices_bmesh(const Set<BMVert *, 0> &verts,
                                                        Vector<int> &indices)
{
  indices.reserve(verts.size());
  for (const BMVert *vert : verts) {
    if (!BM_elem_flag_test(vert, BM_ELEM_HIDDEN)) {
      indices.append(BM_elem_index_get(vert));
    }
  }
  return indices;
}

static GroupedSpan<int> calc_vert_neighbor_indices_grids(const SubdivCCG &subdiv_ccg,
                                                         const Span<int> verts,
                                                         Vector<int> &r_offset_data,
                                                         Vector<int> &r_data)
{
  const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);

  r_offset_data.resize(verts.size() + 1);
  r_data.clear();

  for (const int i : verts.index_range()) {
    r_offset_data[i] = r_data.size();
    SubdivCCGNeighbors neighbors;
    BKE_subdiv_ccg_neighbor_coords_get(
        subdiv_ccg, SubdivCCGCoord::from_index(key, verts[i]), false, neighbors);

    for (const SubdivCCGCoord coord : neighbors.coords) {
      r_data.append(coord.to_index(key));
    }
  }

  r_offset_data.last() = r_data.size();
  return GroupedSpan<int>(r_offset_data.as_span(), r_data.as_span());
}

static GroupedSpan<int> calc_vert_neighbor_indices_bmesh(const BMesh &bm,
                                                         const Span<int> verts,
                                                         Vector<int> &r_offset_data,
                                                         Vector<int> &r_data)
{
  BMeshNeighborVerts neighbors;

  r_offset_data.resize(verts.size() + 1);
  r_data.clear();

  for (const int i : verts.index_range()) {
    r_offset_data[i] = r_data.size();
    BMVert *vert = BM_vert_at_index(&const_cast<BMesh &>(bm), verts[i]);
    for (const BMVert *neighbor : vert_neighbors_get_bmesh(*vert, neighbors)) {
      r_data.append(BM_elem_index_get(neighbor));
    }
  }
  r_offset_data.last() = r_data.size();
  return GroupedSpan<int>(r_offset_data.as_span(), r_data.as_span());
}

static float3 cloth_brush_simulation_location_get(const SculptSession &ss, const Brush *brush)
{
  if (!ss.cache || !brush) {
    return float3(0);
  }
  if (brush->cloth_simulation_area_type == BRUSH_CLOTH_SIMULATION_AREA_LOCAL) {
    return ss.cache->initial_location_symm;
  }
  return ss.cache->location_symm;
}

IndexMask brush_affected_nodes_gather(const Object &object,
                                      const Brush &brush,
                                      IndexMaskMemory &memory)
{
  const SculptSession &ss = *object.sculpt;
  BLI_assert(ss.cache);
  BLI_assert(brush.sculpt_brush_type == SCULPT_BRUSH_TYPE_CLOTH);
  const bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);

  switch (brush.cloth_simulation_area_type) {
    case BRUSH_CLOTH_SIMULATION_AREA_LOCAL: {
      const float radius_squared = math::square(ss.cache->initial_radius *
                                                (1.0 + brush.cloth_sim_limit));
      return bke::pbvh::search_nodes(pbvh, memory, [&](const bke::pbvh::Node &node) {
        return node_in_sphere(node, ss.cache->initial_location_symm, radius_squared, false);
      });
    }
    case BRUSH_CLOTH_SIMULATION_AREA_GLOBAL:
      return bke::pbvh::all_leaf_nodes(pbvh, memory);
    case BRUSH_CLOTH_SIMULATION_AREA_DYNAMIC: {
      const float radius_squared = math::square(ss.cache->radius * (1.0 + brush.cloth_sim_limit));
      return bke::pbvh::search_nodes(pbvh, memory, [&](const bke::pbvh::Node &node) {
        return node_in_sphere(node, ss.cache->location_symm, radius_squared, false);
      });
    }
  }

  BLI_assert_unreachable();
  return {};
}

bool is_cloth_deform_brush(const Brush &brush)
{
  return (brush.sculpt_brush_type == SCULPT_BRUSH_TYPE_CLOTH &&
          ELEM(brush.cloth_deform_type, BRUSH_CLOTH_DEFORM_GRAB, BRUSH_CLOTH_DEFORM_SNAKE_HOOK)) ||
         /* All brushes that are not the cloth brush deform the simulation using softbody
          * constraints instead of applying forces. */
         (brush.sculpt_brush_type != SCULPT_BRUSH_TYPE_CLOTH &&
          brush.deform_target == BRUSH_DEFORM_TARGET_CLOTH_SIM);
}

static float cloth_brush_simulation_falloff_get(const Brush &brush,
                                                const float radius,
                                                const float3 &location,
                                                const float3 &co)
{
  if (brush.sculpt_brush_type != SCULPT_BRUSH_TYPE_CLOTH) {
    /* All brushes that are not the cloth brush do not use simulation areas. */
    return 1.0f;
  }

  /* Global simulation does not have any falloff as the entire mesh is being simulated. */
  if (brush.cloth_simulation_area_type == BRUSH_CLOTH_SIMULATION_AREA_GLOBAL) {
    return 1.0f;
  }

  const float distance = math::distance(location, co);
  const float limit = radius + (radius * brush.cloth_sim_limit);
  const float falloff = radius + (radius * brush.cloth_sim_limit * brush.cloth_sim_falloff);

  if (distance > limit) {
    /* Outside the limits. */
    return 0.0f;
  }
  if (distance < falloff) {
    /* Before the falloff area. */
    return 1.0f;
  }
  /* Do a smooth-step transition inside the falloff area. */
  float p = 1.0f - ((distance - falloff) / (limit - falloff));
  return 3.0f * p * p - 2.0f * p * p * p;
}

BLI_NOINLINE static void calc_brush_simulation_falloff(const Brush &brush,
                                                       const float radius,
                                                       const float3 &location,
                                                       const Span<float3> positions,
                                                       const MutableSpan<float> factors)
{
  BLI_assert(positions.size() == factors.size());

  for (const int i : positions.index_range()) {
    factors[i] *= cloth_brush_simulation_falloff_get(brush, radius, location, positions[i]);
  }
}

#define CLOTH_LENGTH_CONSTRAINTS_BLOCK 100000
#define CLOTH_SIMULATION_ITERATIONS 5

#define CLOTH_SOLVER_DISPLACEMENT_FACTOR 0.6f
#define CLOTH_MAX_CONSTRAINTS_PER_VERTEX 1024
#define CLOTH_SIMULATION_TIME_STEP 0.01f
#define CLOTH_DEFORMATION_SNAKEHOOK_STRENGTH 0.35f
#define CLOTH_DEFORMATION_TARGET_STRENGTH 0.01f
#define CLOTH_DEFORMATION_GRAB_STRENGTH 0.1f

static void cloth_brush_add_length_constraint(SimulationData &cloth_sim,
                                              const int node_index,
                                              const int v1,
                                              const int v2,
                                              const Span<float3> init_positions)
{
  LengthConstraint length_constraint{};

  length_constraint.elem_index_a = v1;
  length_constraint.elem_index_b = v2;

  length_constraint.node = node_index;

  length_constraint.elem_position_a = cloth_sim.pos[v1];
  length_constraint.elem_position_b = cloth_sim.pos[v2];

  length_constraint.type = SCULPT_CLOTH_CONSTRAINT_STRUCTURAL;

  length_constraint.length = math::distance(init_positions[v1], init_positions[v2]);
  length_constraint.strength = 1.0f;

  cloth_sim.length_constraints.append(length_constraint);
}

static void cloth_brush_add_softbody_constraint(SimulationData &cloth_sim,
                                                const int node_index,
                                                const int v,
                                                const float strength)
{
  LengthConstraint length_constraint{};

  length_constraint.elem_index_a = v;
  length_constraint.elem_index_b = v;

  length_constraint.node = node_index;

  length_constraint.elem_position_a = cloth_sim.pos[v];
  length_constraint.elem_position_b = cloth_sim.softbody_pos[v];

  length_constraint.type = SCULPT_CLOTH_CONSTRAINT_SOFTBODY;

  length_constraint.length = 0.0f;
  length_constraint.strength = strength;

  cloth_sim.length_constraints.append(length_constraint);
}

static void cloth_brush_add_pin_constraint(SimulationData &cloth_sim,
                                           const int node_index,
                                           const int v,
                                           const float strength)
{
  LengthConstraint length_constraint{};

  length_constraint.elem_index_a = v;
  length_constraint.elem_index_b = v;

  length_constraint.node = node_index;

  length_constraint.elem_position_a = cloth_sim.pos[v];
  length_constraint.elem_position_b = cloth_sim.init_pos[v];

  length_constraint.type = SCULPT_CLOTH_CONSTRAINT_PIN;

  length_constraint.length = 0.0f;
  length_constraint.strength = strength;

  cloth_sim.length_constraints.append(length_constraint);
}

static void cloth_brush_add_deformation_constraint(SimulationData &cloth_sim,
                                                   const int node_index,
                                                   const int v,
                                                   const float strength)
{
  LengthConstraint length_constraint{};

  length_constraint.elem_index_a = v;
  length_constraint.elem_index_b = v;

  length_constraint.node = node_index;

  length_constraint.type = SCULPT_CLOTH_CONSTRAINT_DEFORMATION;

  length_constraint.elem_position_a = cloth_sim.pos[v];
  length_constraint.elem_position_b = cloth_sim.deformation_pos[v];

  length_constraint.length = 0.0f;
  length_constraint.strength = strength;

  cloth_sim.length_constraints.append(length_constraint);
}

static void add_constraints_for_verts(const Object &object,
                                      const Brush *brush,
                                      const float3 &cloth_sim_initial_location,
                                      const float cloth_sim_radius,
                                      const Span<float3> init_positions,
                                      const int node_index,
                                      const Span<int> verts,
                                      const GroupedSpan<int> vert_neighbors,
                                      SimulationData &cloth_sim,
                                      Set<OrderedEdge> &created_length_constraints)
{
  const SculptSession &ss = *object.sculpt;

  const bool is_brush_has_stroke_cache = ss.cache != nullptr && brush != nullptr;
  const bool pin_simulation_boundary = is_brush_has_stroke_cache &&
                                       brush->flag2 & BRUSH_CLOTH_PIN_SIMULATION_BOUNDARY &&
                                       brush->cloth_simulation_area_type !=
                                           BRUSH_CLOTH_SIMULATION_AREA_DYNAMIC;

  /* Brush can be nullptr in tools that use the solver without relying of constraints with
   * deformation positions. */
  const bool cloth_is_deform_brush = is_brush_has_stroke_cache && is_cloth_deform_brush(*brush);

  const bool use_falloff_plane = brush->cloth_force_falloff_type ==
                                 BRUSH_CLOTH_FORCE_FALLOFF_PLANE;
  float radius_squared = 0.0f;
  if (cloth_is_deform_brush) {
    radius_squared = ss.cache->initial_radius * ss.cache->initial_radius;
  }

  /* Only limit the constraint creation to a radius when the simulation is local. */
  const float cloth_sim_radius_squared = brush->cloth_simulation_area_type ==
                                                 BRUSH_CLOTH_SIMULATION_AREA_LOCAL ?
                                             cloth_sim_radius * cloth_sim_radius :
                                             FLT_MAX;

  for (const int i : verts.index_range()) {
    const int vert = verts[i];
    const float len_squared = math::distance_squared(init_positions[vert],
                                                     cloth_sim_initial_location);
    if (len_squared < cloth_sim_radius_squared) {
      if (cloth_sim.softbody_strength > 0.0f) {
        cloth_brush_add_softbody_constraint(cloth_sim, node_index, vert, 1.0f);
      }

      const Span<int> neighbors = vert_neighbors[i];

      /* As we don't know the order of the neighbor vertices, we create all possible combinations
       * between the neighbor and the original vertex as length constraints. */
      /* This results on a pattern that contains structural, shear and bending constraints for all
       * vertices, but constraints are repeated taking more memory than necessary. */
      for (const int neighbor : neighbors) {
        if (created_length_constraints.add({vert, neighbor})) {
          cloth_brush_add_length_constraint(cloth_sim, node_index, vert, neighbor, init_positions);
        }
      }
      for (const int a : neighbors) {
        for (const int b : neighbors) {
          if (a != b) {
            if (created_length_constraints.add({a, b})) {
              cloth_brush_add_length_constraint(cloth_sim, node_index, a, b, init_positions);
            }
          }
        }
      }
    }

    if (is_brush_has_stroke_cache && brush->sculpt_brush_type == SCULPT_BRUSH_TYPE_CLOTH) {
      /* The cloth brush works by applying forces in most of its modes, but some of them require
       * deformation coordinates to make the simulation stable. */
      if (brush->cloth_deform_type == BRUSH_CLOTH_DEFORM_GRAB) {
        if (use_falloff_plane) {
          /* With plane falloff the strength of the constraints is set when applying the
           * deformation forces. */
          cloth_brush_add_deformation_constraint(
              cloth_sim, node_index, vert, CLOTH_DEFORMATION_GRAB_STRENGTH);
        }
        else if (len_squared < radius_squared) {
          /* With radial falloff deformation constraints are created with different strengths and
           * only inside the radius of the brush. */
          const float fade = BKE_brush_curve_strength(
              brush, std::sqrt(len_squared), ss.cache->radius);
          cloth_brush_add_deformation_constraint(
              cloth_sim, node_index, vert, fade * CLOTH_DEFORMATION_GRAB_STRENGTH);
        }
      }
      else if (brush->cloth_deform_type == BRUSH_CLOTH_DEFORM_SNAKE_HOOK) {
        /* Cloth Snake Hook creates deformation constraint with fixed strength because the strength
         * is controlled per iteration using cloth_sim.deformation_strength. */
        cloth_brush_add_deformation_constraint(
            cloth_sim, node_index, vert, CLOTH_DEFORMATION_SNAKEHOOK_STRENGTH);
      }
    }
    else if (!cloth_sim.deformation_pos.is_empty()) {
      /* Any other tool that target the cloth simulation handle the falloff in
       * their own code when modifying the deformation coordinates of the simulation, so
       * deformation constraints are created with a fixed strength for all vertices. */
      cloth_brush_add_deformation_constraint(
          cloth_sim, node_index, vert, CLOTH_DEFORMATION_TARGET_STRENGTH);
    }

    if (pin_simulation_boundary) {
      const float sim_falloff = cloth_brush_simulation_falloff_get(
          *brush, ss.cache->initial_radius, ss.cache->location_symm, init_positions[vert]);
      /* Vertex is inside the area of the simulation without any falloff applied. */
      if (sim_falloff < 1.0f) {
        /* Create constraints with more strength the closer the vertex is to the simulation
         * boundary. */
        cloth_brush_add_pin_constraint(cloth_sim, node_index, vert, 1.0f - sim_falloff);
      }
    }
  }
}

void ensure_nodes_constraints(const Sculpt &sd,
                              Object &object,
                              const IndexMask &node_mask,
                              SimulationData &cloth_sim,
                              const float3 &initial_location,
                              const float radius)
{
  SculptSession &ss = *object.sculpt;
  bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
  const Brush *brush = BKE_paint_brush_for_read(&sd.paint);

  /* TODO: Multi-threaded needs to be disabled for this task until implementing the optimization of
   * storing the constraints per node. */
  /* Currently all constrains are added to the same global array which can't be accessed from
   * different threads. */

  IndexMaskMemory memory;
  Set<OrderedEdge> created_length_constraints;
  Vector<int> vert_indices;
  Vector<int> neighbor_offsets;
  Vector<int> neighbor_data;
  switch (pbvh.type()) {
    case bke::pbvh::Type::Mesh: {
      MutableSpan<bke::pbvh::MeshNode> nodes = pbvh.nodes<bke::pbvh::MeshNode>();
      const IndexMask uninitialized_nodes = IndexMask::from_predicate(
          node_mask, GrainSize(1024), memory, [&](const int i) {
            const int node_index = cloth_sim.node_state_index.lookup(&nodes[i]);
            return cloth_sim.node_state[node_index] == SCULPT_CLOTH_NODE_UNINITIALIZED;
          });
      const Mesh &mesh = *static_cast<const Mesh *>(object.data);
      const OffsetIndices faces = mesh.faces();
      const Span<int> corner_verts = mesh.corner_verts();
      const GroupedSpan<int> vert_to_face_map = mesh.vert_to_face_map();
      const bke::AttributeAccessor attributes = mesh.attributes();
      const VArraySpan<bool> hide_vert = *attributes.lookup<bool>(".hide_vert",
                                                                  bke::AttrDomain::Point);
      const VArraySpan<bool> hide_poly = *attributes.lookup<bool>(".hide_poly",
                                                                  bke::AttrDomain::Face);

      Span<float3> init_positions;
      VArraySpan<float3> persistent_position;
      if (brush != nullptr && brush->flag & BRUSH_PERSISTENT) {
        persistent_position = *attributes.lookup<float3>(".sculpt_persistent_co",
                                                         bke::AttrDomain::Point);
      }
      if (persistent_position.is_empty()) {
        init_positions = cloth_sim.init_pos;
      }
      else {
        init_positions = persistent_position;
      }
      uninitialized_nodes.foreach_index([&](const int i) {
        const Span<int> verts = hide::node_visible_verts(nodes[i], hide_vert, vert_indices);
        const GroupedSpan<int> neighbors = calc_vert_neighbors(faces,
                                                               corner_verts,
                                                               vert_to_face_map,
                                                               hide_poly,
                                                               verts,
                                                               neighbor_offsets,
                                                               neighbor_data);
        add_constraints_for_verts(object,
                                  brush,
                                  initial_location,
                                  radius,
                                  init_positions,
                                  cloth_sim.node_state_index.lookup(&nodes[i]),
                                  verts,
                                  neighbors,
                                  cloth_sim,
                                  created_length_constraints);
      });
      break;
    }
    case bke::pbvh::Type::Grids: {
      MutableSpan<bke::pbvh::GridsNode> nodes = pbvh.nodes<bke::pbvh::GridsNode>();
      const IndexMask uninitialized_nodes = IndexMask::from_predicate(
          node_mask, GrainSize(1024), memory, [&](const int i) {
            const int node_index = cloth_sim.node_state_index.lookup(&nodes[i]);
            return cloth_sim.node_state[node_index] == SCULPT_CLOTH_NODE_UNINITIALIZED;
          });
      const SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;
      const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);
      const BitGroupVector<> &grid_hidden = subdiv_ccg.grid_hidden;

      Span<float3> init_positions;
      Span<float3> persistent_position;
      const std::optional<PersistentMultiresData> persistent_multires_data =
          ss.persistent_multires_data();
      if (brush != nullptr && brush->flag & BRUSH_PERSISTENT && persistent_multires_data) {
        persistent_position = persistent_multires_data->positions;
      }
      if (persistent_position.is_empty()) {
        init_positions = cloth_sim.init_pos;
      }
      else {
        init_positions = persistent_position;
      }
      uninitialized_nodes.foreach_index([&](const int i) {
        const Span<int> verts = calc_visible_vert_indices_grids(
            key, grid_hidden, nodes[i].grids(), vert_indices);
        const GroupedSpan<int> neighbors = calc_vert_neighbor_indices_grids(
            subdiv_ccg, verts, neighbor_offsets, neighbor_data);
        add_constraints_for_verts(object,
                                  brush,
                                  initial_location,
                                  radius,
                                  init_positions,
                                  cloth_sim.node_state_index.lookup(&nodes[i]),
                                  verts,
                                  neighbors,
                                  cloth_sim,
                                  created_length_constraints);
      });
      break;
    }
    case bke::pbvh::Type::BMesh: {
      MutableSpan<bke::pbvh::BMeshNode> nodes = pbvh.nodes<bke::pbvh::BMeshNode>();
      const IndexMask uninitialized_nodes = IndexMask::from_predicate(
          node_mask, GrainSize(1024), memory, [&](const int i) {
            const int node_index = cloth_sim.node_state_index.lookup(&nodes[i]);
            return cloth_sim.node_state[node_index] == SCULPT_CLOTH_NODE_UNINITIALIZED;
          });
      BMesh &bm = *ss.bm;
      vert_random_access_ensure(object);
      uninitialized_nodes.foreach_index([&](const int i) {
        const Set<BMVert *, 0> &bm_verts = BKE_pbvh_bmesh_node_unique_verts(&nodes[i]);
        const Span<int> verts = calc_visible_vert_indices_bmesh(bm_verts, vert_indices);
        const GroupedSpan<int> neighbors = calc_vert_neighbor_indices_bmesh(
            bm, verts, neighbor_offsets, neighbor_data);
        add_constraints_for_verts(object,
                                  brush,
                                  initial_location,
                                  radius,
                                  cloth_sim.init_pos,
                                  cloth_sim.node_state_index.lookup(&nodes[i]),
                                  verts,
                                  neighbors,
                                  cloth_sim,
                                  created_length_constraints);
      });
      break;
    }
  }
}

BLI_NOINLINE static void apply_forces(SimulationData &cloth_sim,
                                      const Span<float3> forces,
                                      const Span<int> verts)
{
  const float mass_inv = math::rcp(cloth_sim.mass);
  for (const int i : verts.index_range()) {
    cloth_sim.acceleration[verts[i]] += forces[i] * mass_inv;
  }
}

BLI_NOINLINE static void expand_length_constraints(SimulationData &cloth_sim,
                                                   const Span<int> verts,
                                                   const Span<float> factors)
{
  MutableSpan<float> length_constraint_tweak = cloth_sim.length_constraint_tweak;
  for (const int i : verts.index_range()) {
    length_constraint_tweak[verts[i]] += factors[i] * 0.01f;
  }
}

BLI_NOINLINE static void calc_distances_to_plane(const Span<float3> positions,
                                                 const float4 &plane,
                                                 const MutableSpan<float> distances)
{
  for (const int i : positions.index_range()) {
    distances[i] = dist_to_plane_v3(positions[i], plane);
  }
}

BLI_NOINLINE static void clamp_factors(const MutableSpan<float> factors,
                                       const float min,
                                       const float max)
{
  for (float &factor : factors) {
    factor = std::clamp(factor, min, max);
  }
}

BLI_NOINLINE static void apply_grab_brush(SimulationData &cloth_sim,
                                          const Span<int> verts,
                                          const MutableSpan<float> factors,
                                          const bool use_falloff_plane,
                                          const float3 &grab_delta_symmetry)
{
  for (const int i : verts.index_range()) {
    cloth_sim.deformation_pos[verts[i]] = cloth_sim.init_pos[verts[i]] +
                                          grab_delta_symmetry * factors[i];
  }
  if (use_falloff_plane) {
    clamp_factors(factors, 0.0f, 1.0f);
    scatter_data_mesh(factors.as_span(), verts, cloth_sim.deformation_strength.as_mutable_span());
  }
  else {
    cloth_sim.deformation_strength.as_mutable_span().fill_indices(verts, 1.0f);
  }
}

BLI_NOINLINE static void apply_snake_hook_brush(SimulationData &cloth_sim,
                                                const Span<int> verts,
                                                const MutableSpan<float> factors,
                                                const float3 &grab_delta_symmetry)
{
  for (const int i : verts.index_range()) {
    const int vert = verts[i];
    cloth_sim.deformation_pos[vert] = cloth_sim.pos[vert] + grab_delta_symmetry * factors[i];
  }
  scatter_data_mesh(factors.as_span(), verts, cloth_sim.deformation_strength.as_mutable_span());
}

BLI_NOINLINE static void calc_pinch_forces(const Span<float3> positions,
                                           const float3 &location,
                                           const MutableSpan<float3> forces)
{
  for (const int i : forces.index_range()) {
    forces[i] = math::normalize(location - positions[i]);
  }
}

BLI_NOINLINE static void calc_plane_pinch_forces(const Span<float3> positions,
                                                 const float4 &plane,
                                                 const float3 &plane_normal,
                                                 const MutableSpan<float3> forces)
{
  for (const int i : positions.index_range()) {
    const float distance = dist_signed_to_plane_v3(positions[i], plane);
    forces[i] = math::normalize(plane_normal * -distance);
  }
}

BLI_NOINLINE static void calc_perpendicular_pinch_forces(const Span<float3> positions,
                                                         const float4x4 &imat,
                                                         const float3 &location,
                                                         const MutableSpan<float3> forces)
{
  const float3 x_object_space = math::normalize(imat.x_axis());
  const float3 z_object_space = math::normalize(imat.z_axis());
  for (const int i : positions.index_range()) {
    const float3 disp_center = math::normalize(location - positions[i]);
    const float3 x_disp = x_object_space * math::dot(disp_center, x_object_space);
    const float3 z_disp = z_object_space * math::dot(disp_center, z_object_space);
    forces[i] = x_disp + z_disp;
  }
}

struct LocalData {
  Vector<int> vert_indices;
  Vector<float> factors;
  Vector<float> distances;
  Vector<float> sim_factors;
  Vector<float3> positions;
  Vector<float3> init_positions;
  Vector<float3> diffs;
  Vector<float3> translations;
};

struct FalloffPlane {
  float4 plane;
  float3 normal;
};

static void calc_forces_mesh(const Depsgraph &depsgraph,
                             Object &ob,
                             const Brush &brush,
                             const float3 &offset,
                             const float4x4 &imat,
                             const float3 &sim_location,
                             const float3 &gravity,
                             const std::optional<FalloffPlane> &falloff_plane,
                             const MeshAttributeData &attribute_data,
                             const Span<float3> positions_eval,
                             const Span<float3> vert_normals,
                             const bke::pbvh::MeshNode &node,
                             LocalData &tls)
{
  SculptSession &ss = *ob.sculpt;
  SimulationData &cloth_sim = *ss.cache->cloth_sim;
  const StrokeCache &cache = *ss.cache;

  const Span<int> verts = node.verts();
  const MutableSpan positions = gather_data_mesh(positions_eval, verts, tls.positions);
  const MutableSpan init_positions = gather_data_mesh(
      cloth_sim.init_pos.as_span(), verts, tls.init_positions);
  const Span<float3> current_positions = brush.cloth_deform_type == BRUSH_CLOTH_DEFORM_GRAB ?
                                             init_positions :
                                             positions;

  tls.factors.resize(verts.size());
  const MutableSpan<float> factors = tls.factors;
  fill_factor_from_hide_and_mask(attribute_data.hide_vert, attribute_data.mask, verts, factors);
  filter_region_clip_factors(ss, current_positions, factors);

  calc_brush_simulation_falloff(brush, cache.radius, sim_location, positions, factors);

  tls.translations.resize(verts.size());
  const MutableSpan<float3> forces = tls.translations;

  /* Apply gravity in the entire simulation area before brush distances are taken into account. */
  if (!math::is_zero(gravity)) {
    translations_from_offset_and_factors(gravity, factors, forces);
    apply_forces(cloth_sim, forces, verts);
  }

  if (brush.flag & BRUSH_FRONTFACE) {
    calc_front_face(cache.view_normal_symm, vert_normals, verts, factors);
  }

  tls.distances.resize(verts.size());
  const MutableSpan<float> distances = tls.distances;
  if (falloff_plane) {
    calc_distances_to_plane(current_positions, falloff_plane->plane, distances);
  }
  else {
    calc_brush_distances(
        ss, current_positions, eBrushFalloffShape(brush.falloff_shape), distances);
  }
  filter_distances_with_radius(cache.radius, distances, factors);
  apply_hardness_to_distances(cache, distances);
  calc_brush_strength_factors(cache, brush, distances, factors);

  const auto_mask::Cache *automask = auto_mask::active_cache_get(ss);
  auto_mask::calc_vert_factors(depsgraph, ob, automask, node, verts, factors);

  calc_brush_texture_factors(ss, brush, current_positions, factors);

  scale_factors(factors, cache.bstrength);

  switch (brush.cloth_deform_type) {
    case BRUSH_CLOTH_DEFORM_DRAG:
      translations_from_offset_and_factors(
          math::normalize(cache.location_symm - cache.last_location_symm), factors, forces);
      apply_forces(cloth_sim, forces, verts);
      break;
    case BRUSH_CLOTH_DEFORM_PUSH:
      translations_from_offset_and_factors(-offset, factors, forces);
      apply_forces(cloth_sim, forces, verts);
      break;
    case BRUSH_CLOTH_DEFORM_GRAB:
      apply_grab_brush(
          cloth_sim, verts, factors, falloff_plane.has_value(), cache.grab_delta_symm);
      break;
    case BRUSH_CLOTH_DEFORM_SNAKE_HOOK:
      apply_snake_hook_brush(cloth_sim, verts, factors, cache.grab_delta_symm);
      break;
    case BRUSH_CLOTH_DEFORM_PINCH_POINT:
      if (falloff_plane) {
        calc_plane_pinch_forces(positions, falloff_plane->plane, falloff_plane->normal, forces);
      }
      else {
        calc_pinch_forces(positions, cache.location_symm, forces);
      }
      scale_translations(forces, factors);
      apply_forces(cloth_sim, forces, verts);
      break;
    case BRUSH_CLOTH_DEFORM_PINCH_PERPENDICULAR: {
      calc_perpendicular_pinch_forces(positions, imat, cache.location_symm, forces);
      scale_translations(forces, factors);
      apply_forces(cloth_sim, forces, verts);
      break;
    }
    case BRUSH_CLOTH_DEFORM_INFLATE:
      gather_data_mesh(vert_normals, verts, forces);
      scale_translations(forces, factors);
      apply_forces(cloth_sim, forces, verts);
      break;
    case BRUSH_CLOTH_DEFORM_EXPAND:
      expand_length_constraints(cloth_sim, verts, factors);
      break;
  }
}

static void calc_forces_grids(const Depsgraph &depsgraph,
                              Object &ob,
                              const Brush &brush,
                              const float3 &offset,
                              const float4x4 &imat,
                              const float3 &sim_location,
                              const float3 &gravity,
                              const std::optional<FalloffPlane> &falloff_plane,
                              const bke::pbvh::GridsNode &node,
                              LocalData &tls)
{
  SculptSession &ss = *ob.sculpt;
  SimulationData &cloth_sim = *ss.cache->cloth_sim;
  const StrokeCache &cache = *ss.cache;
  const SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;
  const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);

  const Span<int> grids = node.grids();
  const MutableSpan positions = gather_grids_positions(subdiv_ccg, grids, tls.positions);
  const MutableSpan init_positions = gather_data_grids(
      subdiv_ccg, cloth_sim.init_pos.as_span(), grids, tls.init_positions);
  const Span<float3> current_positions = brush.cloth_deform_type == BRUSH_CLOTH_DEFORM_GRAB ?
                                             init_positions :
                                             positions;

  tls.factors.resize(positions.size());
  const MutableSpan<float> factors = tls.factors;
  fill_factor_from_hide_and_mask(subdiv_ccg, grids, factors);
  filter_region_clip_factors(ss, current_positions, factors);

  calc_brush_simulation_falloff(brush, cache.radius, sim_location, positions, factors);

  const Span<int> verts = calc_vert_indices_grids(key, grids, tls.vert_indices);

  tls.translations.resize(verts.size());
  const MutableSpan<float3> forces = tls.translations;

  /* Apply gravity in the entire simulation area before brush distances are taken into account. */
  if (!math::is_zero(gravity)) {
    translations_from_offset_and_factors(gravity, factors, forces);
    apply_forces(cloth_sim, forces, verts);
  }

  if (brush.flag & BRUSH_FRONTFACE) {
    calc_front_face(cache.view_normal_symm, subdiv_ccg, grids, factors);
  }

  tls.distances.resize(verts.size());
  const MutableSpan<float> distances = tls.distances;
  if (falloff_plane) {
    calc_distances_to_plane(current_positions, falloff_plane->plane, distances);
  }
  else {
    calc_brush_distances(
        ss, current_positions, eBrushFalloffShape(brush.falloff_shape), distances);
  }
  filter_distances_with_radius(cache.radius, distances, factors);
  apply_hardness_to_distances(cache, distances);
  calc_brush_strength_factors(cache, brush, distances, factors);

  const auto_mask::Cache *automask = auto_mask::active_cache_get(ss);
  auto_mask::calc_grids_factors(depsgraph, ob, automask, node, grids, factors);

  calc_brush_texture_factors(ss, brush, current_positions, factors);

  scale_factors(factors, cache.bstrength);

  switch (brush.cloth_deform_type) {
    case BRUSH_CLOTH_DEFORM_DRAG:
      translations_from_offset_and_factors(
          math::normalize(cache.location_symm - cache.last_location_symm), factors, forces);
      apply_forces(cloth_sim, forces, verts);
      break;
    case BRUSH_CLOTH_DEFORM_PUSH:
      translations_from_offset_and_factors(-offset, factors, forces);
      apply_forces(cloth_sim, forces, verts);
      break;
    case BRUSH_CLOTH_DEFORM_GRAB:
      apply_grab_brush(
          cloth_sim, verts, factors, falloff_plane.has_value(), cache.grab_delta_symm);
      break;
    case BRUSH_CLOTH_DEFORM_SNAKE_HOOK:
      apply_snake_hook_brush(cloth_sim, verts, factors, cache.grab_delta_symm);
      break;
    case BRUSH_CLOTH_DEFORM_PINCH_POINT:
      if (falloff_plane) {
        calc_plane_pinch_forces(positions, falloff_plane->plane, falloff_plane->normal, forces);
      }
      else {
        calc_pinch_forces(positions, cache.location_symm, forces);
      }
      scale_translations(forces, factors);
      apply_forces(cloth_sim, forces, verts);
      break;
    case BRUSH_CLOTH_DEFORM_PINCH_PERPENDICULAR: {
      calc_perpendicular_pinch_forces(positions, imat, cache.location_symm, forces);
      scale_translations(forces, factors);
      apply_forces(cloth_sim, forces, verts);
      break;
    }
    case BRUSH_CLOTH_DEFORM_INFLATE:
      gather_grids_normals(subdiv_ccg, grids, forces);
      scale_translations(forces, factors);
      apply_forces(cloth_sim, forces, verts);
      break;
    case BRUSH_CLOTH_DEFORM_EXPAND:
      expand_length_constraints(cloth_sim, verts, factors);
      break;
  }
}

static void calc_forces_bmesh(const Depsgraph &depsgraph,
                              Object &ob,
                              const Brush &brush,
                              const float3 &offset,
                              const float4x4 &imat,
                              const float3 &sim_location,
                              const float3 &gravity,
                              const std::optional<FalloffPlane> &falloff_plane,
                              bke::pbvh::BMeshNode &node,
                              LocalData &tls)
{
  SculptSession &ss = *ob.sculpt;
  SimulationData &cloth_sim = *ss.cache->cloth_sim;
  const StrokeCache &cache = *ss.cache;

  const Set<BMVert *, 0> &bm_verts = BKE_pbvh_bmesh_node_unique_verts(&node);
  const Span<int> verts = calc_vert_indices_bmesh(bm_verts, tls.vert_indices);

  const MutableSpan positions = gather_bmesh_positions(bm_verts, tls.positions);
  const MutableSpan init_positions = gather_data_mesh(
      cloth_sim.init_pos.as_span(), verts, tls.init_positions);
  const Span<float3> current_positions = brush.cloth_deform_type == BRUSH_CLOTH_DEFORM_GRAB ?
                                             init_positions :
                                             positions;

  tls.factors.resize(verts.size());
  const MutableSpan<float> factors = tls.factors;
  fill_factor_from_hide_and_mask(*ss.bm, bm_verts, factors);
  filter_region_clip_factors(ss, current_positions, factors);

  calc_brush_simulation_falloff(brush, cache.radius, sim_location, positions, factors);

  tls.translations.resize(verts.size());
  const MutableSpan<float3> forces = tls.translations;

  /* Apply gravity in the entire simulation area before brush distances are taken into account. */
  if (!math::is_zero(gravity)) {
    translations_from_offset_and_factors(gravity, factors, forces);
    apply_forces(cloth_sim, forces, verts);
  }

  if (brush.flag & BRUSH_FRONTFACE) {
    calc_front_face(cache.view_normal_symm, bm_verts, factors);
  }

  tls.distances.resize(verts.size());
  const MutableSpan<float> distances = tls.distances;
  if (falloff_plane) {
    calc_distances_to_plane(current_positions, falloff_plane->plane, distances);
  }
  else {
    calc_brush_distances(
        ss, current_positions, eBrushFalloffShape(brush.falloff_shape), distances);
  }
  filter_distances_with_radius(cache.radius, distances, factors);
  apply_hardness_to_distances(cache, distances);
  calc_brush_strength_factors(cache, brush, distances, factors);

  const auto_mask::Cache *automask = auto_mask::active_cache_get(ss);
  auto_mask::calc_vert_factors(depsgraph, ob, automask, node, bm_verts, factors);

  calc_brush_texture_factors(ss, brush, current_positions, factors);

  scale_factors(factors, cache.bstrength);

  switch (brush.cloth_deform_type) {
    case BRUSH_CLOTH_DEFORM_DRAG:
      translations_from_offset_and_factors(
          math::normalize(cache.location_symm - cache.last_location_symm), factors, forces);
      apply_forces(cloth_sim, forces, verts);
      break;
    case BRUSH_CLOTH_DEFORM_PUSH:
      translations_from_offset_and_factors(-offset, factors, forces);
      apply_forces(cloth_sim, forces, verts);
      break;
    case BRUSH_CLOTH_DEFORM_GRAB:
      apply_grab_brush(
          cloth_sim, verts, factors, falloff_plane.has_value(), cache.grab_delta_symm);
      break;
    case BRUSH_CLOTH_DEFORM_SNAKE_HOOK:
      apply_snake_hook_brush(cloth_sim, verts, factors, cache.grab_delta_symm);
      break;
    case BRUSH_CLOTH_DEFORM_PINCH_POINT:
      if (falloff_plane) {
        calc_plane_pinch_forces(positions, falloff_plane->plane, falloff_plane->normal, forces);
      }
      else {
        calc_pinch_forces(positions, cache.location_symm, forces);
      }
      scale_translations(forces, factors);
      apply_forces(cloth_sim, forces, verts);
      break;
    case BRUSH_CLOTH_DEFORM_PINCH_PERPENDICULAR: {
      calc_perpendicular_pinch_forces(positions, imat, cache.location_symm, forces);
      scale_translations(forces, factors);
      apply_forces(cloth_sim, forces, verts);
      break;
    }
    case BRUSH_CLOTH_DEFORM_INFLATE:
      gather_bmesh_normals(bm_verts, forces);
      scale_translations(forces, factors);
      apply_forces(cloth_sim, forces, verts);
      break;
    case BRUSH_CLOTH_DEFORM_EXPAND:
      expand_length_constraints(cloth_sim, verts, factors);
      break;
  }
}

static Vector<ColliderCache> cloth_brush_collider_cache_create(Object &object,
                                                               const Depsgraph &depsgraph)
{
  Vector<ColliderCache> cache;
  DEGObjectIterSettings deg_iter_settings = {nullptr};
  deg_iter_settings.depsgraph = &const_cast<Depsgraph &>(depsgraph);
  deg_iter_settings.flags = DEG_ITER_OBJECT_FLAG_LINKED_DIRECTLY | DEG_ITER_OBJECT_FLAG_VISIBLE |
                            DEG_ITER_OBJECT_FLAG_DUPLI;
  DEG_OBJECT_ITER_BEGIN (&deg_iter_settings, ob) {
    if (STREQ(object.id.name, ob->id.name)) {
      continue;
    }

    CollisionModifierData *cmd = (CollisionModifierData *)BKE_modifiers_findby_type(
        ob, eModifierType_Collision);
    if (!cmd) {
      continue;
    }

    if (!cmd->bvhtree) {
      continue;
    }

    ColliderCache col{};
    col.ob = ob;
    col.collmd = cmd;
    collision_move_object(cmd, 1.0, 0.0, true);
    cache.append(col);
  }
  DEG_OBJECT_ITER_END;
  return cache;
}

struct ClothBrushCollision {
  CollisionModifierData *col_data;
  IsectRayPrecalc isect_precalc;
};

static void cloth_brush_collision_cb(void *userdata,
                                     int index,
                                     const BVHTreeRay *ray,
                                     BVHTreeRayHit *hit)
{
  ClothBrushCollision *col = (ClothBrushCollision *)userdata;
  CollisionModifierData *col_data = col->col_data;
  const int3 vert_tri = col_data->vert_tris[index];
  float (*positions)[3] = col_data->x;
  float *tri[3], no[3], co[3];

  tri[0] = positions[vert_tri[0]];
  tri[1] = positions[vert_tri[1]];
  tri[2] = positions[vert_tri[2]];
  float dist = 0.0f;

  bool tri_hit = isect_ray_tri_watertight_v3(
      ray->origin, &col->isect_precalc, UNPACK3(tri), &dist, nullptr);
  normal_tri_v3(no, UNPACK3(tri));
  madd_v3_v3v3fl(co, ray->origin, ray->direction, dist);

  if (tri_hit && dist < hit->dist) {
    hit->index = index;
    hit->dist = dist;

    copy_v3_v3(hit->co, co);
    copy_v3_v3(hit->no, no);
  }
}

static void cloth_brush_solve_collision(const Object &object,
                                        SimulationData &cloth_sim,
                                        const int i)
{
  const int raycast_flag = BVH_RAYCAST_DEFAULT & ~BVH_RAYCAST_WATERTIGHT;

  const float4x4 &object_to_world = object.object_to_world();
  const float4x4 &world_to_object = object.world_to_object();

  for (const ColliderCache &collider_cache : cloth_sim.collider_list) {
    const float3 pos_world_space = math::transform_point(object_to_world, cloth_sim.pos[i]);
    const float3 prev_pos_world_space = math::transform_point(object_to_world,
                                                              cloth_sim.last_iteration_pos[i]);

    BVHTreeRayHit hit{};
    hit.index = -1;

    const float3 ray_normal = math::normalize_and_get_length(
        pos_world_space - prev_pos_world_space, hit.dist);

    ClothBrushCollision col;
    CollisionModifierData *collmd = collider_cache.collmd;
    col.col_data = collmd;
    isect_ray_tri_watertight_v3_precalc(&col.isect_precalc, ray_normal);

    BLI_bvhtree_ray_cast_ex(collmd->bvhtree,
                            prev_pos_world_space,
                            ray_normal,
                            0.3f,
                            &hit,
                            cloth_brush_collision_cb,
                            &col,
                            raycast_flag);

    if (hit.index == -1) {
      continue;
    }

    const float3 collision_disp = float3(hit.no) * 0.005f;

    float4 friction_plane;
    plane_from_point_normal_v3(friction_plane, hit.co, hit.no);
    float3 pos_on_friction_plane;
    closest_to_plane_v3(pos_on_friction_plane, friction_plane, pos_world_space);
    constexpr float friction_factor = 0.35f;
    const float3 movement_disp = (pos_on_friction_plane - float3(hit.co)) * friction_factor;

    cloth_sim.pos[i] = math::transform_point(world_to_object,
                                             float3(hit.co) + movement_disp + collision_disp);
  }
}

BLI_NOINLINE static void solve_verts_simulation(const Object &object,
                                                const Brush *brush,
                                                const float3 &sim_location,
                                                const Span<int> verts,
                                                const MutableSpan<float> factors,
                                                LocalData &tls,
                                                SimulationData &cloth_sim)
{
  const SculptSession &ss = *object.sculpt;

  tls.diffs.resize(verts.size());
  const MutableSpan<float3> pos_diff = tls.diffs;
  for (const int i : verts.index_range()) {
    pos_diff[i] = cloth_sim.pos[verts[i]] - cloth_sim.prev_pos[verts[i]];
  }

  for (const int vert : verts) {
    cloth_sim.prev_pos[vert] = cloth_sim.pos[vert];
  }

  for (const int i : verts.index_range()) {
    const int vert = verts[i];
    cloth_sim.pos[vert] += cloth_sim.acceleration[vert] * factors[i] * CLOTH_SIMULATION_TIME_STEP;
  }

  scale_factors(factors, 1.0f - cloth_sim.damping);
  if (ss.cache) {
    const MutableSpan positions = gather_data_mesh(
        cloth_sim.init_pos.as_span(), verts, tls.positions);
    calc_brush_simulation_falloff(*brush, ss.cache->radius, sim_location, positions, factors);
  }
  scale_translations(pos_diff, factors);

  for (const int i : verts.index_range()) {
    const int vert = verts[i];
    cloth_sim.pos[vert] += pos_diff[i];
  }

  for (const int vert : verts) {
    cloth_brush_solve_collision(object, cloth_sim, vert);
  }

  for (const int vert : verts) {
    cloth_sim.last_iteration_pos[vert] = cloth_sim.pos[vert];
  }

  cloth_sim.acceleration.as_mutable_span().fill_indices(verts, float3(0));
}

static void calc_constraint_factors(const Depsgraph &depsgraph,
                                    const Object &object,
                                    const Brush *brush,
                                    const float3 &sim_location,
                                    const Span<float3> init_positions,
                                    const MutableSpan<float> cloth_factors)
{
  const SculptSession &ss = *object.sculpt;
  const bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
  IndexMaskMemory memory;
  const IndexMask node_mask = bke::pbvh::all_leaf_nodes(pbvh, memory);

  const auto_mask::Cache *automasking = auto_mask::active_cache_get(ss);

  struct LocalData {
    Vector<float> factors;
    Vector<float3> positions;
  };
  threading::EnumerableThreadSpecific<LocalData> all_tls;
  switch (pbvh.type()) {
    case bke::pbvh::Type::Mesh: {
      const Mesh &mesh = *static_cast<const Mesh *>(object.data);
      const MeshAttributeData attribute_data(mesh);
      const Span<bke::pbvh::MeshNode> nodes = pbvh.nodes<bke::pbvh::MeshNode>();
      node_mask.foreach_index(GrainSize(1), [&](const int i) {
        LocalData &tls = all_tls.local();
        const Span<int> verts = nodes[i].verts();
        tls.factors.resize(verts.size());
        const MutableSpan<float> factors = tls.factors;
        fill_factor_from_hide_and_mask(
            attribute_data.hide_vert, attribute_data.mask, verts, factors);
        auto_mask::calc_vert_factors(depsgraph, object, automasking, nodes[i], verts, factors);
        if (ss.cache) {
          const MutableSpan positions = gather_data_mesh(init_positions, verts, tls.positions);
          calc_brush_simulation_falloff(
              *brush, ss.cache->radius, sim_location, positions, factors);
        }
        scatter_data_mesh(factors.as_span(), verts, cloth_factors);
      });
      break;
    }
    case bke::pbvh::Type::Grids: {
      const SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;
      const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);
      const Span<bke::pbvh::GridsNode> nodes = pbvh.nodes<bke::pbvh::GridsNode>();
      node_mask.foreach_index(GrainSize(1), [&](const int i) {
        LocalData &tls = all_tls.local();
        const Span<int> grids = nodes[i].grids();
        const int grid_verts_num = grids.size() * key.grid_area;
        tls.factors.resize(grid_verts_num);
        const MutableSpan<float> factors = tls.factors;
        fill_factor_from_hide_and_mask(subdiv_ccg, grids, factors);
        auto_mask::calc_grids_factors(depsgraph, object, automasking, nodes[i], grids, factors);
        if (ss.cache) {
          const Span<float3> positions = gather_data_grids(
              subdiv_ccg, init_positions, grids, tls.positions);
          calc_brush_simulation_falloff(
              *brush, ss.cache->radius, sim_location, positions, factors);
        }
        scatter_data_grids(subdiv_ccg, factors.as_span(), grids, cloth_factors);
      });
      break;
    }
    case bke::pbvh::Type::BMesh: {
      const BMesh &bm = *ss.bm;
      const Span<bke::pbvh::BMeshNode> nodes = pbvh.nodes<bke::pbvh::BMeshNode>();
      node_mask.foreach_index(GrainSize(1), [&](const int i) {
        LocalData &tls = all_tls.local();
        const Set<BMVert *, 0> &verts = BKE_pbvh_bmesh_node_unique_verts(
            const_cast<bke::pbvh::BMeshNode *>(&nodes[i]));
        tls.factors.resize(verts.size());
        const MutableSpan<float> factors = tls.factors;
        fill_factor_from_hide_and_mask(bm, verts, factors);
        auto_mask::calc_vert_factors(depsgraph, object, automasking, nodes[i], verts, factors);
        if (ss.cache) {
          const MutableSpan positions = gather_data_bmesh(init_positions, verts, tls.positions);
          calc_brush_simulation_falloff(
              *brush, ss.cache->radius, sim_location, positions, factors);
        }
        scatter_data_bmesh(factors.as_span(), verts, cloth_factors);
      });
      break;
    }
  }
}

static void cloth_brush_satisfy_constraints(const Depsgraph &depsgraph,
                                            const Object &object,
                                            const Brush *brush,
                                            SimulationData &cloth_sim)
{
  const SculptSession &ss = *object.sculpt;

  const float3 sim_location = cloth_brush_simulation_location_get(ss, brush);

  /* Precalculate factors into an array since we need random access to specific vertex values. */
  Array<float> factors(SCULPT_vertex_count_get(object));
  calc_constraint_factors(depsgraph, object, brush, sim_location, cloth_sim.init_pos, factors);

  for (int constraint_it = 0; constraint_it < CLOTH_SIMULATION_ITERATIONS; constraint_it++) {
    for (const LengthConstraint &constraint : cloth_sim.length_constraints) {
      if (cloth_sim.node_state[constraint.node] != SCULPT_CLOTH_NODE_ACTIVE) {
        /* Skip all constraints that were created for inactive nodes. */
        continue;
      }

      const int v1 = constraint.elem_index_a;
      const int v2 = constraint.elem_index_b;

      const float3 v1_to_v2 = float3(constraint.elem_position_b) -
                              float3(constraint.elem_position_a);
      const float current_distance = math::length(v1_to_v2);
      float3 correction_vector;

      const float constraint_distance = constraint.length +
                                        (cloth_sim.length_constraint_tweak[v1] * 0.5f) +
                                        (cloth_sim.length_constraint_tweak[v2] * 0.5f);

      if (current_distance > 0.0f) {
        correction_vector = v1_to_v2 * CLOTH_SOLVER_DISPLACEMENT_FACTOR *
                            (1.0f - (constraint_distance / current_distance));
      }
      else {
        correction_vector = v1_to_v2 * CLOTH_SOLVER_DISPLACEMENT_FACTOR;
      }

      const float3 correction_vector_half = correction_vector * 0.5f;

      const float factor_v1 = factors[v1];
      const float factor_v2 = factors[v2];

      float deformation_strength = 1.0f;
      if (constraint.type == SCULPT_CLOTH_CONSTRAINT_DEFORMATION) {
        deformation_strength = (cloth_sim.deformation_strength[v1] +
                                cloth_sim.deformation_strength[v2]) *
                               0.5f;
      }

      if (constraint.type == SCULPT_CLOTH_CONSTRAINT_SOFTBODY) {
        const float softbody_plasticity = brush ? brush->cloth_constraint_softbody_strength : 0.0f;
        cloth_sim.pos[v1] += correction_vector_half *
                             (1.0f * factor_v1 * constraint.strength * softbody_plasticity);
        cloth_sim.softbody_pos[v1] += correction_vector_half * -1.0f * factor_v1 *
                                      constraint.strength * (1.0f - softbody_plasticity);
      }
      else {
        cloth_sim.pos[v1] += correction_vector_half * 1.0f * factor_v1 * constraint.strength *
                             deformation_strength;
        if (v1 != v2) {
          cloth_sim.pos[v2] += correction_vector_half * -1.0f * factor_v2 * constraint.strength *
                               deformation_strength;
        }
      }
    }
  }
}

void do_simulation_step(const Depsgraph &depsgraph,
                        const Sculpt &sd,
                        Object &object,
                        SimulationData &cloth_sim,
                        const IndexMask &node_mask)
{
  SculptSession &ss = *object.sculpt;
  bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
  const Brush *brush = BKE_paint_brush_for_read(&sd.paint);

  /* Update the constraints. */
  cloth_brush_satisfy_constraints(depsgraph, object, brush, cloth_sim);

  const float3 sim_location = cloth_brush_simulation_location_get(ss, brush);

  IndexMaskMemory memory;
  threading::EnumerableThreadSpecific<LocalData> all_tls;
  switch (pbvh.type()) {
    case bke::pbvh::Type::Mesh: {
      MutableSpan<bke::pbvh::MeshNode> nodes = pbvh.nodes<bke::pbvh::MeshNode>();
      const IndexMask active_nodes = IndexMask::from_predicate(
          node_mask, GrainSize(1024), memory, [&](const int i) {
            const int node_index = cloth_sim.node_state_index.lookup(&nodes[i]);
            return cloth_sim.node_state[node_index] == SCULPT_CLOTH_NODE_ACTIVE;
          });
      Mesh &mesh = *static_cast<Mesh *>(object.data);
      const MeshAttributeData attribute_data(mesh);
      const PositionDeformData position_data(depsgraph, object);
      active_nodes.foreach_index(GrainSize(1), [&](const int i) {
        LocalData &tls = all_tls.local();
        const Span<int> verts = nodes[i].verts();

        tls.factors.resize(verts.size());
        const MutableSpan<float> factors = tls.factors;
        fill_factor_from_hide_and_mask(
            attribute_data.hide_vert, attribute_data.mask, verts, factors);
        const auto_mask::Cache *automasking = auto_mask::active_cache_get(ss);
        auto_mask::calc_vert_factors(depsgraph, object, automasking, nodes[i], verts, factors);

        solve_verts_simulation(object, brush, sim_location, verts, factors, tls, cloth_sim);

        tls.translations.resize(verts.size());
        const MutableSpan<float3> translations = tls.translations;
        for (const int i : verts.index_range()) {
          translations[i] = cloth_sim.pos[verts[i]] - position_data.eval[verts[i]];
        }

        clip_and_lock_translations(sd, ss, position_data.eval, verts, translations);
        position_data.deform(translations, verts);

        cloth_sim.node_state[cloth_sim.node_state_index.lookup(&nodes[i])] =
            SCULPT_CLOTH_NODE_INACTIVE;
        bke::pbvh::update_node_bounds_mesh(position_data.eval, nodes[i]);
      });
      break;
    }
    case bke::pbvh::Type::Grids: {
      MutableSpan<bke::pbvh::GridsNode> nodes = pbvh.nodes<bke::pbvh::GridsNode>();
      const IndexMask active_nodes = IndexMask::from_predicate(
          node_mask, GrainSize(1024), memory, [&](const int i) {
            const int node_index = cloth_sim.node_state_index.lookup(&nodes[i]);
            return cloth_sim.node_state[node_index] == SCULPT_CLOTH_NODE_ACTIVE;
          });
      SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;
      const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);
      const Span<float3> cloth_positions = cloth_sim.pos;
      MutableSpan<float3> positions = subdiv_ccg.positions;
      active_nodes.foreach_index(GrainSize(1), [&](const int i) {
        LocalData &tls = all_tls.local();
        const Span<int> grids = nodes[i].grids();
        const int grid_verts_num = grids.size() * key.grid_area;

        tls.factors.resize(grid_verts_num);
        const MutableSpan<float> factors = tls.factors;
        fill_factor_from_hide_and_mask(subdiv_ccg, grids, factors);
        const auto_mask::Cache *automasking = auto_mask::active_cache_get(ss);
        auto_mask::calc_grids_factors(depsgraph, object, automasking, nodes[i], grids, factors);

        const Span<int> verts = calc_vert_indices_grids(key, grids, tls.vert_indices);
        solve_verts_simulation(object, brush, sim_location, verts, factors, tls, cloth_sim);

        for (const int grid : grids) {
          const IndexRange grid_range = bke::ccg::grid_range(key, grid);
          positions.slice(grid_range).copy_from(cloth_positions.slice(grid_range));
        }

        cloth_sim.node_state[cloth_sim.node_state_index.lookup(&nodes[i])] =
            SCULPT_CLOTH_NODE_INACTIVE;
        bke::pbvh::update_node_bounds_grids(subdiv_ccg.grid_area, positions, nodes[i]);
      });
      break;
    }
    case bke::pbvh::Type::BMesh: {
      MutableSpan<bke::pbvh::BMeshNode> nodes = pbvh.nodes<bke::pbvh::BMeshNode>();
      const IndexMask active_nodes = IndexMask::from_predicate(
          node_mask, GrainSize(1024), memory, [&](const int i) {
            const int node_index = cloth_sim.node_state_index.lookup(&nodes[i]);
            return cloth_sim.node_state[node_index] == SCULPT_CLOTH_NODE_ACTIVE;
          });
      BMesh &bm = *ss.bm;
      active_nodes.foreach_index(GrainSize(1), [&](const int i) {
        LocalData &tls = all_tls.local();
        const Set<BMVert *, 0> &verts = BKE_pbvh_bmesh_node_unique_verts(&nodes[i]);

        tls.factors.resize(verts.size());
        const MutableSpan<float> factors = tls.factors;
        fill_factor_from_hide_and_mask(bm, verts, factors);
        const auto_mask::Cache *automasking = auto_mask::active_cache_get(ss);
        auto_mask::calc_vert_factors(depsgraph, object, automasking, nodes[i], verts, factors);

        const Span<int> vert_indices = calc_vert_indices_bmesh(verts, tls.vert_indices);
        solve_verts_simulation(object, brush, sim_location, vert_indices, factors, tls, cloth_sim);

        for (BMVert *vert : verts) {
          copy_v3_v3(vert->co, cloth_sim.pos[BM_elem_index_get(vert)]);
        }

        cloth_sim.node_state[cloth_sim.node_state_index.lookup(&nodes[i])] =
            SCULPT_CLOTH_NODE_INACTIVE;
        bke::pbvh::update_node_bounds_bmesh(nodes[i]);
      });
      break;
    }
  }
  pbvh.tag_positions_changed(node_mask);
  pbvh.flush_bounds_to_parents();
}

static void cloth_brush_apply_brush_forces(const Depsgraph &depsgraph,
                                           const Sculpt &sd,
                                           Object &ob,
                                           const IndexMask &node_mask)
{
  SculptSession &ss = *ob.sculpt;
  StrokeCache &cache = *ss.cache;
  const Brush &brush = *BKE_paint_brush_for_read(&sd.paint);

  float3 area_no;
  float3 area_co;
  float3 offset;

  if (math::is_zero(cache.grab_delta_symm)) {
    return;
  }

  float3 grab_delta = math::normalize(cache.grab_delta_symm);

  /* Calculate push offset. */
  if (brush.cloth_deform_type == BRUSH_CLOTH_DEFORM_PUSH) {
    offset = cache.sculpt_normal_symm * cache.radius * cache.scale * 2.0f;
  }

  float4x4 mat = float4x4::identity();
  if (brush.cloth_deform_type == BRUSH_CLOTH_DEFORM_PINCH_PERPENDICULAR ||
      brush.cloth_force_falloff_type == BRUSH_CLOTH_FORCE_FALLOFF_PLANE)
  {
    calc_brush_plane(depsgraph, brush, ob, node_mask, area_no, area_co);

    /* Initialize stroke local space matrix. */
    mat.x_axis() = math::cross(area_no, cache.grab_delta_symm);
    mat.y_axis() = math::cross(area_no, mat.x_axis());
    mat.z_axis() = area_no;
    mat.location() = cache.location_symm;
    normalize_m4(mat.ptr());

    /* Update matrix for the cursor preview. */
    if (cache.mirror_symmetry_pass == 0) {
      cache.stroke_local_mat = mat;
    }
  }

  if (ELEM(brush.cloth_deform_type, BRUSH_CLOTH_DEFORM_SNAKE_HOOK, BRUSH_CLOTH_DEFORM_GRAB)) {
    /* Set the deformation strength to 0. Brushes will initialize the strength in the required
     * area. */
    cache.cloth_sim->deformation_strength.fill(0.0f);
  }

  const float3 sim_location = cloth_brush_simulation_location_get(ss, &brush);

  /* Gravity */
  float3 gravity(0);
  if (cache.supports_gravity) {
    gravity += cache.gravity_direction_symm * -sd.gravity_factor;
  }

  std::optional<FalloffPlane> falloff_plane;
  if (brush.cloth_force_falloff_type == BRUSH_CLOTH_FORCE_FALLOFF_PLANE) {
    falloff_plane.emplace();
    falloff_plane->normal = math::normalize(grab_delta);
    plane_from_point_normal_v3(falloff_plane->plane, area_co, falloff_plane->normal);
  }

  bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(ob);

  threading::EnumerableThreadSpecific<LocalData> all_tls;
  switch (pbvh.type()) {
    case bke::pbvh::Type::Mesh: {
      const Mesh &mesh = *static_cast<Mesh *>(ob.data);
      const MeshAttributeData attribute_data(mesh);
      const Span<float3> positions_eval = bke::pbvh::vert_positions_eval(depsgraph, ob);
      const Span<float3> vert_normals = bke::pbvh::vert_normals_eval(depsgraph, ob);
      MutableSpan<bke::pbvh::MeshNode> nodes = pbvh.nodes<bke::pbvh::MeshNode>();
      node_mask.foreach_index(GrainSize(1), [&](const int i) {
        LocalData &tls = all_tls.local();
        calc_forces_mesh(depsgraph,
                         ob,
                         brush,
                         offset,
                         mat,
                         sim_location,
                         gravity,
                         falloff_plane,
                         attribute_data,
                         positions_eval,
                         vert_normals,
                         nodes[i],
                         tls);
      });
      break;
    }
    case bke::pbvh::Type::Grids: {
      MutableSpan<bke::pbvh::GridsNode> nodes = pbvh.nodes<bke::pbvh::GridsNode>();
      node_mask.foreach_index(GrainSize(1), [&](const int i) {
        LocalData &tls = all_tls.local();
        calc_forces_grids(depsgraph,
                          ob,
                          brush,
                          offset,
                          mat,
                          sim_location,
                          gravity,
                          falloff_plane,
                          nodes[i],
                          tls);
      });
      break;
    }
    case bke::pbvh::Type::BMesh: {
      MutableSpan<bke::pbvh::BMeshNode> nodes = pbvh.nodes<bke::pbvh::BMeshNode>();
      node_mask.foreach_index(GrainSize(1), [&](const int i) {
        LocalData &tls = all_tls.local();
        calc_forces_bmesh(depsgraph,
                          ob,
                          brush,
                          offset,
                          mat,
                          sim_location,
                          gravity,
                          falloff_plane,
                          nodes[i],
                          tls);
      });
      break;
    }
  }
}

/* Allocates nodes state and initializes them to Uninitialized, so constraints can be created for
 * them. */
static void cloth_sim_initialize_default_node_state(Object &object, SimulationData &cloth_sim)
{
  bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
  IndexMaskMemory memory;
  const IndexMask node_mask = bke::pbvh::all_leaf_nodes(pbvh, memory);
  cloth_sim.node_state = Array<NodeSimState>(pbvh.nodes_num(), SCULPT_CLOTH_NODE_UNINITIALIZED);

  switch (pbvh.type()) {
    case bke::pbvh::Type::Mesh: {
      MutableSpan<bke::pbvh::MeshNode> nodes = pbvh.nodes<bke::pbvh::MeshNode>();
      node_mask.foreach_index([&](const int i) { cloth_sim.node_state_index.add(&nodes[i], i); });
      break;
    }
    case bke::pbvh::Type::Grids: {
      MutableSpan<bke::pbvh::GridsNode> nodes = pbvh.nodes<bke::pbvh::GridsNode>();
      node_mask.foreach_index([&](const int i) { cloth_sim.node_state_index.add(&nodes[i], i); });
      break;
    }
    case bke::pbvh::Type::BMesh: {
      MutableSpan<bke::pbvh::BMeshNode> nodes = pbvh.nodes<bke::pbvh::BMeshNode>();
      node_mask.foreach_index([&](const int i) { cloth_sim.node_state_index.add(&nodes[i], i); });
      break;
    }
  }
}

static void copy_positions_to_array(const Depsgraph &depsgraph,
                                    const Object &object,
                                    MutableSpan<float3> positions)
{
  const SculptSession &ss = *object.sculpt;
  const bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
  switch (pbvh.type()) {
    case bke::pbvh::Type::Mesh:
      positions.copy_from(bke::pbvh::vert_positions_eval(depsgraph, object));
      break;
    case bke::pbvh::Type::Grids: {
      const SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;
      positions.copy_from(subdiv_ccg.positions);
      break;
    }
    case bke::pbvh::Type::BMesh:
      BM_mesh_vert_coords_get(ss.bm, positions);
      break;
  }
}

static void copy_normals_to_array(const Depsgraph &depsgraph,
                                  const Object &object,
                                  MutableSpan<float3> normals)
{
  const SculptSession &ss = *object.sculpt;
  const bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
  switch (pbvh.type()) {
    case bke::pbvh::Type::Mesh:
      normals.copy_from(bke::pbvh::vert_normals_eval(depsgraph, object));
      break;
    case bke::pbvh::Type::Grids: {
      const SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;
      normals.copy_from(subdiv_ccg.normals);
      break;
    }
    case bke::pbvh::Type::BMesh:
      BM_mesh_vert_normals_get(ss.bm, normals);
      break;
  }
}

std::unique_ptr<SimulationData> brush_simulation_create(const Depsgraph &depsgraph,
                                                        Object &ob,
                                                        const float cloth_mass,
                                                        const float cloth_damping,
                                                        const float cloth_softbody_strength,
                                                        const bool use_collisions,
                                                        const bool needs_deform_coords)
{
  const int totverts = SCULPT_vertex_count_get(ob);
  std::unique_ptr<SimulationData> cloth_sim = std::make_unique<SimulationData>();

  cloth_sim->length_constraints.reserve(CLOTH_LENGTH_CONSTRAINTS_BLOCK);

  cloth_sim->acceleration = Array<float3>(totverts, float3(0));
  cloth_sim->pos = Array<float3>(totverts, float3(0));
  cloth_sim->length_constraint_tweak = Array<float>(totverts, 0.0f);

  cloth_sim->init_pos.reinitialize(totverts);
  copy_positions_to_array(depsgraph, ob, cloth_sim->init_pos);

  cloth_sim->last_iteration_pos = cloth_sim->init_pos;
  cloth_sim->prev_pos = cloth_sim->init_pos;

  cloth_sim->init_no.reinitialize(totverts);
  copy_normals_to_array(depsgraph, ob, cloth_sim->init_no);

  if (needs_deform_coords) {
    cloth_sim->deformation_pos = cloth_sim->init_pos;
    cloth_sim->deformation_strength = Array<float>(totverts, 1.0f);
  }

  if (cloth_softbody_strength > 0.0f) {
    cloth_sim->softbody_pos = cloth_sim->init_pos;
  }

  cloth_sim->mass = cloth_mass;
  cloth_sim->damping = cloth_damping;
  cloth_sim->softbody_strength = cloth_softbody_strength;

  if (use_collisions) {
    cloth_sim->collider_list = cloth_brush_collider_cache_create(ob, depsgraph);
  }

  cloth_sim_initialize_default_node_state(ob, *cloth_sim);

  return cloth_sim;
}

void brush_store_simulation_state(const Depsgraph &depsgraph,
                                  const Object &object,
                                  SimulationData &cloth_sim)
{
  copy_positions_to_array(depsgraph, object, cloth_sim.pos);
}

void sim_activate_nodes(Object &object, SimulationData &cloth_sim, const IndexMask &node_mask)
{
  bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);

  /* Activate the nodes inside the simulation area. */
  switch (pbvh.type()) {
    case bke::pbvh::Type::Mesh: {
      MutableSpan<bke::pbvh::MeshNode> nodes = pbvh.nodes<bke::pbvh::MeshNode>();
      node_mask.foreach_index([&](const int i) {
        const int node_index = cloth_sim.node_state_index.lookup(&nodes[i]);
        cloth_sim.node_state[node_index] = SCULPT_CLOTH_NODE_ACTIVE;
      });
      break;
    }
    case bke::pbvh::Type::Grids: {
      MutableSpan<bke::pbvh::GridsNode> nodes = pbvh.nodes<bke::pbvh::GridsNode>();
      node_mask.foreach_index([&](const int i) {
        const int node_index = cloth_sim.node_state_index.lookup(&nodes[i]);
        cloth_sim.node_state[node_index] = SCULPT_CLOTH_NODE_ACTIVE;
      });
      break;
    }
    case bke::pbvh::Type::BMesh: {
      MutableSpan<bke::pbvh::BMeshNode> nodes = pbvh.nodes<bke::pbvh::BMeshNode>();
      node_mask.foreach_index([&](const int i) {
        const int node_index = cloth_sim.node_state_index.lookup(&nodes[i]);
        cloth_sim.node_state[node_index] = SCULPT_CLOTH_NODE_ACTIVE;
      });
      break;
    }
  }
}

static void sculpt_cloth_ensure_constraints_in_simulation_area(const Sculpt &sd,
                                                               Object &ob,
                                                               const IndexMask &node_mask)
{
  SculptSession &ss = *ob.sculpt;
  const Brush *brush = BKE_paint_brush_for_read(&sd.paint);
  const float radius = ss.cache->initial_radius;
  const float limit = radius + (radius * brush->cloth_sim_limit);
  const float3 sim_location = cloth_brush_simulation_location_get(ss, brush);
  ensure_nodes_constraints(sd, ob, node_mask, *ss.cache->cloth_sim, sim_location, limit);
}

void do_cloth_brush(const Depsgraph &depsgraph,
                    const Sculpt &sd,
                    Object &ob,
                    const IndexMask &node_mask)
{
  SculptSession &ss = *ob.sculpt;
  const Brush *brush = BKE_paint_brush_for_read(&sd.paint);

  if (!ss.cache->cloth_sim) {
    ss.cache->cloth_sim = brush_simulation_create(depsgraph,
                                                  ob,
                                                  brush->cloth_mass,
                                                  brush->cloth_damping,
                                                  brush->cloth_constraint_softbody_strength,
                                                  (brush->flag2 & BRUSH_CLOTH_USE_COLLISION),
                                                  is_cloth_deform_brush(*brush));
  }

  if (SCULPT_stroke_is_first_brush_step_of_symmetry_pass(*ss.cache)) {
    if (brush->cloth_simulation_area_type == BRUSH_CLOTH_SIMULATION_AREA_LOCAL) {
      /* When using simulation a fixed local simulation area, constraints are created only using
       * the initial stroke position and initial radius (per symmetry pass) instead of per node.
       * This allows to skip unnecessary constraints that will never be simulated, making the
       * solver faster. When the simulation starts for a node, the node gets activated and all its
       * constraints are considered final. As the same node can be included inside the brush radius
       * from multiple symmetry passes, the cloth brush can't activate the node for simulation yet
       * as this will cause the ensure constraints function to skip the node in the next symmetry
       * passes. It needs to build the constraints here and skip simulating the first step, so all
       * passes can add their constraints to all affected nodes. */
      sculpt_cloth_ensure_constraints_in_simulation_area(sd, ob, node_mask);
    }
    /* The first step of a symmetry pass is never simulated as deformation modes need valid delta
     * for brush tip alignment. */
    return;
  }

  /* Ensure the constraints for the nodes. */
  sculpt_cloth_ensure_constraints_in_simulation_area(sd, ob, node_mask);

  /* Store the initial state in the simulation. */
  brush_store_simulation_state(depsgraph, ob, *ss.cache->cloth_sim);

  /* Enable the nodes that should be simulated. */
  sim_activate_nodes(ob, *ss.cache->cloth_sim, node_mask);

  /* Apply forces to the vertices. */
  cloth_brush_apply_brush_forces(depsgraph, sd, ob, node_mask);

  /* Update and write the simulation to the nodes. */
  do_simulation_step(depsgraph, sd, ob, *ss.cache->cloth_sim, node_mask);
}

SimulationData::~SimulationData() = default;

void simulation_limits_draw(const uint gpuattr,
                            const Brush &brush,
                            const float location[3],
                            const float normal[3],
                            const float rds,
                            const float line_width,
                            const float outline_col[3],
                            const float alpha)
{
  float cursor_trans[4][4], cursor_rot[4][4];
  const float z_axis[4] = {0.0f, 0.0f, 1.0f, 0.0f};
  float quat[4];
  unit_m4(cursor_trans);
  translate_m4(cursor_trans, location[0], location[1], location[2]);
  rotation_between_vecs_to_quat(quat, z_axis, normal);
  quat_to_mat4(cursor_rot, quat);
  GPU_matrix_push();
  GPU_matrix_mul(cursor_trans);
  GPU_matrix_mul(cursor_rot);

  GPU_line_width(line_width);
  immUniformColor3fvAlpha(outline_col, alpha * 0.5f);
  imm_draw_circle_dashed_3d(
      gpuattr, 0, 0, rds + (rds * brush.cloth_sim_limit * brush.cloth_sim_falloff), 320);
  immUniformColor3fvAlpha(outline_col, alpha * 0.7f);
  imm_draw_circle_wire_3d(gpuattr, 0, 0, rds + rds * brush.cloth_sim_limit, 80);
  GPU_matrix_pop();
}

void plane_falloff_preview_draw(const uint gpuattr,
                                SculptSession &ss,
                                const float outline_col[3],
                                float outline_alpha)
{
  float4x4 local_mat = ss.cache->stroke_local_mat;

  if (ss.cache->brush->cloth_deform_type == BRUSH_CLOTH_DEFORM_GRAB) {
    add_v3_v3v3(local_mat[3], ss.cache->location, ss.cache->grab_delta);
  }

  GPU_matrix_mul(local_mat.ptr());

  const float dist = ss.cache->radius;
  const float arrow_x = ss.cache->radius * 0.2f;
  const float arrow_y = ss.cache->radius * 0.1f;

  immUniformColor3fvAlpha(outline_col, outline_alpha);
  GPU_line_width(2.0f);
  immBegin(GPU_PRIM_LINES, 2);
  immVertex3f(gpuattr, dist, 0.0f, 0.0f);
  immVertex3f(gpuattr, -dist, 0.0f, 0.0f);
  immEnd();

  immBegin(GPU_PRIM_TRIS, 6);
  immVertex3f(gpuattr, dist, 0.0f, 0.0f);
  immVertex3f(gpuattr, dist - arrow_x, arrow_y, 0.0f);
  immVertex3f(gpuattr, dist - arrow_x, -arrow_y, 0.0f);

  immVertex3f(gpuattr, -dist, 0.0f, 0.0f);
  immVertex3f(gpuattr, -dist + arrow_x, arrow_y, 0.0f);
  immVertex3f(gpuattr, -dist + arrow_x, -arrow_y, 0.0f);

  immEnd();
}

/* Cloth Filter. */

enum class ClothFilterType {
  Gravity,
  Inflate,
  Expand,
  Pinch,
  Scale,
};

static EnumPropertyItem prop_cloth_filter_type[] = {
    {int(ClothFilterType::Gravity), "GRAVITY", 0, "Gravity", "Applies gravity to the simulation"},
    {int(ClothFilterType::Inflate), "INFLATE", 0, "Inflate", "Inflates the cloth"},
    {int(ClothFilterType::Expand), "EXPAND", 0, "Expand", "Expands the cloth's dimensions"},
    {int(ClothFilterType::Pinch),
     "PINCH",
     0,
     "Pinch",
     "Pulls the cloth to the cursor's start position"},
    {int(ClothFilterType::Scale),
     "SCALE",
     0,
     "Scale",
     "Scales the mesh as a soft body using the origin of the object as scale"},
    {0, nullptr, 0, nullptr, nullptr},
};

static EnumPropertyItem prop_cloth_filter_orientation_items[] = {
    {int(filter::FilterOrientation::Local),
     "LOCAL",
     0,
     "Local",
     "Use the local axis to limit the force and set the gravity direction"},
    {int(filter::FilterOrientation::World),
     "WORLD",
     0,
     "World",
     "Use the global axis to limit the force and set the gravity direction"},
    {int(filter::FilterOrientation::View),
     "VIEW",
     0,
     "View",
     "Use the view axis to limit the force and set the gravity direction"},
    {0, nullptr, 0, nullptr, nullptr},
};

enum eClothFilterForceAxis {
  CLOTH_FILTER_FORCE_X = 1 << 0,
  CLOTH_FILTER_FORCE_Y = 1 << 1,
  CLOTH_FILTER_FORCE_Z = 1 << 2,
};

static EnumPropertyItem prop_cloth_filter_force_axis_items[] = {
    {CLOTH_FILTER_FORCE_X, "X", 0, "X", "Apply force in the X axis"},
    {CLOTH_FILTER_FORCE_Y, "Y", 0, "Y", "Apply force in the Y axis"},
    {CLOTH_FILTER_FORCE_Z, "Z", 0, "Z", "Apply force in the Z axis"},
    {0, nullptr, 0, nullptr, nullptr},
};

static bool cloth_filter_is_deformation_filter(ClothFilterType filter_type)
{
  return ELEM(filter_type, ClothFilterType::Scale);
}

BLI_NOINLINE static void calc_gravity_forces(const Span<float> factors,
                                             const filter::Cache &filter_cache,
                                             const MutableSpan<float3> forces)
{
  const float3x3 to_object_space = filter::to_object_space(filter_cache);
  for (const int i : forces.index_range()) {
    float3 force(0.0f);
    if (filter_cache.orientation == filter::FilterOrientation::View) {
      /* When using the view orientation apply gravity in the -Y axis, this way objects will
       * fall down instead of backwards. */
      force[1] = -factors[i];
    }
    else {
      force[2] = -factors[i];
    }
    forces[i] = to_object_space * force;
  }
}

struct FilterLocalData {
  Vector<float> factors;
  Vector<int> vert_indices;
  Vector<float3> positions;
  Vector<float3> normals;
  Vector<float3> forces;
};

BLI_NOINLINE static void apply_scale_filter(filter::Cache &filter_cache,
                                            const Span<int> verts,
                                            const Span<float> factors,
                                            FilterLocalData &tls)
{
  const MutableSpan translations = gather_data_mesh(
      filter_cache.cloth_sim->init_pos.as_span(), verts, tls.forces);
  scale_translations(translations, factors);
  filter::zero_disabled_axis_components(filter_cache, translations);
  for (const int i : verts.index_range()) {
    filter_cache.cloth_sim->deformation_pos[verts[i]] =
        filter_cache.cloth_sim->init_pos[verts[i]] + translations[i];
  }
}

static void apply_filter_forces_mesh(const Depsgraph &depsgraph,
                                     const ClothFilterType filter_type,
                                     const float filter_strength,
                                     const float3 &gravity,
                                     const Span<float3> positions_eval,
                                     const Span<float3> vert_normals,
                                     const GroupedSpan<int> vert_to_face_map,
                                     const MeshAttributeData &attribute_data,
                                     const bke::pbvh::MeshNode &node,
                                     Object &object,
                                     FilterLocalData &tls)
{
  const SculptSession &ss = *object.sculpt;
  SimulationData &cloth_sim = *ss.filter_cache->cloth_sim;

  const Span<int> verts = node.verts();

  tls.factors.resize(verts.size());
  const MutableSpan<float> factors = tls.factors;
  fill_factor_from_hide_and_mask(attribute_data.hide_vert, attribute_data.mask, verts, factors);
  const auto_mask::Cache *automasking = auto_mask::active_cache_get(ss);
  auto_mask::calc_vert_factors(depsgraph, object, automasking, node, verts, factors);

  if (ss.filter_cache->active_face_set != SCULPT_FACE_SET_NONE) {
    for (const int i : verts.index_range()) {
      const int vert = verts[i];
      if (!face_set::vert_has_face_set(
              vert_to_face_map, attribute_data.face_sets, vert, ss.filter_cache->active_face_set))
      {
        factors[i] = 0.0f;
      }
    }
  }

  scale_factors(factors, filter_strength);

  tls.forces.resize(verts.size());
  const MutableSpan<float3> forces = tls.forces;
  if (!math::is_zero(gravity)) {
    forces.fill(gravity);
    apply_forces(cloth_sim, forces, verts);
  }

  switch (filter_type) {
    case ClothFilterType::Gravity:
      calc_gravity_forces(factors, *ss.filter_cache, forces);
      apply_forces(cloth_sim, tls.forces, verts);
      break;
    case ClothFilterType::Inflate:
      gather_data_mesh(vert_normals, verts, forces);
      scale_translations(forces, factors);
      apply_forces(cloth_sim, tls.forces, verts);
      break;
    case ClothFilterType::Expand:
      expand_length_constraints(cloth_sim, verts, factors);
      break;
    case ClothFilterType::Pinch:
      calc_pinch_forces(

          gather_data_mesh(positions_eval, verts, tls.positions),
          ss.filter_cache->cloth_sim_pinch_point,
          forces);
      scale_translations(forces, factors);
      apply_forces(cloth_sim, tls.forces, verts);
      break;
    case ClothFilterType::Scale:
      apply_scale_filter(*ss.filter_cache, verts, factors, tls);
      break;
  }
}

static void apply_filter_forces_grids(const Depsgraph &depsgraph,
                                      const Span<int> face_sets,
                                      const ClothFilterType filter_type,
                                      const float filter_strength,
                                      const float3 &gravity,
                                      const bke::pbvh::GridsNode &node,
                                      Object &object,
                                      FilterLocalData &tls)
{
  const SculptSession &ss = *object.sculpt;
  SimulationData &cloth_sim = *ss.filter_cache->cloth_sim;
  const SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;
  const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);

  const Span<int> grids = node.grids();
  const int grid_verts_num = grids.size() * key.grid_area;

  tls.factors.resize(grid_verts_num);
  const MutableSpan<float> factors = tls.factors;
  fill_factor_from_hide_and_mask(subdiv_ccg, grids, factors);
  const auto_mask::Cache *automasking = auto_mask::active_cache_get(ss);
  auto_mask::calc_grids_factors(depsgraph, object, automasking, node, grids, factors);

  if (ss.filter_cache->active_face_set != SCULPT_FACE_SET_NONE) {
    for (const int i : grids.index_range()) {
      if (!face_set::vert_has_face_set(
              subdiv_ccg, face_sets, grids[i], ss.filter_cache->active_face_set))
      {
        factors.slice(i * key.grid_area, key.grid_area).fill(0.0f);
      }
    }
  }

  scale_factors(factors, filter_strength);

  const Span<int> verts = calc_vert_indices_grids(key, grids, tls.vert_indices);

  tls.forces.resize(verts.size());
  const MutableSpan<float3> forces = tls.forces;
  if (!math::is_zero(gravity)) {
    forces.fill(gravity);
    apply_forces(cloth_sim, forces, verts);
  }

  switch (filter_type) {
    case ClothFilterType::Gravity:
      calc_gravity_forces(factors, *ss.filter_cache, forces);
      apply_forces(cloth_sim, tls.forces, verts);
      break;
    case ClothFilterType::Inflate:
      gather_grids_normals(subdiv_ccg, grids, forces);
      scale_translations(forces, factors);
      apply_forces(cloth_sim, tls.forces, verts);
      break;
    case ClothFilterType::Expand:
      expand_length_constraints(cloth_sim, verts, factors);
      break;
    case ClothFilterType::Pinch:
      calc_pinch_forces(

          gather_grids_positions(subdiv_ccg, grids, tls.positions),
          ss.filter_cache->cloth_sim_pinch_point,
          forces);
      scale_translations(forces, factors);
      apply_forces(cloth_sim, tls.forces, verts);
      break;
    case ClothFilterType::Scale:
      apply_scale_filter(*ss.filter_cache, verts, factors, tls);
      break;
  }
}

static void apply_filter_forces_bmesh(const Depsgraph &depsgraph,
                                      const ClothFilterType filter_type,
                                      const float filter_strength,
                                      const float3 &gravity,
                                      bke::pbvh::BMeshNode &node,
                                      Object &object,
                                      FilterLocalData &tls)
{
  const SculptSession &ss = *object.sculpt;
  SimulationData &cloth_sim = *ss.filter_cache->cloth_sim;
  const BMesh &bm = *ss.bm;

  const Set<BMVert *, 0> &verts = BKE_pbvh_bmesh_node_unique_verts(&node);

  tls.factors.resize(verts.size());
  const MutableSpan<float> factors = tls.factors;
  fill_factor_from_hide_and_mask(bm, verts, factors);
  const auto_mask::Cache *automasking = auto_mask::active_cache_get(ss);
  auto_mask::calc_vert_factors(depsgraph, object, automasking, node, verts, factors);

  if (ss.filter_cache->active_face_set != SCULPT_FACE_SET_NONE) {
    const int face_set_offset = CustomData_get_offset_named(
        &bm.pdata, CD_PROP_INT32, ".sculpt_face_set");
    int i = 0;
    for (const BMVert *vert : verts) {
      if (!face_set::vert_has_face_set(face_set_offset, *vert, ss.filter_cache->active_face_set)) {
        factors[i] = 0.0f;
      }
      i++;
    }
  }

  scale_factors(factors, filter_strength);

  const Span<int> vert_indices = calc_vert_indices_bmesh(verts, tls.vert_indices);

  tls.forces.resize(verts.size());
  const MutableSpan<float3> forces = tls.forces;
  if (!math::is_zero(gravity)) {
    forces.fill(gravity);
    apply_forces(cloth_sim, forces, vert_indices);
  }

  switch (filter_type) {
    case ClothFilterType::Gravity:
      calc_gravity_forces(factors, *ss.filter_cache, forces);
      apply_forces(cloth_sim, tls.forces, vert_indices);
      break;
    case ClothFilterType::Inflate:
      gather_bmesh_normals(verts, forces);
      scale_translations(forces, factors);
      apply_forces(cloth_sim, tls.forces, vert_indices);
      break;
    case ClothFilterType::Expand:
      expand_length_constraints(cloth_sim, vert_indices, factors);
      break;
    case ClothFilterType::Pinch:
      calc_pinch_forces(

          gather_bmesh_positions(verts, tls.positions),
          ss.filter_cache->cloth_sim_pinch_point,
          forces);
      scale_translations(forces, factors);
      apply_forces(cloth_sim, tls.forces, vert_indices);
      break;
    case ClothFilterType::Scale:
      apply_scale_filter(*ss.filter_cache, vert_indices, factors, tls);
      break;
  }
}

static wmOperatorStatus sculpt_cloth_filter_modal(bContext *C,
                                                  wmOperator *op,
                                                  const wmEvent *event)
{
  Object &object = *CTX_data_active_object(C);
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  SculptSession &ss = *object.sculpt;
  const Sculpt &sd = *CTX_data_tool_settings(C)->sculpt;
  const ClothFilterType filter_type = ClothFilterType(RNA_enum_get(op->ptr, "type"));
  float filter_strength = RNA_float_get(op->ptr, "strength");

  if (event->type == LEFTMOUSE && event->val == KM_RELEASE) {
    MEM_delete(ss.filter_cache);
    ss.filter_cache = nullptr;
    undo::push_end(object);
    flush_update_done(C, object, UpdateType::Position);
    return OPERATOR_FINISHED;
  }

  if (event->type != MOUSEMOVE) {
    return OPERATOR_RUNNING_MODAL;
  }

  const float len = event->prev_press_xy[0] - event->xy[0];
  filter_strength = filter_strength * -len * 0.001f * UI_SCALE_FAC;

  vert_random_access_ensure(object);

  BKE_sculpt_update_object_for_edit(depsgraph, &object, false);

  brush_store_simulation_state(*depsgraph, object, *ss.filter_cache->cloth_sim);

  const IndexMask &node_mask = ss.filter_cache->node_mask;

  if (auto_mask::is_enabled(sd, object, nullptr) && ss.filter_cache->automasking &&
      ss.filter_cache->automasking->settings.flags & BRUSH_AUTOMASKING_CAVITY_ALL)
  {
    ss.filter_cache->automasking->calc_cavity_factor(*depsgraph, object, node_mask);
  }

  float3 gravity(0.0f);
  if (sd.gravity_object) {
    gravity = sd.gravity_object->object_to_world().ptr()[2];
  }
  else {
    gravity[2] = -1.0f;
  }
  gravity *= sd.gravity_factor * filter_strength;

  bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);

  threading::EnumerableThreadSpecific<FilterLocalData> all_tls;
  switch (pbvh.type()) {
    case bke::pbvh::Type::Mesh: {
      const Span<float3> positions_eval = bke::pbvh::vert_positions_eval(*depsgraph, object);
      const Span<float3> vert_normals = bke::pbvh::vert_normals_eval(*depsgraph, object);
      const Mesh &mesh = *static_cast<const Mesh *>(object.data);
      const GroupedSpan<int> vert_to_face_map = mesh.vert_to_face_map();
      const MeshAttributeData attribute_data(mesh);
      MutableSpan<bke::pbvh::MeshNode> nodes = pbvh.nodes<bke::pbvh::MeshNode>();
      node_mask.foreach_index(GrainSize(1), [&](const int i) {
        FilterLocalData &tls = all_tls.local();
        apply_filter_forces_mesh(*depsgraph,
                                 filter_type,
                                 filter_strength,
                                 gravity,
                                 positions_eval,
                                 vert_normals,
                                 vert_to_face_map,
                                 attribute_data,
                                 nodes[i],
                                 object,
                                 tls);
        bke::pbvh::update_node_bounds_mesh(positions_eval, nodes[i]);
      });
      break;
    }
    case bke::pbvh::Type::Grids: {
      const Mesh &base_mesh = *static_cast<const Mesh *>(object.data);
      const bke::AttributeAccessor attributes = base_mesh.attributes();
      const VArraySpan face_sets = *attributes.lookup<int>(".sculpt_face_set",
                                                           bke::AttrDomain::Face);
      SubdivCCG &subdiv_ccg = *object.sculpt->subdiv_ccg;
      MutableSpan<float3> positions = subdiv_ccg.positions;
      MutableSpan<bke::pbvh::GridsNode> nodes = pbvh.nodes<bke::pbvh::GridsNode>();
      node_mask.foreach_index(GrainSize(1), [&](const int i) {
        FilterLocalData &tls = all_tls.local();
        apply_filter_forces_grids(
            *depsgraph, face_sets, filter_type, filter_strength, gravity, nodes[i], object, tls);
        bke::pbvh::update_node_bounds_grids(subdiv_ccg.grid_area, positions, nodes[i]);
      });
      break;
    }
    case bke::pbvh::Type::BMesh: {
      MutableSpan<bke::pbvh::BMeshNode> nodes = pbvh.nodes<bke::pbvh::BMeshNode>();
      node_mask.foreach_index(GrainSize(1), [&](const int i) {
        FilterLocalData &tls = all_tls.local();
        apply_filter_forces_bmesh(
            *depsgraph, filter_type, filter_strength, gravity, nodes[i], object, tls);
        bke::pbvh::update_node_bounds_bmesh(nodes[i]);
      });
      break;
    }
  }
  pbvh.tag_positions_changed(node_mask);
  pbvh.flush_bounds_to_parents();

  /* Activate all nodes. */
  sim_activate_nodes(object, *ss.filter_cache->cloth_sim, node_mask);

  /* Update and write the simulation to the nodes. */
  do_simulation_step(*depsgraph, sd, object, *ss.filter_cache->cloth_sim, node_mask);

  flush_update_step(C, UpdateType::Position);
  return OPERATOR_RUNNING_MODAL;
}

static wmOperatorStatus sculpt_cloth_filter_invoke(bContext *C,
                                                   wmOperator *op,
                                                   const wmEvent *event)
{
  const Scene &scene = *CTX_data_scene(C);
  Object &ob = *CTX_data_active_object(C);
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Sculpt &sd = *CTX_data_tool_settings(C)->sculpt;
  SculptSession &ss = *ob.sculpt;

  const View3D *v3d = CTX_wm_view3d(C);
  const Base *base = CTX_data_active_base(C);
  if (!BKE_base_is_visible(v3d, base)) {
    return OPERATOR_CANCELLED;
  }

  const ClothFilterType filter_type = ClothFilterType(RNA_enum_get(op->ptr, "type"));

  /* Update the active vertex */
  float2 mval_fl{float(event->mval[0]), float(event->mval[1])};
  CursorGeometryInfo cgi;
  cursor_geometry_info_update(C, &cgi, mval_fl, false);

  /* Needs mask data to be available as it is used when solving the constraints. */
  BKE_sculpt_update_object_for_edit(depsgraph, &ob, false);

  if (report_if_shape_key_is_locked(ob, op->reports)) {
    return OPERATOR_CANCELLED;
  }

  undo::push_begin(scene, ob, op);
  filter::cache_init(C,
                     ob,
                     sd,
                     undo::Type::Position,
                     mval_fl,
                     RNA_float_get(op->ptr, "area_normal_radius"),
                     RNA_float_get(op->ptr, "strength"));

  if (auto_mask::is_enabled(sd, ob, nullptr)) {
    auto_mask::filter_cache_ensure(*depsgraph, sd, ob);
  }

  const float cloth_mass = RNA_float_get(op->ptr, "cloth_mass");
  const float cloth_damping = RNA_float_get(op->ptr, "cloth_damping");
  const bool use_collisions = RNA_boolean_get(op->ptr, "use_collisions");
  ss.filter_cache->cloth_sim = brush_simulation_create(
      *depsgraph,
      ob,
      cloth_mass,
      cloth_damping,
      0.0f,
      use_collisions,
      cloth_filter_is_deformation_filter(filter_type));

  ss.filter_cache->cloth_sim_pinch_point = ss.active_vert_position(*depsgraph, ob);

  float3 origin(0);
  ensure_nodes_constraints(
      sd, ob, ss.filter_cache->node_mask, *ss.filter_cache->cloth_sim, origin, FLT_MAX);

  const bool use_face_sets = RNA_boolean_get(op->ptr, "use_face_sets");
  if (use_face_sets) {
    ss.filter_cache->active_face_set = face_set::active_face_set_get(ob);
  }
  else {
    ss.filter_cache->active_face_set = SCULPT_FACE_SET_NONE;
  }

  const int force_axis = RNA_enum_get(op->ptr, "force_axis");
  ss.filter_cache->enabled_axis[0] = force_axis & CLOTH_FILTER_FORCE_X;
  ss.filter_cache->enabled_axis[1] = force_axis & CLOTH_FILTER_FORCE_Y;
  ss.filter_cache->enabled_axis[2] = force_axis & CLOTH_FILTER_FORCE_Z;

  ss.filter_cache->orientation = filter::FilterOrientation(RNA_enum_get(op->ptr, "orientation"));

  WM_event_add_modal_handler(C, op);
  return OPERATOR_RUNNING_MODAL;
}

void SCULPT_OT_cloth_filter(wmOperatorType *ot)
{
  ot->name = "Filter Cloth";
  ot->idname = "SCULPT_OT_cloth_filter";
  ot->description = "Applies a cloth simulation deformation to the entire mesh";

  ot->invoke = sculpt_cloth_filter_invoke;
  ot->modal = sculpt_cloth_filter_modal;
  ot->poll = SCULPT_mode_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  filter::register_operator_props(ot);

  ot->prop = RNA_def_enum(ot->srna,
                          "type",
                          prop_cloth_filter_type,
                          int(ClothFilterType::Gravity),
                          "Filter Type",
                          "Operation that is going to be applied to the mesh");
  RNA_def_property_translation_context(ot->prop, BLT_I18NCONTEXT_OPERATOR_DEFAULT);
  RNA_def_enum_flag(ot->srna,
                    "force_axis",
                    prop_cloth_filter_force_axis_items,
                    CLOTH_FILTER_FORCE_X | CLOTH_FILTER_FORCE_Y | CLOTH_FILTER_FORCE_Z,
                    "Force Axis",
                    "Apply the force in the selected axis");
  RNA_def_enum(ot->srna,
               "orientation",
               prop_cloth_filter_orientation_items,
               int(filter::FilterOrientation::Local),
               "Orientation",
               "Orientation of the axis to limit the filter force");
  RNA_def_float(ot->srna,
                "cloth_mass",
                1.0f,
                0.0f,
                2.0f,
                "Cloth Mass",
                "Mass of each simulation particle",
                0.0f,
                1.0f);
  RNA_def_float(ot->srna,
                "cloth_damping",
                0.0f,
                0.0f,
                1.0f,
                "Cloth Damping",
                "How much the applied forces are propagated through the cloth",
                0.0f,
                1.0f);
  ot->prop = RNA_def_boolean(ot->srna,
                             "use_face_sets",
                             false,
                             "Use Face Sets",
                             "Apply the filter only to the Face Set under the cursor");
  ot->prop = RNA_def_boolean(ot->srna,
                             "use_collisions",
                             false,
                             "Use Collisions",
                             "Collide with other collider objects in the scene");
}

}  // namespace blender::ed::sculpt_paint::cloth
