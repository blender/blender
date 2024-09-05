/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edsculpt
 */

#pragma once

#include "BLI_array.hh"
#include "BLI_index_mask_fwd.hh"
#include "BLI_map.hh"
#include "BLI_math_vector.hh"
#include "BLI_vector.hh"

#include "BKE_collision.h"

struct Brush;
struct Sculpt;
struct SculptSession;
namespace blender::bke::pbvh {
class Node;
}

namespace blender::ed::sculpt_paint::cloth {

/* Cloth Simulation. */
enum NodeSimState {
  /* Constraints were not built for this node, so it can't be simulated. */
  SCULPT_CLOTH_NODE_UNINITIALIZED,

  /* There are constraints for the geometry in this node, but it should not be simulated. */
  SCULPT_CLOTH_NODE_INACTIVE,

  /* There are constraints for this node and they should be used by the solver. */
  SCULPT_CLOTH_NODE_ACTIVE,
};

enum ConstraintType {
  /* Constraint that creates the structure of the cloth. */
  SCULPT_CLOTH_CONSTRAINT_STRUCTURAL = 0,
  /* Constraint that references the position of a vertex and a position in deformation_pos which
   * can be deformed by the tools. */
  SCULPT_CLOTH_CONSTRAINT_DEFORMATION = 1,
  /* Constraint that references the vertex position and a editable soft-body position for
   * plasticity. */
  SCULPT_CLOTH_CONSTRAINT_SOFTBODY = 2,
  /* Constraint that references the vertex position and its initial position. */
  SCULPT_CLOTH_CONSTRAINT_PIN = 3,
};

struct LengthConstraint {
  /* Elements that are affected by the constraint. */
  /* Element a should always be a mesh vertex with the index stored in elem_index_a as it is always
   * deformed. Element b could be another vertex of the same mesh or any other position (arbitrary
   * point, position for a previous state). In that case, elem_index_a and elem_index_b should be
   * the same to avoid affecting two different vertices when solving the constraints.
   * *elem_position points to the position which is owned by the element. */
  int elem_index_a;
  float *elem_position_a;

  int elem_index_b;
  float *elem_position_b;

  float length;
  float strength;

  /* Index in #SimulationData.node_state of the node from where this constraint was created.
   * This constraints will only be used by the solver if the state is active. */
  int node;

  ConstraintType type;
};

struct SimulationData {
  Vector<LengthConstraint> length_constraints;
  Array<float> length_constraint_tweak;

  /* Position anchors for deformation brushes. These positions are modified by the brush and the
   * final positions of the simulated vertices are updated with constraints that use these points
   * as targets. */
  Array<float3> deformation_pos;
  Array<float> deformation_strength;

  float mass;
  float damping;
  float softbody_strength;

  Array<float3> acceleration;
  Array<float3> pos;
  Array<float3> init_pos;
  Array<float3> init_no;
  Array<float3> softbody_pos;
  Array<float3> prev_pos;
  Array<float3> last_iteration_pos;

  Vector<ColliderCache> collider_list;

  int totnode;
  Map<const bke::pbvh::Node *, int> node_state_index;
  Array<NodeSimState> node_state;

  ~SimulationData();
};

/* Public functions. */

std::unique_ptr<SimulationData> brush_simulation_create(const Depsgraph &depsgraph,
                                                        Object &ob,
                                                        float cloth_mass,
                                                        float cloth_damping,
                                                        float cloth_softbody_strength,
                                                        bool use_collisions,
                                                        bool needs_deform_coords);

void sim_activate_nodes(Object &object, SimulationData &cloth_sim, const IndexMask &node_mask);

void brush_store_simulation_state(const Depsgraph &depsgraph,
                                  const Object &object,
                                  SimulationData &cloth_sim);

void do_simulation_step(const Depsgraph &depsgraph,
                        const Sculpt &sd,
                        Object &ob,
                        SimulationData &cloth_sim,
                        const IndexMask &node_mask);

void ensure_nodes_constraints(const Sculpt &sd,
                              Object &ob,
                              const IndexMask &node_mask,
                              SimulationData &cloth_sim,
                              const float3 &initial_location,
                              float radius);

/**
 * Cursor drawing function.
 */
void simulation_limits_draw(uint gpuattr,
                            const Brush &brush,
                            const float location[3],
                            const float normal[3],
                            float rds,
                            float line_width,
                            const float outline_col[3],
                            float alpha);
void plane_falloff_preview_draw(uint gpuattr,
                                SculptSession &ss,
                                const float outline_col[3],
                                float outline_alpha);

IndexMask brush_affected_nodes_gather(const Object &object,
                                      const Brush &brush,
                                      IndexMaskMemory &memory);

bool is_cloth_deform_brush(const Brush &brush);

}  // namespace blender::ed::sculpt_paint::cloth
