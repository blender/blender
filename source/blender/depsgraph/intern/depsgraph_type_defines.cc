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

/** \file blender/depsgraph/intern/depsgraph_type_defines.cc
 *  \ingroup depsgraph
 *
 * Defines and code for core node types.
 */

#include <cstdlib>  // for BLI_assert()


#include "BLI_utildefines.h"
#include "BLI_ghash.h"

#include "DEG_depsgraph.h"

#include "intern/nodes/deg_node.h"
#include "intern/nodes/deg_node_component.h"
#include "intern/nodes/deg_node_operation.h"

#include "intern/depsgraph_intern.h"

namespace DEG {

/* ************ */
/* External API */

/* Global type registry */

/**
 * \note For now, this is a hashtable not array, since the core node types
 * currently do not have contiguous ID values. Using a hash here gives us
 * more flexibility, albeit using more memory and also sacrificing a little
 * speed. Later on, when things stabilise we may turn this back to an array
 * since there are only just a few node types that an array would cope fine...
 */
static GHash *_depsnode_typeinfo_registry = NULL;

/* Registration ------------------------------------------- */

/* Register node type */
void deg_register_node_typeinfo(DepsNodeFactory *factory)
{
	BLI_assert(factory != NULL);
	BLI_ghash_insert(_depsnode_typeinfo_registry, SET_INT_IN_POINTER(factory->type()), factory);
}

/* Getters ------------------------------------------------- */

/* Get typeinfo for specified type */
DepsNodeFactory *deg_get_node_factory(const eDepsNode_Type type)
{
	/* look up type - at worst, it doesn't exist in table yet, and we fail */
	return (DepsNodeFactory *)BLI_ghash_lookup(_depsnode_typeinfo_registry, SET_INT_IN_POINTER(type));
}

/* Get typeinfo for provided node */
DepsNodeFactory *deg_node_get_factory(const DepsNode *node)
{
	if (node != NULL) {
		return NULL;
	}
	return deg_get_node_factory(node->type);
}

/* Stringified opcodes ------------------------------------- */

DepsOperationStringifier DEG_OPNAMES;

static const char *stringify_opcode(eDepsOperation_Code opcode)
{
	switch (opcode) {
#define STRINGIFY_OPCODE(name) case DEG_OPCODE_##name: return #name
		STRINGIFY_OPCODE(OPERATION);
		STRINGIFY_OPCODE(PLACEHOLDER);
		STRINGIFY_OPCODE(ANIMATION);
		STRINGIFY_OPCODE(DRIVER);
		STRINGIFY_OPCODE(TRANSFORM_LOCAL);
		STRINGIFY_OPCODE(TRANSFORM_PARENT);
		STRINGIFY_OPCODE(TRANSFORM_CONSTRAINTS);
		STRINGIFY_OPCODE(RIGIDBODY_REBUILD);
		STRINGIFY_OPCODE(RIGIDBODY_SIM);
		STRINGIFY_OPCODE(TRANSFORM_RIGIDBODY);
		STRINGIFY_OPCODE(TRANSFORM_FINAL);
		STRINGIFY_OPCODE(OBJECT_UBEREVAL);
		STRINGIFY_OPCODE(GEOMETRY_UBEREVAL);
		STRINGIFY_OPCODE(GEOMETRY_PATH);
		STRINGIFY_OPCODE(POSE_INIT);
		STRINGIFY_OPCODE(POSE_INIT_IK);
		STRINGIFY_OPCODE(POSE_DONE);
		STRINGIFY_OPCODE(POSE_IK_SOLVER);
		STRINGIFY_OPCODE(POSE_SPLINE_IK_SOLVER);
		STRINGIFY_OPCODE(BONE_LOCAL);
		STRINGIFY_OPCODE(BONE_POSE_PARENT);
		STRINGIFY_OPCODE(BONE_CONSTRAINTS);
		STRINGIFY_OPCODE(BONE_READY);
		STRINGIFY_OPCODE(BONE_DONE);
		STRINGIFY_OPCODE(PSYS_EVAL);
		STRINGIFY_OPCODE(PSYS_EVAL_INIT);
		STRINGIFY_OPCODE(MASK_ANIMATION);
		STRINGIFY_OPCODE(MASK_EVAL);

		case DEG_NUM_OPCODES: return "SpecialCase";
#undef STRINGIFY_OPCODE
	}
	return "UNKNOWN";
}

DepsOperationStringifier::DepsOperationStringifier()
{
	for (int i = 0; i < DEG_NUM_OPCODES; ++i) {
		names_[i] = stringify_opcode((eDepsOperation_Code)i);
	}
}

const char *DepsOperationStringifier::operator[](eDepsOperation_Code opcode)
{
	BLI_assert((opcode >= 0) && (opcode < DEG_NUM_OPCODES));
	if (opcode >= 0 && opcode < DEG_NUM_OPCODES) {
		return names_[opcode];
	}
	return "UnknownOpcode";
}

}  // namespace DEG

/* Register all node types */
void DEG_register_node_types(void)
{
	/* initialise registry */
	DEG::_depsnode_typeinfo_registry = BLI_ghash_int_new("Depsgraph Node Type Registry");

	/* register node types */
	DEG::deg_register_base_depsnodes();
	DEG::deg_register_component_depsnodes();
	DEG::deg_register_operation_depsnodes();
}

/* Free registry on exit */
void DEG_free_node_types(void)
{
	BLI_ghash_free(DEG::_depsnode_typeinfo_registry, NULL, NULL);
}
