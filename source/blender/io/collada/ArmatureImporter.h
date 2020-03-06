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
 */

/** \file
 * \ingroup collada
 */

#ifndef __ARMATUREIMPORTER_H__
#define __ARMATUREIMPORTER_H__

#include "COLLADAFWNode.h"
#include "COLLADAFWUniqueId.h"
#include "COLLADAFWMorphController.h"

extern "C" {
#include "BKE_context.h"
#include "BKE_key.h"

#include "DNA_armature_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_key_types.h"

#include "ED_armature.h"
}

#include "AnimationImporter.h"
#include "MeshImporter.h"
#include "SkinInfo.h"
#include "TransformReader.h"
#include "ExtraTags.h"

#include <map>
#include <vector>

#include "collada_internal.h"
#include "collada_utils.h"
#include "ImportSettings.h"

#define UNLIMITED_CHAIN_MAX INT_MAX
#define MINIMUM_BONE_LENGTH 0.000001f

class ArmatureImporter : private TransformReader {
 private:
  Main *m_bmain;
  Scene *scene;
  ViewLayer *view_layer;
  UnitConverter *unit_converter;
  const ImportSettings *import_settings;

  // std::map<int, JointData> joint_index_to_joint_info_map;
  // std::map<COLLADAFW::UniqueId, int> joint_id_to_joint_index_map;
  BoneExtensionManager bone_extension_manager;
  // int bone_direction_row; // XXX not used
  float leaf_bone_length;
  int totbone;
  // XXX not used
  // float min_angle; // minimum angle between bone head-tail and a row of bone matrix

#if 0
  struct ArmatureJoints {
    Object *ob_arm;
    std::vector<COLLADAFW::Node *> root_joints;
  };
  std::vector<ArmatureJoints> armature_joints;
#endif

  Object *empty;  // empty for leaf bones

  std::map<COLLADAFW::UniqueId, COLLADAFW::UniqueId> geom_uid_by_controller_uid;
  std::map<COLLADAFW::UniqueId, COLLADAFW::Node *> joint_by_uid;  // contains all joints
  std::vector<COLLADAFW::Node *> root_joints;
  std::vector<COLLADAFW::Node *> finished_joints;
  std::vector<COLLADAFW::MorphController *> morph_controllers;
  std::map<COLLADAFW::UniqueId, Object *> joint_parent_map;
  std::map<COLLADAFW::UniqueId, Object *> unskinned_armature_map;

  MeshImporterBase *mesh_importer;

  // This is used to store data passed in write_controller_data.
  // Arrays from COLLADAFW::SkinControllerData lose ownership, so do this class members
  // so that arrays don't get freed until we free them explicitly.

  std::map<COLLADAFW::UniqueId, SkinInfo> skin_by_data_uid;  // data UID = skin controller data UID
#if 0
  JointData *get_joint_data(COLLADAFW::Node *node);
#endif

  int create_bone(SkinInfo *skin,
                  COLLADAFW::Node *node,
                  EditBone *parent,
                  int totchild,
                  float parent_mat[4][4],
                  bArmature *arm,
                  std::vector<std::string> &layer_labels);

  BoneExtended &add_bone_extended(EditBone *bone,
                                  COLLADAFW::Node *node,
                                  int sibcount,
                                  std::vector<std::string> &layer_labels,
                                  BoneExtensionMap &extended_bones);

  void fix_leaf_bone_hierarchy(bArmature *armature, Bone *bone, bool fix_orientation);
  void fix_leaf_bone(bArmature *armature, EditBone *ebone, BoneExtended *be, bool fix_orientation);
  void fix_parent_connect(bArmature *armature, Bone *bone);
  void connect_bone_chains(bArmature *armature, Bone *bone, const int max_chain_length);

  void set_pose(Object *ob_arm,
                COLLADAFW::Node *root_node,
                const char *parentname,
                float parent_mat[4][4]);

  void set_bone_transformation_type(const COLLADAFW::Node *node, Object *ob_arm);
  bool node_is_decomposed(const COLLADAFW::Node *node);
#if 0
  void set_leaf_bone_shapes(Object *ob_arm);
  void set_euler_rotmode();
#endif

  Object *get_empty_for_leaves();

#if 0
  Object *find_armature(COLLADAFW::Node *node);

  ArmatureJoints &get_armature_joints(Object *ob_arm);
#endif

  Object *create_armature_bones(Main *bmain, SkinInfo &skin);
  void create_armature_bones(Main *bmain, std::vector<Object *> &arm_objs);

  /** TagsMap typedef for uid_tags_map. */
  typedef std::map<std::string, ExtraTags *> TagsMap;
  TagsMap uid_tags_map;

 public:
  ArmatureImporter(UnitConverter *conv,
                   MeshImporterBase *mesh,
                   Main *bmain,
                   Scene *sce,
                   ViewLayer *view_layer,
                   const ImportSettings *import_settings);
  ~ArmatureImporter();

  void add_root_joint(COLLADAFW::Node *node, Object *parent);

  // here we add bones to armatures, having armatures previously created in write_controller
  void make_armatures(bContext *C, std::vector<Object *> &objects_to_scale);

  void make_shape_keys(bContext *C);

#if 0
  // link with meshes, create vertex groups, assign weights
  void link_armature(Object *ob_arm,
                     const COLLADAFW::UniqueId &geom_id,
                     const COLLADAFW::UniqueId &controller_data_id);
#endif

  bool write_skin_controller_data(const COLLADAFW::SkinControllerData *data);

  bool write_controller(const COLLADAFW::Controller *controller);

  COLLADAFW::UniqueId *get_geometry_uid(const COLLADAFW::UniqueId &controller_uid);

  Object *get_armature_for_joint(COLLADAFW::Node *node);

  void get_rna_path_for_joint(COLLADAFW::Node *node, char *joint_path, size_t count);

  // gives a world-space mat
  bool get_joint_bind_mat(float m[4][4], COLLADAFW::Node *joint);

  void set_tags_map(TagsMap &tags_map);
};

#endif
