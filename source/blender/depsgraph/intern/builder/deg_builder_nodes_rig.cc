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

/** \file blender/depsgraph/intern/builder/deg_builder_nodes_rig.cc
 *  \ingroup depsgraph
 *
 * Methods for constructing depsgraph's nodes
 */

#include "intern/builder/deg_builder_nodes.h"

#include <stdio.h>
#include <stdlib.h>

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_blenlib.h"
#include "BLI_string.h"

extern "C" {
#include "DNA_anim_types.h"
#include "DNA_armature_types.h"
#include "DNA_constraint_types.h"
#include "DNA_object_types.h"

#include "BKE_action.h"
#include "BKE_armature.h"
} /* extern "C" */

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"

#include "intern/builder/deg_builder.h"
#include "intern/nodes/deg_node.h"
#include "intern/nodes/deg_node_component.h"
#include "intern/nodes/deg_node_operation.h"
#include "intern/depsgraph_types.h"
#include "intern/depsgraph_intern.h"
#include "util/deg_util_foreach.h"

namespace DEG {

void DepsgraphNodeBuilder::build_pose_constraints(Object *object, bPoseChannel *pchan)
{
	/* create node for constraint stack */
	add_operation_node(&object->id, DEG_NODE_TYPE_BONE, pchan->name,
	                   function_bind(BKE_pose_constraints_evaluate, _1, scene_, object, pchan),
	                   DEG_OPCODE_BONE_CONSTRAINTS);
}

/* IK Solver Eval Steps */
void DepsgraphNodeBuilder::build_ik_pose(Object *object, bPoseChannel *pchan, bConstraint *con)
{
	bKinematicConstraint *data = (bKinematicConstraint *)con->data;

	/* Find the chain's root. */
	bPoseChannel *rootchan = BKE_armature_ik_solver_find_root(pchan, data);
	if (rootchan == NULL) {
		return;
	}

	if (has_operation_node(&object->id, DEG_NODE_TYPE_EVAL_POSE, rootchan->name,
	                       DEG_OPCODE_POSE_IK_SOLVER))
	{
		return;
	}

	/* Operation node for evaluating/running IK Solver. */
	add_operation_node(&object->id, DEG_NODE_TYPE_EVAL_POSE, rootchan->name,
	                   function_bind(BKE_pose_iktree_evaluate, _1, scene_, object, rootchan),
	                   DEG_OPCODE_POSE_IK_SOLVER);
}

/* Spline IK Eval Steps */
void DepsgraphNodeBuilder::build_splineik_pose(Object *object, bPoseChannel *pchan, bConstraint *con)
{
	bSplineIKConstraint *data = (bSplineIKConstraint *)con->data;

	/* Find the chain's root. */
	bPoseChannel *rootchan = BKE_armature_splineik_solver_find_root(pchan, data);

	/* Operation node for evaluating/running Spline IK Solver.
	 * Store the "root bone" of this chain in the solver, so it knows where to start.
	 */
	add_operation_node(&object->id, DEG_NODE_TYPE_EVAL_POSE, rootchan->name,
	                   function_bind(BKE_pose_splineik_evaluate, _1, scene_, object, rootchan),
	                   DEG_OPCODE_POSE_SPLINE_IK_SOLVER);
}

/* Pose/Armature Bones Graph */
void DepsgraphNodeBuilder::build_rig(Object *object)
{
	bArmature *arm = (bArmature *)object->data;
	OperationDepsNode *op_node;

	/* animation and/or drivers linking posebones to base-armature used to define them
	 * NOTE: AnimData here is really used to control animated deform properties,
	 *       which ideally should be able to be unique across different instances.
	 *       Eventually, we need some type of proxy/isolation mechanism in-between here
	 *       to ensure that we can use same rig multiple times in same scene...
	 */
	if ((arm->id.tag & LIB_TAG_DOIT) == 0) {
		build_animdata(&arm->id);

		/* Make sure pose is up-to-date with armature updates. */
		add_operation_node(&arm->id,
		                   DEG_NODE_TYPE_PARAMETERS,
		                   NULL,
		                   DEG_OPCODE_PLACEHOLDER,
		                   "Armature Eval");
	}

	/* Rebuild pose if not up to date. */
	if (object->pose == NULL || (object->pose->flag & POSE_RECALC)) {
		BKE_pose_rebuild_ex(object, arm, false);
		/* XXX: Without this animation gets lost in certain circumstances
		 * after loading file. Need to investigate further since it does
		 * not happen with simple scenes..
		 */
		if (object->adt) {
			object->adt->recalc |= ADT_RECALC_ANIM;
		}
	}

	/* speed optimization for animation lookups */
	if (object->pose) {
		BKE_pose_channels_hash_make(object->pose);
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
	 * - Everything else which depends on bone-results hook up to the component only
	 *   so that we can redirect those to point at either the the post-IK/
	 *   post-constraint/post-matrix steps, as needed.
	 */

	/* pose eval context */
	op_node = add_operation_node(&object->id,
	                             DEG_NODE_TYPE_EVAL_POSE,
	                             function_bind(BKE_pose_eval_init, _1, scene_, object, object->pose),
	                             DEG_OPCODE_POSE_INIT);
	op_node->set_as_entry();

	op_node = add_operation_node(&object->id,
	                             DEG_NODE_TYPE_EVAL_POSE,
	                             function_bind(BKE_pose_eval_init_ik, _1, scene_, object, object->pose),
	                             DEG_OPCODE_POSE_INIT_IK);

	op_node = add_operation_node(&object->id,
	                             DEG_NODE_TYPE_EVAL_POSE,
	                             function_bind(BKE_pose_eval_flush, _1, scene_, object, object->pose),
	                             DEG_OPCODE_POSE_DONE);
	op_node->set_as_exit();

	/* bones */
	LISTBASE_FOREACH (bPoseChannel *, pchan, &object->pose->chanbase) {
		/* Node for bone evaluation. */
		op_node = add_operation_node(&object->id, DEG_NODE_TYPE_BONE, pchan->name, NULL,
		                             DEG_OPCODE_BONE_LOCAL);
		op_node->set_as_entry();

		add_operation_node(&object->id, DEG_NODE_TYPE_BONE, pchan->name,
		                   function_bind(BKE_pose_eval_bone, _1, scene_, object, pchan),
		                   DEG_OPCODE_BONE_POSE_PARENT);

		add_operation_node(&object->id, DEG_NODE_TYPE_BONE, pchan->name,
		                   NULL, /* NOTE: dedicated noop for easier relationship construction */
		                   DEG_OPCODE_BONE_READY);

		op_node = add_operation_node(&object->id, DEG_NODE_TYPE_BONE, pchan->name,
		                             function_bind(BKE_pose_bone_done, _1, pchan),
		                             DEG_OPCODE_BONE_DONE);
		op_node->set_as_exit();
		/* Custom properties. */
		if (pchan->prop != NULL) {
			add_operation_node(&object->id,
			                   DEG_NODE_TYPE_PARAMETERS,
			                   NULL,
			                   DEG_OPCODE_PARAMETERS_EVAL,
			                   pchan->name);
		}
		/* Constraints. */
		if (pchan->constraints.first != NULL) {
			build_pose_constraints(object, pchan);
		}
		/**
		 * IK Solvers.
		 *
		 * - These require separate processing steps are pose-level
		 *   to be executed between chains of bones (i.e. once the
		 *   base transforms of a bunch of bones is done)
		 *
		 * Unsolved Issues:
		 * - Care is needed to ensure that multi-headed trees work out the same as in ik-tree building
		 * - Animated chain-lengths are a problem...
		 */
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
	}
}

void DepsgraphNodeBuilder::build_proxy_rig(Object *object)
{
	bArmature *arm = (bArmature *)object->data;
	OperationDepsNode *op_node;

	build_animdata(&arm->id);

	BLI_assert(object->pose != NULL);

	/* speed optimization for animation lookups */
	BKE_pose_channels_hash_make(object->pose);
	if (object->pose->flag & POSE_CONSTRAINTS_NEED_UPDATE_FLAGS) {
		BKE_pose_update_constraint_flags(object->pose);
	}

	op_node = add_operation_node(&object->id,
	                             DEG_NODE_TYPE_EVAL_POSE,
	                             function_bind(BKE_pose_eval_proxy_copy, _1, object),
	                             DEG_OPCODE_POSE_INIT);
	op_node->set_as_entry();


	LISTBASE_FOREACH (bPoseChannel *, pchan, &object->pose->chanbase) {
		op_node = add_operation_node(&object->id, DEG_NODE_TYPE_BONE, pchan->name,
		                             NULL, DEG_OPCODE_BONE_LOCAL);
		op_node->set_as_entry();

		add_operation_node(&object->id, DEG_NODE_TYPE_BONE, pchan->name,
		                   NULL, DEG_OPCODE_BONE_READY);

		op_node = add_operation_node(&object->id, DEG_NODE_TYPE_BONE, pchan->name,
		                             NULL, DEG_OPCODE_BONE_DONE);
		op_node->set_as_exit();

		/* Custom properties. */
		if (pchan->prop != NULL) {
			add_operation_node(&object->id,
			                   DEG_NODE_TYPE_PARAMETERS,
			                   NULL,
			                   DEG_OPCODE_PARAMETERS_EVAL,
			                   pchan->name);
		}
	}

	op_node = add_operation_node(&object->id, DEG_NODE_TYPE_EVAL_POSE,
	                             NULL, DEG_OPCODE_POSE_DONE);
	op_node->set_as_exit();
}

}  // namespace DEG
