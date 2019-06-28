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
 *
 * Methods for constructing depsgraph
 */

#include "intern/builder/deg_builder_relations.h"

#include <stdio.h>
#include <stdlib.h>
#include <cstring> /* required for STREQ later on. */

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_blenlib.h"

extern "C" {
#include "DNA_action_types.h"
#include "DNA_anim_types.h"
#include "DNA_armature_types.h"
#include "DNA_constraint_types.h"
#include "DNA_customdata_types.h"
#include "DNA_object_types.h"

#include "BKE_action.h"
#include "BKE_armature.h"
#include "BKE_constraint.h"
} /* extern "C" */

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"

#include "intern/builder/deg_builder.h"
#include "intern/builder/deg_builder_pchanmap.h"
#include "intern/debug/deg_debug.h"
#include "intern/node/deg_node.h"
#include "intern/node/deg_node_component.h"
#include "intern/node/deg_node_operation.h"

#include "intern/depsgraph_type.h"

namespace DEG {

/* IK Solver Eval Steps */
void DepsgraphRelationBuilder::build_ik_pose(Object *object,
                                             bPoseChannel *pchan,
                                             bConstraint *con,
                                             RootPChanMap *root_map)
{
  bKinematicConstraint *data = (bKinematicConstraint *)con->data;
  /* Attach owner to IK Solver to. */
  bPoseChannel *rootchan = BKE_armature_ik_solver_find_root(pchan, data);
  if (rootchan == NULL) {
    return;
  }
  OperationKey pchan_local_key(
      &object->id, NodeType::BONE, pchan->name, OperationCode::BONE_LOCAL);
  OperationKey init_ik_key(&object->id, NodeType::EVAL_POSE, OperationCode::POSE_INIT_IK);
  OperationKey solver_key(
      &object->id, NodeType::EVAL_POSE, rootchan->name, OperationCode::POSE_IK_SOLVER);
  OperationKey pose_cleanup_key(&object->id, NodeType::EVAL_POSE, OperationCode::POSE_CLEANUP);
  add_relation(pchan_local_key, init_ik_key, "IK Constraint -> Init IK Tree");
  add_relation(init_ik_key, solver_key, "Init IK -> IK Solver");
  /* Never cleanup before solver is run. */
  add_relation(solver_key, pose_cleanup_key, "IK Solver -> Cleanup", RELATION_FLAG_GODMODE);
  /* The ITASC solver currently accesses the target transforms in init tree :(
   * TODO: Fix ITASC and remove this.
   */
  bool is_itasc = (object->pose->iksolver == IKSOLVER_ITASC);
  OperationKey target_dependent_key = is_itasc ? init_ik_key : solver_key;
  /* IK target */
  /* TODO(sergey): This should get handled as part of the constraint code. */
  if (data->tar != NULL) {
    /* Different object - requires its transform. */
    if (data->tar != object) {
      ComponentKey target_key(&data->tar->id, NodeType::TRANSFORM);
      add_relation(target_key, target_dependent_key, con->name);
    }
    /* Subtarget references: */
    if ((data->tar->type == OB_ARMATURE) && (data->subtarget[0])) {
      /* Bone - use the final transformation. */
      OperationKey target_key(
          &data->tar->id, NodeType::BONE, data->subtarget, OperationCode::BONE_DONE);
      add_relation(target_key, target_dependent_key, con->name);
    }
    else if (data->subtarget[0] && ELEM(data->tar->type, OB_MESH, OB_LATTICE)) {
      /* Vertex group target. */
      /* NOTE: for now, we don't need to represent vertex groups
       * separately. */
      ComponentKey target_key(&data->tar->id, NodeType::GEOMETRY);
      add_relation(target_key, target_dependent_key, con->name);
      add_customdata_mask(data->tar, DEGCustomDataMeshMasks::MaskVert(CD_MASK_MDEFORMVERT));
    }
    if (data->tar == object && data->subtarget[0]) {
      /* Prevent target's constraints from linking to anything from same
       * chain that it controls. */
      root_map->add_bone(data->subtarget, rootchan->name);
    }
  }
  /* Pole Target. */
  /* TODO(sergey): This should get handled as part of the constraint code. */
  if (data->poletar != NULL) {
    /* Different object - requires its transform. */
    if (data->poletar != object) {
      ComponentKey target_key(&data->poletar->id, NodeType::TRANSFORM);
      add_relation(target_key, target_dependent_key, con->name);
    }
    /* Subtarget references: */
    if ((data->poletar->type == OB_ARMATURE) && (data->polesubtarget[0])) {
      /* Bone - use the final transformation. */
      OperationKey target_key(
          &data->poletar->id, NodeType::BONE, data->polesubtarget, OperationCode::BONE_DONE);
      add_relation(target_key, target_dependent_key, con->name);
    }
    else if (data->polesubtarget[0] && ELEM(data->poletar->type, OB_MESH, OB_LATTICE)) {
      /* Vertex group target. */
      /* NOTE: for now, we don't need to represent vertex groups
       * separately. */
      ComponentKey target_key(&data->poletar->id, NodeType::GEOMETRY);
      add_relation(target_key, target_dependent_key, con->name);
      add_customdata_mask(data->poletar, DEGCustomDataMeshMasks::MaskVert(CD_MASK_MDEFORMVERT));
    }
  }
  DEG_DEBUG_PRINTF((::Depsgraph *)graph_,
                   BUILD,
                   "\nStarting IK Build: pchan = %s, target = (%s, %s), "
                   "segcount = %d\n",
                   pchan->name,
                   data->tar ? data->tar->id.name : "NULL",
                   data->subtarget,
                   data->rootbone);
  bPoseChannel *parchan = pchan;
  /* Exclude tip from chain if needed. */
  if (!(data->flag & CONSTRAINT_IK_TIP)) {
    parchan = pchan->parent;
  }
  root_map->add_bone(parchan->name, rootchan->name);
  OperationKey parchan_transforms_key(
      &object->id, NodeType::BONE, parchan->name, OperationCode::BONE_READY);
  add_relation(parchan_transforms_key, solver_key, "IK Solver Owner");
  /* Walk to the chain's root. */
  int segcount = 0;
  while (parchan != NULL) {
    /* Make IK-solver dependent on this bone's result, since it can only run
     * after the standard results of the bone are know. Validate links step
     * on the bone will ensure that users of this bone only grab the result
     * with IK solver results. */
    if (parchan != pchan) {
      OperationKey parent_key(
          &object->id, NodeType::BONE, parchan->name, OperationCode::BONE_READY);
      add_relation(parent_key, solver_key, "IK Chain Parent");
      OperationKey bone_done_key(
          &object->id, NodeType::BONE, parchan->name, OperationCode::BONE_DONE);
      add_relation(solver_key, bone_done_key, "IK Chain Result");
    }
    else {
      OperationKey final_transforms_key(
          &object->id, NodeType::BONE, parchan->name, OperationCode::BONE_DONE);
      add_relation(solver_key, final_transforms_key, "IK Solver Result");
    }
    parchan->flag |= POSE_DONE;
    root_map->add_bone(parchan->name, rootchan->name);
    /* continue up chain, until we reach target number of items. */
    DEG_DEBUG_PRINTF((::Depsgraph *)graph_, BUILD, "  %d = %s\n", segcount, parchan->name);
    /* TODO(sergey): This is an arbitrary value, which was just following
     * old code convention. */
    segcount++;
    if ((segcount == data->rootbone) || (segcount > 255)) {
      break;
    }
    parchan = parchan->parent;
  }
  OperationKey pose_done_key(&object->id, NodeType::EVAL_POSE, OperationCode::POSE_DONE);
  add_relation(solver_key, pose_done_key, "PoseEval Result-Bone Link");
}

/* Spline IK Eval Steps */
void DepsgraphRelationBuilder::build_splineik_pose(Object *object,
                                                   bPoseChannel *pchan,
                                                   bConstraint *con,
                                                   RootPChanMap *root_map)
{
  bSplineIKConstraint *data = (bSplineIKConstraint *)con->data;
  bPoseChannel *rootchan = BKE_armature_splineik_solver_find_root(pchan, data);
  OperationKey transforms_key(&object->id, NodeType::BONE, pchan->name, OperationCode::BONE_READY);
  OperationKey init_ik_key(&object->id, NodeType::EVAL_POSE, OperationCode::POSE_INIT_IK);
  OperationKey solver_key(
      &object->id, NodeType::EVAL_POSE, rootchan->name, OperationCode::POSE_SPLINE_IK_SOLVER);
  OperationKey pose_cleanup_key(&object->id, NodeType::EVAL_POSE, OperationCode::POSE_CLEANUP);
  /* Solver depends on initialization. */
  add_relation(init_ik_key, solver_key, "Init IK -> IK Solver");
  /* Never cleanup before solver is run. */
  add_relation(solver_key, pose_cleanup_key, "IK Solver -> Cleanup");
  /* Attach owner to IK Solver. */
  add_relation(transforms_key, solver_key, "Spline IK Solver Owner", RELATION_FLAG_GODMODE);
  /* Attach path dependency to solver. */
  if (data->tar != NULL) {
    ComponentKey target_geometry_key(&data->tar->id, NodeType::GEOMETRY);
    add_relation(target_geometry_key, solver_key, "Curve.Path -> Spline IK");
    ComponentKey target_transform_key(&data->tar->id, NodeType::TRANSFORM);
    add_relation(target_transform_key, solver_key, "Curve.Transform -> Spline IK");
    add_special_eval_flag(&data->tar->id, DAG_EVAL_NEED_CURVE_PATH);
  }
  pchan->flag |= POSE_DONE;
  OperationKey final_transforms_key(
      &object->id, NodeType::BONE, pchan->name, OperationCode::BONE_DONE);
  add_relation(solver_key, final_transforms_key, "Spline IK Result");
  root_map->add_bone(pchan->name, rootchan->name);
  /* Walk to the chain's root/ */
  int segcount = 1;
  for (bPoseChannel *parchan = pchan->parent; parchan != NULL && segcount < data->chainlen;
       parchan = parchan->parent, segcount++) {
    /* Make Spline IK solver dependent on this bone's result, since it can
     * only run after the standard results of the bone are know. Validate
     * links step on the bone will ensure that users of this bone only grab
     * the result with IK solver results. */
    OperationKey parent_key(&object->id, NodeType::BONE, parchan->name, OperationCode::BONE_READY);
    add_relation(parent_key, solver_key, "Spline IK Solver Update");
    OperationKey bone_done_key(
        &object->id, NodeType::BONE, parchan->name, OperationCode::BONE_DONE);
    add_relation(solver_key, bone_done_key, "Spline IK Solver Result");
    parchan->flag |= POSE_DONE;
    root_map->add_bone(parchan->name, rootchan->name);
  }
  OperationKey pose_done_key(&object->id, NodeType::EVAL_POSE, OperationCode::POSE_DONE);
  add_relation(solver_key, pose_done_key, "PoseEval Result-Bone Link");
}

/* Pose/Armature Bones Graph */
void DepsgraphRelationBuilder::build_rig(Object *object)
{
  /* Armature-Data */
  bArmature *armature = (bArmature *)object->data;
  // TODO: selection status?
  /* Attach links between pose operations. */
  ComponentKey local_transform(&object->id, NodeType::TRANSFORM);
  OperationKey pose_init_key(&object->id, NodeType::EVAL_POSE, OperationCode::POSE_INIT);
  OperationKey pose_init_ik_key(&object->id, NodeType::EVAL_POSE, OperationCode::POSE_INIT_IK);
  OperationKey pose_cleanup_key(&object->id, NodeType::EVAL_POSE, OperationCode::POSE_CLEANUP);
  OperationKey pose_done_key(&object->id, NodeType::EVAL_POSE, OperationCode::POSE_DONE);
  add_relation(local_transform, pose_init_key, "Local Transform -> Pose Init");
  add_relation(pose_init_key, pose_init_ik_key, "Pose Init -> Pose Init IK");
  add_relation(pose_init_ik_key, pose_done_key, "Pose Init IK -> Pose Cleanup");
  /* Make sure pose is up-to-date with armature updates. */
  build_armature(armature);
  OperationKey armature_key(&armature->id, NodeType::ARMATURE, OperationCode::ARMATURE_EVAL);
  add_relation(armature_key, pose_init_key, "Data dependency");
  /* Run cleanup even when there are no bones. */
  add_relation(pose_init_key, pose_cleanup_key, "Init -> Cleanup");
  /* IK Solvers.
   *
   * - These require separate processing steps are pose-level to be executed
   *   between chains of bones (i.e. once the base transforms of a bunch of
   *   bones is done).
   *
   * - We build relations for these before the dependencies between operations
   *   in the same component as it is necessary to check whether such bones
   *   are in the same IK chain (or else we get weird issues with either
   *   in-chain references, or with bones being parented to IK'd bones).
   *
   * Unsolved Issues:
   * - Care is needed to ensure that multi-headed trees work out the same as
   *   in ik-tree building
   * - Animated chain-lengths are a problem. */
  RootPChanMap root_map;
  bool pose_depends_on_local_transform = false;
  LISTBASE_FOREACH (bPoseChannel *, pchan, &object->pose->chanbase) {
    LISTBASE_FOREACH (bConstraint *, con, &pchan->constraints) {
      switch (con->type) {
        case CONSTRAINT_TYPE_KINEMATIC:
          build_ik_pose(object, pchan, con, &root_map);
          pose_depends_on_local_transform = true;
          break;
        case CONSTRAINT_TYPE_SPLINEIK:
          build_splineik_pose(object, pchan, con, &root_map);
          pose_depends_on_local_transform = true;
          break;
        /* Constraints which needs world's matrix for transform.
         * TODO(sergey): More constraints here? */
        case CONSTRAINT_TYPE_ROTLIKE:
        case CONSTRAINT_TYPE_SIZELIKE:
        case CONSTRAINT_TYPE_LOCLIKE:
        case CONSTRAINT_TYPE_TRANSLIKE:
          /* TODO(sergey): Add used space check. */
          pose_depends_on_local_transform = true;
          break;
        default:
          break;
      }
    }
  }
  // root_map.print_debug();
  if (pose_depends_on_local_transform) {
    /* TODO(sergey): Once partial updates are possible use relation between
     * object transform and solver itself in it's build function. */
    ComponentKey pose_key(&object->id, NodeType::EVAL_POSE);
    ComponentKey local_transform_key(&object->id, NodeType::TRANSFORM);
    add_relation(local_transform_key, pose_key, "Local Transforms");
  }
  /* Links between operations for each bone. */
  LISTBASE_FOREACH (bPoseChannel *, pchan, &object->pose->chanbase) {
    OperationKey bone_local_key(
        &object->id, NodeType::BONE, pchan->name, OperationCode::BONE_LOCAL);
    OperationKey bone_pose_key(
        &object->id, NodeType::BONE, pchan->name, OperationCode::BONE_POSE_PARENT);
    OperationKey bone_ready_key(
        &object->id, NodeType::BONE, pchan->name, OperationCode::BONE_READY);
    OperationKey bone_done_key(&object->id, NodeType::BONE, pchan->name, OperationCode::BONE_DONE);
    pchan->flag &= ~POSE_DONE;
    /* Pose init to bone local. */
    add_relation(pose_init_key, bone_local_key, "Pose Init - Bone Local", RELATION_FLAG_GODMODE);
    /* Local to pose parenting operation. */
    add_relation(bone_local_key, bone_pose_key, "Bone Local - Bone Pose");
    /* Parent relation. */
    if (pchan->parent != NULL) {
      OperationCode parent_key_opcode;
      /* NOTE: this difference in handling allows us to prevent lockups
       * while ensuring correct poses for separate chains. */
      if (root_map.has_common_root(pchan->name, pchan->parent->name)) {
        parent_key_opcode = OperationCode::BONE_READY;
      }
      else {
        parent_key_opcode = OperationCode::BONE_DONE;
      }

      OperationKey parent_key(&object->id, NodeType::BONE, pchan->parent->name, parent_key_opcode);
      add_relation(parent_key, bone_pose_key, "Parent Bone -> Child Bone");
    }
    /* Build constraints. */
    if (pchan->constraints.first != NULL) {
      /* Build relations for indirectly linked objects. */
      BuilderWalkUserData data;
      data.builder = this;
      BKE_constraints_id_loop(&pchan->constraints, constraint_walk, &data);
      /* Constraints stack and constraint dependencies. */
      build_constraints(&object->id, NodeType::BONE, pchan->name, &pchan->constraints, &root_map);
      /* Pose -> constraints. */
      OperationKey constraints_key(
          &object->id, NodeType::BONE, pchan->name, OperationCode::BONE_CONSTRAINTS);
      add_relation(bone_pose_key, constraints_key, "Pose -> Constraints Stack");
      add_relation(bone_local_key, constraints_key, "Local -> Constraints Stack");
      /* Constraints -> ready/ */
      /* TODO(sergey): When constraint stack is exploded, this step should
       * occur before the first IK solver.  */
      add_relation(constraints_key, bone_ready_key, "Constraints -> Ready");
    }
    else {
      /* Pose -> Ready */
      add_relation(bone_pose_key, bone_ready_key, "Pose -> Ready");
    }
    /* Bone ready -> Bone done.
     * NOTE: For bones without IK, this is all that's needed.
     *       For IK chains however, an additional rel is created from IK
     *       to done, with transitive reduction removing this one. */
    add_relation(bone_ready_key, bone_done_key, "Ready -> Done");
    /* B-Bone shape is the real final step after Done if present. */
    if (check_pchan_has_bbone(object, pchan)) {
      OperationKey bone_segments_key(
          &object->id, NodeType::BONE, pchan->name, OperationCode::BONE_SEGMENTS);
      /* B-Bone shape depends on the final position of the bone. */
      add_relation(bone_done_key, bone_segments_key, "Done -> B-Bone Segments");
      /* B-Bone shape depends on final position of handle bones. */
      bPoseChannel *prev, *next;
      BKE_pchan_bbone_handles_get(pchan, &prev, &next);
      if (prev) {
        OperationCode opcode = OperationCode::BONE_DONE;
        /* Inheriting parent roll requires access to prev handle's B-Bone properties. */
        if ((pchan->bone->flag & BONE_ADD_PARENT_END_ROLL) != 0 &&
            check_pchan_has_bbone_segments(object, prev)) {
          opcode = OperationCode::BONE_SEGMENTS;
        }
        OperationKey prev_key(&object->id, NodeType::BONE, prev->name, opcode);
        add_relation(prev_key, bone_segments_key, "Prev Handle -> B-Bone Segments");
      }
      if (next) {
        OperationKey next_key(&object->id, NodeType::BONE, next->name, OperationCode::BONE_DONE);
        add_relation(next_key, bone_segments_key, "Next Handle -> B-Bone Segments");
      }
      /* Pose requires the B-Bone shape. */
      add_relation(
          bone_segments_key, pose_done_key, "PoseEval Result-Bone Link", RELATION_FLAG_GODMODE);
      add_relation(bone_segments_key, pose_cleanup_key, "Cleanup dependency");
    }
    else {
      /* Assume that all bones must be done for the pose to be ready
       * (for deformers). */
      add_relation(bone_done_key, pose_done_key, "PoseEval Result-Bone Link");

      /* Bones must be traversed before cleanup. */
      add_relation(bone_done_key, pose_cleanup_key, "Done -> Cleanup");

      add_relation(bone_ready_key, pose_cleanup_key, "Ready -> Cleanup");
    }
    /* Custom shape. */
    if (pchan->custom != NULL) {
      build_object(NULL, pchan->custom);
    }
  }
}

void DepsgraphRelationBuilder::build_proxy_rig(Object *object)
{
  bArmature *armature = (bArmature *)object->data;
  Object *proxy_from = object->proxy_from;
  build_armature(armature);
  OperationKey pose_init_key(&object->id, NodeType::EVAL_POSE, OperationCode::POSE_INIT);
  OperationKey pose_done_key(&object->id, NodeType::EVAL_POSE, OperationCode::POSE_DONE);
  OperationKey pose_cleanup_key(&object->id, NodeType::EVAL_POSE, OperationCode::POSE_CLEANUP);
  LISTBASE_FOREACH (bPoseChannel *, pchan, &object->pose->chanbase) {
    OperationKey bone_local_key(
        &object->id, NodeType::BONE, pchan->name, OperationCode::BONE_LOCAL);
    OperationKey bone_ready_key(
        &object->id, NodeType::BONE, pchan->name, OperationCode::BONE_READY);
    OperationKey bone_done_key(&object->id, NodeType::BONE, pchan->name, OperationCode::BONE_DONE);
    OperationKey from_bone_done_key(
        &proxy_from->id, NodeType::BONE, pchan->name, OperationCode::BONE_DONE);
    add_relation(pose_init_key, bone_local_key, "Pose Init -> Bone Local");
    add_relation(bone_local_key, bone_ready_key, "Local -> Ready");
    add_relation(bone_ready_key, bone_done_key, "Ready -> Done");
    add_relation(bone_done_key, pose_cleanup_key, "Bone Done -> Pose Cleanup");
    add_relation(bone_done_key, pose_done_key, "Bone Done -> Pose Done", RELATION_FLAG_GODMODE);
    /* Make sure bone in the proxy is not done before it's FROM is done. */
    if (check_pchan_has_bbone(object, pchan)) {
      OperationKey from_bone_segments_key(
          &proxy_from->id, NodeType::BONE, pchan->name, OperationCode::BONE_SEGMENTS);
      add_relation(from_bone_segments_key,
                   bone_done_key,
                   "Bone Segments -> Bone Done",
                   RELATION_FLAG_GODMODE);
    }
    else {
      add_relation(from_bone_done_key, bone_done_key, "Bone Done -> Bone Done");
    }

    /* Parent relation: even though the proxy bone itself doesn't need
     * the parent bone, some users expect the parent to be ready if the
     * bone itself is (e.g. for computing the local space matrix).
     */
    if (pchan->parent != NULL) {
      OperationKey parent_key(
          &object->id, NodeType::BONE, pchan->parent->name, OperationCode::BONE_DONE);
      add_relation(parent_key, bone_done_key, "Parent Bone -> Child Bone");
    }

    if (pchan->prop != NULL) {
      OperationKey bone_parameters(
          &object->id, NodeType::PARAMETERS, OperationCode::PARAMETERS_EVAL, pchan->name);
      OperationKey from_bone_parameters(
          &proxy_from->id, NodeType::PARAMETERS, OperationCode::PARAMETERS_EVAL, pchan->name);
      add_relation(from_bone_parameters, bone_parameters, "Proxy Bone Parameters");
    }
  }
}

}  // namespace DEG
