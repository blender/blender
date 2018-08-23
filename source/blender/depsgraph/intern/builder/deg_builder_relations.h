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
#include <cstring>

#include "intern/depsgraph_types.h"

#include "DNA_ID.h"

#include "RNA_access.h"
#include "RNA_types.h"

#include "BLI_utildefines.h"
#include "BLI_string.h"

#include "intern/builder/deg_builder_map.h"
#include "intern/nodes/deg_node.h"
#include "intern/nodes/deg_node_component.h"
#include "intern/nodes/deg_node_operation.h"

struct Base;
struct bArmature;
struct bAction;
struct bGPdata;
struct CacheFile;
struct Camera;
struct ListBase;
struct GHash;
struct ID;
struct FCurve;
struct Collection;
struct Key;
struct Lamp;
struct LayerCollection;
struct LightProbe;
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
struct ParticleSystem;
struct ParticleSettings;
struct Scene;
struct Speaker;
struct ViewLayer;
struct Tex;
struct World;
struct EffectorWeights;

struct PropertyRNA;

namespace DEG {

struct Depsgraph;
struct DepsNode;
struct DepsNodeHandle;
struct DepsRelation;
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
	DepsRelation *add_relation(const KeyFrom& key_from,
	                           const KeyTo& key_to,
	                           const char *description,
	                           bool check_unique = false);

	template <typename KeyTo>
	DepsRelation *add_relation(const TimeSourceKey& key_from,
	                           const KeyTo& key_to,
	                           const char *description,
	                           bool check_unique = false);

	template <typename KeyType>
	DepsRelation *add_node_handle_relation(const KeyType& key_from,
	                                       const DepsNodeHandle *handle,
	                                       const char *description,
	                                       bool check_unique = false);

	void build_id(ID *id);
	void build_layer_collections(ListBase *lb);
	void build_view_layer(Scene *scene, ViewLayer *view_layer);
	void build_collection(Object *object, Collection *collection);
	void build_object(Base *base, Object *object);
	void build_object_flags(Base *base, Object *object);
	void build_object_data(Object *object);
	void build_object_data_camera(Object *object);
	void build_object_data_geometry(Object *object);
	void build_object_data_geometry_datablock(ID *obdata);
	void build_object_data_lamp(Object *object);
	void build_object_data_lightprobe(Object *object);
	void build_object_data_speaker(Object *object);
	void build_object_parent(Object *object);
	void build_constraints(ID *id,
	                       eDepsNode_Type component_type,
	                       const char *component_subdata,
	                       ListBase *constraints,
	                       RootPChanMap *root_map);
	void build_animdata(ID *id);
	void build_animdata_curves(ID *id);
	void build_animdata_curves_targets(ID *id,
	                                   ComponentKey &adt_key,
	                                   OperationDepsNode *operation_from,
	                                   ListBase *curves);
	void build_animdata_nlastrip_targets(ID *id,
	                                     ComponentKey &adt_key,
	                                     OperationDepsNode *operation_from,
	                                     ListBase *strips);
	void build_animdata_drivers(ID *id);
	void build_action(bAction *action);
	void build_driver(ID *id, FCurve *fcurve);
	void build_driver_data(ID *id, FCurve *fcurve);
	void build_driver_variables(ID *id, FCurve *fcurve);
	void build_world(World *world);
	void build_rigidbody(Scene *scene);
	void build_particles(Object *object);
	void build_particle_settings(ParticleSettings *part);
	void build_particles_visualization_object(Object *object,
	                                          ParticleSystem *psys,
	                                          Object *draw_object);
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
	void build_shapekeys(Key *key);
	void build_armature(bArmature *armature);
	void build_camera(Camera *camera);
	void build_lamp(Lamp *lamp);
	void build_nodetree(bNodeTree *ntree);
	void build_material(Material *ma);
	void build_texture(Tex *tex);
	void build_compositor(Scene *scene);
	void build_gpencil(bGPdata *gpd);
	void build_cachefile(CacheFile *cache_file);
	void build_mask(Mask *mask);
	void build_movieclip(MovieClip *clip);
	void build_lightprobe(LightProbe *probe);
	void build_speaker(Speaker *speaker);

	void build_nested_datablock(ID *owner, ID *id);
	void build_nested_nodetree(ID *owner, bNodeTree *ntree);
	void build_nested_shapekey(ID *owner, Key *key);

	void add_collision_relations(const OperationKey &key,
	                             Object *object,
	                             Collection *collection,
	                             const char *name);
	void add_forcefield_relations(const OperationKey &key,
	                              Object *object,
	                              ParticleSystem *psys,
	                              EffectorWeights *eff,
	                              bool add_absorption, const char *name);

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

	DepsRelation *add_time_relation(TimeSourceDepsNode *timesrc,
	                                DepsNode *node_to,
	                                const char *description,
	                                bool check_unique = false);
	DepsRelation *add_operation_relation(OperationDepsNode *node_from,
	                                     OperationDepsNode *node_to,
	                                     const char *description,
	                                     bool check_unique = false);

	template <typename KeyType>
	DepsNodeHandle create_node_handle(const KeyType& key,
	                                  const char *default_name = "");

	/* TODO(sergey): All those is_same* functions are to be generalized. */

	/* Check whether two keys correponds to the same bone from same armature.
	 *
	 * This is used by drivers relations builder to avoid possible fake
	 * dependency cycle when one bone property drives another property of the
	 * same bone.
	 */
	template <typename KeyFrom, typename KeyTo>
	bool is_same_bone_dependency(const KeyFrom& key_from, const KeyTo& key_to);

	/* Similar to above, but used to check whether driver is using node from
	 * the same node tree as a driver variable.
	 */
	template <typename KeyFrom, typename KeyTo>
	bool is_same_nodetree_node_dependency(const KeyFrom& key_from,
	                                      const KeyTo& key_to);

	/* Similar to above, but used to check whether driver is using key from
	 * the same key datablock as a driver variable.
	 */
	template <typename KeyFrom, typename KeyTo>
	bool is_same_shapekey_dependency(const KeyFrom& key_from,
	                                 const KeyTo& key_to);

private:
	struct BuilderWalkUserData {
		DepsgraphRelationBuilder *builder;
	};

	static void modifier_walk(void *user_data,
	                          struct Object *object,
	                          struct ID **idpoin,
	                          int cb_flag);

	static void constraint_walk(bConstraint *con,
	                            ID **idpoin,
	                            bool is_reference,
	                            void *user_data);

	/* State which never changes, same for the whole builder time. */
	Main *bmain_;
	Depsgraph *graph_;

	/* State which demotes currently built entities. */
	Scene *scene_;

	BuilderMap built_map_;
};

struct DepsNodeHandle
{
	DepsNodeHandle(DepsgraphRelationBuilder *builder,
	               OperationDepsNode *node,
	               const char *default_name = "")
	        : builder(builder),
	          node(node),
	          default_name(default_name)
	{
		BLI_assert(node != NULL);
	}

	DepsgraphRelationBuilder *builder;
	OperationDepsNode *node;
	const char *default_name;
};

}  // namespace DEG


#include "intern/builder/deg_builder_relations_impl.h"
