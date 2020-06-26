/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2013 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup depsgraph
 */

#pragma once

#include "intern/node/deg_node.h"

#include "intern/depsgraph_type.h"

struct Depsgraph;

namespace DEG {

struct ComponentNode;

/* Evaluation Operation for atomic operation */
// XXX: move this to another header that can be exposed?
typedef function<void(struct ::Depsgraph *)> DepsEvalOperationCb;

/* Identifiers for common operations (as an enum). */
enum class OperationCode {
  /* Generic Operations. -------------------------------------------------- */

  /* Placeholder for operations which don't need special mention */
  OPERATION = 0,

  /* Generic parameters evaluation. */
  ID_PROPERTY,
  PARAMETERS_ENTRY,
  PARAMETERS_EVAL,
  PARAMETERS_EXIT,

  /* Animation, Drivers, etc. --------------------------------------------- */
  /* NLA + Action */
  ANIMATION_ENTRY,
  ANIMATION_EVAL,
  ANIMATION_EXIT,
  /* Driver */
  DRIVER,

  /* Scene related. ------------------------------------------------------- */
  SCENE_EVAL,
  AUDIO_VOLUME,

  /* Object related. ------------------------------------------------------ */
  OBJECT_FROM_LAYER_ENTRY,
  OBJECT_BASE_FLAGS,
  OBJECT_FROM_LAYER_EXIT,
  DIMENSIONS,

  /* Transform. ----------------------------------------------------------- */
  /* Transform entry point. */
  TRANSFORM_INIT,
  /* Local transforms only */
  TRANSFORM_LOCAL,
  /* Parenting */
  TRANSFORM_PARENT,
  /* Constraints */
  TRANSFORM_CONSTRAINTS,
  /* Handle object-level updates, mainly proxies hacks and recalc flags.  */
  TRANSFORM_EVAL,
  /* Initializes transformation for simulation.
   * For example, ensures point cache is properly reset before doing rigid
   * body simulation. */
  TRANSFORM_SIMULATION_INIT,
  /* Transform exit point */
  TRANSFORM_FINAL,

  /* Rigid body. ---------------------------------------------------------- */
  /* Perform Simulation */
  RIGIDBODY_REBUILD,
  RIGIDBODY_SIM,
  /* Copy results to object */
  RIGIDBODY_TRANSFORM_COPY,

  /* Geometry. ------------------------------------------------------------ */

  /* Initialize evaluation of the geometry. Is an entry operation of geometry
   * component. */
  GEOMETRY_EVAL_INIT,
  /* Evaluate the whole geometry, including modifiers. */
  GEOMETRY_EVAL,
  /* Evaluation of geometry is completely done.. */
  GEOMETRY_EVAL_DONE,
  /* Evaluation of a shape key.
   * NOTE: Currently only for object data data-blocks. */
  GEOMETRY_SHAPEKEY,

  /* Object data. --------------------------------------------------------- */
  LIGHT_PROBE_EVAL,
  SPEAKER_EVAL,
  SOUND_EVAL,
  ARMATURE_EVAL,

  /* Pose. ---------------------------------------------------------------- */
  /* Init pose, clear flags, etc. */
  POSE_INIT,
  /* Initialize IK solver related pose stuff. */
  POSE_INIT_IK,
  /* Pose is evaluated, and runtime data can be freed. */
  POSE_CLEANUP,
  /* Pose has been fully evaluated and ready to be used by others. */
  POSE_DONE,
  /* IK/Spline Solvers */
  POSE_IK_SOLVER,
  POSE_SPLINE_IK_SOLVER,

  /* Bone. ---------------------------------------------------------------- */
  /* Bone local transforms - entry point */
  BONE_LOCAL,
  /* Pose-space conversion (includes parent + restpose, */
  BONE_POSE_PARENT,
  /* Constraints */
  BONE_CONSTRAINTS,
  /* Bone transforms are ready
   *
   * - "READY"  This (internal, noop is used to signal that all pre-IK
   *            operations are done. Its role is to help mediate situations
   *            where cyclic relations may otherwise form (i.e. one bone in
   *            chain targeting another in same chain,
   *
   * - "DONE"   This noop is used to signal that the bone's final pose
   *            transform can be read by others. */
  // TODO: deform mats could get calculated in the final_transform ops...
  BONE_READY,
  BONE_DONE,
  /* B-Bone segment shape computation (after DONE) */
  BONE_SEGMENTS,

  /* Particle System. ----------------------------------------------------- */
  PARTICLE_SYSTEM_INIT,
  PARTICLE_SYSTEM_EVAL,
  PARTICLE_SYSTEM_DONE,

  /* Particle Settings. --------------------------------------------------- */
  PARTICLE_SETTINGS_INIT,
  PARTICLE_SETTINGS_EVAL,
  PARTICLE_SETTINGS_RESET,

  /* Point Cache. --------------------------------------------------------- */
  POINT_CACHE_RESET,

  /* File cache. ---------------------------------------------------------- */
  FILE_CACHE_UPDATE,

  /* Collections. --------------------------------------------------------- */
  VIEW_LAYER_EVAL,

  /* Copy on Write. ------------------------------------------------------- */
  COPY_ON_WRITE,

  /* Shading. ------------------------------------------------------------- */
  SHADING,
  MATERIAL_UPDATE,
  LIGHT_UPDATE,
  WORLD_UPDATE,

  /* Batch caches. -------------------------------------------------------- */
  GEOMETRY_SELECT_UPDATE,

  /* Masks. --------------------------------------------------------------- */
  MASK_ANIMATION,
  MASK_EVAL,

  /* Movie clips. --------------------------------------------------------- */
  MOVIECLIP_EVAL,
  MOVIECLIP_SELECT_UPDATE,

  /* Images. -------------------------------------------------------------- */
  IMAGE_ANIMATION,

  /* Synchronization. ----------------------------------------------------- */
  SYNCHRONIZE_TO_ORIGINAL,

  /* Generic data-block --------------------------------------------------- */
  GENERIC_DATABLOCK_UPDATE,

  /* Sequencer. ----------------------------------------------------------- */

  SEQUENCES_EVAL,

  /* Duplication/instancing system. --------------------------------------- */
  DUPLI,

  /* Simulation. ---------------------------------------------------------- */
  SIMULATION_EVAL,
};
const char *operationCodeAsString(OperationCode opcode);

/* Flags for Depsgraph Nodes.
 * NOTE: IS a bit shifts to allow usage as an accumulated. bitmask.
 */
enum OperationFlag {
  /* Node needs to be updated. */
  DEPSOP_FLAG_NEEDS_UPDATE = (1 << 0),
  /* Node was directly modified, causing need for update. */
  DEPSOP_FLAG_DIRECTLY_MODIFIED = (1 << 1),
  /* Node was updated due to user input. */
  DEPSOP_FLAG_USER_MODIFIED = (1 << 2),
  /* Node may not be removed, even when it has no evaluation callback and no
   * outgoing relations. This is for NO-OP nodes that are purely used to indicate a
   * relation between components/IDs, and not for connecting to an operation. */
  DEPSOP_FLAG_PINNED = (1 << 3),

  /* Set of flags which gets flushed along the relations. */
  DEPSOP_FLAG_FLUSH = (DEPSOP_FLAG_USER_MODIFIED),
};

/* Atomic Operation - Base type for all operations */
struct OperationNode : public Node {
  OperationNode();
  ~OperationNode();

  virtual string identifier() const override;
  string full_identifier() const;

  virtual void tag_update(Depsgraph *graph, eUpdateSource source) override;

  bool is_noop() const
  {
    return (bool)evaluate == false;
  }

  virtual OperationNode *get_entry_operation() override
  {
    return this;
  }
  virtual OperationNode *get_exit_operation() override
  {
    return this;
  }

  /* Set this operation as component's entry/exit operation. */
  void set_as_entry();
  void set_as_exit();

  /* Component that contains the operation. */
  ComponentNode *owner;

  /* Callback for operation. */
  DepsEvalOperationCb evaluate;

  /* How many inlinks are we still waiting on before we can be evaluated. */
  uint32_t num_links_pending;
  bool scheduled;

  /* Identifier for the operation being performed. */
  OperationCode opcode;
  int name_tag;

  /* (OperationFlag) extra settings affecting evaluation. */
  int flag;

  DEG_DEPSNODE_DECLARE;
};

void deg_register_operation_depsnodes();

}  // namespace DEG
