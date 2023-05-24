/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2013 Blender Foundation */

/** \file
 * \ingroup depsgraph
 */

#pragma once

#include <cstdio>
#include <cstring>

#include "intern/depsgraph_type.h"

#include "DNA_ID.h"

#include "RNA_path.h"

#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "intern/builder/deg_builder.h"
#include "intern/builder/deg_builder_key.h"
#include "intern/builder/deg_builder_map.h"
#include "intern/builder/deg_builder_rna.h"
#include "intern/builder/deg_builder_stack.h"
#include "intern/depsgraph.h"
#include "intern/node/deg_node.h"
#include "intern/node/deg_node_component.h"
#include "intern/node/deg_node_id.h"
#include "intern/node/deg_node_operation.h"

struct CacheFile;
struct Camera;
struct Collection;
struct EffectorWeights;
struct FCurve;
struct FreestyleLineSet;
struct FreestyleLineStyle;
struct ID;
struct IDProperty;
struct Image;
struct Key;
struct LayerCollection;
struct Light;
struct LightProbe;
struct ListBase;
struct Main;
struct Mask;
struct Material;
struct MovieClip;
struct Object;
struct ParticleSettings;
struct ParticleSystem;
struct Scene;
struct Simulation;
struct Speaker;
struct Tex;
struct VFont;
struct ViewLayer;
struct World;
struct bAction;
struct bArmature;
struct bConstraint;
struct bNodeSocket;
struct bNodeTree;
struct bPoseChannel;
struct bSound;

namespace blender::deg {

struct ComponentNode;
struct DepsNodeHandle;
struct Depsgraph;
class DepsgraphBuilderCache;
struct IDNode;
struct Node;
struct OperationNode;
struct Relation;
struct RootPChanMap;
struct TimeSourceNode;

class DepsgraphRelationBuilder : public DepsgraphBuilder {
 public:
  DepsgraphRelationBuilder(Main *bmain, Depsgraph *graph, DepsgraphBuilderCache *cache);

  void begin_build();

  template<typename KeyFrom, typename KeyTo>
  Relation *add_relation(const KeyFrom &key_from,
                         const KeyTo &key_to,
                         const char *description,
                         int flags = 0);

  template<typename KeyTo>
  Relation *add_relation(const TimeSourceKey &key_from,
                         const KeyTo &key_to,
                         const char *description,
                         int flags = 0);

  template<typename KeyType>
  Relation *add_node_handle_relation(const KeyType &key_from,
                                     const DepsNodeHandle *handle,
                                     const char *description,
                                     int flags = 0);

  template<typename KeyTo>
  Relation *add_depends_on_transform_relation(ID *id,
                                              const KeyTo &key_to,
                                              const char *description,
                                              int flags = 0);

  /* Adds relation from proper transformation operation to the modifier.
   * Takes care of checking for possible physics solvers modifying position
   * of this object. */
  void add_depends_on_transform_relation(const DepsNodeHandle *handle, const char *description);

  void add_customdata_mask(Object *object, const DEGCustomDataMeshMasks &customdata_masks);
  void add_special_eval_flag(ID *id, uint32_t flag);

  virtual void build_id(ID *id);

  /* Build function for ID types that do not need their own build_xxx() function. */
  virtual void build_generic_id(ID *id);

  virtual void build_idproperties(IDProperty *id_property);

  virtual void build_scene_render(Scene *scene, ViewLayer *view_layer);
  virtual void build_scene_parameters(Scene *scene);
  virtual void build_scene_compositor(Scene *scene);

  virtual bool build_layer_collection(LayerCollection *layer_collection);
  virtual void build_view_layer_collections(ViewLayer *view_layer);

  virtual void build_view_layer(Scene *scene,
                                ViewLayer *view_layer,
                                eDepsNode_LinkedState_Type linked_state);
  virtual void build_collection(LayerCollection *from_layer_collection, Collection *collection);
  virtual void build_object(Object *object);
  virtual void build_object_from_view_layer_base(Object *object);
  virtual void build_object_layer_component_relations(Object *object);
  virtual void build_object_modifiers(Object *object);
  virtual void build_object_data(Object *object);
  virtual void build_object_data_camera(Object *object);
  virtual void build_object_data_geometry(Object *object);
  virtual void build_object_data_geometry_datablock(ID *obdata);
  virtual void build_object_data_light(Object *object);
  virtual void build_object_data_lightprobe(Object *object);
  virtual void build_object_data_speaker(Object *object);
  virtual void build_object_parent(Object *object);
  virtual void build_object_pointcache(Object *object);
  virtual void build_object_instance_collection(Object *object);

  virtual void build_object_light_linking(Object *emitter);
  virtual void build_light_linking_collection(Object *emitter, Collection *collection);

  virtual void build_constraints(ID *id,
                                 NodeType component_type,
                                 const char *component_subdata,
                                 ListBase *constraints,
                                 RootPChanMap *root_map);
  virtual void build_animdata(ID *id);
  virtual void build_animdata_curves(ID *id);
  virtual void build_animdata_curves_targets(ID *id,
                                             ComponentKey &adt_key,
                                             OperationNode *operation_from,
                                             ListBase *curves);
  virtual void build_animdata_nlastrip_targets(ID *id,
                                               ComponentKey &adt_key,
                                               OperationNode *operation_from,
                                               ListBase *strips);
  virtual void build_animdata_drivers(ID *id);
  virtual void build_animdata_force(ID *id);
  virtual void build_animation_images(ID *id);
  virtual void build_action(bAction *action);
  virtual void build_driver(ID *id, FCurve *fcurve);
  virtual void build_driver_data(ID *id, FCurve *fcurve);
  virtual void build_driver_variables(ID *id, FCurve *fcurve);

  /* Build operations of a property value from which is read by a driver target.
   *
   * The driver target points to a data-block (or a sub-data-block like View Layer).
   * This data-block is presented in the interface as a "Prop" and its resolved RNA pointer is
   * passed here as `target_prop`.
   *
   * The tricky part (and a bit confusing naming) is that the driver target accesses a property of
   * the `target_prop` to get its value. The property which is read to give an actual target value
   * is denoted by its RNA path relative to the `target_prop`. In the interface it is called "Path"
   * and here it is called `rna_path_from_target_prop`. */
  virtual void build_driver_id_property(const PointerRNA &target_prop,
                                        const char *rna_path_from_target_prop);

  virtual void build_parameters(ID *id);
  virtual void build_dimensions(Object *object);
  virtual void build_world(World *world);
  virtual void build_rigidbody(Scene *scene);
  virtual void build_particle_systems(Object *object);
  virtual void build_particle_settings(ParticleSettings *part);
  virtual void build_particle_system_visualization_object(Object *object,
                                                          ParticleSystem *psys,
                                                          Object *draw_object);
  virtual void build_ik_pose(Object *object,
                             bPoseChannel *pchan,
                             bConstraint *con,
                             RootPChanMap *root_map);
  virtual void build_splineik_pose(Object *object,
                                   bPoseChannel *pchan,
                                   bConstraint *con,
                                   RootPChanMap *root_map);
  virtual void build_inter_ik_chains(Object *object,
                                     const OperationKey &solver_key,
                                     const bPoseChannel *rootchan,
                                     const RootPChanMap *root_map);
  virtual void build_rig(Object *object);
  virtual void build_shapekeys(Key *key);
  virtual void build_armature(bArmature *armature);
  virtual void build_armature_bones(ListBase *bones);
  virtual void build_camera(Camera *camera);
  virtual void build_light(Light *lamp);
  virtual void build_nodetree(bNodeTree *ntree);
  virtual void build_nodetree_socket(bNodeSocket *socket);
  virtual void build_material(Material *ma);
  virtual void build_materials(Material **materials, int num_materials);
  virtual void build_freestyle_lineset(FreestyleLineSet *fls);
  virtual void build_freestyle_linestyle(FreestyleLineStyle *linestyle);
  virtual void build_texture(Tex *tex);
  virtual void build_image(Image *image);
  virtual void build_cachefile(CacheFile *cache_file);
  virtual void build_mask(Mask *mask);
  virtual void build_movieclip(MovieClip *clip);
  virtual void build_lightprobe(LightProbe *probe);
  virtual void build_speaker(Speaker *speaker);
  virtual void build_sound(bSound *sound);
  virtual void build_simulation(Simulation *simulation);
  virtual void build_scene_sequencer(Scene *scene);
  virtual void build_scene_audio(Scene *scene);
  virtual void build_scene_speakers(Scene *scene, ViewLayer *view_layer);
  virtual void build_vfont(VFont *vfont);

  virtual void build_nested_datablock(ID *owner, ID *id, bool flush_cow_changes);
  virtual void build_nested_nodetree(ID *owner, bNodeTree *ntree);
  virtual void build_nested_shapekey(ID *owner, Key *key);

  void add_particle_collision_relations(const OperationKey &key,
                                        Object *object,
                                        Collection *collection,
                                        const char *name);
  void add_particle_forcefield_relations(const OperationKey &key,
                                         Object *object,
                                         ParticleSystem *psys,
                                         EffectorWeights *eff,
                                         bool add_absorption,
                                         const char *name);

  virtual void build_copy_on_write_relations();
  virtual void build_copy_on_write_relations(IDNode *id_node);
  virtual void build_driver_relations();
  virtual void build_driver_relations(IDNode *id_node);

  template<typename KeyType> OperationNode *find_operation_node(const KeyType &key);

  Depsgraph *getGraph();

 protected:
  TimeSourceNode *get_node(const TimeSourceKey &key) const;
  ComponentNode *get_node(const ComponentKey &key) const;
  OperationNode *get_node(const OperationKey &key) const;
  Node *get_node(const RNAPathKey &key);

  OperationNode *find_node(const OperationKey &key) const;
  ComponentNode *find_node(const ComponentKey &key) const;
  bool has_node(const ComponentKey &key) const;
  bool has_node(const OperationKey &key) const;

  Relation *add_time_relation(TimeSourceNode *timesrc,
                              Node *node_to,
                              const char *description,
                              int flags = 0);

  /* Add relation which ensures visibility of `id_from` when `id_to` is visible.
   * For the more detailed explanation see comment for `NodeType::VISIBILITY`. */
  void add_visibility_relation(ID *id_from, ID *id_to);

  Relation *add_operation_relation(OperationNode *node_from,
                                   OperationNode *node_to,
                                   const char *description,
                                   int flags = 0);

  template<typename KeyType>
  DepsNodeHandle create_node_handle(const KeyType &key, const char *default_name = "");

  /* TODO(sergey): All those is_same* functions are to be generalized. */

  /* Check whether two keys corresponds to the same bone from same armature.
   *
   * This is used by drivers relations builder to avoid possible fake
   * dependency cycle when one bone property drives another property of the
   * same bone. */
  template<typename KeyFrom, typename KeyTo>
  bool is_same_bone_dependency(const KeyFrom &key_from, const KeyTo &key_to);

  /* Similar to above, but used to check whether driver is using node from
   * the same node tree as a driver variable. */
  template<typename KeyFrom, typename KeyTo>
  bool is_same_nodetree_node_dependency(const KeyFrom &key_from, const KeyTo &key_to);

 private:
  struct BuilderWalkUserData {
    DepsgraphRelationBuilder *builder;
  };

  static void modifier_walk(void *user_data,
                            struct Object *object,
                            struct ID **idpoin,
                            int cb_flag);

  static void constraint_walk(bConstraint *con, ID **idpoin, bool is_reference, void *user_data);

  /* State which demotes currently built entities. */
  Scene *scene_;

  BuilderMap built_map_;
  RNANodeQuery rna_node_query_;
  BuilderStack stack_;
};

struct DepsNodeHandle {
  DepsNodeHandle(DepsgraphRelationBuilder *builder,
                 OperationNode *node,
                 const char *default_name = "")
      : builder(builder), node(node), default_name(default_name)
  {
    BLI_assert(node != nullptr);
  }

  DepsgraphRelationBuilder *builder;
  OperationNode *node;
  const char *default_name;
};

}  // namespace blender::deg

#include "intern/builder/deg_builder_relations_impl.h"
