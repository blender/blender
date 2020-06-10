/*
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
 */

/** \file
 * \ingroup depsgraph
 */

#pragma once

#include "intern/builder/deg_builder.h"
#include "intern/builder/deg_builder_map.h"
#include "intern/depsgraph_type.h"
#include "intern/node/deg_node_id.h"
#include "intern/node/deg_node_operation.h"

#include "DEG_depsgraph.h"

struct CacheFile;
struct Camera;
struct Collection;
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
struct Scene;
struct Simulation;
struct Speaker;
struct Tex;
struct World;
struct bAction;
struct bArmature;
struct bConstraint;
struct bGPdata;
struct bNodeTree;
struct bPoseChannel;
struct bSound;

namespace DEG {

struct ComponentNode;
struct Depsgraph;
class DepsgraphBuilderCache;
struct IDNode;
struct OperationNode;
struct TimeSourceNode;

class DepsgraphNodeBuilder : public DepsgraphBuilder {
 public:
  DepsgraphNodeBuilder(Main *bmain, Depsgraph *graph, DepsgraphBuilderCache *cache);
  ~DepsgraphNodeBuilder();

  /* For given original ID get ID which is created by CoW system. */
  ID *get_cow_id(const ID *id_orig) const;
  /* Similar to above, but for the cases when there is no ID node we create
   * one. */
  ID *ensure_cow_id(ID *id_orig);

  /* Helper wrapper function which wraps get_cow_id with a needed type cast. */
  template<typename T> T *get_cow_datablock(const T *orig) const
  {
    return (T *)get_cow_id(&orig->id);
  }

  /* For a given COW datablock get corresponding original one. */
  template<typename T> T *get_orig_datablock(const T *cow) const
  {
    return (T *)cow->id.orig_id;
  }

  virtual void begin_build();
  virtual void end_build();

  IDNode *add_id_node(ID *id);
  IDNode *find_id_node(ID *id);
  TimeSourceNode *add_time_source();

  ComponentNode *add_component_node(ID *id, NodeType comp_type, const char *comp_name = "");

  OperationNode *add_operation_node(ComponentNode *comp_node,
                                    OperationCode opcode,
                                    const DepsEvalOperationCb &op = nullptr,
                                    const char *name = "",
                                    int name_tag = -1);
  OperationNode *add_operation_node(ID *id,
                                    NodeType comp_type,
                                    const char *comp_name,
                                    OperationCode opcode,
                                    const DepsEvalOperationCb &op = nullptr,
                                    const char *name = "",
                                    int name_tag = -1);
  OperationNode *add_operation_node(ID *id,
                                    NodeType comp_type,
                                    OperationCode opcode,
                                    const DepsEvalOperationCb &op = nullptr,
                                    const char *name = "",
                                    int name_tag = -1);

  OperationNode *ensure_operation_node(ID *id,
                                       NodeType comp_type,
                                       OperationCode opcode,
                                       const DepsEvalOperationCb &op = nullptr,
                                       const char *name = "",
                                       int name_tag = -1);

  bool has_operation_node(ID *id,
                          NodeType comp_type,
                          const char *comp_name,
                          OperationCode opcode,
                          const char *name = "",
                          int name_tag = -1);

  OperationNode *find_operation_node(ID *id,
                                     NodeType comp_type,
                                     const char *comp_name,
                                     OperationCode opcode,
                                     const char *name = "",
                                     int name_tag = -1);

  OperationNode *find_operation_node(
      ID *id, NodeType comp_type, OperationCode opcode, const char *name = "", int name_tag = -1);

  virtual void build_id(ID *id);

  virtual void build_idproperties(IDProperty *id_property);

  virtual void build_scene_render(Scene *scene, ViewLayer *view_layer);
  virtual void build_scene_parameters(Scene *scene);
  virtual void build_scene_compositor(Scene *scene);

  virtual void build_layer_collections(ListBase *lb);
  virtual void build_view_layer(Scene *scene,
                                ViewLayer *view_layer,
                                eDepsNode_LinkedState_Type linked_state);
  virtual void build_collection(LayerCollection *from_layer_collection, Collection *collection);
  virtual void build_object(int base_index,
                            Object *object,
                            eDepsNode_LinkedState_Type linked_state,
                            bool is_visible);
  virtual void build_object_proxy_from(Object *object, bool is_object_visible);
  virtual void build_object_proxy_group(Object *object, bool is_object_visible);
  virtual void build_object_instance_collection(Object *object, bool is_object_visible);
  virtual void build_object_flags(int base_index,
                                  Object *object,
                                  eDepsNode_LinkedState_Type linked_state);
  virtual void build_object_data(Object *object, bool is_object_visible);
  virtual void build_object_data_camera(Object *object);
  virtual void build_object_data_geometry(Object *object, bool is_object_visible);
  virtual void build_object_data_geometry_datablock(ID *obdata, bool is_object_visible);
  virtual void build_object_data_light(Object *object);
  virtual void build_object_data_lightprobe(Object *object);
  virtual void build_object_data_speaker(Object *object);
  virtual void build_object_transform(Object *object);
  virtual void build_object_constraints(Object *object);
  virtual void build_object_pointcache(Object *object);
  virtual void build_pose_constraints(Object *object,
                                      bPoseChannel *pchan,
                                      int pchan_index,
                                      bool is_object_visible);
  virtual void build_rigidbody(Scene *scene);
  virtual void build_particle_systems(Object *object, bool is_object_visible);
  virtual void build_particle_settings(ParticleSettings *part);
  virtual void build_animdata(ID *id);
  virtual void build_animdata_nlastrip_targets(ListBase *strips);
  virtual void build_animation_images(ID *id);
  virtual void build_action(bAction *action);
  virtual void build_driver(ID *id, FCurve *fcurve, int driver_index);
  virtual void build_driver_variables(ID *id, FCurve *fcurve);
  virtual void build_driver_id_property(ID *id, const char *rna_path);
  virtual void build_parameters(ID *id);
  virtual void build_dimensions(Object *object);
  virtual void build_ik_pose(Object *object, bPoseChannel *pchan, bConstraint *con);
  virtual void build_splineik_pose(Object *object, bPoseChannel *pchan, bConstraint *con);
  virtual void build_rig(Object *object, bool is_object_visible);
  virtual void build_proxy_rig(Object *object);
  virtual void build_armature(bArmature *armature);
  virtual void build_armature_bones(ListBase *bones);
  virtual void build_shapekeys(Key *key);
  virtual void build_camera(Camera *camera);
  virtual void build_light(Light *lamp);
  virtual void build_nodetree(bNodeTree *ntree);
  virtual void build_material(Material *ma);
  virtual void build_materials(Material **materials, int num_materials);
  virtual void build_freestyle_lineset(FreestyleLineSet *fls);
  virtual void build_freestyle_linestyle(FreestyleLineStyle *linestyle);
  virtual void build_texture(Tex *tex);
  virtual void build_image(Image *image);
  virtual void build_world(World *world);
  virtual void build_gpencil(bGPdata *gpd);
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

  /* Per-ID information about what was already in the dependency graph.
   * Allows to re-use certain values, to speed up following evaluation. */
  struct IDInfo {
    /* Copy-on-written pointer of the corresponding ID. */
    ID *id_cow;
    /* Mask of visible components from previous state of the
     * dependency graph. */
    IDComponentsMask previously_visible_components_mask;
    /* Special evaluation flag mask from the previous depsgraph. */
    uint32_t previous_eval_flags;
    /* Mesh CustomData mask from the previous depsgraph. */
    DEGCustomDataMeshMasks previous_customdata_masks;
  };

 protected:
  /* Allows to identify an operation which was tagged for update at the time
   * relations are being updated. We can not reuse operation node pointer
   * since it will change during dependency graph construction. */
  struct SavedEntryTag {
    ID *id_orig;
    NodeType component_type;
    OperationCode opcode;
    string name;
    int name_tag;
  };
  Vector<SavedEntryTag> saved_entry_tags_;

  struct BuilderWalkUserData {
    DepsgraphNodeBuilder *builder;
    /* Denotes whether object the walk is invoked from is visible. */
    bool is_parent_visible;
  };
  static void modifier_walk(void *user_data,
                            struct Object *object,
                            struct ID **idpoin,
                            int cb_flag);
  static void constraint_walk(bConstraint *constraint,
                              ID **idpoin,
                              bool is_reference,
                              void *user_data);

  /* State which demotes currently built entities. */
  Scene *scene_;
  ViewLayer *view_layer_;
  int view_layer_index_;
  /* NOTE: Collection are possibly built recursively, so be careful when
   * setting the current state. */
  Collection *collection_;
  /* Accumulated flag over the hierarchy of currently building collections.
   * Denotes whether all the hierarchy from parent of collection_ to the
   * very root is visible (aka not restricted.). */
  bool is_parent_collection_visible_;

  /* Indexed by original ID, values are IDInfo. */
  Map<const ID *, IDInfo *> id_info_hash_;

  /* Set of IDs which were already build. Makes it easier to keep track of
   * what was already built and what was not. */
  BuilderMap built_map_;
};

}  // namespace DEG
