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
#include <algorithm>

struct bAction;
struct ChannelDriver;
struct ModifierData;
struct PointerRNA;
struct FCurve;
struct Depsgraph;

namespace DEG {

/* TODO(sergey): Find a better place for this. */
using std::string;
using std::vector;
using std::max;

/* Evaluation Operation for atomic operation */
// XXX: move this to another header that can be exposed?
typedef function<void(struct ::Depsgraph *)> DepsEvalOperationCb;

/* Metatype of Nodes - The general "level" in the graph structure
 * the node serves.
 */
typedef enum eDepsNode_Class {
	/* Types generally unassociated with user-visible entities,
	 * but needed for graph functioning.
	 */
	DEG_NODE_CLASS_GENERIC         = 0,
	/* [Outer Node] An "aspect" of evaluating/updating an ID-Block, requiring
	 * certain types of evaluation behavior.
	 */
	DEG_NODE_CLASS_COMPONENT       = 1,
	/* [Inner Node] A glorified function-pointer/callback for scheduling up
	 * evaluation operations for components, subject to relationship
	 * requirements.
	 */
	DEG_NODE_CLASS_OPERATION       = 2,
} eDepsNode_Class;

/* Note: We use max comparison to mark an id node that is linked more than once
 * So keep this enum ordered accordingly.
 */
typedef enum eDepsNode_LinkedState_Type {
	/* Generic indirectly linked id node. */
	DEG_ID_LINKED_INDIRECTLY       = 0,
	/* Id node present in the set (background) only. */
	DEG_ID_LINKED_VIA_SET          = 1,
	/* Id node directly linked via the ScenLayer. */
	DEG_ID_LINKED_DIRECTLY         = 2,
} eDepsNode_LinkedState_Type;

/* Types of Nodes */
typedef enum eDepsNode_Type {
	/* Fallback type for invalid return value */
	DEG_NODE_TYPE_UNDEFINED        = 0,
	/* Inner Node (Operation) */
	DEG_NODE_TYPE_OPERATION,

	/* **** Generic Types **** */

	/* Time-Source */
	DEG_NODE_TYPE_TIMESOURCE,
	/* ID-Block reference - used as landmarks/collection point for components,
	 * but not usually part of main graph.
	 */
	DEG_NODE_TYPE_ID_REF,

	/* **** Outer Types **** */

	/* Parameters Component - Default when nothing else fits
	 * (i.e. just SDNA property setting).
	 */
	DEG_NODE_TYPE_PARAMETERS,
	/* Generic "Proxy-Inherit" Component. */
	DEG_NODE_TYPE_PROXY,
	/* Animation Component */
	DEG_NODE_TYPE_ANIMATION,
	/* Transform Component (Parenting/Constraints) */
	DEG_NODE_TYPE_TRANSFORM,
	/* Geometry Component (DerivedMesh/Displist) */
	DEG_NODE_TYPE_GEOMETRY,
	/* Sequencer Component (Scene Only) */
	DEG_NODE_TYPE_SEQUENCER,
	/* Component which contains all operations needed for layer collections
	 * evaluation.
	 */
	DEG_NODE_TYPE_LAYER_COLLECTIONS,
	/* Entry component of majority of ID nodes: prepares CoW pointers for
	 * execution.
	 */
	DEG_NODE_TYPE_COPY_ON_WRITE,
	/* Used by all operations which are updating object when something is
	 * changed in view layer.
	 */
	DEG_NODE_TYPE_OBJECT_FROM_LAYER,

	/* **** Evaluation-Related Outer Types (with Subdata) **** */

	/* Pose Component - Owner/Container of Bones Eval */
	DEG_NODE_TYPE_EVAL_POSE,
	/* Bone Component - Child/Subcomponent of Pose */
	DEG_NODE_TYPE_BONE,
	/* Particle Systems Component */
	DEG_NODE_TYPE_EVAL_PARTICLES,
	/* Material Shading Component */
	DEG_NODE_TYPE_SHADING,
	DEG_NODE_TYPE_SHADING_PARAMETERS,
	/* Cache Component */
	DEG_NODE_TYPE_CACHE,
	/* Batch Cache Component - TODO (dfelinto/sergey) rename to make it more generic. */
	DEG_NODE_TYPE_BATCH_CACHE,

	/* Total number of meaningful node types. */
	NUM_DEG_NODE_TYPES,
} eDepsNode_Type;

const char *nodeTypeAsString(eDepsNode_Type type);

/* Identifiers for common operations (as an enum). */
typedef enum eDepsOperation_Code {
	/* Generic Operations. ------------------------------ */

	/* Placeholder for operations which don't need special mention */
	DEG_OPCODE_OPERATION = 0,

	/* Generic parameters evaluation. */
	DEG_OPCODE_ID_PROPERTY,
	DEG_OPCODE_PARAMETERS_EVAL,

	// XXX: Placeholder while porting depsgraph code
	DEG_OPCODE_PLACEHOLDER,

	/* Animation, Drivers, etc. ------------------------ */
	/* NLA + Action */
	DEG_OPCODE_ANIMATION,
	/* Driver */
	DEG_OPCODE_DRIVER,

	/* Object related. --------------------------------- */
	DEG_OPCODE_OBJECT_BASE_FLAGS,

	/* Transform. -------------------------------------- */
	/* Transform entry point - local transforms only */
	DEG_OPCODE_TRANSFORM_LOCAL,
	/* Parenting */
	DEG_OPCODE_TRANSFORM_PARENT,
	/* Constraints */
	DEG_OPCODE_TRANSFORM_CONSTRAINTS,
	/* Transform exit point */
	DEG_OPCODE_TRANSFORM_FINAL,
	/* Handle object-level updates, mainly proxies hacks and recalc flags.  */
	DEG_OPCODE_TRANSFORM_OBJECT_UBEREVAL,

	/* Rigid body. -------------------------------------- */
	/* Perform Simulation */
	DEG_OPCODE_RIGIDBODY_REBUILD,
	DEG_OPCODE_RIGIDBODY_SIM,
	/* Copy results to object */
	DEG_OPCODE_RIGIDBODY_TRANSFORM_COPY,

	/* Geometry. ---------------------------------------- */
	/* Evaluate the whole geometry, including modifiers. */
	DEG_OPCODE_GEOMETRY_UBEREVAL,
	DEG_OPCODE_GEOMETRY_CLOTH_MODIFIER,
	DEG_OPCODE_GEOMETRY_SHAPEKEY,

	/* Object data. ------------------------------------- */
	DEG_OPCODE_LIGHT_PROBE_EVAL,
	DEG_OPCODE_SPEAKER_EVAL,

	/* Pose. -------------------------------------------- */
	/* Init pose, clear flags, etc. */
	DEG_OPCODE_POSE_INIT,
	/* Initialize IK solver related pose stuff. */
	DEG_OPCODE_POSE_INIT_IK,
	/* Free IK Trees + Compute Deform Matrices */
	DEG_OPCODE_POSE_DONE,
	/* IK/Spline Solvers */
	DEG_OPCODE_POSE_IK_SOLVER,
	DEG_OPCODE_POSE_SPLINE_IK_SOLVER,

	/* Bone. -------------------------------------------- */
	/* Bone local transforms - entry point */
	DEG_OPCODE_BONE_LOCAL,
	/* Pose-space conversion (includes parent + restpose, */
	DEG_OPCODE_BONE_POSE_PARENT,
	/* Constraints */
	DEG_OPCODE_BONE_CONSTRAINTS,
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

	/* Particles. --------------------------------------- */
	/* Particle System evaluation. */
	DEG_OPCODE_PARTICLE_SYSTEM_EVAL_INIT,
	DEG_OPCODE_PARTICLE_SYSTEM_EVAL,
	DEG_OPCODE_PARTICLE_SETTINGS_EVAL,

	/* Point Cache. ------------------------------------- */
	DEG_OPCODE_POINT_CACHE_RESET,

	/* Collections. ------------------------------------- */
	DEG_OPCODE_VIEW_LAYER_EVAL,

	/* Copy on Write. ------------------------------------ */
	DEG_OPCODE_COPY_ON_WRITE,

	/* Shading. ------------------------------------------- */
	DEG_OPCODE_SHADING,
	DEG_OPCODE_MATERIAL_UPDATE,
	DEG_OPCODE_WORLD_UPDATE,

	/* Batch caches. -------------------------------------- */
	DEG_OPCODE_GEOMETRY_SELECT_UPDATE,

	/* Masks. ------------------------------------------ */
	DEG_OPCODE_MASK_ANIMATION,
	DEG_OPCODE_MASK_EVAL,

	/* Movie clips. ------------------------------------ */
	DEG_OPCODE_MOVIECLIP_EVAL,
	DEG_OPCODE_MOVIECLIP_SELECT_UPDATE,

	DEG_NUM_OPCODES,
} eDepsOperation_Code;
const char *operationCodeAsString(eDepsOperation_Code opcode);

typedef enum eDepsNode_CollectionOwner {
	/* Unknown owner of collection, collection is pulled directly, maybe
	 * via driver.
	 */
	DEG_COLLECTION_OWNER_UNKNOWN,
	/* Collection belongs to a scene. */
	DEG_COLLECTION_OWNER_SCENE,
	/* Collection is used by object, as a dupli-system. */
	DEG_COLLECTION_OWNER_OBJECT,
} eDepsNode_CollectionOwner;

}  // namespace DEG
