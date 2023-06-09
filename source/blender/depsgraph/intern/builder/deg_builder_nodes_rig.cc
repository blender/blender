/* SPDX-FileCopyrightText: 2013 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup depsgraph
 *
 * Methods for constructing depsgraph's nodes
 */

#include "intern/builder/deg_builder_nodes.h"

#include <cstdio>
#include <cstdlib>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "DNA_anim_types.h"
#include "DNA_armature_types.h"
#include "DNA_constraint_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_action.h"
#include "BKE_armature.h"
#include "BKE_constraint.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"

#include "intern/builder/deg_builder.h"
#include "intern/depsgraph_type.h"
#include "intern/eval/deg_eval_copy_on_write.h"
#include "intern/node/deg_node.h"
#include "intern/node/deg_node_component.h"
#include "intern/node/deg_node_operation.h"

namespace blender::deg {

void DepsgraphNodeBuilder::build_pose_constraints(Object *object,
                                                  bPoseChannel *pchan,
                                                  int pchan_index)
{
  /* Pull indirect dependencies via constraints. */
  BuilderWalkUserData data;
  data.builder = this;
  BKE_constraints_id_loop(&pchan->constraints, constraint_walk, &data);

  /* Create node for constraint stack. */
  Scene *scene_cow = get_cow_datablock(scene_);
  Object *object_cow = get_cow_datablock(object);
  add_operation_node(&object->id,
                     NodeType::BONE,
                     pchan->name,
                     OperationCode::BONE_CONSTRAINTS,
                     [scene_cow, object_cow, pchan_index](::Depsgraph *depsgraph) {
                       BKE_pose_constraints_evaluate(
                           depsgraph, scene_cow, object_cow, pchan_index);
                     });
}

/* IK Solver Eval Steps */
void DepsgraphNodeBuilder::build_ik_pose(Object *object, bPoseChannel *pchan, bConstraint *con)
{
  bKinematicConstraint *data = (bKinematicConstraint *)con->data;

  /* Find the chain's root. */
  bPoseChannel *rootchan = BKE_armature_ik_solver_find_root(pchan, data);
  if (rootchan == nullptr) {
    return;
  }

  if (has_operation_node(
          &object->id, NodeType::EVAL_POSE, rootchan->name, OperationCode::POSE_IK_SOLVER))
  {
    return;
  }

  int rootchan_index = BLI_findindex(&object->pose->chanbase, rootchan);
  BLI_assert(rootchan_index != -1);

  /* Operation node for evaluating/running IK Solver. */
  Scene *scene_cow = get_cow_datablock(scene_);
  Object *object_cow = get_cow_datablock(object);
  add_operation_node(&object->id,
                     NodeType::EVAL_POSE,
                     rootchan->name,
                     OperationCode::POSE_IK_SOLVER,
                     [scene_cow, object_cow, rootchan_index](::Depsgraph *depsgraph) {
                       BKE_pose_iktree_evaluate(depsgraph, scene_cow, object_cow, rootchan_index);
                     });
}

/* Spline IK Eval Steps */
void DepsgraphNodeBuilder::build_splineik_pose(Object *object,
                                               bPoseChannel *pchan,
                                               bConstraint *con)
{
  bSplineIKConstraint *data = (bSplineIKConstraint *)con->data;

  /* Find the chain's root. */
  bPoseChannel *rootchan = BKE_armature_splineik_solver_find_root(pchan, data);

  if (has_operation_node(
          &object->id, NodeType::EVAL_POSE, rootchan->name, OperationCode::POSE_SPLINE_IK_SOLVER))
  {
    return;
  }

  /* Operation node for evaluating/running Spline IK Solver.
   * Store the "root bone" of this chain in the solver, so it knows where to
   * start. */
  int rootchan_index = BLI_findindex(&object->pose->chanbase, rootchan);
  BLI_assert(rootchan_index != -1);

  Scene *scene_cow = get_cow_datablock(scene_);
  Object *object_cow = get_cow_datablock(object);
  add_operation_node(&object->id,
                     NodeType::EVAL_POSE,
                     rootchan->name,
                     OperationCode::POSE_SPLINE_IK_SOLVER,
                     [scene_cow, object_cow, rootchan_index](::Depsgraph *depsgraph) {
                       BKE_pose_splineik_evaluate(
                           depsgraph, scene_cow, object_cow, rootchan_index);
                     });
}

/* Pose/Armature Bones Graph */
void DepsgraphNodeBuilder::build_rig(Object *object)
{
  bArmature *armature = (bArmature *)object->data;
  Scene *scene_cow = get_cow_datablock(scene_);
  Object *object_cow = get_cow_datablock(object);
  OperationNode *op_node;
  /* Animation and/or drivers linking pose-bones to base-armature used to define them.
   *
   * NOTE: AnimData here is really used to control animated deform properties,
   *       which ideally should be able to be unique across different
   *       instances. Eventually, we need some type of proxy/isolation
   *       mechanism in-between here to ensure that we can use same rig
   *       multiple times in same scene. */
  /* Armature. */
  build_armature(armature);
  /* Rebuild pose if not up to date. */
  if (object->pose == nullptr || (object->pose->flag & POSE_RECALC)) {
    /* By definition, no need to tag depsgraph as dirty from here, so we can pass nullptr bmain. */
    BKE_pose_rebuild(nullptr, object, armature, true);
  }
  /* Speed optimization for animation lookups. */
  if (object->pose != nullptr) {
    BKE_pose_channels_hash_ensure(object->pose);
    if (object->pose->flag & POSE_CONSTRAINTS_NEED_UPDATE_FLAGS) {
      BKE_pose_update_constraint_flags(object->pose);
    }
  }
  /**
   * Pose Rig Graph
   * ==============
   *
   * Pose Component:
   * - Mainly used for referencing Bone components.
   * - This is where the evaluation operations for init/exec/cleanup
   *   (ik) solvers live, and are later hooked up (so that they can be
   *   interleaved during runtime) with bone-operations they depend on/affect.
   * - init_pose_eval() and cleanup_pose_eval() are absolute first and last
   *   steps of pose eval process. ALL bone operations must be performed
   *   between these two...
   *
   * Bone Component:
   * - Used for representing each bone within the rig
   * - Acts to encapsulate the evaluation operations (base matrix + parenting,
   *   and constraint stack) so that they can be easily found.
   * - Everything else which depends on bone-results hook up to the component
   *   only so that we can redirect those to point at either the
   *   post-IK/post-constraint/post-matrix steps, as needed. */
  /* Pose eval context. */
  op_node = add_operation_node(&object->id,
                               NodeType::EVAL_POSE,
                               OperationCode::POSE_INIT,
                               [scene_cow, object_cow](::Depsgraph *depsgraph) {
                                 BKE_pose_eval_init(depsgraph, scene_cow, object_cow);
                               });
  op_node->set_as_entry();

  op_node = add_operation_node(&object->id,
                               NodeType::EVAL_POSE,
                               OperationCode::POSE_INIT_IK,
                               [scene_cow, object_cow](::Depsgraph *depsgraph) {
                                 BKE_pose_eval_init_ik(depsgraph, scene_cow, object_cow);
                               });

  add_operation_node(&object->id,
                     NodeType::EVAL_POSE,
                     OperationCode::POSE_CLEANUP,
                     [scene_cow, object_cow](::Depsgraph *depsgraph) {
                       BKE_pose_eval_cleanup(depsgraph, scene_cow, object_cow);
                     });

  op_node = add_operation_node(
      &object->id,
      NodeType::EVAL_POSE,
      OperationCode::POSE_DONE,
      [object_cow](::Depsgraph *depsgraph) { BKE_pose_eval_done(depsgraph, object_cow); });
  op_node->set_as_exit();
  /* Bones. */
  int pchan_index = 0;
  LISTBASE_FOREACH (bPoseChannel *, pchan, &object->pose->chanbase) {
    /* Node for bone evaluation. */
    op_node = add_operation_node(
        &object->id, NodeType::BONE, pchan->name, OperationCode::BONE_LOCAL);
    op_node->set_as_entry();

    add_operation_node(&object->id,
                       NodeType::BONE,
                       pchan->name,
                       OperationCode::BONE_POSE_PARENT,
                       [scene_cow, object_cow, pchan_index](::Depsgraph *depsgraph) {
                         BKE_pose_eval_bone(depsgraph, scene_cow, object_cow, pchan_index);
                       });

    /* NOTE: Dedicated noop for easier relationship construction. */
    add_operation_node(&object->id, NodeType::BONE, pchan->name, OperationCode::BONE_READY);

    op_node = add_operation_node(&object->id,
                                 NodeType::BONE,
                                 pchan->name,
                                 OperationCode::BONE_DONE,
                                 [object_cow, pchan_index](::Depsgraph *depsgraph) {
                                   BKE_pose_bone_done(depsgraph, object_cow, pchan_index);
                                 });

    /* B-Bone shape computation - the real last step if present. */
    if (check_pchan_has_bbone(object, pchan)) {
      op_node = add_operation_node(&object->id,
                                   NodeType::BONE,
                                   pchan->name,
                                   OperationCode::BONE_SEGMENTS,
                                   [object_cow, pchan_index](::Depsgraph *depsgraph) {
                                     BKE_pose_eval_bbone_segments(
                                         depsgraph, object_cow, pchan_index);
                                   });
    }

    op_node->set_as_exit();

    /* Custom properties. */
    if (pchan->prop != nullptr) {
      build_idproperties(pchan->prop);
      add_operation_node(
          &object->id, NodeType::PARAMETERS, OperationCode::PARAMETERS_EVAL, nullptr, pchan->name);
    }
    /* Build constraints. */
    if (pchan->constraints.first != nullptr) {
      build_pose_constraints(object, pchan, pchan_index);
    }
    /**
     * IK Solvers.
     *
     * - These require separate processing steps are pose-level
     *   to be executed between chains of bones (i.e. once the
     *   base transforms of a bunch of bones is done)
     *
     * Unsolved Issues:
     * - Care is needed to ensure that multi-headed trees work out the same
     *   as in ik-tree building
     * - Animated chain-lengths are a problem. */
    LISTBASE_FOREACH (bConstraint *, con, &pchan->constraints) {
      switch (con->type) {
        case CONSTRAINT_TYPE_KINEMATIC:
          build_ik_pose(object, pchan, con);
          break;

        case CONSTRAINT_TYPE_SPLINEIK:
          build_splineik_pose(object, pchan, con);
          break;

        default:
          break;
      }
    }
    /* Custom shape. */
    if (pchan->custom != nullptr) {
      /* NOTE: The relation builder will ensure visibility of the custom shape object. */
      build_object(-1, pchan->custom, DEG_ID_LINKED_INDIRECTLY, false);
    }
    pchan_index++;
  }
}

}  // namespace blender::deg
