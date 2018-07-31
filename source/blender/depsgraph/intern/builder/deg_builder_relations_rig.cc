/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 *
 * Original Author: Joshua Leung
 * Contributor(s): Based on original depsgraph.c code - Blender Foundation (2005-2013)
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/depsgraph/intern/builder/deg_builder_relations_rig.cc
 *  \ingroup depsgraph
 *
 * Methods for constructing depsgraph
 */

#include "intern/builder/deg_builder_relations.h"

#include <stdio.h>
#include <stdlib.h>
#include <cstring>  /* required for STREQ later on. */

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

#include "intern/nodes/deg_node.h"
#include "intern/nodes/deg_node_component.h"
#include "intern/nodes/deg_node_operation.h"

#include "intern/depsgraph_intern.h"
#include "intern/depsgraph_types.h"

#include "util/deg_util_foreach.h"

namespace DEG {

/* IK Solver Eval Steps */
void DepsgraphRelationBuilder::build_ik_pose(Object *object,
                                             bPoseChannel *pchan,
                                             bConstraint *con,
                                             RootPChanMap *root_map)
{
	bKinematicConstraint *data = (bKinematicConstraint *)con->data;

	/* attach owner to IK Solver too
	 * - assume that owner is always part of chain
	 * - see notes on direction of rel below...
	 */
	bPoseChannel *rootchan = BKE_armature_ik_solver_find_root(pchan, data);
	if (rootchan == NULL) {
		return;
	}
	OperationKey pchan_local_key(&object->id, DEG_NODE_TYPE_BONE,
	                             pchan->name, DEG_OPCODE_BONE_LOCAL);
	OperationKey init_ik_key(&object->id, DEG_NODE_TYPE_EVAL_POSE, DEG_OPCODE_POSE_INIT_IK);
	OperationKey solver_key(&object->id, DEG_NODE_TYPE_EVAL_POSE,
	                        rootchan->name,
	                        DEG_OPCODE_POSE_IK_SOLVER);

	add_relation(pchan_local_key, init_ik_key, "IK Constraint -> Init IK Tree");
	add_relation(init_ik_key, solver_key, "Init IK -> IK Solver");

	/* IK target */
	// XXX: this should get handled as part of the constraint code
	if (data->tar != NULL) {
		/* TODO(sergey): For until we'll store partial matricies in the depsgraph,
		 * we create dependency between target object and pose eval component.
		 *
		 * This way we ensuring the whole subtree is updated from scratch without
		 * need of intermediate matricies. This is an overkill, but good enough for
		 * testing IK solver.
		 */
		// FIXME: geometry targets...
		ComponentKey pose_key(&object->id, DEG_NODE_TYPE_EVAL_POSE);
		if ((data->tar->type == OB_ARMATURE) && (data->subtarget[0])) {
			/* TODO(sergey): This is only for until granular update stores intermediate result. */
			if (data->tar != object) {
				/* different armature - can just read the results */
				ComponentKey target_key(&data->tar->id, DEG_NODE_TYPE_BONE, data->subtarget);
				add_relation(target_key, pose_key, con->name);
			}
			else {
				/* same armature - we'll use the ready state only, just in case this bone is in the chain we're solving */
				OperationKey target_key(&data->tar->id, DEG_NODE_TYPE_BONE, data->subtarget, DEG_OPCODE_BONE_DONE);
				add_relation(target_key, solver_key, con->name);
			}
		}
		else if (ELEM(data->tar->type, OB_MESH, OB_LATTICE) && (data->subtarget[0])) {
			/* vertex group target */
			/* NOTE: for now, we don't need to represent vertex groups separately... */
			ComponentKey target_key(&data->tar->id, DEG_NODE_TYPE_GEOMETRY);
			add_relation(target_key, solver_key, con->name);

			if (data->tar->type == OB_MESH) {
				OperationDepsNode *node2 = find_operation_node(target_key);
				if (node2 != NULL) {
					node2->customdata_mask |= CD_MASK_MDEFORMVERT;
				}
			}
		}
		else {
			/* Standard Object Target */
			ComponentKey target_key(&data->tar->id, DEG_NODE_TYPE_TRANSFORM);
			add_relation(target_key, pose_key, con->name);
		}

		if ((data->tar == object) && (data->subtarget[0])) {
			/* Prevent target's constraints from linking to anything from same
			 * chain that it controls.
			 */
			root_map->add_bone(data->subtarget, rootchan->name);
		}
	}

	/* Pole Target */
	// XXX: this should get handled as part of the constraint code
	if (data->poletar != NULL) {
		if ((data->poletar->type == OB_ARMATURE) && (data->polesubtarget[0])) {
			// XXX: same armature issues - ready vs done?
			ComponentKey target_key(&data->poletar->id, DEG_NODE_TYPE_BONE, data->polesubtarget);
			add_relation(target_key, solver_key, con->name);
		}
		else if (ELEM(data->poletar->type, OB_MESH, OB_LATTICE) && (data->polesubtarget[0])) {
			/* vertex group target */
			/* NOTE: for now, we don't need to represent vertex groups separately... */
			ComponentKey target_key(&data->poletar->id, DEG_NODE_TYPE_GEOMETRY);
			add_relation(target_key, solver_key, con->name);

			if (data->poletar->type == OB_MESH) {
				OperationDepsNode *node2 = find_operation_node(target_key);
				if (node2 != NULL) {
					node2->customdata_mask |= CD_MASK_MDEFORMVERT;
				}
			}
		}
		else {
			ComponentKey target_key(&data->poletar->id, DEG_NODE_TYPE_TRANSFORM);
			add_relation(target_key, solver_key, con->name);
		}
	}

	DEG_DEBUG_PRINTF((::Depsgraph *)graph_,
	                 BUILD, "\nStarting IK Build: pchan = %s, target = (%s, %s), segcount = %d\n",
	                 pchan->name, data->tar->id.name, data->subtarget, data->rootbone);

	bPoseChannel *parchan = pchan;
	/* exclude tip from chain? */
	if (!(data->flag & CONSTRAINT_IK_TIP)) {
		parchan = pchan->parent;
	}

	root_map->add_bone(parchan->name, rootchan->name);

	OperationKey parchan_transforms_key(&object->id, DEG_NODE_TYPE_BONE,
	                                    parchan->name, DEG_OPCODE_BONE_READY);
	add_relation(parchan_transforms_key, solver_key, "IK Solver Owner");

	/* Walk to the chain's root */
	//size_t segcount = 0;
	int segcount = 0;

	while (parchan) {
		/* Make IK-solver dependent on this bone's result,
		 * since it can only run after the standard results
		 * of the bone are know. Validate links step on the
		 * bone will ensure that users of this bone only
		 * grab the result with IK solver results...
		 */
		if (parchan != pchan) {
			OperationKey parent_key(&object->id, DEG_NODE_TYPE_BONE, parchan->name, DEG_OPCODE_BONE_READY);
			add_relation(parent_key, solver_key, "IK Chain Parent");

			OperationKey done_key(&object->id, DEG_NODE_TYPE_BONE, parchan->name, DEG_OPCODE_BONE_DONE);
			add_relation(solver_key, done_key, "IK Chain Result");
		}
		else {
			OperationKey final_transforms_key(&object->id, DEG_NODE_TYPE_BONE, parchan->name, DEG_OPCODE_BONE_DONE);
			add_relation(solver_key, final_transforms_key, "IK Solver Result");
		}
		parchan->flag |= POSE_DONE;


		root_map->add_bone(parchan->name, rootchan->name);

		/* continue up chain, until we reach target number of items... */
		DEG_DEBUG_PRINTF((::Depsgraph *)graph_, BUILD, "  %d = %s\n", segcount, parchan->name);
		segcount++;
		if ((segcount == data->rootbone) || (segcount > 255)) break;  /* 255 is weak */

		parchan  = parchan->parent;
	}

	OperationKey flush_key(&object->id, DEG_NODE_TYPE_EVAL_POSE, DEG_OPCODE_POSE_DONE);
	add_relation(solver_key, flush_key, "PoseEval Result-Bone Link");
}

/* Spline IK Eval Steps */
void DepsgraphRelationBuilder::build_splineik_pose(Object *object,
                                                   bPoseChannel *pchan,
                                                   bConstraint *con,
                                                   RootPChanMap *root_map)
{
	bSplineIKConstraint *data = (bSplineIKConstraint *)con->data;
	bPoseChannel *rootchan = BKE_armature_splineik_solver_find_root(pchan, data);
	OperationKey transforms_key(&object->id, DEG_NODE_TYPE_BONE, pchan->name, DEG_OPCODE_BONE_READY);
	OperationKey solver_key(&object->id, DEG_NODE_TYPE_EVAL_POSE, rootchan->name, DEG_OPCODE_POSE_SPLINE_IK_SOLVER);

	/* attach owner to IK Solver too
	 * - assume that owner is always part of chain
	 * - see notes on direction of rel below...
	 */
	add_relation(transforms_key, solver_key, "Spline IK Solver Owner");

	/* attach path dependency to solver */
	if (data->tar) {
		/* TODO(sergey): For until we'll store partial matricies in the depsgraph,
		 * we create dependency between target object and pose eval component.
		 * See IK pose for a bit more information.
		 */
		// TODO: the bigggest point here is that we need the curve PATH and not just the general geometry...
		ComponentKey target_key(&data->tar->id, DEG_NODE_TYPE_GEOMETRY);
		ComponentKey pose_key(&object->id, DEG_NODE_TYPE_EVAL_POSE);
		add_relation(target_key, pose_key, "Curve.Path -> Spline IK");
	}

	pchan->flag |= POSE_DONE;
	OperationKey final_transforms_key(&object->id, DEG_NODE_TYPE_BONE, pchan->name, DEG_OPCODE_BONE_DONE);
	add_relation(solver_key, final_transforms_key, "Spline IK Result");

	root_map->add_bone(pchan->name, rootchan->name);

	/* Walk to the chain's root */
	//size_t segcount = 0;
	int segcount = 0;

	for (bPoseChannel *parchan = pchan->parent; parchan; parchan = parchan->parent) {
		/* Make Spline IK solver dependent on this bone's result,
		 * since it can only run after the standard results
		 * of the bone are know. Validate links step on the
		 * bone will ensure that users of this bone only
		 * grab the result with IK solver results...
		 */
		if (parchan != pchan) {
			OperationKey parent_key(&object->id, DEG_NODE_TYPE_BONE, parchan->name, DEG_OPCODE_BONE_READY);
			add_relation(parent_key, solver_key, "Spline IK Solver Update");

			OperationKey done_key(&object->id, DEG_NODE_TYPE_BONE, parchan->name, DEG_OPCODE_BONE_DONE);
			add_relation(solver_key, done_key, "IK Chain Result");
		}
		parchan->flag |= POSE_DONE;

		OperationKey final_transforms_key(&object->id, DEG_NODE_TYPE_BONE, parchan->name, DEG_OPCODE_BONE_DONE);
		add_relation(solver_key, final_transforms_key, "Spline IK Solver Result");

		root_map->add_bone(parchan->name, rootchan->name);

		/* continue up chain, until we reach target number of items... */
		segcount++;
		if ((segcount == data->chainlen) || (segcount > 255)) break;  /* 255 is weak */
	}

	OperationKey flush_key(&object->id, DEG_NODE_TYPE_EVAL_POSE, DEG_OPCODE_POSE_DONE);
	add_relation(solver_key, flush_key, "PoseEval Result-Bone Link");
}

/* Pose/Armature Bones Graph */
void DepsgraphRelationBuilder::build_rig(Object *object)
{
	/* Armature-Data */
	bArmature *armature = (bArmature *)object->data;
	// TODO: selection status?
	/* Attach links between pose operations. */
	ComponentKey local_transform(&object->id, DEG_NODE_TYPE_TRANSFORM);
	OperationKey init_key(&object->id, DEG_NODE_TYPE_EVAL_POSE, DEG_OPCODE_POSE_INIT);
	OperationKey init_ik_key(&object->id, DEG_NODE_TYPE_EVAL_POSE, DEG_OPCODE_POSE_INIT_IK);
	OperationKey flush_key(&object->id, DEG_NODE_TYPE_EVAL_POSE, DEG_OPCODE_POSE_DONE);
	add_relation(local_transform, init_key, "Local Transform -> Pose Init");
	add_relation(init_key, init_ik_key, "Pose Init -> Pose Init IK");
	add_relation(init_ik_key, flush_key, "Pose Init IK -> Pose Cleanup");
	/* Make sure pose is up-to-date with armature updates. */
	build_armature(armature);
	OperationKey armature_key(&armature->id,
	                          DEG_NODE_TYPE_PARAMETERS,
	                          DEG_OPCODE_PLACEHOLDER,
	                          "Armature Eval");
	add_relation(armature_key, init_key, "Data dependency");
	/* IK Solvers...
	 * - These require separate processing steps are pose-level
	 *   to be executed between chains of bones (i.e. once the
	 *   base transforms of a bunch of bones is done)
	 *
	 * - We build relations for these before the dependencies
	 *   between ops in the same component as it is necessary
	 *   to check whether such bones are in the same IK chain
	 *   (or else we get weird issues with either in-chain
	 *   references, or with bones being parented to IK'd bones)
	 *
	 * Unsolved Issues:
	 * - Care is needed to ensure that multi-headed trees work out the same as
	 *   in ik-tree building
	 * - Animated chain-lengths are a problem...
	 */
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
				 * TODO(sergey): More constraints here?
				 */
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
	//root_map.print_debug();
	if (pose_depends_on_local_transform) {
		/* TODO(sergey): Once partial updates are possible use relation between
		 * object transform and solver itself in it's build function.
		 */
		ComponentKey pose_key(&object->id, DEG_NODE_TYPE_EVAL_POSE);
		ComponentKey local_transform_key(&object->id, DEG_NODE_TYPE_TRANSFORM);
		add_relation(local_transform_key, pose_key, "Local Transforms");
	}
	/* Links between operations for each bone. */
	LISTBASE_FOREACH (bPoseChannel *, pchan, &object->pose->chanbase) {
		OperationKey bone_local_key(&object->id, DEG_NODE_TYPE_BONE, pchan->name, DEG_OPCODE_BONE_LOCAL);
		OperationKey bone_pose_key(&object->id, DEG_NODE_TYPE_BONE, pchan->name, DEG_OPCODE_BONE_POSE_PARENT);
		OperationKey bone_ready_key(&object->id, DEG_NODE_TYPE_BONE, pchan->name, DEG_OPCODE_BONE_READY);
		OperationKey bone_done_key(&object->id, DEG_NODE_TYPE_BONE, pchan->name, DEG_OPCODE_BONE_DONE);
		pchan->flag &= ~POSE_DONE;
		/* Pose init to bone local. */
		add_relation(init_key, bone_local_key, "PoseEval Source-Bone Link");
		/* Local to pose parenting operation. */
		add_relation(bone_local_key, bone_pose_key, "Bone Local - PoseSpace Link");
		/* Parent relation. */
		if (pchan->parent != NULL) {
			eDepsOperation_Code parent_key_opcode;

			/* NOTE: this difference in handling allows us to prevent lockups
			 * while ensuring correct poses for separate chains.
			 */
			if (root_map.has_common_root(pchan->name, pchan->parent->name)) {
				parent_key_opcode = DEG_OPCODE_BONE_READY;
			}
			else {
				parent_key_opcode = DEG_OPCODE_BONE_DONE;
			}

			OperationKey parent_key(&object->id, DEG_NODE_TYPE_BONE, pchan->parent->name, parent_key_opcode);
			add_relation(parent_key, bone_pose_key, "Parent Bone -> Child Bone");
		}
		/* Buil constraints. */
		if (pchan->constraints.first != NULL) {
			/* Build relations for indirectly linked objects. */
			BuilderWalkUserData data;
			data.builder = this;
			BKE_constraints_id_loop(&pchan->constraints, constraint_walk, &data);

			/* constraints stack and constraint dependencies */
			build_constraints(&object->id, DEG_NODE_TYPE_BONE, pchan->name, &pchan->constraints, &root_map);

			/* pose -> constraints */
			OperationKey constraints_key(&object->id, DEG_NODE_TYPE_BONE, pchan->name, DEG_OPCODE_BONE_CONSTRAINTS);
			add_relation(bone_pose_key, constraints_key, "Constraints Stack");

			/* constraints -> ready */
			// TODO: when constraint stack is exploded, this step should occur before the first IK solver
			add_relation(constraints_key, bone_ready_key, "Constraints -> Ready");
		}
		else {
			/* pose -> ready */
			add_relation(bone_pose_key, bone_ready_key, "Pose -> Ready");
		}

		/* bone ready -> done
		 * NOTE: For bones without IK, this is all that's needed.
		 *       For IK chains however, an additional rel is created from IK
		 *       to done, with transitive reduction removing this one..
		 */
		add_relation(bone_ready_key, bone_done_key, "Ready -> Done");

		/* assume that all bones must be done for the pose to be ready
		 * (for deformers)
		 */
		add_relation(bone_done_key, flush_key, "PoseEval Result-Bone Link");
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
	OperationKey pose_init_key(&object->id,
	                           DEG_NODE_TYPE_EVAL_POSE,
	                           DEG_OPCODE_POSE_INIT);
	OperationKey pose_done_key(&object->id,
	                           DEG_NODE_TYPE_EVAL_POSE,
	                           DEG_OPCODE_POSE_DONE);
	LISTBASE_FOREACH (bPoseChannel *, pchan, &object->pose->chanbase) {
		OperationKey bone_local_key(&object->id,
		                            DEG_NODE_TYPE_BONE, pchan->name,
		                            DEG_OPCODE_BONE_LOCAL);
		OperationKey bone_ready_key(&object->id,
		                            DEG_NODE_TYPE_BONE,
		                            pchan->name,
		                            DEG_OPCODE_BONE_READY);
		OperationKey bone_done_key(&object->id,
		                           DEG_NODE_TYPE_BONE,
		                           pchan->name,
		                           DEG_OPCODE_BONE_DONE);
		OperationKey from_bone_done_key(&proxy_from->id,
		                                DEG_NODE_TYPE_BONE,
		                                pchan->name,
		                                DEG_OPCODE_BONE_DONE);
		add_relation(pose_init_key, bone_local_key, "Pose Init -> Bone Local");
		add_relation(bone_local_key, bone_ready_key, "Local -> Ready");
		add_relation(bone_ready_key, bone_done_key, "Ready -> Done");
		add_relation(bone_done_key, pose_done_key, "Bone Done -> Pose Done");

		/* Make sure bone in the proxy is not done before it's FROM is done. */
		add_relation(from_bone_done_key,
		             bone_done_key,
		             "From Bone Done -> Pose Done");

		if (pchan->prop != NULL) {
			OperationKey bone_parameters(&object->id,
			                             DEG_NODE_TYPE_PARAMETERS,
			                             DEG_OPCODE_PARAMETERS_EVAL,
			                             pchan->name);
			OperationKey from_bone_parameters(&proxy_from->id,
			                                  DEG_NODE_TYPE_PARAMETERS,
			                                  DEG_OPCODE_PARAMETERS_EVAL,
			                                  pchan->name);
			add_relation(from_bone_parameters,
			             bone_parameters,
			             "Proxy Bone Parameters");
		}
	}
}

}  // namespace DEG
