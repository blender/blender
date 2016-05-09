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

/** \file blender/depsgraph/intern/depsnode.h
 *  \ingroup depsgraph
 */

#ifndef __DEPSNODE_H__
#define __DEPSNODE_H__

#include "depsgraph_types.h"

#include "depsgraph_util_hash.h"
#include "depsgraph_util_map.h"
#include "depsgraph_util_set.h"

struct ID;
struct Scene;

struct Depsgraph;
struct DepsRelation;
struct DepsgraphCopyContext;
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
	string name;

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

	/* Generic tag for traversal algorithms */
	int done;

	/* Methods. */

	DepsNode();
	virtual ~DepsNode();

	virtual string identifier() const;
	string full_identifier() const;

	virtual void init(const ID * /*id*/,
	                  const string &/*subdata*/) {}
	virtual void copy(DepsgraphCopyContext * /*dcc*/,
	                  const DepsNode * /*src*/) {}

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

/* Root Node. */
struct RootDepsNode : public DepsNode {
	RootDepsNode();
	~RootDepsNode();

	TimeSourceDepsNode *add_time_source(const string &name = "");

	/* scene that this corresponds to */
	Scene *scene;

	/* Entrypoint node for time-changed. */
	TimeSourceDepsNode *time_source;

	DEG_DEPSNODE_DECLARE;
};

/* ID-Block Reference */
struct IDDepsNode : public DepsNode {
	struct ComponentIDKey {
		ComponentIDKey(eDepsNode_Type type, const string &name = "")
		    : type(type), name(name) {}

		bool operator== (const ComponentIDKey &other) const
		{
			return type == other.type && name == other.name;
		}

		eDepsNode_Type type;
		string name;
	};

	/* XXX can't specialize std::hash for this purpose, because ComponentIDKey is
	 * a nested type ...
	 *
	 *   http://stackoverflow.com/a/951245
	 */
	struct component_key_hash {
		bool operator() (const ComponentIDKey &key) const
		{
			return hash_combine(hash<int>()(key.type), hash<string>()(key.name));
		}
	};

	typedef unordered_map<ComponentIDKey,
	                      ComponentDepsNode *,
	                      component_key_hash> ComponentMap;

	void init(const ID *id, const string &subdata);
	void copy(DepsgraphCopyContext *dcc, const IDDepsNode *src);
	~IDDepsNode();

	ComponentDepsNode *find_component(eDepsNode_Type type,
	                                  const string &name = "") const;
	ComponentDepsNode *add_component(eDepsNode_Type type,
	                                 const string &name = "");
	void remove_component(eDepsNode_Type type, const string &name = "");
	void clear_components();

	void tag_update(Depsgraph *graph);

	/* ID Block referenced. */
	ID *id;

	/* Hash to make it faster to look up components. */
	ComponentMap components;

	/* Layers of this node with accumulated layers of it's output relations. */
	int layers;

	/* Additional flags needed for scene evaluation.
	 * TODO(sergey): Only needed for until really granular updates
	 * of all the entities.
	 */
	int eval_flags;

	DEG_DEPSNODE_DECLARE;
};

/* Subgraph Reference. */
struct SubgraphDepsNode : public DepsNode {
	void init(const ID *id, const string &subdata);
	void copy(DepsgraphCopyContext *dcc, const SubgraphDepsNode *src);
	~SubgraphDepsNode();

	/* Instanced graph. */
	Depsgraph *graph;

	/* ID-block at root of subgraph (if applicable). */
	ID *root_id;

	/* Number of nodes which use/reference this subgraph - if just 1, it may be
	 * possible to merge into main,
	 */
	size_t num_users;

	/* (eSubgraphRef_Flag) assorted settings for subgraph node. */
	int flag;

	DEG_DEPSNODE_DECLARE;
};

/* Flags for subgraph node */
typedef enum eSubgraphRef_Flag {
	/* Subgraph referenced is shared with another reference, so shouldn't
	 * free on exit.
	 */
	SUBGRAPH_FLAG_SHARED      = (1 << 0),

	/* Node is first reference to subgraph, so it can be freed when we are
	 * removed.
	 */
	SUBGRAPH_FLAG_FIRSTREF    = (1 << 1),
} eSubgraphRef_Flag;

void DEG_register_base_depsnodes();

#endif  /* __DEPSNODE_H__ */
