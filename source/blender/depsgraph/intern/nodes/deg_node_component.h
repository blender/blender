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

/** \file blender/depsgraph/intern/nodes/deg_node_component.h
 *  \ingroup depsgraph
 */

#pragma once

#include "intern/nodes/deg_node.h"

#include "BLI_utildefines.h"
#include "BLI_string.h"

struct ID;
struct bPoseChannel;
struct GHash;

namespace DEG {

struct Depsgraph;
struct OperationDepsNode;
struct BoneComponentDepsNode;
struct IDDepsNode;

/* ID Component - Base type for all components */
struct ComponentDepsNode : public DepsNode {
	/* Key used to look up operations within a component */
	struct OperationIDKey
	{
		eDepsOperation_Code opcode;
		const char *name;
		int name_tag;

		OperationIDKey();
		OperationIDKey(eDepsOperation_Code opcode);
		OperationIDKey(eDepsOperation_Code opcode,
		               const char *name,
		               int name_tag);

		string identifier() const;
		bool operator==(const OperationIDKey &other) const;
	};

	/* Typedef for container of operations */
	ComponentDepsNode();
	~ComponentDepsNode();

	void init(const ID *id, const char *subdata);

	string identifier() const;

	/* Find an existing operation, if requested operation does not exist
	 * NULL will be returned.
	 */
	OperationDepsNode *find_operation(OperationIDKey key) const;
	OperationDepsNode *find_operation(eDepsOperation_Code opcode,
	                                 const char *name,
	                                 int name_tag) const;

	/* Find an existing operation, will throw an assert() if it does not exist. */
	OperationDepsNode *get_operation(OperationIDKey key) const;
	OperationDepsNode *get_operation(eDepsOperation_Code opcode,
	                                 const char *name,
	                                 int name_tag) const;

	/* Check operation exists and return it. */
	bool has_operation(OperationIDKey key) const;
	bool has_operation(eDepsOperation_Code opcode,
	                   const char *name,
	                   int name_tag) const;

	/**
	 * Create a new node for representing an operation and add this to graph
	 * \warning If an existing node is found, it will be modified. This helps
	 * when node may have been partially created earlier (e.g. parent ref before
	 * parent item is added)
	 *
	 * \param type: Operation node type (corresponding to context/component that
	 *              it operates in)
	 * \param optype: Role that operation plays within component
	 *                (i.e. where in eval process)
	 * \param op: The operation to perform
	 * \param name: Identifier for operation - used to find/locate it again
	 */
	OperationDepsNode *add_operation(const DepsEvalOperationCb& op,
	                                 eDepsOperation_Code opcode,
	                                 const char *name,
	                                 int name_tag);

	/* Entry/exit operations management.
	 *
	 * Use those instead of direct set since this will perform sanity checks.
	 */
	void set_entry_operation(OperationDepsNode *op_node);
	void set_exit_operation(OperationDepsNode *op_node);

	void clear_operations();

	void tag_update(Depsgraph *graph);

	OperationDepsNode *get_entry_operation();
	OperationDepsNode *get_exit_operation();

	void finalize_build(Depsgraph *graph);

	IDDepsNode *owner;

	/* ** Inner nodes for this component ** */

	/* Operations stored as a hash map, for faster build.
	 * This hash map will be freed when graph is fully built.
	 */
	GHash *operations_map;

	/* This is a "normal" list of operations, used by evaluation
	 * and other routines after construction.
	 */
	vector<OperationDepsNode *> operations;

	OperationDepsNode *entry_operation;
	OperationDepsNode *exit_operation;

	virtual bool depends_on_cow() { return true; }

	/* Denotes whether COW component is to be tagged when this component
	 * is tagged for update.
	 */
	virtual bool need_tag_cow_before_update() { return true; }
};

/* ---------------------------------------- */

#define DEG_COMPONENT_NODE_DEFINE_TYPEINFO(NodeType, type_, tname_, id_recalc_tag) \
    const DepsNode::TypeInfo NodeType::typeinfo = \
        DepsNode::TypeInfo(type_, tname_, id_recalc_tag)

#define DEG_COMPONENT_NODE_DECLARE DEG_DEPSNODE_DECLARE

#define DEG_COMPONENT_NODE_DEFINE(name, NAME, id_recalc_tag)            \
    DEG_COMPONENT_NODE_DEFINE_TYPEINFO(name ## ComponentDepsNode,       \
                                       DEG_NODE_TYPE_ ## NAME,          \
                                       #name  " Component",             \
                                       id_recalc_tag) ;                 \
    static DepsNodeFactoryImpl<name ## ComponentDepsNode> DNTI_ ## NAME

#define DEG_COMPONENT_NODE_DECLARE_GENERIC(name)                   \
	struct name ## ComponentDepsNode : public ComponentDepsNode {  \
		DEG_COMPONENT_NODE_DECLARE;                                \
	}

#define DEG_COMPONENT_NODE_DECLARE_NO_COW_TAG_ON_UPDATE(name)      \
	struct name ## ComponentDepsNode : public ComponentDepsNode {  \
		DEG_COMPONENT_NODE_DECLARE;                                \
		virtual bool need_tag_cow_before_update() { return false; }  \
	}

DEG_COMPONENT_NODE_DECLARE_GENERIC(Animation);
DEG_COMPONENT_NODE_DECLARE_NO_COW_TAG_ON_UPDATE(BatchCache);
DEG_COMPONENT_NODE_DECLARE_GENERIC(Cache);
DEG_COMPONENT_NODE_DECLARE_GENERIC(CopyOnWrite);
DEG_COMPONENT_NODE_DECLARE_GENERIC(Geometry);
DEG_COMPONENT_NODE_DECLARE_GENERIC(LayerCollections);
DEG_COMPONENT_NODE_DECLARE_GENERIC(Parameters);
DEG_COMPONENT_NODE_DECLARE_GENERIC(Particles);
DEG_COMPONENT_NODE_DECLARE_GENERIC(Proxy);
DEG_COMPONENT_NODE_DECLARE_GENERIC(Pose);
DEG_COMPONENT_NODE_DECLARE_GENERIC(Sequencer);
DEG_COMPONENT_NODE_DECLARE_NO_COW_TAG_ON_UPDATE(Shading);
DEG_COMPONENT_NODE_DECLARE_GENERIC(ShadingParameters);
DEG_COMPONENT_NODE_DECLARE_GENERIC(Transform);
DEG_COMPONENT_NODE_DECLARE_NO_COW_TAG_ON_UPDATE(ObjectFromLayer);

/* Bone Component */
struct BoneComponentDepsNode : public ComponentDepsNode {
	void init(const ID *id, const char *subdata);

	struct bPoseChannel *pchan;     /* the bone that this component represents */

	DEG_COMPONENT_NODE_DECLARE;
};

void deg_register_component_depsnodes();

}  // namespace DEG
