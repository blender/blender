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
 * Contributor(s): Sergey Sharybin
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/depsgraph/intern/depsgraph.h
 *  \ingroup depsgraph
 *
 * Datatypes for internal use in the Depsgraph
 *
 * All of these datatypes are only really used within the "core" depsgraph.
 * In particular, node types declared here form the structure of operations
 * in the graph.
 */

#pragma once

#include <stdlib.h>

#include "DNA_ID.h" /* for ID_Type */

#include "BKE_main.h" /* for MAX_LIBARRAY */

#include "BLI_threads.h"  /* for SpinLock */

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_physics.h"

#include "intern/depsgraph_types.h"

struct ID;
struct GHash;
struct Main;
struct GSet;
struct PointerRNA;
struct PropertyRNA;
struct Scene;
struct ViewLayer;

namespace DEG {

struct DepsNode;
struct TimeSourceDepsNode;
struct IDDepsNode;
struct ComponentDepsNode;
struct OperationDepsNode;

/* *************************** */
/* Relationships Between Nodes */

/* Settings/Tags on Relationship */
typedef enum eDepsRelation_Flag {
	/* "cyclic" link - when detecting cycles, this relationship was the one
	 * which triggers a cyclic relationship to exist in the graph. */
	DEPSREL_FLAG_CYCLIC               = (1 << 0),
	/* Update flush will not go through this relation. */
	DEPSREL_FLAG_NO_FLUSH             = (1 << 1),
	/* Only flush along the relation is update comes from a node which was
	 * affected by user input. */
	DEPSREL_FLAG_FLUSH_USER_EDIT_ONLY = (1 << 2),
	/* The relation can not be killed by the cyclic dependencies solver. */
	DEPSREL_FLAG_GODMODE              = (1 << 3),
} eDepsRelation_Flag;

/* B depends on A (A -> B) */
struct DepsRelation {
	/* the nodes in the relationship (since this is shared between the nodes) */
	DepsNode *from;               /* A */
	DepsNode *to;                 /* B */

	/* relationship attributes */
	const char *name;             /* label for debugging */

	int flag;                     /* (eDepsRelation_Flag) */

	DepsRelation(DepsNode *from,
	             DepsNode *to,
	             const char *description);

	~DepsRelation();

	void unlink();
};

/* ********* */
/* Depsgraph */

/* Dependency Graph object */
struct Depsgraph {
	// TODO(sergey): Go away from C++ container and use some native BLI.
	typedef vector<OperationDepsNode *> OperationNodes;
	typedef vector<IDDepsNode *> IDDepsNodes;

	Depsgraph(Scene *scene,
	          ViewLayer *view_layer,
	          eEvaluationMode mode);
	~Depsgraph();

	/**
	 * Convenience wrapper to find node given just pointer + property.
	 *
	 * \param ptr: pointer to the data that node will represent
	 * \param prop: optional property affected - providing this effectively
	 *              results in inner nodes being returned
	 *
	 * \return A node matching the required characteristics if it exists
	 * or NULL if no such node exists in the graph
	 */
	DepsNode *find_node_from_pointer(const PointerRNA *ptr, const PropertyRNA *prop) const;

	TimeSourceDepsNode *add_time_source();
	TimeSourceDepsNode *find_time_source() const;

	IDDepsNode *find_id_node(const ID *id) const;
	IDDepsNode *add_id_node(ID *id, ID *id_cow_hint = NULL);
	void clear_id_nodes();
	void clear_id_nodes_conditional(const std::function <bool (ID_Type id_type)>& filter);

	/* Add new relationship between two nodes. */
	DepsRelation *add_new_relation(OperationDepsNode *from,
	                               OperationDepsNode *to,
	                               const char *description,
	                               bool check_unique = false,
	                               int flags = 0);

	DepsRelation *add_new_relation(DepsNode *from,
	                               DepsNode *to,
	                               const char *description,
	                               bool check_unique = false,
	                               int flags = 0);

	/* Check whether two nodes are connected by relation with given
	 * description. Description might be NULL to check ANY relation between
	 * given nodes.
	 */
	DepsRelation *check_nodes_connected(const DepsNode *from,
	                                    const DepsNode *to,
	                                    const char *description);

	/* Tag a specific node as needing updates. */
	void add_entry_tag(OperationDepsNode *node);

	/* Clear storage used by all nodes. */
	void clear_all_nodes();

	/* Copy-on-Write Functionality ........ */

	/* For given original ID get ID which is created by CoW system. */
	ID *get_cow_id(const ID *id_orig) const;

	/* Core Graph Functionality ........... */

	/* <ID : IDDepsNode> mapping from ID blocks to nodes representing these
	 * blocks, used for quick lookups.
	 */
	GHash *id_hash;

	/* Ordered list of ID nodes, order matches ID allocation order.
	 * Used for faster iteration, especially for areas which are critical to
	 * keep exact order of iteration.
	 */
	IDDepsNodes id_nodes;

	/* Top-level time source node. */
	TimeSourceDepsNode *time_source;

	/* Indicates whether relations needs to be updated. */
	bool need_update;

	/* Indicates which ID types were updated. */
	char id_type_updated[MAX_LIBARRAY];

	/* Quick-Access Temp Data ............. */

	/* Nodes which have been tagged as "directly modified". */
	GSet *entry_tags;

	/* Convenience Data ................... */

	/* XXX: should be collected after building (if actually needed?) */
	/* All operation nodes, sorted in order of single-thread traversal order. */
	OperationNodes operations;

	/* Spin lock for threading-critical operations.
	 * Mainly used by graph evaluation.
	 */
	SpinLock lock;

	/* Scene, layer, mode this dependency graph is built for. */
	Scene *scene;
	ViewLayer *view_layer;
	eEvaluationMode mode;

	/* Time at which dependency graph is being or was last evaluated. */
	float ctime;

	/* Evaluated version of datablocks we access a lot.
	 * Stored here to save us form doing hash lookup.
	 */
	Scene *scene_cow;

	/* Active dependency graph is a dependency graph which is used by the
	 * currently active window. When dependency graph is active, it is allowed
	 * for evaluation functions to write animation f-curve result, drivers
	 * result and other selective things (object matrix?) to original object.
	 *
	 * This way we simplify operators, which don't need to worry about where
	 * to read stuff from.
	 */
	bool is_active;

	/* NITE: Corresponds to G_DEBUG_DEPSGRAPH_* flags. */
	int debug_flags;
	string debug_name;

	bool debug_is_evaluating;

	/* Cached list of colliders/effectors for collections and the scene
	 * created along with relations, for fast lookup during evaluation. */
	GHash *physics_relations[DEG_PHYSICS_RELATIONS_NUM];
};

}  // namespace DEG
