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
 *
 * Datatypes for internal use in the Depsgraph
 *
 * All of these datatypes are only really used within the "core" depsgraph.
 * In particular, node types declared here form the structure of operations
 * in the graph.
 */

#ifndef __DEPSGRAPH_TYPES_H__
#define __DEPSGRAPH_TYPES_H__

#include "depsgraph_util_function.h"

/* TODO(sergey): Ideally we'll just use char* and statically allocated strings
 * to avoid any possible overhead caused by string (re)allocation/formatting.
 */
#include <string>
#include <vector>

using std::string;
using std::vector;

struct bAction;
struct ChannelDriver;
struct ModifierData;
struct PointerRNA;
struct EvaluationContext;
struct FCurve;

/* Evaluation Operation for atomic operation */
// XXX: move this to another header that can be exposed?
typedef function<void(struct EvaluationContext*)> DepsEvalOperationCb;

/* Metatype of Nodes - The general "level" in the graph structure the node serves */
typedef enum eDepsNode_Class {
	DEPSNODE_CLASS_GENERIC         = 0,        /* Types generally unassociated with user-visible entities, but needed for graph functioning */

	DEPSNODE_CLASS_COMPONENT       = 1,        /* [Outer Node] An "aspect" of evaluating/updating an ID-Block, requiring certain types of evaluation behaviours */
	DEPSNODE_CLASS_OPERATION       = 2,        /* [Inner Node] A glorified function-pointer/callback for scheduling up evaluation operations for components, subject to relationship requirements */
} eDepsNode_Class;

/* Types of Nodes */
typedef enum eDepsNode_Type {
	DEPSNODE_TYPE_UNDEFINED        = -1,       /* fallback type for invalid return value */

	DEPSNODE_TYPE_OPERATION        = 0,        /* Inner Node (Operation) */

	/* Generic Types */
	DEPSNODE_TYPE_ROOT             = 1,        /* "Current Scene" - basically whatever kicks off the evaluation process */
	DEPSNODE_TYPE_TIMESOURCE       = 2,        /* Time-Source */

	DEPSNODE_TYPE_ID_REF           = 3,        /* ID-Block reference - used as landmarks/collection point for components, but not usually part of main graph */
	DEPSNODE_TYPE_SUBGRAPH         = 4,        /* Isolated sub-graph - used for keeping instanced data separate from instances using them */

	/* Outer Types */
	DEPSNODE_TYPE_PARAMETERS       = 11,       /* Parameters Component - Default when nothing else fits (i.e. just SDNA property setting) */
	DEPSNODE_TYPE_PROXY            = 12,       /* Generic "Proxy-Inherit" Component */   // XXX: Also for instancing of subgraphs?
	DEPSNODE_TYPE_ANIMATION        = 13,       /* Animation Component */                 // XXX: merge in with parameters?
	DEPSNODE_TYPE_TRANSFORM        = 14,       /* Transform Component (Parenting/Constraints) */
	DEPSNODE_TYPE_GEOMETRY         = 15,       /* Geometry Component (DerivedMesh/Displist) */
	DEPSNODE_TYPE_SEQUENCER        = 16,       /* Sequencer Component (Scene Only) */

	/* Evaluation-Related Outer Types (with Subdata) */
	DEPSNODE_TYPE_EVAL_POSE        = 21,       /* Pose Component - Owner/Container of Bones Eval */
	DEPSNODE_TYPE_BONE             = 22,       /* Bone Component - Child/Subcomponent of Pose */

	DEPSNODE_TYPE_EVAL_PARTICLES   = 23,       /* Particle Systems Component */
	DEPSNODE_TYPE_SHADING          = 24,       /* Material Shading Component */
} eDepsNode_Type;

/* Identifiers for common operations (as an enum) */
typedef enum eDepsOperation_Code {
#define DEF_DEG_OPCODE(label) DEG_OPCODE_##label,
#include "depsnode_opcodes.h"
#undef DEF_DEG_OPCODE
} eDepsOperation_Code;

/* String defines for these opcodes, defined in depsnode_operation.cpp */
extern const char *DEG_OPNAMES[];


/* Type of operation */
typedef enum eDepsOperation_Type {
	/* Primary operation types */
	DEPSOP_TYPE_INIT    = 0, /* initialise evaluation data */
	DEPSOP_TYPE_EXEC    = 1, /* standard evaluation step */
	DEPSOP_TYPE_POST    = 2, /* cleanup evaluation data + flush results */

	/* Additional operation types */
	DEPSOP_TYPE_OUT     = 3, /* indicator for outputting a temporary result that other components can use */ // XXX?
	DEPSOP_TYPE_SIM     = 4, /* indicator for things like IK Solvers and Rigidbody Sim steps which modify final results of separate entities at once */
	DEPSOP_TYPE_REBUILD = 5, /* rebuild internal evaluation data - used for Rigidbody Reset and Armature Rebuild-On-Load */
} eDepsOperation_Type;

/* Types of relationships between nodes
 *
 * This is used to provide additional hints to use when filtering
 * the graph, so that we can go without doing more extensive
 * data-level checks...
 */
typedef enum eDepsRelation_Type {
	/* reationship type unknown/irrelevant */
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
} eDepsRelation_Type;

#endif  /* __DEPSGRAPH_TYPES_H__ */
