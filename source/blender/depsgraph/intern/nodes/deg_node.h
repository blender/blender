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

/** \file depsgraph/intern/nodes/deg_node.h
 *  \ingroup depsgraph
 */

#pragma once

#include "intern/depsgraph_types.h"

#include "BLI_utildefines.h"

struct ID;
struct GHash;
struct Scene;

namespace DEG {

struct Depsgraph;
struct DepsRelation;
struct OperationDepsNode;

/* *********************************** */
/* Base-Defines for Nodes in Depsgraph */

/* All nodes in Depsgraph are descended from this. */
struct DepsNode {
	/* Helper class for static typeinfo in subclasses. */
	struct TypeInfo {
		TypeInfo(eDepsNode_Type type, const char *tname);

		eDepsNode_Type type;
		eDepsNode_Class tclass;
		const char *tname;
	};

	/* Identifier - mainly for debugging purposes. */
	const char *name;

	/* Structural type of node. */
	eDepsNode_Type type;

	/* Type of data/behaviour represented by node... */
	eDepsNode_Class tclass;

	/* Relationships between nodes
	 * The reason why all depsgraph nodes are descended from this type (apart
	 * from basic serialization benefits - from the typeinfo) is that we can have
	 * relationships between these nodes!
	 */
	typedef vector<DepsRelation *> Relations;

	/* Nodes which this one depends on. */
	Relations inlinks;

	/* Nodes which depend on this one. */
	Relations outlinks;

	/* Generic tags for traversal algorithms. */
	int done;
	int tag;

	/* Methods. */

	DepsNode();
	virtual ~DepsNode();

	virtual string identifier() const;
	string full_identifier() const;

	virtual void init(const ID * /*id*/,
	                  const char * /*subdata*/) {}

	virtual void tag_update(Depsgraph * /*graph*/) {}

	virtual OperationDepsNode *get_entry_operation() { return NULL; }
	virtual OperationDepsNode *get_exit_operation() { return NULL; }
};

/* Macros for common static typeinfo. */
#define DEG_DEPSNODE_DECLARE \
	static const DepsNode::TypeInfo typeinfo
#define DEG_DEPSNODE_DEFINE(NodeType, type_, tname_) \
	const DepsNode::TypeInfo NodeType::typeinfo = DepsNode::TypeInfo(type_, tname_)

/* Generic Nodes ======================= */

struct ComponentDepsNode;
struct IDDepsNode;

/* Time Source Node. */
struct TimeSourceDepsNode : public DepsNode {
	/* New "current time". */
	float cfra;

	/* time-offset relative to the "official" time source that this one has. */
	float offset;

	// TODO: evaluate() operation needed

	void tag_update(Depsgraph *graph);

	DEG_DEPSNODE_DECLARE;
};

/* ID-Block Reference */
struct IDDepsNode : public DepsNode {
	struct ComponentIDKey {
		ComponentIDKey(eDepsNode_Type type, const char *name = "");
		bool operator==(const ComponentIDKey &other) const;

		eDepsNode_Type type;
		const char *name;
	};

	void init(const ID *id, const char *subdata);
	~IDDepsNode();

	ComponentDepsNode *find_component(eDepsNode_Type type,
	                                  const char *name = "") const;
	ComponentDepsNode *add_component(eDepsNode_Type type,
	                                 const char *name = "");

	void tag_update(Depsgraph *graph);

	void finalize_build();

	/* ID Block referenced. */
	ID *id;

	/* Hash to make it faster to look up components. */
	GHash *components;

	/* Layers of this node with accumulated layers of it's output relations. */
	unsigned int layers;

	/* Additional flags needed for scene evaluation.
	 * TODO(sergey): Only needed for until really granular updates
	 * of all the entities.
	 */
	int eval_flags;

	DEG_DEPSNODE_DECLARE;
};

void deg_register_base_depsnodes();

}  // namespace DEG
