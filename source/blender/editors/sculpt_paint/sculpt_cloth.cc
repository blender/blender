/* SPDX-FileCopyrightText: 2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edsculpt
 */

#include "MEM_guardedalloc.h"

#include "BLI_array_utils.hh"
#include "BLI_enumerable_thread_specific.hh"
#include "BLI_math_matrix.h"
#include "BLI_math_matrix.hh"
#include "BLI_math_rotation.h"
#include "BLI_math_vector.hh"
#include "BLI_task.h"
#include "BLI_utildefines.h"
#include "BLI_vector.hh"

#include "BLT_translation.hh"

#include "DNA_brush_types.h"
#include "DNA_customdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_brush.hh"
#include "BKE_bvhutils.hh"
#include "BKE_ccg.hh"
#include "BKE_collision.h"
#include "BKE_colortools.hh"
#include "BKE_context.hh"
#include "BKE_layer.hh"
#include "BKE_mesh.hh"
#include "BKE_modifier.hh"
#include "BKE_paint.hh"
#include "BKE_pbvh_api.hh"
#include "BKE_subdiv_ccg.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_query.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "ED_sculpt.hh"

#include "mesh_brush_common.hh"
#include "paint_intern.hh"
#include "sculpt_intern.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "GPU_immediate.hh"
#include "GPU_immediate_util.hh"
#include "GPU_matrix.hh"
#include "GPU_state.hh"

#include "UI_interface.hh"

#include "bmesh.hh"

#include <cmath>
#include <cstdlib>
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

static void calc_vert_neighbor_indices_grids(const SubdivCCG &subdiv_ccg,
                                             const Span<int> verts,
                                             const MutableSpan<Vector<int>> neighbor_indices)
{
  BLI_assert(verts.size() == neighbor_indices.size());
  const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);

  for (const int i : verts.index_range()) {
    SubdivCCGNeighbors neighbors;
    BKE_subdiv_ccg_neighbor_coords_get(
        subdiv_ccg, SubdivCCGCoord::from_index(key, verts[i]), false, neighbors);

    neighbor_indices[i].clear();
    for (const SubdivCCGCoord coord : neighbors.coords) {
      neighbor_indices[i].append(coord.to_index(key));
    }
  }
}

static void calc_vert_neighbor_indices_bmesh(const BMesh &bm,
                                             const Span<int> verts,
                                             const MutableSpan<Vector<int>> neighbor_indices)
{
  BLI_assert(verts.size() == neighbor_indices.size());
  Vector<BMVert *, 64> neighbors;

  for (const int i : verts.index_range()) {
    BMVert *vert = BM_vert_at_index(&const_cast<BMesh &>(bm), verts[i]);
    neighbors.clear();
    neighbor_indices[i].clear();
    for (const BMVert *neighbor : vert_neighbors_get_bmesh(*vert, neighbors)) {
      neighbor_indices[i].append(BM_elem_index_get(neighbor));
    }
  }
}

static float3 cloth_brush_simulation_location_get(const SculptSession &ss, const Brush *brush)
{
  if (!ss.cache || !brush) {
    return float3(0);
  }
  if (brush->cloth_simulation_area_type == BRUSH_CLOTH_SIMULATION_AREA_LOCAL) {
    return ss.cache->initial_location;
  }
  return ss.cache->location;
}

Vector<bke::pbvh::Node *> brush_affected_nodes_gather(SculptSession &ss, const Brush &brush)
{
  BLI_assert(ss.cache);
  BLI_assert(brush.sculpt_tool == SCULPT_TOOL_CLOTH);

  switch (brush.cloth_simulation_area_type) {
    case BRUSH_CLOTH_SIMULATION_AREA_LOCAL: {
      const float radius_squared = math::square(ss.cache->initial_radius *
                                                (1.0 + brush.cloth_sim_limit));
      return bke::pbvh::search_gather(*ss.pbvh, [&](bke::pbvh::Node &node) {
        return node_in_sphere(node, ss.cache->initial_location, radius_squared, false);
      });
    }
    case BRUSH_CLOTH_SIMULATION_AREA_GLOBAL:
      return bke::pbvh::search_gather(*ss.pbvh, {});
    case BRUSH_CLOTH_SIMULATION_AREA_DYNAMIC: {
      const float radius_squared = math::square(ss.cache->radius * (1.0 + brush.cloth_sim_limit));
      return bke::pbvh::search_gather(*ss.pbvh, [&](bke::pbvh::Node &node) {
        return node_in_sphere(node, ss.cache->location, radius_squared, false);
      });
    }
  }

  BLI_assert_unreachable();
  return {};
}

bool is_cloth_deform_brush(const Brush &brush)
{
  return (brush.sculpt_tool == SCULPT_TOOL_CLOTH &&
          ELEM(brush.cloth_deform_type, BRUSH_CLOTH_DEFORM_GRAB, BRUSH_CLOTH_DEFORM_SNAKE_HOOK)) ||
         /* All brushes that are not the cloth brush deform the simulation using softbody
          * constraints instead of applying forces. */
         (brush.sculpt_tool != SCULPT_TOOL_CLOTH &&
          brush.deform_target == BRUSH_DEFORM_TARGET_CLOTH_SIM);
}

static float cloth_brush_simulation_falloff_get(const Brush &brush,
                                                const float radius,
                                                const float3 &location,
                                                const float3 &co)
{
  if (brush.sculpt_tool != SCULPT_TOOL_CLOTH) {
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

static void calc_brush_simulation_falloff(const Brush &brush,
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
                                      const Span<Vector<int>> vert_neighbors,
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

    if (is_brush_has_stroke_cache && brush->sculpt_tool == SCULPT_TOOL_CLOTH) {
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
          *brush, ss.cache->initial_radius, ss.cache->location, init_positions[vert]);
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
                              const Object &object,
                              const Span<bke::pbvh::Node *> all_nodes,
                              SimulationData &cloth_sim,
                              const float3 &initial_location,
                              const float radius)
{
  SculptSession &ss = *object.sculpt;
  const Brush *brush = BKE_paint_brush_for_read(&sd.paint);

  /* TODO: Multi-threaded needs to be disabled for this task until implementing the optimization of
   * storing the constraints per node. */
  /* Currently all constrains are added to the same global array which can't be accessed from
   * different threads. */

  Vector<bke::pbvh::Node *> nodes;
  for (bke::pbvh::Node *node : all_nodes) {
    const int node_index = cloth_sim.node_state_index.lookup(node);
    if (cloth_sim.node_state[node_index] == SCULPT_CLOTH_NODE_UNINITIALIZED) {
      /* The simulation already contains constraints for this node. */
      nodes.append(node);
    }
  }

  Set<OrderedEdge> created_length_constraints;
  Vector<int> vert_indices;
  Vector<Vector<int>> vert_neighbors;
  switch (ss.pbvh->type()) {
    case bke::pbvh::Type::Mesh: {
      const Mesh &mesh = *static_cast<const Mesh *>(object.data);
      const OffsetIndices faces = mesh.faces();
      const Span<int> corner_verts = mesh.corner_verts();
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
      for (const int i : nodes.index_range()) {
        const Span<int> verts = hide::node_visible_verts(*nodes[i], hide_vert, vert_indices);
        vert_neighbors.resize(verts.size());
        calc_vert_neighbors(
            faces, corner_verts, ss.vert_to_face_map, hide_poly, verts, vert_neighbors);
        add_constraints_for_verts(object,
                                  brush,
                                  initial_location,
                                  radius,
                                  init_positions,
                                  cloth_sim.node_state_index.lookup(nodes[i]),
                                  verts,
                                  vert_neighbors,
                                  cloth_sim,
                                  created_length_constraints);
      }
      break;
    }
    case bke::pbvh::Type::Grids: {
      const SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;
      const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);
      const BitGroupVector<> &grid_hidden = subdiv_ccg.grid_hidden;
      for (const int i : nodes.index_range()) {
        const Span<int> verts = calc_visible_vert_indices_grids(
            key, grid_hidden, bke::pbvh::node_grid_indices(*nodes[i]), vert_indices);
        vert_neighbors.resize(verts.size());
        calc_vert_neighbor_indices_grids(subdiv_ccg, verts, vert_neighbors);
        add_constraints_for_verts(object,
                                  brush,
                                  initial_location,
                                  radius,
                                  cloth_sim.init_pos,
                                  cloth_sim.node_state_index.lookup(nodes[i]),
                                  verts,
                                  vert_neighbors,
                                  cloth_sim,
                                  created_length_constraints);
      }
      break;
    }
    case bke::pbvh::Type::BMesh: {
      BMesh &bm = *ss.bm;
      BM_mesh_elem_index_ensure(&bm, BM_VERT);
      BM_mesh_elem_table_ensure(&bm, BM_VERT);
      for (const int i : nodes.index_range()) {
        const Set<BMVert *, 0> &bm_verts = BKE_pbvh_bmesh_node_unique_verts(nodes[i]);
        const Span<int> verts = calc_visible_vert_indices_bmesh(bm_verts, vert_indices);
        vert_neighbors.resize(verts.size());
        calc_vert_neighbor_indices_bmesh(bm, verts, vert_neighbors);
        add_constraints_for_verts(object,
                                  brush,
                                  initial_location,
                                  radius,
                                  cloth_sim.init_pos,
                                  cloth_sim.node_state_index.lookup(nodes[i]),
                                  verts,
                                  vert_neighbors,
                                  cloth_sim,
                                  created_length_constraints);
      }
      break;
    }
  }
}

static void cloth_brush_apply_force_to_vertex(SimulationData &cloth_sim,
                                              const float3 &force,
                                              const int vertex_index)
{
  cloth_sim.acceleration[vertex_index] += force / cloth_sim.mass;
}

static void do_cloth_brush_apply_forces_task(Object &ob,
                                             const Sculpt &sd,
                                             const Brush &brush,
                                             const float3 &offset,
                                             const float3 &grab_delta,
                                             const float4x4 &imat,
                                             const float3 &area_co,
                                             bke::pbvh::Node *node)
{
  SculptSession &ss = *ob.sculpt;
  SimulationData &cloth_sim = *ss.cache->cloth_sim;

  const bool use_falloff_plane = brush.cloth_force_falloff_type == BRUSH_CLOTH_FORCE_FALLOFF_PLANE;

  PBVHVertexIter vd;
  const float bstrength = ss.cache->bstrength;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, test, brush.falloff_shape);
  const int thread_id = BLI_task_parallel_thread_id(nullptr);

  /* For Pinch Perpendicular Deform Type. */
  float3 x_object_space;
  float3 z_object_space;
  if (brush.cloth_deform_type == BRUSH_CLOTH_DEFORM_PINCH_PERPENDICULAR) {
    x_object_space = math::normalize(imat.x_axis());
    z_object_space = math::normalize(imat.z_axis());
  }

  /* For Plane Force Falloff. */
  float4 deform_plane;
  float3 plane_normal;
  if (use_falloff_plane) {
    plane_normal = math::normalize(grab_delta);
    plane_from_point_normal_v3(deform_plane, area_co, plane_normal);
  }

  /* Gravity */
  float3 gravity(0);
  if (ss.cache->supports_gravity) {
    gravity += ss.cache->gravity_direction * -sd.gravity_factor;
  }

  auto_mask::NodeData automask_data = auto_mask::node_begin(
      ob, auto_mask::active_cache_get(ss), *node);

  BKE_pbvh_vertex_iter_begin (*ss.pbvh, node, vd, PBVH_ITER_UNIQUE) {
    auto_mask::node_update(automask_data, vd);

    float3 force;
    const float3 sim_location = cloth_brush_simulation_location_get(ss, &brush);
    const float sim_factor = cloth_brush_simulation_falloff_get(
        brush, ss.cache->radius, sim_location, cloth_sim.init_pos[vd.index]);

    float3 current_vertex_location;
    if (brush.cloth_deform_type == BRUSH_CLOTH_DEFORM_GRAB) {
      current_vertex_location = ss.cache->cloth_sim->init_pos[vd.index];
    }
    else {
      current_vertex_location = vd.co;
    }

    /* Apply gravity in the entire simulation area. */
    float3 vertex_gravity = gravity * sim_factor;
    cloth_brush_apply_force_to_vertex(*ss.cache->cloth_sim, vertex_gravity, vd.index);

    /* When using the plane falloff mode the falloff is not constrained by the brush radius. */
    if (!sculpt_brush_test_sq_fn(test, current_vertex_location) && !use_falloff_plane) {
      continue;
    }

    float dist = std::sqrt(test.dist);

    if (use_falloff_plane) {
      dist = dist_to_plane_v3(current_vertex_location, deform_plane);
    }

    const float fade = sim_factor * bstrength *
                       SCULPT_brush_strength_factor(ss,
                                                    brush,
                                                    current_vertex_location,
                                                    dist,
                                                    vd.no,
                                                    vd.fno,
                                                    vd.mask,
                                                    vd.vertex,
                                                    thread_id,
                                                    &automask_data);

    float3 brush_disp;

    switch (brush.cloth_deform_type) {
      case BRUSH_CLOTH_DEFORM_DRAG:
        brush_disp = math::normalize(ss.cache->location - ss.cache->last_location);
        force = brush_disp * fade;
        break;
      case BRUSH_CLOTH_DEFORM_PUSH:
        /* Invert the fade to push inwards. */
        force = offset * -fade;
        break;
      case BRUSH_CLOTH_DEFORM_GRAB:
        cloth_sim.deformation_pos[vd.index] = cloth_sim.init_pos[vd.index] +
                                              ss.cache->grab_delta_symmetry * fade;
        if (use_falloff_plane) {
          cloth_sim.deformation_strength[vd.index] = std::clamp(fade, 0.0f, 1.0f);
        }
        else {
          cloth_sim.deformation_strength[vd.index] = 1.0f;
        }
        force = float3(0);
        break;
      case BRUSH_CLOTH_DEFORM_SNAKE_HOOK:
        cloth_sim.deformation_pos[vd.index] = cloth_sim.pos[vd.index];
        cloth_sim.deformation_pos[vd.index] += ss.cache->grab_delta_symmetry * fade;
        cloth_sim.deformation_strength[vd.index] = fade;
        force = float3(0);
        break;
      case BRUSH_CLOTH_DEFORM_PINCH_POINT:
        if (use_falloff_plane) {
          float distance = dist_signed_to_plane_v3(vd.co, deform_plane);
          brush_disp = math::normalize(plane_normal * -distance);
        }
        else {
          brush_disp = math::normalize(ss.cache->location - float3(vd.co));
        }
        force = brush_disp * fade;
        break;
      case BRUSH_CLOTH_DEFORM_PINCH_PERPENDICULAR: {
        const float3 disp_center = math::normalize(ss.cache->location - float3(vd.co));
        const float3 x_disp = x_object_space - math::dot(disp_center, x_object_space);
        const float3 z_disp = z_object_space - math::dot(disp_center, z_object_space);
        force = (x_disp + z_disp) * fade;
        break;
      }
      case BRUSH_CLOTH_DEFORM_INFLATE:
        force = float3(vd.no) * fade;
        break;
      case BRUSH_CLOTH_DEFORM_EXPAND:
        cloth_sim.length_constraint_tweak[vd.index] += fade * 0.1f;
        force = float3(0);
        break;
    }

    cloth_brush_apply_force_to_vertex(*ss.cache->cloth_sim, force, vd.index);
  }
  BKE_pbvh_vertex_iter_end;
}

static Vector<ColliderCache> cloth_brush_collider_cache_create(Object &object,
                                                               Depsgraph *depsgraph)
{
  Vector<ColliderCache> cache;
  DEGObjectIterSettings deg_iter_settings = {nullptr};
  deg_iter_settings.depsgraph = depsgraph;
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
  float(*positions)[3] = col_data->x;
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

struct LocalData {
  Vector<int> vert_indices;
  Vector<float> factors;
  Vector<float> sim_factors;
  Vector<float3> positions;
  Vector<float3> diffs;
  Vector<float3> translations;
};

static void cloth_brush_solve_collision(const Object &object,
                                        SimulationData &cloth_sim,
                                        const int i)
{
  const int raycast_flag = BVH_RAYCAST_DEFAULT & ~(BVH_RAYCAST_WATERTIGHT);

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

static void calc_constraint_factors(const Object &object,
                                    const Brush *brush,
                                    const float3 &sim_location,
                                    const Span<float3> init_positions,
                                    const MutableSpan<float> cloth_factors)
{
  const SculptSession &ss = *object.sculpt;
  const bke::pbvh::Tree &pbvh = *ss.pbvh;
  const Vector<bke::pbvh::Node *> nodes = bke::pbvh::search_gather(
      const_cast<bke::pbvh::Tree &>(pbvh), {});

  const auto_mask::Cache *automasking = auto_mask::active_cache_get(ss);

  struct LocalData {
    Vector<float> factors;
    Vector<float3> positions;
  };
  threading::EnumerableThreadSpecific<LocalData> all_tls;
  switch (ss.pbvh->type()) {
    case bke::pbvh::Type::Mesh: {
      const Mesh &mesh = *static_cast<const Mesh *>(object.data);
      threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
        LocalData &tls = all_tls.local();
        for (const int i : range) {
          const Span<int> verts = bke::pbvh::node_unique_verts(*nodes[i]);
          tls.factors.resize(verts.size());
          const MutableSpan<float> factors = tls.factors;
          fill_factor_from_hide_and_mask(mesh, verts, factors);
          if (automasking) {
            auto_mask::calc_vert_factors(object, *automasking, *nodes[i], verts, factors);
          }
          if (ss.cache) {
            const MutableSpan positions = gather_data_mesh(init_positions, verts, tls.positions);
            calc_brush_simulation_falloff(
                *brush, ss.cache->radius, sim_location, positions, factors);
          }
          scatter_data_mesh(factors.as_span(), verts, cloth_factors);
        }
      });
      break;
    }
    case bke::pbvh::Type::Grids: {
      const SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;
      const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);
      threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
        LocalData &tls = all_tls.local();
        for (const int i : range) {
          const Span<int> grids = bke::pbvh::node_grid_indices(*nodes[i]);
          const int grid_verts_num = grids.size() * key.grid_area;
          tls.factors.resize(grid_verts_num);
          const MutableSpan<float> factors = tls.factors;
          fill_factor_from_hide_and_mask(subdiv_ccg, grids, factors);
          if (automasking) {
            auto_mask::calc_grids_factors(object, *automasking, *nodes[i], grids, factors);
          }
          if (ss.cache) {
            const Span<float3> positions = gather_data_grids(
                subdiv_ccg, init_positions, grids, tls.positions);
            calc_brush_simulation_falloff(
                *brush, ss.cache->radius, sim_location, positions, factors);
          }
          scatter_data_grids(subdiv_ccg, factors.as_span(), grids, cloth_factors);
        }
      });
      break;
    }
    case bke::pbvh::Type::BMesh: {
      const BMesh &bm = *ss.bm;
      threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
        LocalData &tls = all_tls.local();
        for (const int i : range) {
          const Set<BMVert *, 0> &verts = BKE_pbvh_bmesh_node_unique_verts(nodes[i]);
          tls.factors.resize(verts.size());
          const MutableSpan<float> factors = tls.factors;
          fill_factor_from_hide_and_mask(bm, verts, factors);
          if (automasking) {
            auto_mask::calc_vert_factors(object, *automasking, *nodes[i], verts, factors);
          }
          if (ss.cache) {
            const MutableSpan<float3> positions = gather_data_vert_bmesh(
                init_positions, verts, tls.positions);
            calc_brush_simulation_falloff(
                *brush, ss.cache->radius, sim_location, positions, factors);
          }
          scatter_data_vert_bmesh(factors.as_span(), verts, cloth_factors);
        }
      });
      break;
    }
  }
}

static void cloth_brush_satisfy_constraints(const Object &object,
                                            const Brush *brush,
                                            SimulationData &cloth_sim)
{
  const SculptSession &ss = *object.sculpt;

  const float3 sim_location = cloth_brush_simulation_location_get(ss, brush);

  /* Precalculate factors into an array since we need random access to specific vertex values. */
  Array<float> factors(SCULPT_vertex_count_get(ss));
  calc_constraint_factors(object, brush, sim_location, cloth_sim.init_pos, factors);

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

void do_simulation_step(const Sculpt &sd,
                        Object &object,
                        SimulationData &cloth_sim,
                        Span<bke::pbvh::Node *> nodes)
{
  SculptSession &ss = *object.sculpt;
  const bke::pbvh::Tree &pbvh = *ss.pbvh;
  const Brush *brush = BKE_paint_brush_for_read(&sd.paint);

  /* Update the constraints. */
  cloth_brush_satisfy_constraints(object, brush, cloth_sim);

  Vector<bke::pbvh::Node *> active_nodes;
  for (bke::pbvh::Node *node : nodes) {
    const int node_index = cloth_sim.node_state_index.lookup(node);
    if (cloth_sim.node_state[node_index] == SCULPT_CLOTH_NODE_ACTIVE) {
      active_nodes.append(node);
      /* Nodes need to be enabled again to continue. */
      cloth_sim.node_state[node_index] = SCULPT_CLOTH_NODE_INACTIVE;
    }
  }

  const float3 sim_location = cloth_brush_simulation_location_get(ss, brush);

  threading::EnumerableThreadSpecific<LocalData> all_tls;
  switch (pbvh.type()) {
    case bke::pbvh::Type::Mesh: {
      Mesh &mesh = *static_cast<Mesh *>(object.data);
      const Span<float3> positions_eval = BKE_pbvh_get_vert_positions(pbvh);
      MutableSpan<float3> positions_orig = mesh.vert_positions_for_write();
      threading::parallel_for(active_nodes.index_range(), 1, [&](const IndexRange range) {
        LocalData &tls = all_tls.local();
        for (const int i : range) {
          const Span<int> verts = bke::pbvh::node_unique_verts(*nodes[i]);

          tls.factors.resize(verts.size());
          const MutableSpan<float> factors = tls.factors;
          fill_factor_from_hide_and_mask(mesh, verts, factors);
          if (const auto_mask::Cache *automasking = auto_mask::active_cache_get(ss)) {
            auto_mask::calc_vert_factors(object, *automasking, *nodes[i], verts, factors);
          }

          solve_verts_simulation(object, brush, sim_location, verts, factors, tls, cloth_sim);

          tls.translations.resize(verts.size());
          const MutableSpan<float3> translations = tls.translations;
          for (const int i : verts.index_range()) {
            translations[i] = cloth_sim.pos[verts[i]] - positions_eval[verts[i]];
          }
          write_translations(sd, object, positions_eval, verts, translations, positions_orig);
        }
      });
      break;
    }
    case bke::pbvh::Type::Grids: {
      SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;
      const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);
      const Span<CCGElem *> elems = subdiv_ccg.grids;
      threading::parallel_for(active_nodes.index_range(), 1, [&](const IndexRange range) {
        LocalData &tls = all_tls.local();
        for (const int i : range) {
          const Span<int> grids = bke::pbvh::node_grid_indices(*nodes[i]);
          const int grid_verts_num = grids.size() * key.grid_area;

          tls.factors.resize(grid_verts_num);
          const MutableSpan<float> factors = tls.factors;
          fill_factor_from_hide_and_mask(subdiv_ccg, grids, factors);
          if (const auto_mask::Cache *automasking = auto_mask::active_cache_get(ss)) {
            auto_mask::calc_grids_factors(object, *automasking, *nodes[i], grids, factors);
          }

          const Span<int> verts = calc_vert_indices_grids(key, grids, tls.vert_indices);
          solve_verts_simulation(object, brush, sim_location, verts, factors, tls, cloth_sim);

          for (const int i : grids.index_range()) {
            const int grid = grids[i];
            const int start = grid * key.grid_area;
            CCGElem *elem = elems[grid];
            for (const int offset : IndexRange(key.grid_area)) {
              const int grid_vert_index = start + offset;
              CCG_elem_offset_co(key, elem, offset) = cloth_sim.pos[grid_vert_index];
            }
          }
        }
      });
      break;
    }
    case bke::pbvh::Type::BMesh: {
      BMesh &bm = *ss.bm;
      threading::parallel_for(active_nodes.index_range(), 1, [&](const IndexRange range) {
        LocalData &tls = all_tls.local();
        for (const int i : range) {
          const Set<BMVert *, 0> &verts = BKE_pbvh_bmesh_node_unique_verts(nodes[i]);

          tls.factors.resize(verts.size());
          const MutableSpan<float> factors = tls.factors;
          fill_factor_from_hide_and_mask(bm, verts, factors);
          if (const auto_mask::Cache *automasking = auto_mask::active_cache_get(ss)) {
            auto_mask::calc_vert_factors(object, *automasking, *nodes[i], verts, factors);
          }

          const Span<int> vert_indices = calc_vert_indices_bmesh(verts, tls.vert_indices);
          solve_verts_simulation(
              object, brush, sim_location, vert_indices, factors, tls, cloth_sim);

          for (BMVert *vert : verts) {
            copy_v3_v3(vert->co, cloth_sim.pos[BM_elem_index_get(vert)]);
          }
        }
      });
      break;
    }
  }
}

static void cloth_brush_apply_brush_foces(const Sculpt &sd,
                                          Object &ob,
                                          Span<bke::pbvh::Node *> nodes)
{
  SculptSession &ss = *ob.sculpt;
  const Brush &brush = *BKE_paint_brush_for_read(&sd.paint);

  float3 area_no;
  float3 area_co;
  float3 offset;

  if (math::is_zero(ss.cache->grab_delta_symmetry)) {
    return;
  }

  float3 grab_delta = math::normalize(ss.cache->grab_delta_symmetry);

  /* Calculate push offset. */
  if (brush.cloth_deform_type == BRUSH_CLOTH_DEFORM_PUSH) {
    offset = ss.cache->sculpt_normal_symm * ss.cache->radius * ss.cache->scale * 2.0f;
  }

  float4x4 mat = float4x4::identity();
  if (brush.cloth_deform_type == BRUSH_CLOTH_DEFORM_PINCH_PERPENDICULAR ||
      brush.cloth_force_falloff_type == BRUSH_CLOTH_FORCE_FALLOFF_PLANE)
  {
    calc_brush_plane(brush, ob, nodes, area_no, area_co);

    /* Initialize stroke local space matrix. */
    mat.x_axis() = math::cross(area_no, ss.cache->grab_delta_symmetry);
    mat.y_axis() = math::cross(area_no, mat.x_axis());
    mat.z_axis() = area_no;
    mat.location() = ss.cache->location;
    normalize_m4(mat.ptr());

    /* Update matrix for the cursor preview. */
    if (ss.cache->mirror_symmetry_pass == 0) {
      ss.cache->stroke_local_mat = mat;
    }
  }

  if (ELEM(brush.cloth_deform_type, BRUSH_CLOTH_DEFORM_SNAKE_HOOK, BRUSH_CLOTH_DEFORM_GRAB)) {
    /* Set the deformation strength to 0. Brushes will initialize the strength in the required
     * area. */
    ss.cache->cloth_sim->deformation_strength.fill(0.0f);
  }

  threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
    for (const int i : range) {
      do_cloth_brush_apply_forces_task(ob, sd, brush, offset, grab_delta, mat, area_co, nodes[i]);
    }
  });
}

/* Allocates nodes state and initializes them to Uninitialized, so constraints can be created for
 * them. */
static void cloth_sim_initialize_default_node_state(SculptSession &ss, SimulationData &cloth_sim)
{
  Vector<bke::pbvh::Node *> nodes = bke::pbvh::search_gather(*ss.pbvh, {});

  cloth_sim.node_state = Array<NodeSimState>(nodes.size());
  for (const int i : nodes.index_range()) {
    cloth_sim.node_state[i] = SCULPT_CLOTH_NODE_UNINITIALIZED;
    cloth_sim.node_state_index.add(nodes[i], i);
  }
}

static void copy_positions_to_array(const SculptSession &ss, MutableSpan<float3> positions)
{
  bke::pbvh::Tree &pbvh = *ss.pbvh;
  switch (pbvh.type()) {
    case bke::pbvh::Type::Mesh:
      positions.copy_from(BKE_pbvh_get_vert_positions(pbvh));
      break;
    case bke::pbvh::Type::Grids: {
      SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;
      Vector<bke::pbvh::Node *> all_nodes = bke::pbvh::search_gather(pbvh, {});
      threading::parallel_for(all_nodes.index_range(), 8, [&](const IndexRange range) {
        Vector<float3> node_positions;
        for (const int i : range) {
          const Span<int> grids = bke::pbvh::node_grid_indices(*all_nodes[i]);
          gather_grids_positions(subdiv_ccg, grids, node_positions);
          scatter_data_grids(subdiv_ccg, node_positions.as_span(), grids, positions);
        }
      });
      break;
    }
    case bke::pbvh::Type::BMesh:
      BM_mesh_vert_coords_get(ss.bm, positions);
      break;
  }
}

static void copy_normals_to_array(const SculptSession &ss, MutableSpan<float3> normals)
{
  bke::pbvh::Tree &pbvh = *ss.pbvh;
  switch (pbvh.type()) {
    case bke::pbvh::Type::Mesh:
      normals.copy_from(BKE_pbvh_get_vert_normals(pbvh));
      break;
    case bke::pbvh::Type::Grids: {
      SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;
      const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);
      Vector<bke::pbvh::Node *> all_nodes = bke::pbvh::search_gather(pbvh, {});
      threading::parallel_for(all_nodes.index_range(), 8, [&](const IndexRange range) {
        Vector<float3> node_normals;
        for (const int i : range) {
          const Span<int> grids = bke::pbvh::node_grid_indices(*all_nodes[i]);

          const int grid_verts_num = grids.size() * key.grid_area;
          node_normals.resize(grid_verts_num);
          gather_grids_normals(subdiv_ccg, grids, node_normals);
          scatter_data_grids(subdiv_ccg, node_normals.as_span(), grids, normals);
        }
      });
      break;
    }
    case bke::pbvh::Type::BMesh:
      BM_mesh_vert_normals_get(ss.bm, normals);
      break;
  }
}

std::unique_ptr<SimulationData> brush_simulation_create(Object &ob,
                                                        const float cloth_mass,
                                                        const float cloth_damping,
                                                        const float cloth_softbody_strength,
                                                        const bool use_collisions,
                                                        const bool needs_deform_coords)
{
  SculptSession &ss = *ob.sculpt;
  const int totverts = SCULPT_vertex_count_get(ss);
  std::unique_ptr<SimulationData> cloth_sim = std::make_unique<SimulationData>();

  cloth_sim->length_constraints.reserve(CLOTH_LENGTH_CONSTRAINTS_BLOCK);

  cloth_sim->acceleration = Array<float3>(totverts, float3(0));
  cloth_sim->pos = Array<float3>(totverts, float3(0));
  cloth_sim->length_constraint_tweak = Array<float>(totverts, 0.0f);

  cloth_sim->init_pos.reinitialize(totverts);
  copy_positions_to_array(ss, cloth_sim->init_pos);

  cloth_sim->last_iteration_pos = cloth_sim->init_pos;
  cloth_sim->prev_pos = cloth_sim->init_pos;

  cloth_sim->init_no.reinitialize(totverts);
  copy_normals_to_array(ss, cloth_sim->init_no);

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
    cloth_sim->collider_list = cloth_brush_collider_cache_create(ob, ss.depsgraph);
  }

  cloth_sim_initialize_default_node_state(ss, *cloth_sim);

  return cloth_sim;
}

void brush_store_simulation_state(const SculptSession &ss, SimulationData &cloth_sim)
{
  copy_positions_to_array(ss, cloth_sim.pos);
}

void sim_activate_nodes(SimulationData &cloth_sim, Span<bke::pbvh::Node *> nodes)
{
  /* Activate the nodes inside the simulation area. */
  for (bke::pbvh::Node *node : nodes) {
    const int node_index = cloth_sim.node_state_index.lookup(node);
    cloth_sim.node_state[node_index] = SCULPT_CLOTH_NODE_ACTIVE;
  }
}

static void sculpt_cloth_ensure_constraints_in_simulation_area(const Sculpt &sd,
                                                               Object &ob,
                                                               Span<bke::pbvh::Node *> nodes)
{
  SculptSession &ss = *ob.sculpt;
  const Brush *brush = BKE_paint_brush_for_read(&sd.paint);
  const float radius = ss.cache->initial_radius;
  const float limit = radius + (radius * brush->cloth_sim_limit);
  const float3 sim_location = cloth_brush_simulation_location_get(ss, brush);
  ensure_nodes_constraints(sd, ob, nodes, *ss.cache->cloth_sim, sim_location, limit);
}

void do_cloth_brush(const Sculpt &sd, Object &ob, Span<bke::pbvh::Node *> nodes)
{
  SculptSession &ss = *ob.sculpt;
  const Brush *brush = BKE_paint_brush_for_read(&sd.paint);

  /* Brushes that use anchored strokes and restore the mesh can't rely on symmetry passes and steps
   * count as it is always the first step, so the simulation needs to be created when it does not
   * exist for this stroke. */
  if (SCULPT_stroke_is_first_brush_step_of_symmetry_pass(*ss.cache) || !ss.cache->cloth_sim) {

    /* The simulation structure only needs to be created on the first symmetry pass. */
    if (SCULPT_stroke_is_first_brush_step(*ss.cache) || !ss.cache->cloth_sim) {
      ss.cache->cloth_sim = brush_simulation_create(ob,
                                                    brush->cloth_mass,
                                                    brush->cloth_damping,
                                                    brush->cloth_constraint_softbody_strength,
                                                    (brush->flag2 & BRUSH_CLOTH_USE_COLLISION),
                                                    is_cloth_deform_brush(*brush));
    }

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
      sculpt_cloth_ensure_constraints_in_simulation_area(sd, ob, nodes);
    }
    /* The first step of a symmetry pass is never simulated as deformation modes need valid delta
     * for brush tip alignment. */
    return;
  }

  /* Ensure the constraints for the nodes. */
  sculpt_cloth_ensure_constraints_in_simulation_area(sd, ob, nodes);

  /* Store the initial state in the simulation. */
  brush_store_simulation_state(ss, *ss.cache->cloth_sim);

  /* Enable the nodes that should be simulated. */
  sim_activate_nodes(*ss.cache->cloth_sim, nodes);

  /* Apply forces to the vertices. */
  cloth_brush_apply_brush_foces(sd, ob, nodes);

  /* Update and write the simulation to the nodes. */
  do_simulation_step(sd, ob, *ss.cache->cloth_sim, nodes);
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
    add_v3_v3v3(local_mat[3], ss.cache->true_location, ss.cache->grab_delta);
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

static void cloth_filter_apply_displacement_to_deform_co(const int v_index,
                                                         const float3 &disp,
                                                         filter::Cache &filter_cache)
{
  float3 final_disp = filter::zero_disabled_axis_components(filter_cache, disp);
  filter_cache.cloth_sim->deformation_pos[v_index] = filter_cache.cloth_sim->init_pos[v_index] +
                                                     final_disp;
}

static void cloth_filter_apply_forces_to_vertices(const int v_index,
                                                  const float3 &force,
                                                  const float3 &gravity,
                                                  filter::Cache &filter_cache)
{
  const float3 final_force = filter::zero_disabled_axis_components(filter_cache, force) + gravity;
  cloth_brush_apply_force_to_vertex(*filter_cache.cloth_sim, final_force, v_index);
}

BLI_NOINLINE static void apply_gravity_to_verts(const float3 &gravity,
                                                const Span<int> verts,
                                                const Span<float> factors,
                                                filter::Cache &filter_cache)
{
  for (const int i : verts.index_range()) {
    const int vert = verts[i];
    float3 force(0.0f);
    if (filter_cache.orientation == filter::FilterOrientation::View) {
      /* When using the view orientation apply gravity in the -Y axis, this way objects will
       * fall down instead of backwards. */
      force[1] = -factors[i];
    }
    else {
      force[2] = -factors[i];
    }
    force = filter::to_object_space(filter_cache, force);
    cloth_filter_apply_forces_to_vertices(vert, force, gravity, filter_cache);
  }
}

BLI_NOINLINE static void apply_inflate_to_verts(const float3 &gravity,
                                                const Span<int> verts,
                                                const Span<float> factors,
                                                const Span<float3> normals,
                                                filter::Cache &filter_cache)
{
  for (const int i : verts.index_range()) {
    const int vert = verts[i];
    const float3 force = normals[i] * factors[i];
    cloth_filter_apply_forces_to_vertices(vert, force, gravity, filter_cache);
  }
}

BLI_NOINLINE static void apply_expand_to_verts(const float3 &gravity,
                                               const Span<int> verts,
                                               const Span<float> factors,
                                               filter::Cache &filter_cache)
{
  MutableSpan<float> length_constraint_tweak = filter_cache.cloth_sim->length_constraint_tweak;
  for (const int i : verts.index_range()) {
    const int vert = verts[i];
    length_constraint_tweak[vert] += factors[i] * 0.01f;
    cloth_filter_apply_forces_to_vertices(vert, float3(0), gravity, filter_cache);
  }
}

BLI_NOINLINE static void apply_pinch_to_verts(const float3 &gravity,
                                              const Span<int> verts,
                                              const Span<float> factors,
                                              const Span<float3> positions,
                                              filter::Cache &filter_cache)
{
  for (const int i : verts.index_range()) {
    const int vert = verts[i];
    float3 force = math::normalize(filter_cache.cloth_sim_pinch_point - positions[i]);
    force *= factors[i];
    cloth_filter_apply_forces_to_vertices(vert, force, gravity, filter_cache);
  }
}

BLI_NOINLINE static void apply_scale_to_verts(const Span<int> verts,
                                              const Span<float> factors,
                                              filter::Cache &filter_cache)
{
  MutableSpan<float3> init_pos = filter_cache.cloth_sim->init_pos;
  for (const int i : verts.index_range()) {
    const int vert = verts[i];
    float3x3 transform = math::from_scale<float3x3>(float3(1.0f + factors[i]));
    float3 disp = transform * init_pos[vert] - init_pos[vert];
    cloth_filter_apply_displacement_to_deform_co(vert, disp, filter_cache);
  }
}

struct FilterLocalData {
  Vector<float> factors;
  Vector<int> vert_indices;
  Vector<float3> positions;
  Vector<float3> normals;
};

static void apply_filter_forces_mesh(const ClothFilterType filter_type,
                                     const float filter_strength,
                                     const float3 &gravity,
                                     const Span<float3> positions_eval,
                                     const Span<float3> vert_normals,
                                     const bke::pbvh::Node &node,
                                     Object &object,
                                     FilterLocalData &tls)
{
  const SculptSession &ss = *object.sculpt;
  const Mesh &mesh = *static_cast<Mesh *>(object.data);

  const Span<int> verts = bke::pbvh::node_unique_verts(node);

  tls.factors.resize(verts.size());
  const MutableSpan<float> factors = tls.factors;
  fill_factor_from_hide_and_mask(mesh, verts, factors);
  if (const auto_mask::Cache *automasking = auto_mask::active_cache_get(ss)) {
    auto_mask::calc_vert_factors(object, *automasking, node, verts, factors);
  }

  if (ss.filter_cache->active_face_set != SCULPT_FACE_SET_NONE) {
    for (const int i : verts.index_range()) {
      const int vert = verts[i];
      if (!face_set::vert_has_face_set(
              ss.vert_to_face_map, ss.face_sets, vert, ss.filter_cache->active_face_set))
      {
        factors[i] = 0.0f;
      }
    }
  }

  scale_factors(factors, filter_strength);

  switch (filter_type) {
    case ClothFilterType::Gravity:
      apply_gravity_to_verts(gravity, verts, factors, *ss.filter_cache);
      break;
    case ClothFilterType::Inflate:
      apply_inflate_to_verts(gravity,
                             verts,
                             factors,
                             gather_data_mesh(vert_normals, verts, tls.normals),
                             *ss.filter_cache);
      break;
    case ClothFilterType::Expand:
      apply_expand_to_verts(gravity, verts, factors, *ss.filter_cache);
      break;
    case ClothFilterType::Pinch:
      apply_pinch_to_verts(gravity,
                           verts,
                           factors,
                           gather_data_mesh(positions_eval, verts, tls.positions),
                           *ss.filter_cache);
      break;
    case ClothFilterType::Scale:
      apply_scale_to_verts(verts, factors, *ss.filter_cache);
      break;
  }
}

static void apply_filter_forces_grids(const ClothFilterType filter_type,
                                      const float filter_strength,
                                      const float3 &gravity,
                                      const bke::pbvh::Node &node,
                                      Object &object,
                                      FilterLocalData &tls)
{
  const SculptSession &ss = *object.sculpt;
  const SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;
  const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);

  const Span<int> grids = bke::pbvh::node_grid_indices(node);
  const int grid_verts_num = grids.size() * key.grid_area;

  tls.factors.resize(grid_verts_num);
  const MutableSpan<float> factors = tls.factors;
  fill_factor_from_hide_and_mask(subdiv_ccg, grids, factors);
  if (const auto_mask::Cache *automasking = auto_mask::active_cache_get(ss)) {
    auto_mask::calc_grids_factors(object, *automasking, node, grids, factors);
  }

  if (ss.filter_cache->active_face_set != SCULPT_FACE_SET_NONE) {
    for (const int i : grids.index_range()) {
      if (!face_set::vert_has_face_set(
              subdiv_ccg, ss.face_sets, grids[i], ss.filter_cache->active_face_set))
      {
        factors.slice(i * key.grid_area, key.grid_area).fill(0.0f);
      }
    }
  }

  scale_factors(factors, filter_strength);

  const Span<int> verts = calc_vert_indices_grids(key, grids, tls.vert_indices);

  switch (filter_type) {
    case ClothFilterType::Gravity:
      apply_gravity_to_verts(gravity, verts, factors, *ss.filter_cache);
      break;
    case ClothFilterType::Inflate:
      tls.normals.resize(grid_verts_num);
      gather_grids_normals(subdiv_ccg, grids, tls.normals);
      apply_inflate_to_verts(gravity, verts, factors, tls.normals, *ss.filter_cache);
      break;
    case ClothFilterType::Expand:
      apply_expand_to_verts(gravity, verts, factors, *ss.filter_cache);
      break;
    case ClothFilterType::Pinch: {
      apply_pinch_to_verts(gravity,
                           verts,
                           factors,
                           gather_grids_positions(subdiv_ccg, grids, tls.positions),
                           *ss.filter_cache);
      break;
    }
    case ClothFilterType::Scale:
      apply_scale_to_verts(verts, factors, *ss.filter_cache);
      break;
  }
}

static void apply_filter_forces_bmesh(const ClothFilterType filter_type,
                                      const float filter_strength,
                                      const float3 &gravity,
                                      bke::pbvh::Node &node,
                                      Object &object,
                                      FilterLocalData &tls)
{
  const SculptSession &ss = *object.sculpt;
  const BMesh &bm = *ss.bm;

  const Set<BMVert *, 0> &verts = BKE_pbvh_bmesh_node_unique_verts(&node);

  tls.factors.resize(verts.size());
  const MutableSpan<float> factors = tls.factors;
  fill_factor_from_hide_and_mask(bm, verts, factors);
  if (const auto_mask::Cache *automasking = auto_mask::active_cache_get(ss)) {
    auto_mask::calc_vert_factors(object, *automasking, node, verts, factors);
  }

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

  switch (filter_type) {
    case ClothFilterType::Gravity:
      apply_gravity_to_verts(gravity, vert_indices, factors, *ss.filter_cache);
      break;
    case ClothFilterType::Inflate:
      tls.normals.resize(verts.size());
      gather_bmesh_normals(verts, tls.normals);
      apply_inflate_to_verts(gravity, vert_indices, factors, tls.normals, *ss.filter_cache);
      break;
    case ClothFilterType::Expand:
      apply_expand_to_verts(gravity, vert_indices, factors, *ss.filter_cache);
      break;
    case ClothFilterType::Pinch:
      apply_pinch_to_verts(gravity,
                           vert_indices,
                           factors,
                           gather_bmesh_positions(verts, tls.positions),
                           *ss.filter_cache);
      break;
    case ClothFilterType::Scale:
      apply_scale_to_verts(vert_indices, factors, *ss.filter_cache);
      break;
  }
}

static int sculpt_cloth_filter_modal(bContext *C, wmOperator *op, const wmEvent *event)
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

  SCULPT_vertex_random_access_ensure(ss);

  BKE_sculpt_update_object_for_edit(depsgraph, &object, false);

  brush_store_simulation_state(ss, *ss.filter_cache->cloth_sim);

  const Span<bke::pbvh::Node *> nodes = ss.filter_cache->nodes;

  float3 gravity(0.0f);
  if (sd.gravity_object) {
    gravity = sd.gravity_object->object_to_world().ptr()[2];
  }
  else {
    gravity[2] = -1.0f;
  }
  gravity *= sd.gravity_factor * filter_strength;

  threading::EnumerableThreadSpecific<FilterLocalData> all_tls;
  switch (ss.pbvh->type()) {
    case bke::pbvh::Type::Mesh: {
      const bke::pbvh::Tree &pbvh = *ss.pbvh;
      const Span<float3> positions_eval = BKE_pbvh_get_vert_positions(pbvh);
      const Span<float3> vert_normals = BKE_pbvh_get_vert_normals(pbvh);
      threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
        FilterLocalData &tls = all_tls.local();
        for (const int i : range) {
          apply_filter_forces_mesh(filter_type,
                                   filter_strength,
                                   gravity,
                                   positions_eval,
                                   vert_normals,
                                   *nodes[i],
                                   object,
                                   tls);
          BKE_pbvh_node_mark_positions_update(nodes[i]);
        }
      });
      break;
    }
    case bke::pbvh::Type::Grids:
      threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
        FilterLocalData &tls = all_tls.local();
        for (const int i : range) {
          apply_filter_forces_grids(filter_type, filter_strength, gravity, *nodes[i], object, tls);
          BKE_pbvh_node_mark_positions_update(nodes[i]);
        }
      });
      break;
    case bke::pbvh::Type::BMesh:
      threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
        FilterLocalData &tls = all_tls.local();
        for (const int i : range) {
          apply_filter_forces_bmesh(filter_type, filter_strength, gravity, *nodes[i], object, tls);
          BKE_pbvh_node_mark_positions_update(nodes[i]);
        }
      });
      break;
  }

  /* Activate all nodes. */
  sim_activate_nodes(*ss.filter_cache->cloth_sim, nodes);

  /* Update and write the simulation to the nodes. */
  do_simulation_step(sd, object, *ss.filter_cache->cloth_sim, nodes);

  flush_update_step(C, UpdateType::Position);
  return OPERATOR_RUNNING_MODAL;
}

static int sculpt_cloth_filter_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  Object &ob = *CTX_data_active_object(C);
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  const Sculpt &sd = *CTX_data_tool_settings(C)->sculpt;
  SculptSession &ss = *ob.sculpt;

  const View3D *v3d = CTX_wm_view3d(C);
  const Base *base = CTX_data_active_base(C);
  if (!BKE_base_is_visible(v3d, base)) {
    return OPERATOR_CANCELLED;
  }

  const ClothFilterType filter_type = ClothFilterType(RNA_enum_get(op->ptr, "type"));

  /* Update the active vertex */
  float2 mval_fl{float(event->mval[0]), float(event->mval[1])};
  SculptCursorGeometryInfo sgi;
  SCULPT_cursor_geometry_info_update(C, &sgi, mval_fl, false);

  /* Needs mask data to be available as it is used when solving the constraints. */
  BKE_sculpt_update_object_for_edit(depsgraph, &ob, false);

  if (ED_sculpt_report_if_shape_key_is_locked(ob, op->reports)) {
    return OPERATOR_CANCELLED;
  }

  SCULPT_stroke_id_next(ob);

  undo::push_begin(ob, op);
  filter::cache_init(C,
                     ob,
                     sd,
                     undo::Type::Position,
                     mval_fl,
                     RNA_float_get(op->ptr, "area_normal_radius"),
                     RNA_float_get(op->ptr, "strength"));

  ss.filter_cache->automasking = auto_mask::cache_init(sd, ob);

  const float cloth_mass = RNA_float_get(op->ptr, "cloth_mass");
  const float cloth_damping = RNA_float_get(op->ptr, "cloth_damping");
  const bool use_collisions = RNA_boolean_get(op->ptr, "use_collisions");
  ss.filter_cache->cloth_sim = brush_simulation_create(
      ob,
      cloth_mass,
      cloth_damping,
      0.0f,
      use_collisions,
      cloth_filter_is_deformation_filter(filter_type));

  ss.filter_cache->cloth_sim_pinch_point = SCULPT_active_vertex_co_get(ss);

  float3 origin(0);
  ensure_nodes_constraints(
      sd, ob, ss.filter_cache->nodes, *ss.filter_cache->cloth_sim, origin, FLT_MAX);

  const bool use_face_sets = RNA_boolean_get(op->ptr, "use_face_sets");
  if (use_face_sets) {
    ss.filter_cache->active_face_set = face_set::active_face_set_get(ss);
  }
  else {
    ss.filter_cache->active_face_set = SCULPT_FACE_SET_NONE;
  }

  const int force_axis = RNA_enum_get(op->ptr, "force_axis");
  ss.filter_cache->enabled_force_axis[0] = force_axis & CLOTH_FILTER_FORCE_X;
  ss.filter_cache->enabled_force_axis[1] = force_axis & CLOTH_FILTER_FORCE_Y;
  ss.filter_cache->enabled_force_axis[2] = force_axis & CLOTH_FILTER_FORCE_Z;

  filter::FilterOrientation orientation = filter::FilterOrientation(
      RNA_enum_get(op->ptr, "orientation"));
  ss.filter_cache->orientation = orientation;

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
