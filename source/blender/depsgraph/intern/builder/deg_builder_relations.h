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

#include "intern/nodes/deg_node.h"
#include "intern/nodes/deg_node_operation.h"

struct Base;
struct bGPdata;
struct CacheFile;
struct ListBase;
struct GHash;
struct ID;
struct FCurve;
struct Group;
struct Key;
struct LayerCollection;
struct Main;
struct Mask;
struct Material;
struct MTex;
struct ModifierData;
struct MovieClip;
struct bNodeTree;
struct Object;
struct bPoseChannel;
struct bConstraint;
struct Scene;
struct ViewLayer;
struct Tex;
struct World;
struct EffectorWeights;
struct ParticleSystem;
struct ParticleSettings;

struct PropertyRNA;

namespace DEG {

struct Depsgraph;
struct DepsNode;
struct DepsNodeHandle;
struct RootDepsNode;
struct IDDepsNode;
struct TimeSourceDepsNode;
struct ComponentDepsNode;
struct OperationDepsNode;
struct RootPChanMap;

struct TimeSourceKey
{
	TimeSourceKey();
	TimeSourceKey(ID *id);

	string identifier() const;

	ID *id;
};

struct ComponentKey
{
	ComponentKey();
	ComponentKey(ID *id, eDepsNode_Type type, const char *name = "");

	string identifier() const;

	ID *id;
	eDepsNode_Type type;
	const char *name;
};

struct OperationKey
{
	OperationKey();
	OperationKey(ID *id,
	             eDepsNode_Type component_type,
	             const char *name,
	             int name_tag = -1);
	OperationKey(ID *id,
	             eDepsNode_Type component_type,
	             const char *component_name,
	             const char *name,
	             int name_tag);

	OperationKey(ID *id,
	             eDepsNode_Type component_type,
	             eDepsOperation_Code opcode);
	OperationKey(ID *id,
	             eDepsNode_Type component_type,
	             const char *component_name,
	             eDepsOperation_Code opcode);

	OperationKey(ID *id,
	             eDepsNode_Type component_type,
	             eDepsOperation_Code opcode,
	             const char *name,
	             int name_tag = -1);
	OperationKey(ID *id,
	             eDepsNode_Type component_type,
	             const char *component_name,
	             eDepsOperation_Code opcode,
	             const char *name,
	             int name_tag = -1);

	string identifier() const;

	ID *id;
	eDepsNode_Type component_type;
	const char *component_name;
	eDepsOperation_Code opcode;
	const char *name;
	int name_tag;
};

struct RNAPathKey
{
	/* NOTE: see depsgraph_build.cpp for implementation */
	RNAPathKey(ID *id, const char *path);

	RNAPathKey(ID *id, const PointerRNA &ptr, PropertyRNA *prop);

	string identifier() const;

	ID *id;
	PointerRNA ptr;
	PropertyRNA *prop;
};

struct DepsgraphRelationBuilder
{
	DepsgraphRelationBuilder(Main *bmain, Depsgraph *graph);

	void begin_build();

	template <typename KeyFrom, typename KeyTo>
	void add_relation(const KeyFrom& key_from,
	                  const KeyTo& key_to,
	                  const char *description);

	template <typename KeyTo>
	void add_relation(const TimeSourceKey& key_from,
	                  const KeyTo& key_to,
	                  const char *description);

	template <typename KeyType>
	void add_node_handle_relation(const KeyType& key_from,
	                              const DepsNodeHandle *handle,
	                              const char *description);

	void build_view_layer(Scene *scene, ViewLayer *view_layer);
	void build_group(Object *object, Group *group);
	void build_object(Base *base, Object *object);
	void build_object_flags(Base *base, Object *object);
	void build_object_data(Object *object);
	void build_object_parent(Object *object);
	void build_constraints(ID *id,
	                       eDepsNode_Type component_type,
	                       const char *component_subdata,
	                       ListBase *constraints,
	                       RootPChanMap *root_map);
	void build_animdata(ID *id);
	void build_driver(ID *id, FCurve *fcurve);
	void build_world(World *world);
	void build_rigidbody(Scene *scene);
	void build_particles(Object *object);
	void build_particle_settings(ParticleSettings *part);
	void build_cloth(Object *object, ModifierData *md);
	void build_ik_pose(Object *object,
	                   bPoseChannel *pchan,
	                   bConstraint *con,
	                   RootPChanMap *root_map);
	void build_splineik_pose(Object *object,
	                         bPoseChannel *pchan,
	                         bConstraint *con,
	                         RootPChanMap *root_map);
	void build_rig(Object *object);
	void build_proxy_rig(Object *object);
	void build_shapekeys(ID *obdata, Key *key);
	void build_obdata_geom(Object *object);
	void build_camera(Object *object);
	void build_lamp(Object *object);
	void build_nodetree(bNodeTree *ntree);
	void build_material(Material *ma);
	void build_texture(Tex *tex);
	void build_texture_stack(MTex **texture_stack);
	void build_compositor(Scene *scene);
	void build_gpencil(bGPdata *gpd);
	void build_cachefile(CacheFile *cache_file);
	void build_mask(Mask *mask);
	void build_movieclip(MovieClip *clip);
	void build_lightprobe(Object *object);

	void add_collision_relations(const OperationKey &key,
	                             Scene *scene,
	                             Object *object,
	                             Group *group,
	                             bool dupli,
	                             const char *name);
	void add_forcefield_relations(const OperationKey &key,
	                              Scene *scene,
	                              Object *object,
	                              ParticleSystem *psys,
	                              EffectorWeights *eff,
	                              bool add_absorption, const char *name);

	struct LayerCollectionState {
		int index;
		OperationKey init_key;
		OperationKey done_key;
		OperationKey prev_key;
	};
	void build_layer_collection(ID *owner_id,
	                            LayerCollection *layer_collection,
	                            LayerCollectionState *state);
	void build_layer_collections(ID *owner_id,
	                             ListBase *layer_collections,
	                             LayerCollectionState *state);
	void build_view_layer_collections(struct ID *owner_id, ViewLayer *view_layer);

	void build_copy_on_write_relations();
	void build_copy_on_write_relations(IDDepsNode *id_node);

	template <typename KeyType>
	OperationDepsNode *find_operation_node(const KeyType &key);

	Depsgraph *getGraph();

protected:
	TimeSourceDepsNode *get_node(const TimeSourceKey &key) const;
	ComponentDepsNode *get_node(const ComponentKey &key) const;
	OperationDepsNode *get_node(const OperationKey &key) const;
	DepsNode *get_node(const RNAPathKey &key) const;

	OperationDepsNode *find_node(const OperationKey &key) const;
	bool has_node(const OperationKey &key) const;

	void add_time_relation(TimeSourceDepsNode *timesrc,
	                       DepsNode *node_to,
	                       const char *description);
	void add_operation_relation(OperationDepsNode *node_from,
	                            OperationDepsNode *node_to,
	                            const char *description);

	template <typename KeyType>
	DepsNodeHandle create_node_handle(const KeyType& key,
	                                  const char *default_name = "");

	bool needs_animdata_node(ID *id);

private:
	/* State which never changes, same for the whole builder time. */
	Main *bmain_;
	Depsgraph *graph_;

	/* State which demotes currently built entities. */
	Scene *scene_;
};

struct DepsNodeHandle
{
	DepsNodeHandle(DepsgraphRelationBuilder *builder, OperationDepsNode *node, const char *default_name = "") :
	    builder(builder),
	    node(node),
	    default_name(default_name)
	{
		BLI_assert(node != NULL);
	}

	DepsgraphRelationBuilder *builder;
	OperationDepsNode *node;
	const char *default_name;
};

/* Utilities for Builders ----------------------------------------------------- */

template <typename KeyType>
OperationDepsNode *DepsgraphRelationBuilder::find_operation_node(const KeyType& key)
{
	DepsNode *node = get_node(key);
	return node != NULL ? node->get_exit_operation() : NULL;
}

template <typename KeyFrom, typename KeyTo>
void DepsgraphRelationBuilder::add_relation(const KeyFrom &key_from,
                                            const KeyTo &key_to,
                                            const char *description)
{
	DepsNode *node_from = get_node(key_from);
	DepsNode *node_to = get_node(key_to);
	OperationDepsNode *op_from = node_from ? node_from->get_exit_operation() : NULL;
	OperationDepsNode *op_to = node_to ? node_to->get_entry_operation() : NULL;
	if (op_from && op_to) {
		add_operation_relation(op_from, op_to, description);
	}
	else {
		if (!op_from) {
			/* XXX TODO handle as error or report if needed */
			fprintf(stderr, "add_relation(%s) - Could not find op_from (%s)\n",
			        description, key_from.identifier().c_str());
		}
		else {
			fprintf(stderr, "add_relation(%s) - Failed, but op_from (%s) was ok\n",
			        description, key_from.identifier().c_str());
		}
		if (!op_to) {
			/* XXX TODO handle as error or report if needed */
			fprintf(stderr, "add_relation(%s) - Could not find op_to (%s)\n",
			        description, key_to.identifier().c_str());
		}
		else {
			fprintf(stderr, "add_relation(%s) - Failed, but op_to (%s) was ok\n",
			        description, key_to.identifier().c_str());
		}
	}
}

template <typename KeyTo>
void DepsgraphRelationBuilder::add_relation(const TimeSourceKey &key_from,
                                            const KeyTo &key_to,
                                            const char *description)
{
	TimeSourceDepsNode *time_from = get_node(key_from);
	DepsNode *node_to = get_node(key_to);
	OperationDepsNode *op_to = node_to ? node_to->get_entry_operation() : NULL;
	if (time_from != NULL && op_to != NULL) {
		add_time_relation(time_from, op_to, description);
	}
}

template <typename KeyType>
void DepsgraphRelationBuilder::add_node_handle_relation(
        const KeyType &key_from,
        const DepsNodeHandle *handle,
        const char *description)
{
	DepsNode *node_from = get_node(key_from);
	OperationDepsNode *op_from = node_from ? node_from->get_exit_operation() : NULL;
	OperationDepsNode *op_to = handle->node->get_entry_operation();
	if (op_from != NULL && op_to != NULL) {
		add_operation_relation(op_from, op_to, description);
	}
	else {
		if (!op_from) {
			fprintf(stderr, "add_node_handle_relation(%s) - Could not find op_from (%s)\n",
			        description, key_from.identifier().c_str());
		}
		if (!op_to) {
			fprintf(stderr, "add_node_handle_relation(%s) - Could not find op_to (%s)\n",
			        description, key_from.identifier().c_str());
		}
	}
}

template <typename KeyType>
DepsNodeHandle DepsgraphRelationBuilder::create_node_handle(
        const KeyType &key,
        const char *default_name)
{
	return DepsNodeHandle(this, get_node(key), default_name);
}

}  // namespace DEG
