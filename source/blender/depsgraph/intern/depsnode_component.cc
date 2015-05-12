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

#include <stdio.h>
#include <string.h>

extern "C" {
#include "BLI_utildefines.h"

#include "DNA_object_types.h"

#include "BKE_action.h"
} /* extern "C" */

#include "depsnode_component.h" /* own include */
#include "depsnode_operation.h"
#include "depsgraph_intern.h"

/* *********** */
/* Outer Nodes */

/* Standard Component Methods ============================= */

ComponentDepsNode::ComponentDepsNode() :
    entry_operation(NULL),
    exit_operation(NULL)
{
}

/* Initialize 'component' node - from pointer data given */
void ComponentDepsNode::init(const ID * /*id*/,
                             const string & /*subdata*/)
{
	/* hook up eval context? */
	// XXX: maybe this needs a special API?
}

/* Copy 'component' node */
void ComponentDepsNode::copy(DepsgraphCopyContext * /*dcc*/,
                             const ComponentDepsNode * /*src*/)
{
#if 0 // XXX: remove all this
	/* duplicate list of operation nodes */
	this->operations.clear();

	for (OperationMap::const_iterator it = src->operations.begin(); it != src->operations.end(); ++it) {
		const string &pchan_name = it->first;
		OperationDepsNode *src_op = it->second;

		/* recursive copy */
		DepsNodeFactory *factory = DEG_node_get_factory(src_op);
		OperationDepsNode *dst_op = (OperationDepsNode *)factory->copy_node(dcc, src_op);
		this->operations[pchan_name] = dst_op;

		/* fix links... */
		// ...
	}

	/* copy evaluation contexts */
	//
#endif
	BLI_assert(!"Not expected to be called");
}

/* Free 'component' node */
ComponentDepsNode::~ComponentDepsNode()
{
	clear_operations();
}

string ComponentDepsNode::identifier() const
{
	string &idname = this->owner->name;

	char typebuf[7];
	sprintf(typebuf, "(%d)", type);

	return string(typebuf) + name + " : " + idname;
}

OperationDepsNode *ComponentDepsNode::find_operation(OperationIDKey key) const
{
	OperationMap::const_iterator it = this->operations.find(key);

	if (it != this->operations.end()) {
		return it->second;
	}
	else {
		fprintf(stderr, "%s: find_operation(%s) failed\n",
		        this->identifier().c_str(), key.identifier().c_str());
		BLI_assert(!"Request for non-existing operation, should not happen");
		return NULL;
	}
}

OperationDepsNode *ComponentDepsNode::find_operation(eDepsOperation_Code opcode, const string &name) const
{
	OperationIDKey key(opcode, name);
	return find_operation(key);
}

OperationDepsNode *ComponentDepsNode::has_operation(OperationIDKey key) const
{
	OperationMap::const_iterator it = this->operations.find(key);
	if (it != this->operations.end()) {
		return it->second;
	}
	return NULL;
}

OperationDepsNode *ComponentDepsNode::has_operation(eDepsOperation_Code opcode,
                                                    const string &name) const
{
	OperationIDKey key(opcode, name);
	return has_operation(key);
}

OperationDepsNode *ComponentDepsNode::add_operation(eDepsOperation_Type optype, DepsEvalOperationCb op, eDepsOperation_Code opcode, const string &name)
{
	OperationDepsNode *op_node = has_operation(opcode, name);
	if (!op_node) {
		DepsNodeFactory *factory = DEG_get_node_factory(DEPSNODE_TYPE_OPERATION);
		op_node = (OperationDepsNode *)factory->create_node(this->owner->id, "", name);

		/* register opnode in this component's operation set */
		OperationIDKey key(opcode, name);
		this->operations[key] = op_node;

		/* set as entry/exit node of component (if appropriate) */
		if (optype == DEPSOP_TYPE_INIT) {
			BLI_assert(this->entry_operation == NULL);
			this->entry_operation = op_node;
		}
		else if (optype == DEPSOP_TYPE_POST) {
			// XXX: review whether DEPSOP_TYPE_OUT is better than DEPSOP_TYPE_POST, or maybe have both?
			BLI_assert(this->exit_operation == NULL);
			this->exit_operation = op_node;
		}

		/* set backlink */
		op_node->owner = this;
	}
	else {
		fprintf(stderr, "add_operation: Operation already exists - %s has %s at %p\n",
		        this->identifier().c_str(), op_node->identifier().c_str(), op_node);
		BLI_assert(!"Should not happen!");
	}

	/* attach extra data */
	op_node->evaluate = op;
	op_node->optype = optype;
	op_node->opcode = opcode;
	op_node->name = name;

	return op_node;
}

void ComponentDepsNode::remove_operation(eDepsOperation_Code opcode, const string &name)
{
	OperationDepsNode *op_node = find_operation(opcode, name);
	if (op_node) {
		/* unregister */
		this->operations.erase(OperationIDKey(opcode, name));
		OBJECT_GUARDED_DELETE(op_node, OperationDepsNode);
	}
}

void ComponentDepsNode::clear_operations()
{
	for (OperationMap::const_iterator it = operations.begin(); it != operations.end(); ++it) {
		OperationDepsNode *op_node = it->second;
		OBJECT_GUARDED_DELETE(op_node, OperationDepsNode);
	}
	operations.clear();
}

void ComponentDepsNode::tag_update(Depsgraph *graph)
{
	OperationDepsNode *entry_op = get_entry_operation();
	if (entry_op != NULL && entry_op->flag & DEPSOP_FLAG_NEEDS_UPDATE) {
		return;
	}
	for (OperationMap::const_iterator it = operations.begin(); it != operations.end(); ++it) {
		OperationDepsNode *op_node = it->second;
		op_node->tag_update(graph);
	}
}

OperationDepsNode *ComponentDepsNode::get_entry_operation()
{
	if (entry_operation)
		return entry_operation;
	else if (operations.size() == 1)
		return operations.begin()->second;
	return NULL;
}

OperationDepsNode *ComponentDepsNode::get_exit_operation()
{
	if (exit_operation)
		return exit_operation;
	else if (operations.size() == 1)
		return operations.begin()->second;
	return NULL;
}

/* Parameter Component Defines ============================ */

DEG_DEPSNODE_DEFINE(ParametersComponentDepsNode, DEPSNODE_TYPE_PARAMETERS, "Parameters Component");
static DepsNodeFactoryImpl<ParametersComponentDepsNode> DNTI_PARAMETERS;

/* Animation Component Defines ============================ */

DEG_DEPSNODE_DEFINE(AnimationComponentDepsNode, DEPSNODE_TYPE_ANIMATION, "Animation Component");
static DepsNodeFactoryImpl<AnimationComponentDepsNode> DNTI_ANIMATION;

/* Transform Component Defines ============================ */

DEG_DEPSNODE_DEFINE(TransformComponentDepsNode, DEPSNODE_TYPE_TRANSFORM, "Transform Component");
static DepsNodeFactoryImpl<TransformComponentDepsNode> DNTI_TRANSFORM;

/* Proxy Component Defines ================================ */

DEG_DEPSNODE_DEFINE(ProxyComponentDepsNode, DEPSNODE_TYPE_PROXY, "Proxy Component");
static DepsNodeFactoryImpl<ProxyComponentDepsNode> DNTI_PROXY;

/* Geometry Component Defines ============================= */

DEG_DEPSNODE_DEFINE(GeometryComponentDepsNode, DEPSNODE_TYPE_GEOMETRY, "Geometry Component");
static DepsNodeFactoryImpl<GeometryComponentDepsNode> DNTI_GEOMETRY;

/* Sequencer Component Defines ============================ */

DEG_DEPSNODE_DEFINE(SequencerComponentDepsNode, DEPSNODE_TYPE_SEQUENCER, "Sequencer Component");
static DepsNodeFactoryImpl<SequencerComponentDepsNode> DNTI_SEQUENCER;

/* Pose Component ========================================= */

DEG_DEPSNODE_DEFINE(PoseComponentDepsNode, DEPSNODE_TYPE_EVAL_POSE, "Pose Eval Component");
static DepsNodeFactoryImpl<PoseComponentDepsNode> DNTI_EVAL_POSE;

/* Bone Component ========================================= */

/* Initialize 'bone component' node - from pointer data given */
void BoneComponentDepsNode::init(const ID *id, const string &subdata)
{
	/* generic component-node... */
	ComponentDepsNode::init(id, subdata);

	/* name of component comes is bone name */
	/* TODO(sergey): This sets name to an empty string because subdata is
	 * empty. Is it a bug?
	 */
	//this->name = subdata;

	/* bone-specific node data */
	Object *ob = (Object *)id;
	this->pchan = BKE_pose_channel_find_name(ob->pose, subdata.c_str());
}

DEG_DEPSNODE_DEFINE(BoneComponentDepsNode, DEPSNODE_TYPE_BONE, "Bone Component");
static DepsNodeFactoryImpl<BoneComponentDepsNode> DNTI_BONE;

/* Particles Component Defines ============================ */

DEG_DEPSNODE_DEFINE(ParticlesComponentDepsNode, DEPSNODE_TYPE_EVAL_PARTICLES, "Particles Component");
static DepsNodeFactoryImpl<ParticlesComponentDepsNode> DNTI_EVAL_PARTICLES;

/* Shading Component Defines ============================ */

DEG_DEPSNODE_DEFINE(ShadingComponentDepsNode, DEPSNODE_TYPE_SHADING, "Shading Component");
static DepsNodeFactoryImpl<ShadingComponentDepsNode> DNTI_SHADING;


/* Node Types Register =================================== */

void DEG_register_component_depsnodes()
{
	DEG_register_node_typeinfo(&DNTI_PARAMETERS);
	DEG_register_node_typeinfo(&DNTI_PROXY);
	DEG_register_node_typeinfo(&DNTI_ANIMATION);
	DEG_register_node_typeinfo(&DNTI_TRANSFORM);
	DEG_register_node_typeinfo(&DNTI_GEOMETRY);
	DEG_register_node_typeinfo(&DNTI_SEQUENCER);

	DEG_register_node_typeinfo(&DNTI_EVAL_POSE);
	DEG_register_node_typeinfo(&DNTI_BONE);

	DEG_register_node_typeinfo(&DNTI_EVAL_PARTICLES);
	DEG_register_node_typeinfo(&DNTI_SHADING);
}
