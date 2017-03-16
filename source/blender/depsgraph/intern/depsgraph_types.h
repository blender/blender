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
 * Contributor(s): None Yet
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/depsgraph/intern/depsgraph_types.h
 *  \ingroup depsgraph
 *
 * Datatypes for internal use in the Depsgraph
 *
 * All of these datatypes are only really used within the "core" depsgraph.
 * In particular, node types declared here form the structure of operations
 * in the graph.
 */

#pragma once

#include "util/deg_util_function.h"

/* TODO(sergey): Ideally we'll just use char* and statically allocated strings
 * to avoid any possible overhead caused by string (re)allocation/formatting.
 */
#include <string>
#include <vector>

struct bAction;
struct ChannelDriver;
struct ModifierData;
struct PointerRNA;
struct EvaluationContext;
struct FCurve;

namespace DEG {

using std::string;
using std::vector;

/* Evaluation Operation for atomic operation */
// XXX: move this to another header that can be exposed?
typedef function<void(struct EvaluationContext *)> DepsEvalOperationCb;

/* Metatype of Nodes - The general "level" in the graph structure
 * the node serves.
 */
typedef enum eDepsNode_Class {
	/* Types generally unassociated with user-visible entities,
	 * but needed for graph functioning.
	 */
	DEPSNODE_CLASS_GENERIC         = 0,
	/* [Outer Node] An "aspect" of evaluating/updating an ID-Block, requiring
	 * certain types of evaluation behavior.
	 */
	DEPSNODE_CLASS_COMPONENT       = 1,
	/* [Inner Node] A glorified function-pointer/callback for scheduling up
	 * evaluation operations for components, subject to relationship
	 * requirements.
	 */
	DEPSNODE_CLASS_OPERATION       = 2,
} eDepsNode_Class;

/* Types of Nodes */
typedef enum eDepsNode_Type {
	/* Fallback type for invalid return value */
	DEPSNODE_TYPE_UNDEFINED        = -1,
	/* Inner Node (Operation) */
	DEPSNODE_TYPE_OPERATION        = 0,

	/* **** Generic Types **** */

	/* "Current Scene" - basically whatever kicks off the evaluation process. */
	DEPSNODE_TYPE_ROOT,
	/* Time-Source */
	DEPSNODE_TYPE_TIMESOURCE,
	/* ID-Block reference - used as landmarks/collection point for components,
	 * but not usually part of main graph.
	 */
	DEPSNODE_TYPE_ID_REF,
	/* Isolated sub-graph - used for keeping instanced data separate from
	 * instances using them.
	 */
	DEPSNODE_TYPE_SUBGRAPH,

	/* **** Outer Types **** */

	/* Parameters Component - Default when nothing else fits
	 * (i.e. just SDNA property setting).
	 */
	DEPSNODE_TYPE_PARAMETERS,
	/* Generic "Proxy-Inherit" Component
	 * XXX: Also for instancing of subgraphs?
	 */
	DEPSNODE_TYPE_PROXY,
	/* Animation Component
	 *
	 * XXX: merge in with parameters?
	 */
	DEPSNODE_TYPE_ANIMATION,
	/* Transform Component (Parenting/Constraints) */
	DEPSNODE_TYPE_TRANSFORM,
	/* Geometry Component (DerivedMesh/Displist) */
	DEPSNODE_TYPE_GEOMETRY,
	/* Sequencer Component (Scene Only) */
	DEPSNODE_TYPE_SEQUENCER,

	/* **** Evaluation-Related Outer Types (with Subdata) **** */

	/* Pose Component - Owner/Container of Bones Eval */
	DEPSNODE_TYPE_EVAL_POSE,
	/* Bone Component - Child/Subcomponent of Pose */
	DEPSNODE_TYPE_BONE,
	/* Particle Systems Component */
	DEPSNODE_TYPE_EVAL_PARTICLES,
	/* Material Shading Component */
	DEPSNODE_TYPE_SHADING,
	/* Cache Component */
	DEPSNODE_TYPE_CACHE,
} eDepsNode_Type;

/* Identifiers for common operations (as an enum). */
typedef enum eDepsOperation_Code {
	/* Generic Operations ------------------------------ */

	/* Placeholder for operations which don't need special mention */
	DEG_OPCODE_OPERATION = 0,

	// XXX: Placeholder while porting depsgraph code
	DEG_OPCODE_PLACEHOLDER,

	DEG_OPCODE_NOOP,

	/* Animation, Drivers, etc. ------------------------ */

	/* NLA + Action */
	DEG_OPCODE_ANIMATION,

	/* Driver */
	DEG_OPCODE_DRIVER,

	/* Proxy Inherit? */
	//DEG_OPCODE_PROXY,

	/* Transform --------------------------------------- */

	/* Transform entry point - local transforms only */
	DEG_OPCODE_TRANSFORM_LOCAL,

	/* Parenting */
	DEG_OPCODE_TRANSFORM_PARENT,

	/* Constraints */
	DEG_OPCODE_TRANSFORM_CONSTRAINTS,
	//DEG_OPCODE_TRANSFORM_CONSTRAINTS_INIT,
	//DEG_OPCODE_TRANSFORM_CONSTRAINT,
	//DEG_OPCODE_TRANSFORM_CONSTRAINTS_DONE,

	/* Rigidbody Sim - Perform Sim */
	DEG_OPCODE_RIGIDBODY_REBUILD,
	DEG_OPCODE_RIGIDBODY_SIM,

	/* Rigidbody Sim - Copy Results to Object */
	DEG_OPCODE_TRANSFORM_RIGIDBODY,

	/* Transform exitpoint */
	DEG_OPCODE_TRANSFORM_FINAL,

	/* XXX: ubereval is for temporary porting purposes only */
	DEG_OPCODE_OBJECT_UBEREVAL,

	/* Geometry ---------------------------------------- */

	/* XXX: Placeholder - UberEval */
	DEG_OPCODE_GEOMETRY_UBEREVAL,

	/* Modifier */
	DEG_OPCODE_GEOMETRY_MODIFIER,

	/* Curve Objects - Path Calculation (used for path-following tools, */
	DEG_OPCODE_GEOMETRY_PATH,

	/* Pose -------------------------------------------- */

	/* Init IK Trees, etc. */
	DEG_OPCODE_POSE_INIT,

	/* Free IK Trees + Compute Deform Matrices */
	DEG_OPCODE_POSE_DONE,

	/* IK/Spline Solvers */
	DEG_OPCODE_POSE_IK_SOLVER,
	DEG_OPCODE_POSE_SPLINE_IK_SOLVER,

	/* Bone -------------------------------------------- */

	/* Bone local transforms - Entrypoint */
	DEG_OPCODE_BONE_LOCAL,

	/* Pose-space conversion (includes parent + restpose, */
	DEG_OPCODE_BONE_POSE_PARENT,

	/* Constraints */
	DEG_OPCODE_BONE_CONSTRAINTS,
	//DEG_OPCODE_BONE_CONSTRAINTS_INIT,
	//DEG_OPCODE_BONE_CONSTRAINT,
	//DEG_OPCODE_BONE_CONSTRAINTS_DONE,

	/* Bone transforms are ready
	 *
	 * - "READY"  This (internal, noop is used to signal that all pre-IK
	 *            operations are done. Its role is to help mediate situations
	 *            where cyclic relations may otherwise form (i.e. one bone in
	 *            chain targetting another in same chain,
	 *
	 * - "DONE"   This noop is used to signal that the bone's final pose
	 *            transform can be read by others
	 */
	// TODO: deform mats could get calculated in the final_transform ops...
	DEG_OPCODE_BONE_READY,
	DEG_OPCODE_BONE_DONE,

	/* Particles --------------------------------------- */

	/* XXX: placeholder - Particle System eval */
	DEG_OPCODE_PSYS_EVAL,

	DEG_NUM_OPCODES,
} eDepsOperation_Code;

/* Some magic to stringify operation codes. */
class DepsOperationStringifier {
public:
	DepsOperationStringifier();
	const char *operator[](eDepsOperation_Code opcodex);
protected:
	const char *names_[DEG_NUM_OPCODES];
};

/* String defines for these opcodes, defined in depsgraph_type_defines.cpp */
extern DepsOperationStringifier DEG_OPNAMES;

/* Type of operation */
typedef enum eDepsOperation_Type {
	/* **** Primary operation types **** */

	/* Initialise evaluation data */
	DEPSOP_TYPE_INIT    = 0,
	/* Standard evaluation step */
	DEPSOP_TYPE_EXEC    = 1,
	/* Cleanup evaluation data + flush results */
	DEPSOP_TYPE_POST    = 2,

	/* **** Additional operation types **** */
	/* Indicator for outputting a temporary result that other components
	 * can use. // XXX?
	 */
	DEPSOP_TYPE_OUT     = 3,
	/* Indicator for things like IK Solvers and Rigidbody Sim steps which
	 * modify final results of separate entities at once.
	 */
	DEPSOP_TYPE_SIM     = 4,
	/* Rebuild internal evaluation data - used for Rigidbody Reset and
	 * Armature Rebuild-On-Load.
	 */
	DEPSOP_TYPE_REBUILD = 5,
} eDepsOperation_Type;

/* Types of relationships between nodes
 *
 * This is used to provide additional hints to use when filtering
 * the graph, so that we can go without doing more extensive
 * data-level checks...
 */
typedef enum eDepsRelation_Type {
	/* relationship type unknown/irrelevant */
	DEPSREL_TYPE_STANDARD = 0,

	/* root -> active scene or entity (screen, image, etc.) */
	DEPSREL_TYPE_ROOT_TO_ACTIVE,

	/* general datablock dependency */
	DEPSREL_TYPE_DATABLOCK,

	/* time dependency */
	DEPSREL_TYPE_TIME,

	/* component depends on results of another */
	DEPSREL_TYPE_COMPONENT_ORDER,

	/* relationship is just used to enforce ordering of operations
	 * (e.g. "init()" callback done before "exec() and "cleanup()")
	 */
	DEPSREL_TYPE_OPERATION,

	/* relationship results from a property driver affecting property */
	DEPSREL_TYPE_DRIVER,

	/* relationship is something driver depends on */
	DEPSREL_TYPE_DRIVER_TARGET,

	/* relationship is used for transform stack
	 * (e.g. parenting, user transforms, constraints)
	 */
	DEPSREL_TYPE_TRANSFORM,

	/* relationship is used for geometry evaluation
	 * (e.g. metaball "motherball" or modifiers)
	 */
	DEPSREL_TYPE_GEOMETRY_EVAL,

	/* relationship is used to trigger a post-change validity updates */
	DEPSREL_TYPE_UPDATE,

	/* relationship is used to trigger editor/screen updates */
	DEPSREL_TYPE_UPDATE_UI,

	/* cache dependency */
	DEPSREL_TYPE_CACHE,
} eDepsRelation_Type;

}  // namespace DEG
