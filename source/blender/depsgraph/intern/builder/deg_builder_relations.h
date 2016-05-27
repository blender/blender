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
 * Original Author: Lukas Toenne
 * Contributor(s): None Yet
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/depsgraph/intern/builder/deg_builder_relations.h
 *  \ingroup depsgraph
 */

#pragma once

#include <cstdio>

#include "intern/depsgraph_types.h"

#include "DNA_ID.h"

#include "RNA_access.h"
#include "RNA_types.h"

#include "BLI_utildefines.h"
#include "BLI_string.h"

#include "intern/depsgraph_types.h"
#include "intern/nodes/deg_node.h"
#include "intern/nodes/deg_node_operation.h"

struct Base;
struct bGPdata;
struct ListBase;
struct GHash;
struct ID;
struct FCurve;
struct Group;
struct Key;
struct Main;
struct Material;
struct MTex;
struct bNodeTree;
struct Object;
struct bPoseChannel;
struct bConstraint;
struct Scene;
struct Tex;
struct World;

struct PropertyRNA;

namespace DEG {

struct Depsgraph;
struct DepsNode;
struct DepsNodeHandle;
struct RootDepsNode;
struct SubgraphDepsNode;
struct IDDepsNode;
struct TimeSourceDepsNode;
struct ComponentDepsNode;
struct OperationDepsNode;
struct RootPChanMap;

struct RootKey
{
	RootKey() {}
};

struct TimeSourceKey
{
	TimeSourceKey() : id(NULL) {}
	TimeSourceKey(ID *id) : id(id) {}

	string identifier() const
	{
		return string("TimeSourceKey");
	}

	ID *id;
};

struct ComponentKey
{
	ComponentKey() :
	    id(NULL), type(DEPSNODE_TYPE_UNDEFINED), name("")
	{}
	ComponentKey(ID *id, eDepsNode_Type type, const string &name = "") :
	    id(id), type(type), name(name)
	{}

	string identifier() const
	{
		const char *idname = (id) ? id->name : "<None>";

		char typebuf[5];
		BLI_snprintf(typebuf, sizeof(typebuf), "%d", type);

		return string("ComponentKey(") + idname + ", " + typebuf + ", '" + name + "')";
	}

	ID *id;
	eDepsNode_Type type;
	string name;
};

struct OperationKey
{
	OperationKey() :
	    id(NULL), component_type(DEPSNODE_TYPE_UNDEFINED), component_name(""), opcode(DEG_OPCODE_OPERATION), name("")
	{}

	OperationKey(ID *id, eDepsNode_Type component_type, const string &name) :
	    id(id), component_type(component_type), component_name(""), opcode(DEG_OPCODE_OPERATION), name(name)
	{}
	OperationKey(ID *id, eDepsNode_Type component_type, const string &component_name, const string &name) :
	    id(id), component_type(component_type), component_name(component_name), opcode(DEG_OPCODE_OPERATION), name(name)
	{}

	OperationKey(ID *id, eDepsNode_Type component_type, eDepsOperation_Code opcode) :
	    id(id), component_type(component_type), component_name(""), opcode(opcode), name("")
	{}
	OperationKey(ID *id, eDepsNode_Type component_type, const string &component_name, eDepsOperation_Code opcode) :
	    id(id), component_type(component_type), component_name(component_name), opcode(opcode), name("")
	{}

	OperationKey(ID *id, eDepsNode_Type component_type, eDepsOperation_Code opcode, const string &name) :
	    id(id), component_type(component_type), component_name(""), opcode(opcode), name(name)
	{}
	OperationKey(ID *id, eDepsNode_Type component_type, const string &component_name, eDepsOperation_Code opcode, const string &name) :
	    id(id), component_type(component_type), component_name(component_name), opcode(opcode), name(name)
	{}

	string identifier() const
	{
		char typebuf[5];
		BLI_snprintf(typebuf, sizeof(typebuf), "%d", component_type);

		return string("OperationKey(") + "t: " + typebuf + ", cn: '" + component_name + "', c: " + DEG_OPNAMES[opcode] + ", n: '" + name + "')";
	}


	ID *id;
	eDepsNode_Type component_type;
	string component_name;
	eDepsOperation_Code opcode;
	string name;
};

struct RNAPathKey
{
	// Note: see depsgraph_build.cpp for implementation
	RNAPathKey(ID *id, const char *path);

	RNAPathKey(ID *id, const PointerRNA &ptr, PropertyRNA *prop) :
	    id(id), ptr(ptr), prop(prop)
	{}

	string identifier() const
	{
		const char *id_name   = (id) ?  id->name : "<No ID>";
		const char *prop_name = (prop) ? RNA_property_identifier(prop) : "<No Prop>";

		return string("RnaPathKey(") + "id: " + id_name + ", prop: " + prop_name +  "')";
	}


	ID *id;
	PointerRNA ptr;
	PropertyRNA *prop;
};

struct DepsgraphRelationBuilder
{
	DepsgraphRelationBuilder(Depsgraph *graph);

	template <typename KeyFrom, typename KeyTo>
	void add_relation(const KeyFrom& key_from,
	                  const KeyTo& key_to,
	                  eDepsRelation_Type type,
	                  const char *description);

	template <typename KeyTo>
	void add_relation(const TimeSourceKey& key_from,
	                  const KeyTo& key_to,
	                  eDepsRelation_Type type,
	                  const char *description);

	template <typename KeyType>
	void add_node_handle_relation(const KeyType& key_from,
	                              const DepsNodeHandle *handle,
	                              eDepsRelation_Type type,
	                              const char *description);

	void build_scene(Main *bmain, Scene *scene);
	void build_group(Main *bmain, Scene *scene, Object *object, Group *group);
	void build_object(Main *bmain, Scene *scene, Object *ob);
	void build_object_parent(Object *ob);
	void build_constraints(Scene *scene, ID *id,
	                       eDepsNode_Type component_type,
	                       const char *component_subdata,
	                       ListBase *constraints,
	                       RootPChanMap *root_map);
	void build_animdata(ID *id);
	void build_driver(ID *id, FCurve *fcurve);
	void build_world(World *world);
	void build_rigidbody(Scene *scene);
	void build_particles(Scene *scene, Object *ob);
	void build_ik_pose(Object *ob,
	                   bPoseChannel *pchan,
	                   bConstraint *con,
	                   RootPChanMap *root_map);
	void build_splineik_pose(Object *ob,
	                         bPoseChannel *pchan,
	                         bConstraint *con,
	                         RootPChanMap *root_map);
	void build_rig(Scene *scene, Object *ob);
	void build_proxy_rig(Object *ob);
	void build_shapekeys(ID *obdata, Key *key);
	void build_obdata_geom(Main *bmain, Scene *scene, Object *ob);
	void build_camera(Object *ob);
	void build_lamp(Object *ob);
	void build_nodetree(ID *owner, bNodeTree *ntree);
	void build_material(ID *owner, Material *ma);
	void build_texture(ID *owner, Tex *tex);
	void build_texture_stack(ID *owner, MTex **texture_stack);
	void build_compositor(Scene *scene);
	void build_gpencil(ID *owner, bGPdata *gpd);

	template <typename KeyType>
	OperationDepsNode *find_operation_node(const KeyType &key);

protected:
	RootDepsNode *find_node(const RootKey &key) const;
	TimeSourceDepsNode *find_node(const TimeSourceKey &key) const;
	ComponentDepsNode *find_node(const ComponentKey &key) const;
	OperationDepsNode *find_node(const OperationKey &key) const;
	DepsNode *find_node(const RNAPathKey &key) const;
	OperationDepsNode *has_node(const OperationKey &key) const;

	void add_time_relation(TimeSourceDepsNode *timesrc,
	                       DepsNode *node_to,
	                       const char *description);
	void add_operation_relation(OperationDepsNode *node_from,
	                            OperationDepsNode *node_to,
	                            eDepsRelation_Type type,
	                            const char *description);

	template <typename KeyType>
	DepsNodeHandle create_node_handle(const KeyType& key,
	                                  const string& default_name = "");

	bool needs_animdata_node(ID *id);

private:
	Depsgraph *m_graph;
};

struct DepsNodeHandle
{
	DepsNodeHandle(DepsgraphRelationBuilder *builder, OperationDepsNode *node, const string &default_name = "") :
	    builder(builder),
	    node(node),
	    default_name(default_name)
	{
		BLI_assert(node != NULL);
	}

	DepsgraphRelationBuilder *builder;
	OperationDepsNode *node;
	const string &default_name;
};

/* Utilities for Builders ----------------------------------------------------- */

template <typename KeyType>
OperationDepsNode *DepsgraphRelationBuilder::find_operation_node(const KeyType& key) {
	DepsNode *node = find_node(key);
	return node != NULL ? node->get_exit_operation() : NULL;
}

template <typename KeyFrom, typename KeyTo>
void DepsgraphRelationBuilder::add_relation(const KeyFrom &key_from,
                                            const KeyTo &key_to,
                                            eDepsRelation_Type type,
                                            const char *description)
{
	DepsNode *node_from = find_node(key_from);
	DepsNode *node_to = find_node(key_to);
	OperationDepsNode *op_from = node_from ? node_from->get_exit_operation() : NULL;
	OperationDepsNode *op_to = node_to ? node_to->get_entry_operation() : NULL;
	if (op_from && op_to) {
		add_operation_relation(op_from, op_to, type, description);
	}
	else {
		if (!op_from) {
			/* XXX TODO handle as error or report if needed */
			fprintf(stderr, "add_relation(%d, %s) - Could not find op_from (%s)\n",
			        type, description, key_from.identifier().c_str());
		}
		else {
			fprintf(stderr, "add_relation(%d, %s) - Failed, but op_from (%s) was ok\n",
			        type, description, key_from.identifier().c_str());
		}
		if (!op_to) {
			/* XXX TODO handle as error or report if needed */
			fprintf(stderr, "add_relation(%d, %s) - Could not find op_to (%s)\n",
			        type, description, key_to.identifier().c_str());
		}
		else {
			fprintf(stderr, "add_relation(%d, %s) - Failed, but op_to (%s) was ok\n",
			        type, description, key_to.identifier().c_str());
		}
	}
}

template <typename KeyTo>
void DepsgraphRelationBuilder::add_relation(const TimeSourceKey &key_from,
                                            const KeyTo &key_to,
                                            eDepsRelation_Type type,
                                            const char *description)
{
	(void)type;  /* Ignored in release builds. */
	BLI_assert(type == DEPSREL_TYPE_TIME);
	TimeSourceDepsNode *time_from = find_node(key_from);
	DepsNode *node_to = find_node(key_to);
	OperationDepsNode *op_to = node_to ? node_to->get_entry_operation() : NULL;
	if (time_from && op_to) {
		add_time_relation(time_from, op_to, description);
	}
	else {
	}
}

template <typename KeyType>
void DepsgraphRelationBuilder::add_node_handle_relation(
        const KeyType &key_from,
        const DepsNodeHandle *handle,
        eDepsRelation_Type type,
        const char *description)
{
	DepsNode *node_from = find_node(key_from);
	OperationDepsNode *op_from = node_from ? node_from->get_exit_operation() : NULL;
	OperationDepsNode *op_to = handle->node->get_entry_operation();
	if (op_from && op_to) {
		add_operation_relation(op_from, op_to, type, description);
	}
	else {
		if (!op_from) {
			/* XXX TODO handle as error or report if needed */
		}
		if (!op_to) {
			/* XXX TODO handle as error or report if needed */
		}
	}
}

template <typename KeyType>
DepsNodeHandle DepsgraphRelationBuilder::create_node_handle(
        const KeyType &key,
        const string &default_name)
{
	return DepsNodeHandle(this, find_node(key), default_name);
}

}  // namespace DEG
