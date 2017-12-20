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

#include "BLI_threads.h"  /* for SpinLock */

#include "intern/depsgraph_types.h"

struct ID;
struct GHash;
struct GSet;
struct PointerRNA;
struct PropertyRNA;

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
	/* "touched" tag is used when filtering, to know which to collect */
	DEPSREL_FLAG_TEMP_TAG   = (1 << 0),

	/* "cyclic" link - when detecting cycles, this relationship was the one
	 * which triggers a cyclic relationship to exist in the graph
	 */
	DEPSREL_FLAG_CYCLIC     = (1 << 1),
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

	Depsgraph();
	~Depsgraph();

	/**
	 * Convenience wrapper to find node given just pointer + property.
	 *
	 * \param ptr: pointer to the data that node will represent
	 * \param prop: optional property affected - providing this effectively results in inner nodes being returned
	 *
	 * \return A node matching the required characteristics if it exists
	 * or NULL if no such node exists in the graph
	 */
	DepsNode *find_node_from_pointer(const PointerRNA *ptr, const PropertyRNA *prop) const;

	TimeSourceDepsNode *add_time_source();
	TimeSourceDepsNode *find_time_source() const;

	IDDepsNode *find_id_node(const ID *id) const;
	IDDepsNode *add_id_node(ID *id, const char *name = "");
	void clear_id_nodes();

	/* Add new relationship between two nodes. */
	DepsRelation *add_new_relation(OperationDepsNode *from,
	                               OperationDepsNode *to,
	                               const char *description,
	                               bool check_unique = false);

	DepsRelation *add_new_relation(DepsNode *from,
	                               DepsNode *to,
	                               const char *description,
	                               bool check_unique = false);

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

	/* Layers Visibility .................. */

	/* Visible layers bitfield, used for skipping invisible objects updates. */
	unsigned int layers;

	// XXX: additional stuff like eval contexts, mempools for allocating nodes from, etc.
};

}  // namespace DEG
