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

/** \file blender/depsgraph/intern/depsgraph_intern.h
 *  \ingroup depsgraph
 *
 * API's for internal use in the Depsgraph
 * - Also, defines for "Node Type Info"
 */

#pragma once

#include <cstdlib>

#include "MEM_guardedalloc.h"

extern "C" {
#include "BKE_global.h"
}

#include "intern/nodes/deg_node.h"
#include "intern/nodes/deg_node_component.h"
#include "intern/nodes/deg_node_operation.h"
#include "intern/depsgraph.h"

#include "DEG_depsgraph_debug.h"

struct DEGEditorUpdateContext;
struct Collection;
struct ListBase;
struct Main;
struct Scene;

namespace DEG {

/* Node Types Handling ================================================= */

/* "Typeinfo" for Node Types ------------------------------------------- */

/* Typeinfo Struct (nti) */
struct DepsNodeFactory {
	virtual eDepsNode_Type type() const = 0;
	virtual const char *tname() const = 0;
	virtual int id_recalc_tag() const = 0;

	virtual DepsNode *create_node(const ID *id,
	                              const char *subdata,
	                              const char *name) const = 0;
};

template <class NodeType>
struct DepsNodeFactoryImpl : public DepsNodeFactory {
	eDepsNode_Type type() const { return NodeType::typeinfo.type; }
	const char *tname() const { return NodeType::typeinfo.tname; }
	int id_recalc_tag() const { return NodeType::typeinfo.id_recalc_tag; }

	DepsNode *create_node(const ID *id, const char *subdata, const char *name) const
	{
		DepsNode *node = OBJECT_GUARDED_NEW(NodeType);

		/* populate base node settings */
		node->type = type();

		if (name[0] != '\0') {
			/* set name if provided ... */
			node->name = name;
		}
		else {
			/* ... otherwise use default type name */
			node->name = tname();
		}

		node->init(id, subdata);

		return node;
	}
};

/* Typeinfo Management -------------------------------------------------- */

/* Register typeinfo */
void deg_register_node_typeinfo(DepsNodeFactory *factory);

/* Get typeinfo for specified type */
DepsNodeFactory *deg_type_get_factory(const eDepsNode_Type type);

/* Editors Integration -------------------------------------------------- */

void deg_editors_id_update(const DEGEditorUpdateContext *update_ctx,
                           struct ID *id);

void deg_editors_scene_update(const DEGEditorUpdateContext *update_ctx,
                              bool updated);

#define DEG_DEBUG_PRINTF(depsgraph, type, ...) \
	do { \
		if (DEG_debug_flags_get(depsgraph) & G_DEBUG_DEPSGRAPH_ ## type) { \
			DEG_debug_print_begin(depsgraph); \
			fprintf(stdout, __VA_ARGS__); \
		} \
	} while (0)

#define DEG_GLOBAL_DEBUG_PRINTF(type, ...) \
	do { \
		if (G.debug & G_DEBUG_DEPSGRAPH_ ## type) { \
			fprintf(stdout, __VA_ARGS__); \
		} \
	} while (0)

#define DEG_ERROR_PRINTF(...)               \
	do {                                    \
		fprintf(stderr, __VA_ARGS__);       \
		fflush(stderr);                     \
	} while (0)

bool deg_terminal_do_color(void);
string deg_color_for_pointer(const void *pointer);
string deg_color_end(void);

/* Physics Utilities -------------------------------------------------- */

struct ListBase *deg_build_effector_relations(Depsgraph *graph, struct Collection *collection);
struct ListBase *deg_build_collision_relations(Depsgraph *graph, struct Collection *collection, unsigned int modifier_type);
void deg_clear_physics_relations(Depsgraph *graph);

/* Tagging Utilities -------------------------------------------------------- */

eDepsNode_Type deg_geometry_tag_to_component(const ID *id);

}  // namespace DEG
