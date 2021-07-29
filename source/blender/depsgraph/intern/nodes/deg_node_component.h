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

struct EvaluationContext;

namespace DEG {

struct Depsgraph;
struct OperationDepsNode;
struct BoneComponentDepsNode;

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

	/* Find an existing operation, will throw an assert() if it does not exist. */
	OperationDepsNode *find_operation(OperationIDKey key) const;
	OperationDepsNode *find_operation(eDepsOperation_Code opcode,
	                                  const char *name,
	                                  int name_tag) const;

	/* Check operation exists and return it. */
	OperationDepsNode *has_operation(OperationIDKey key) const;
	OperationDepsNode *has_operation(eDepsOperation_Code opcode,
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

	/* Evaluation Context Management .................. */

	/* Initialize component's evaluation context used for the specified
	 * purpose.
	 */
	virtual bool eval_context_init(EvaluationContext * /*eval_ctx*/) { return false; }
	/* Free data in component's evaluation context which is used for
	 * the specified purpose
	 *
	 * NOTE: this does not free the actual context in question
	 */
	virtual void eval_context_free(EvaluationContext * /*eval_ctx*/) {}

	OperationDepsNode *get_entry_operation();
	OperationDepsNode *get_exit_operation();

	void finalize_build();

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

	// XXX: a poll() callback to check if component's first node can be started?

	/* Temporary bitmask, used during graph construction. */
	unsigned int layers;
};

/* ---------------------------------------- */

struct ParametersComponentDepsNode : public ComponentDepsNode {
	DEG_DEPSNODE_DECLARE;
};

struct AnimationComponentDepsNode : public ComponentDepsNode {
	DEG_DEPSNODE_DECLARE;
};

struct TransformComponentDepsNode : public ComponentDepsNode {
	DEG_DEPSNODE_DECLARE;
};

struct ProxyComponentDepsNode : public ComponentDepsNode {
	DEG_DEPSNODE_DECLARE;
};

struct GeometryComponentDepsNode : public ComponentDepsNode {
	DEG_DEPSNODE_DECLARE;
};

struct SequencerComponentDepsNode : public ComponentDepsNode {
	DEG_DEPSNODE_DECLARE;
};

struct PoseComponentDepsNode : public ComponentDepsNode {
	DEG_DEPSNODE_DECLARE;
};

/* Bone Component */
struct BoneComponentDepsNode : public ComponentDepsNode {
	void init(const ID *id, const char *subdata);

	struct bPoseChannel *pchan;     /* the bone that this component represents */

	DEG_DEPSNODE_DECLARE;
};

struct ParticlesComponentDepsNode : public ComponentDepsNode {
	DEG_DEPSNODE_DECLARE;
};

struct ShadingComponentDepsNode : public ComponentDepsNode {
	DEG_DEPSNODE_DECLARE;
};

struct CacheComponentDepsNode : public ComponentDepsNode {
	DEG_DEPSNODE_DECLARE;
};


void deg_register_component_depsnodes();

}  // namespace DEG
