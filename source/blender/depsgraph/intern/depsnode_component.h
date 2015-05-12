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

#ifndef __DEPSNODE_COMPONENT_H__
#define __DEPSNODE_COMPONENT_H__

#include "depsnode.h"

#include "depsgraph_util_hash.h"
#include "depsgraph_util_map.h"
#include "depsgraph_util_set.h"

struct ID;
struct bPoseChannel;

struct Depsgraph;
struct DepsgraphCopyContext;
struct EvaluationContext;
struct OperationDepsNode;
struct BoneComponentDepsNode;


/* ID Component - Base type for all components */
struct ComponentDepsNode : public DepsNode {
	/* Key used to look up operations within a component */
	struct OperationIDKey
	{
		eDepsOperation_Code opcode;
		string name;


		OperationIDKey() :
			opcode(DEG_OPCODE_OPERATION), name("")
		{}
		OperationIDKey(eDepsOperation_Code opcode) :
			opcode(opcode), name("")
		{}
		OperationIDKey(eDepsOperation_Code opcode, const string &name) :
		   opcode(opcode), name(name)
		{}

		string identifier() const
		{
			char codebuf[5];
			sprintf(codebuf, "%d", opcode);

			return string("OperationIDKey(") + codebuf + ", " + name + ")";
		}

		bool operator==(const OperationIDKey &other) const
		{
			return (opcode == other.opcode) && (name == other.name);
		}
	};

	/* XXX can't specialize std::hash for this purpose, because ComponentKey is a nested type ...
	 * http://stackoverflow.com/a/951245
	 */
	struct operation_key_hash {
		bool operator() (const OperationIDKey &key) const
		{
			return hash_combine(hash<int>()(key.opcode), hash<string>()(key.name));
		}
	};

	/* Typedef for container of operations */
	typedef unordered_map<OperationIDKey, OperationDepsNode *, operation_key_hash> OperationMap;


	ComponentDepsNode();
	~ComponentDepsNode();

	void init(const ID *id, const string &subdata);
	void copy(DepsgraphCopyContext *dcc, const ComponentDepsNode *src);

	string identifier() const;

	/* Find an existing operation, will throw an assert() if it does not exist. */
	OperationDepsNode *find_operation(OperationIDKey key) const;
	OperationDepsNode *find_operation(eDepsOperation_Code opcode, const string &name) const;

	/* Check operation exists and return it. */
	OperationDepsNode *has_operation(OperationIDKey key) const;
	OperationDepsNode *has_operation(eDepsOperation_Code opcode, const string &name) const;

	/**
	 * Create a new node for representing an operation and add this to graph
	 * \warning If an existing node is found, it will be modified. This helps when node may
	 * have been partially created earlier (e.g. parent ref before parent item is added)
	 *
	 * \param type: Operation node type (corresponding to context/component that it operates in)
	 * \param optype: Role that operation plays within component (i.e. where in eval process)
	 * \param op: The operation to perform
	 * \param name: Identifier for operation - used to find/locate it again
	 */
	OperationDepsNode *add_operation(eDepsOperation_Type optype, DepsEvalOperationCb op, eDepsOperation_Code opcode, const string &name);

	void remove_operation(eDepsOperation_Code opcode, const string &name);
	void clear_operations();

	void tag_update(Depsgraph *graph);

	/* Evaluation Context Management .................. */

	/* Initialize component's evaluation context used for the specified purpose */
	virtual bool eval_context_init(EvaluationContext * /*eval_ctx*/) { return false; }
	/* Free data in component's evaluation context which is used for the specified purpose
	 * NOTE: this does not free the actual context in question
	 */
	virtual void eval_context_free(EvaluationContext * /*eval_ctx*/) {}

	OperationDepsNode *get_entry_operation();
	OperationDepsNode *get_exit_operation();

	IDDepsNode *owner;

	OperationMap operations;    /* inner nodes for this component */
	OperationDepsNode *entry_operation;
	OperationDepsNode *exit_operation;

	// XXX: a poll() callback to check if component's first node can be started?
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
	void init(const ID *id, const string &subdata);

	struct bPoseChannel *pchan;     /* the bone that this component represents */

	DEG_DEPSNODE_DECLARE;
};

struct ParticlesComponentDepsNode : public ComponentDepsNode {
	DEG_DEPSNODE_DECLARE;
};

struct ShadingComponentDepsNode : public ComponentDepsNode {
	DEG_DEPSNODE_DECLARE;
};


void DEG_register_component_depsnodes();

#endif  /* __DEPSNODE_COMPONENT_H__ */
