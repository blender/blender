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

/** \file blender/depsgraph/intern/nodes/deg_node_component.cc
 *  \ingroup depsgraph
 */

#include "intern/nodes/deg_node_component.h"

#include <stdio.h>

extern "C" {
#include "BLI_utildefines.h"

#include "DNA_object_types.h"

#include "BKE_action.h"
} /* extern "C" */

#include "intern/nodes/deg_node_operation.h"
#include "intern/depsgraph_intern.h"
#include "util/deg_util_foreach.h"
#include "util/deg_util_hash.h"

namespace DEG {

/* *********** */
/* Outer Nodes */

/* Standard Component Methods ============================= */

static unsigned int comp_node_hash_key(const void *key_v)
{
	const ComponentDepsNode::OperationIDKey *key =
	        reinterpret_cast<const ComponentDepsNode::OperationIDKey *>(key_v);
	return hash_combine(BLI_ghashutil_uinthash(key->opcode),
	                    BLI_ghashutil_strhash_p(key->name.c_str()));
}

static bool comp_node_hash_key_cmp(const void *a, const void *b)
{
	const ComponentDepsNode::OperationIDKey *key_a =
	        reinterpret_cast<const ComponentDepsNode::OperationIDKey *>(a);
	const ComponentDepsNode::OperationIDKey *key_b =
	        reinterpret_cast<const ComponentDepsNode::OperationIDKey *>(b);
	return !(*key_a == *key_b);
}

static void comp_node_hash_key_free(void *key_v)
{
	typedef ComponentDepsNode::OperationIDKey OperationIDKey;
	OperationIDKey *key = reinterpret_cast<OperationIDKey *>(key_v);
	OBJECT_GUARDED_DELETE(key, OperationIDKey);
}

static void comp_node_hash_value_free(void *value_v)
{
	OperationDepsNode *op_node = reinterpret_cast<OperationDepsNode *>(value_v);
	OBJECT_GUARDED_DELETE(op_node, OperationDepsNode);
}

ComponentDepsNode::ComponentDepsNode() :
    entry_operation(NULL),
    exit_operation(NULL),
    layers(0)
{
	operations_map = BLI_ghash_new(comp_node_hash_key,
	                               comp_node_hash_key_cmp,
	                               "Depsgraph id hash");
}

/* Initialize 'component' node - from pointer data given */
void ComponentDepsNode::init(const ID * /*id*/,
                             const string & /*subdata*/)
{
	/* hook up eval context? */
	// XXX: maybe this needs a special API?
}

/* Free 'component' node */
ComponentDepsNode::~ComponentDepsNode()
{
	clear_operations();
	if (operations_map != NULL) {
		BLI_ghash_free(operations_map,
		               comp_node_hash_key_free,
		               comp_node_hash_value_free);
	}
}

string ComponentDepsNode::identifier() const
{
	string &idname = this->owner->name;

	char typebuf[16];
	sprintf(typebuf, "(%d)", type);

	char layers[16];
	sprintf(layers, "%u", this->layers);

	return string(typebuf) + name + " : " + idname + " (Layers: " + layers + ")";
}

OperationDepsNode *ComponentDepsNode::find_operation(OperationIDKey key) const
{
	OperationDepsNode *node = reinterpret_cast<OperationDepsNode *>(BLI_ghash_lookup(operations_map, &key));
	if (node != NULL) {
		return node;
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
	return reinterpret_cast<OperationDepsNode *>(BLI_ghash_lookup(operations_map, &key));
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
		DepsNodeFactory *factory = deg_get_node_factory(DEPSNODE_TYPE_OPERATION);
		op_node = (OperationDepsNode *)factory->create_node(this->owner->id, "", name);

		/* register opnode in this component's operation set */
		OperationIDKey *key = OBJECT_GUARDED_NEW(OperationIDKey, opcode, name);
		BLI_ghash_insert(operations_map, key, op_node);

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
	/* unregister */
	OperationIDKey key(opcode, name);
	BLI_ghash_remove(operations_map,
	                 &key,
	                 comp_node_hash_key_free,
	                 comp_node_hash_key_free);
}

void ComponentDepsNode::clear_operations()
{
	if (operations_map != NULL) {
		BLI_ghash_clear(operations_map,
		                comp_node_hash_key_free,
		                comp_node_hash_value_free);
	}
	foreach (OperationDepsNode *op_node, operations) {
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
	foreach (OperationDepsNode *op_node, operations) {
		op_node->tag_update(graph);
	}
	// It is possible that tag happens before finalization.
	if (operations_map != NULL) {
		GHASH_FOREACH_BEGIN(OperationDepsNode *, op_node, operations_map)
		{
			op_node->tag_update(graph);
		}
		GHASH_FOREACH_END();
	}
}

OperationDepsNode *ComponentDepsNode::get_entry_operation()
{
	if (entry_operation) {
		return entry_operation;
	}
	else if (operations_map != NULL && BLI_ghash_size(operations_map) == 1) {
		OperationDepsNode *op_node = NULL;
		/* TODO(sergey): This is somewhat slow. */
		GHASH_FOREACH_BEGIN(OperationDepsNode *, tmp, operations_map)
		{
			op_node = tmp;
		}
		GHASH_FOREACH_END();
		/* Cache for the subsequent usage. */
		entry_operation = op_node;
		return op_node;
	}
	else if (operations.size() == 1) {
		return operations[0];
	}
	return NULL;
}

OperationDepsNode *ComponentDepsNode::get_exit_operation()
{
	if (exit_operation) {
		return exit_operation;
	}
	else if (operations_map != NULL && BLI_ghash_size(operations_map) == 1) {
		OperationDepsNode *op_node = NULL;
		/* TODO(sergey): This is somewhat slow. */
		GHASH_FOREACH_BEGIN(OperationDepsNode *, tmp, operations_map)
		{
			op_node = tmp;
		}
		GHASH_FOREACH_END();
		/* Cache for the subsequent usage. */
		exit_operation = op_node;
		return op_node;
	}
	else if (operations.size() == 1) {
		return operations[0];
	}
	return NULL;
}

void ComponentDepsNode::finalize_build()
{
	operations.reserve(BLI_ghash_size(operations_map));
	GHASH_FOREACH_BEGIN(OperationDepsNode *, op_node, operations_map)
	{
		operations.push_back(op_node);
	}
	GHASH_FOREACH_END();
	BLI_ghash_free(operations_map,
	               comp_node_hash_key_free,
	               NULL);
	operations_map = NULL;
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

/* Cache Component Defines ============================ */

DEG_DEPSNODE_DEFINE(CacheComponentDepsNode, DEPSNODE_TYPE_CACHE, "Cache Component");
static DepsNodeFactoryImpl<CacheComponentDepsNode> DNTI_CACHE;


/* Node Types Register =================================== */

void deg_register_component_depsnodes()
{
	deg_register_node_typeinfo(&DNTI_PARAMETERS);
	deg_register_node_typeinfo(&DNTI_PROXY);
	deg_register_node_typeinfo(&DNTI_ANIMATION);
	deg_register_node_typeinfo(&DNTI_TRANSFORM);
	deg_register_node_typeinfo(&DNTI_GEOMETRY);
	deg_register_node_typeinfo(&DNTI_SEQUENCER);

	deg_register_node_typeinfo(&DNTI_EVAL_POSE);
	deg_register_node_typeinfo(&DNTI_BONE);

	deg_register_node_typeinfo(&DNTI_EVAL_PARTICLES);
	deg_register_node_typeinfo(&DNTI_SHADING);

	deg_register_node_typeinfo(&DNTI_CACHE);
}

}  // namespace DEG
