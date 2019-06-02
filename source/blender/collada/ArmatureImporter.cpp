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

/* COLLADABU_ASSERT, may be able to remove later */
#include "COLLADABUPlatform.h"

#include <algorithm>

#include "COLLADAFWUniqueId.h"

extern "C" {
#include "BKE_action.h"
#include "BKE_object.h"
#include "BKE_armature.h"
#include "BLI_string.h"
#include "BLI_listbase.h"
#include "ED_armature.h"
}

#include "DEG_depsgraph.h"

#include "collada_utils.h"
#include "ArmatureImporter.h"

/* use node name, or fall back to original id if not present (name is optional) */
template<class T> static const char *bc_get_joint_name(T *node)
{
  const std::string &id = node->getName();
  return id.size() ? id.c_str() : node->getOriginalId().c_str();
}

ArmatureImporter::ArmatureImporter(UnitConverter *conv,
                                   MeshImporterBase *mesh,
                                   Main *bmain,
                                   Scene *sce,
                                   ViewLayer *view_layer,
                                   const ImportSettings *import_settings)
    : TransformReader(conv),
      m_bmain(bmain),
      scene(sce),
      view_layer(view_layer),
      unit_converter(conv),
      import_settings(import_settings),
      empty(NULL),
      mesh_importer(mesh)
{
}

ArmatureImporter::~ArmatureImporter()
{
  /* free skin controller data if we forget to do this earlier */
  std::map<COLLADAFW::UniqueId, SkinInfo>::iterator it;
  for (it = skin_by_data_uid.begin(); it != skin_by_data_uid.end(); it++) {
    it->second.free();
  }
}

#if 0
JointData *ArmatureImporter::get_joint_data(COLLADAFW::Node *node);
{
  const COLLADAFW::UniqueId &joint_id = node->getUniqueId();

  if (joint_id_to_joint_index_map.find(joint_id) == joint_id_to_joint_index_map.end()) {
    fprintf(
        stderr, "Cannot find a joint index by joint id for %s.\n", node->getOriginalId().c_str());
    return NULL;
  }

  int joint_index = joint_id_to_joint_index_map[joint_id];

  return &joint_index_to_joint_info_map[joint_index];
}
#endif

int ArmatureImporter::create_bone(SkinInfo *skin,
                                  COLLADAFW::Node *node,
                                  EditBone *parent,
                                  int totchild,
                                  float parent_mat[4][4],
                                  bArmature *arm,
                                  std::vector<std::string> &layer_labels)
{
  float mat[4][4];
  float joint_inv_bind_mat[4][4];
  float joint_bind_mat[4][4];
  int chain_length = 0;

  /* Checking if bone is already made. */
  std::vector<COLLADAFW::Node *>::iterator it;
  it = std::find(finished_joints.begin(), finished_joints.end(), node);
  if (it != finished_joints.end()) {
    return chain_length;
  }

  EditBone *bone = ED_armature_ebone_add(arm, bc_get_joint_name(node));
  totbone++;

  /*
   * We use the inv_bind_shape matrix to apply the armature bind pose as its rest pose.
   */

  std::map<COLLADAFW::UniqueId, SkinInfo>::iterator skin_it;
  bool bone_is_skinned = false;
  for (skin_it = skin_by_data_uid.begin(); skin_it != skin_by_data_uid.end(); skin_it++) {

    SkinInfo *b = &skin_it->second;
    if (b->get_joint_inv_bind_matrix(joint_inv_bind_mat, node)) {

      /* get original world-space matrix */
      invert_m4_m4(mat, joint_inv_bind_mat);
      copy_m4_m4(joint_bind_mat, mat);
      /* And make local to armature */
      Object *ob_arm = skin->BKE_armature_from_object();
      if (ob_arm) {
        float invmat[4][4];
        invert_m4_m4(invmat, ob_arm->obmat);
        mul_m4_m4m4(mat, invmat, mat);
      }

      bone_is_skinned = true;
      break;
    }
  }

  /* create a bone even if there's no joint data for it (i.e. it has no influence) */
  if (!bone_is_skinned) {
    get_node_mat(mat, node, NULL, NULL, parent_mat);
  }

  if (parent) {
    bone->parent = parent;
  }

  float loc[3], size[3], rot[3][3];
  BoneExtensionMap &extended_bones = bone_extension_manager.getExtensionMap(arm);
  BoneExtended &be = add_bone_extended(bone, node, totchild, layer_labels, extended_bones);
  int layer = be.get_bone_layers();
  if (layer) {
    bone->layer = layer;
  }
  arm->layer |= layer;  // ensure that all populated bone layers are visible after import

  float *tail = be.get_tail();
  int use_connect = be.get_use_connect();

  switch (use_connect) {
    case 1:
      bone->flag |= BONE_CONNECTED;
      break;
    case -1: /* Connect type not specified */
    case 0:
      bone->flag &= ~BONE_CONNECTED;
      break;
  }

  if (be.has_roll()) {
    bone->roll = be.get_roll();
  }
  else {
    float angle;
    mat4_to_loc_rot_size(loc, rot, size, mat);
    mat3_to_vec_roll(rot, NULL, &angle);
    bone->roll = angle;
  }
  copy_v3_v3(bone->head, mat[3]);

  if (bone_is_skinned && this->import_settings->keep_bind_info) {
    float rest_mat[4][4];
    get_node_mat(rest_mat, node, NULL, NULL, NULL);
    bc_set_IDPropertyMatrix(bone, "bind_mat", joint_bind_mat);
    bc_set_IDPropertyMatrix(bone, "rest_mat", rest_mat);
  }

  add_v3_v3v3(bone->tail, bone->head, tail); /* tail must be non zero */

  /* find smallest bone length in armature (used later for leaf bone length) */
  if (parent) {

    if (use_connect == 1) {
      copy_v3_v3(parent->tail, bone->head);
    }

    /* guess reasonable leaf bone length */
    float length = len_v3v3(parent->head, bone->head);
    if ((length < leaf_bone_length || totbone == 0) && length > MINIMUM_BONE_LENGTH) {
      leaf_bone_length = length;
    }
  }

  COLLADAFW::NodePointerArray &children = node->getChildNodes();

  for (unsigned int i = 0; i < children.getCount(); i++) {
    int cl = create_bone(skin, children[i], bone, children.getCount(), mat, arm, layer_labels);
    if (cl > chain_length) {
      chain_length = cl;
    }
  }

  bone->length = len_v3v3(bone->head, bone->tail);
  joint_by_uid[node->getUniqueId()] = node;
  finished_joints.push_back(node);

  be.set_chain_length(chain_length + 1);

  return chain_length + 1;
}

/**
 * Collada only knows Joints, hence bones at the end of a bone chain
 * don't have a defined length. This function guesses reasonable
 * tail locations for the affected bones (nodes which don't have any connected child)
 * Hint: The extended_bones set gets populated in ArmatureImporter::create_bone
 */
void ArmatureImporter::fix_leaf_bone_hierarchy(bArmature *armature,
                                               Bone *bone,
                                               bool fix_orientation)
{
  if (bone == NULL) {
    return;
  }

  if (bc_is_leaf_bone(bone)) {
    BoneExtensionMap &extended_bones = bone_extension_manager.getExtensionMap(armature);
    BoneExtended *be = extended_bones[bone->name];
    EditBone *ebone = bc_get_edit_bone(armature, bone->name);
    fix_leaf_bone(armature, ebone, be, fix_orientation);
  }

  for (Bone *child = (Bone *)bone->childbase.first; child; child = child->next) {
    fix_leaf_bone_hierarchy(armature, child, fix_orientation);
  }
}

void ArmatureImporter::fix_leaf_bone(bArmature *armature,
                                     EditBone *ebone,
                                     BoneExtended *be,
                                     bool fix_orientation)
{
  if (be == NULL || !be->has_tail()) {

    /* Collada only knows Joints, Here we guess a reasonable leaf bone length */
    float leaf_length = (leaf_bone_length == FLT_MAX) ? 1.0 : leaf_bone_length;

    float vec[3];

    if (fix_orientation && ebone->parent != NULL) {
      EditBone *parent = ebone->parent;
      sub_v3_v3v3(vec, ebone->head, parent->head);
      if (len_squared_v3(vec) < MINIMUM_BONE_LENGTH) {
        sub_v3_v3v3(vec, parent->tail, parent->head);
      }
    }
    else {
      vec[2] = 0.1f;
      sub_v3_v3v3(vec, ebone->tail, ebone->head);
    }

    normalize_v3_v3(vec, vec);
    mul_v3_fl(vec, leaf_length);
    add_v3_v3v3(ebone->tail, ebone->head, vec);
  }
}

void ArmatureImporter::fix_parent_connect(bArmature *armature, Bone *bone)
{
  /* armature has no bones */
  if (bone == NULL) {
    return;
  }

  if (bone->parent && bone->flag & BONE_CONNECTED) {
    copy_v3_v3(bone->parent->tail, bone->head);
  }

  for (Bone *child = (Bone *)bone->childbase.first; child; child = child->next) {
    fix_parent_connect(armature, child);
  }
}

void ArmatureImporter::connect_bone_chains(bArmature *armature, Bone *parentbone, int clip)
{
  BoneExtensionMap &extended_bones = bone_extension_manager.getExtensionMap(armature);
  BoneExtended *dominant_child = NULL;
  int maxlen = 0;

  if (parentbone == NULL) {
    return;
  }

  Bone *child = (Bone *)parentbone->childbase.first;
  if (child && (import_settings->find_chains || child->next == NULL)) {
    for (; child; child = child->next) {
      BoneExtended *be = extended_bones[child->name];
      if (be != NULL) {
        int chain_len = be->get_chain_length();
        if (chain_len <= clip) {
          if (chain_len > maxlen) {
            dominant_child = be;
            maxlen = chain_len;
          }
          else if (chain_len == maxlen) {
            dominant_child = NULL;
          }
        }
      }
    }
  }

  BoneExtended *pbe = extended_bones[parentbone->name];
  if (dominant_child != NULL) {
    /* Found a valid chain. Now connect current bone with that chain.*/
    EditBone *pebone = bc_get_edit_bone(armature, parentbone->name);
    EditBone *cebone = bc_get_edit_bone(armature, dominant_child->get_name());
    if (pebone && !(cebone->flag & BONE_CONNECTED)) {
      float vec[3];
      sub_v3_v3v3(vec, cebone->head, pebone->head);

      /*
       * It is possible that the child's head is located on the parents head.
       * When this happens, then moving the parent's tail to the child's head
       * would result in a zero sized bone and Blender would  silently remove the bone.
       * So we move the tail only when the resulting bone has a minimum length:
       */

      if (len_squared_v3(vec) > MINIMUM_BONE_LENGTH) {
        copy_v3_v3(pebone->tail, cebone->head);
        pbe->set_tail(pebone->tail); /* to make fix_leafbone happy ...*/
        if (pbe && pbe->get_chain_length() >= this->import_settings->min_chain_length) {

          BoneExtended *cbe = extended_bones[cebone->name];
          cbe->set_use_connect(true);

          cebone->flag |= BONE_CONNECTED;
          pbe->set_leaf_bone(false);
          printf("Connect Bone chain: parent (%s --> %s) child)\n", pebone->name, cebone->name);
        }
      }
    }
    for (Bone *ch = (Bone *)parentbone->childbase.first; ch; ch = ch->next) {
      ArmatureImporter::connect_bone_chains(armature, ch, UNLIMITED_CHAIN_MAX);
    }
  }
  else if (maxlen > 1 && maxlen > this->import_settings->min_chain_length) {
    /* Try again with smaller chain length */
    ArmatureImporter::connect_bone_chains(armature, parentbone, maxlen - 1);
  }
  else {
    /* can't connect this Bone. Proceed with children ... */
    if (pbe) {
      pbe->set_leaf_bone(true);
    }
    for (Bone *ch = (Bone *)parentbone->childbase.first; ch; ch = ch->next) {
      ArmatureImporter::connect_bone_chains(armature, ch, UNLIMITED_CHAIN_MAX);
    }
  }
}

#if 0
void ArmatureImporter::set_leaf_bone_shapes(Object *ob_arm)
{
  bPose *pose = ob_arm->pose;

  std::vector<LeafBone>::iterator it;
  for (it = leaf_bones.begin(); it != leaf_bones.end(); it++) {
    LeafBone &leaf = *it;

    bPoseChannel *pchan = BKE_pose_channel_find_name(pose, leaf.name);
    if (pchan) {
      pchan->custom = get_empty_for_leaves();
    }
    else {
      fprintf(stderr, "Cannot find a pose channel for leaf bone %s\n", leaf.name);
    }
  }
}

void ArmatureImporter::set_euler_rotmode()
{
  /* just set rotmode = ROT_MODE_EUL on pose channel for each joint */

  std::map<COLLADAFW::UniqueId, COLLADAFW::Node *>::iterator it;

  for (it = joint_by_uid.begin(); it != joint_by_uid.end(); it++) {

    COLLADAFW::Node *joint = it->second;

    std::map<COLLADAFW::UniqueId, SkinInfo>::iterator sit;

    for (sit = skin_by_data_uid.begin(); sit != skin_by_data_uid.end(); sit++) {
      SkinInfo &skin = sit->second;

      if (skin.uses_joint_or_descendant(joint)) {
        bPoseChannel *pchan = skin.get_pose_channel_from_node(joint);

        if (pchan) {
          pchan->rotmode = ROT_MODE_EUL;
        }
        else {
          fprintf(stderr, "Cannot find pose channel for %s.\n", get_joint_name(joint));
        }

        break;
      }
    }
  }
}
#endif

Object *ArmatureImporter::get_empty_for_leaves()
{
  if (empty) {
    return empty;
  }

  empty = bc_add_object(m_bmain, scene, view_layer, OB_EMPTY, NULL);
  empty->empty_drawtype = OB_EMPTY_SPHERE;

  return empty;
}

#if 0
Object *ArmatureImporter::find_armature(COLLADAFW::Node *node)
{
  JointData *jd = get_joint_data(node);
  if (jd) {
    return jd->ob_arm;
  }

  COLLADAFW::NodePointerArray &children = node->getChildNodes();
  for (int i = 0; i < children.getCount(); i++) {
    Object *ob_arm = find_armature(children[i]);
    if (ob_arm) {
      return ob_arm;
    }
  }

  return NULL;
}

ArmatureJoints &ArmatureImporter::get_armature_joints(Object *ob_arm)
{
  /* try finding it */
  std::vector<ArmatureJoints>::iterator it;
  for (it = armature_joints.begin(); it != armature_joints.end(); it++) {
    if ((*it).ob_arm == ob_arm) {
      return *it;
    }
  }

  /* not found, create one */
  ArmatureJoints aj;
  aj.ob_arm = ob_arm;
  armature_joints.push_back(aj);

  return armature_joints.back();
}
#endif
void ArmatureImporter::create_armature_bones(Main *bmain, std::vector<Object *> &ob_arms)
{
  std::vector<COLLADAFW::Node *>::iterator ri;
  std::vector<std::string> layer_labels;

  /* if there is an armature created for root_joint next root_joint */
  for (ri = root_joints.begin(); ri != root_joints.end(); ri++) {
    COLLADAFW::Node *node = *ri;
    if (get_armature_for_joint(node) != NULL) {
      continue;
    }

    Object *ob_arm = joint_parent_map[node->getUniqueId()];
    if (!ob_arm) {
      continue;
    }

    bArmature *armature = (bArmature *)ob_arm->data;
    if (!armature) {
      continue;
    }

    char *bone_name = (char *)bc_get_joint_name(node);
    Bone *bone = BKE_armature_find_bone_name(armature, bone_name);
    if (bone) {
      fprintf(stderr,
              "Reuse of child bone [%s] as root bone in same Armature is not supported.\n",
              bone_name);
      continue;
    }

    ED_armature_to_edit(armature);
    armature->layer = 0;  // layer is set according to imported bone set in create_bone()

    create_bone(NULL, node, NULL, node->getChildNodes().getCount(), NULL, armature, layer_labels);
    if (this->import_settings->find_chains) {
      connect_bone_chains(armature, (Bone *)armature->bonebase.first, UNLIMITED_CHAIN_MAX);
    }

    /* exit armature edit mode to populate the Armature object */
    ED_armature_from_edit(bmain, armature);
    ED_armature_edit_free(armature);
    ED_armature_to_edit(armature);

    fix_leaf_bone_hierarchy(
        armature, (Bone *)armature->bonebase.first, this->import_settings->fix_orientation);
    unskinned_armature_map[node->getUniqueId()] = ob_arm;

    ED_armature_from_edit(bmain, armature);
    ED_armature_edit_free(armature);

	set_bone_transformation_type(node, ob_arm);

    int index = std::find(ob_arms.begin(), ob_arms.end(), ob_arm) - ob_arms.begin();
    if (index == 0) {
      ob_arms.push_back(ob_arm);
    }

    DEG_id_tag_update(&ob_arm->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
  }
}

Object *ArmatureImporter::create_armature_bones(Main *bmain, SkinInfo &skin)
{
  /* just do like so:
   * - get armature
   * - enter editmode
   * - add edit bones and head/tail properties using matrices and parent-child info
   * - exit edit mode
   * - set a sphere shape to leaf bones */

  Object *ob_arm = NULL;

  /*
   * find if there's another skin sharing at least one bone with this skin
   * if so, use that skin's armature
   */

  /**
   * Pseudocode:
   *
   * find_node_in_tree(node, root_joint)
   *
   * skin::find_root_joints(root_joints):
   *     std::vector root_joints;
   *     for each root in root_joints:
   *         for each joint in joints:
   *             if find_node_in_tree(joint, root):
   *                 if (std::find(root_joints.begin(), root_joints.end(), root) ==
   * root_joints.end()) root_joints.push_back(root);
   *
   * for (each skin B with armature) {
   *     find all root joints for skin B
   *
   *     for each joint X in skin A:
   *         for each root joint R in skin B:
   *             if (find_node_in_tree(X, R)) {
   *                 shared = 1;
   *                 goto endloop;
   *             }
   * }
   *
   * endloop:
   */

  SkinInfo *a = &skin;
  Object *shared = NULL;
  std::vector<COLLADAFW::Node *> skin_root_joints;
  std::vector<std::string> layer_labels;

  std::map<COLLADAFW::UniqueId, SkinInfo>::iterator it;
  for (it = skin_by_data_uid.begin(); it != skin_by_data_uid.end(); it++) {
    SkinInfo *b = &it->second;
    if (b == a || b->BKE_armature_from_object() == NULL) {
      continue;
    }

    skin_root_joints.clear();

    b->find_root_joints(root_joints, joint_by_uid, skin_root_joints);

    std::vector<COLLADAFW::Node *>::iterator ri;
    for (ri = skin_root_joints.begin(); ri != skin_root_joints.end(); ri++) {
      COLLADAFW::Node *node = *ri;
      if (a->uses_joint_or_descendant(node)) {
        shared = b->BKE_armature_from_object();
        break;
      }
    }

    if (shared != NULL) {
      break;
    }
  }

  if (!shared && this->joint_parent_map.size() > 0) {
    /* All armatures have been created while creating the Node tree.
     * The Collada exporter currently does not create a
     * strict relationship between geometries and armatures
     * So when we reimport a Blender collada file, then we have
     * to guess what is meant.
     * XXX This is not safe when we have more than one armatures
     * in the import. */
    shared = this->joint_parent_map.begin()->second;
  }

  if (shared) {
    ob_arm = skin.set_armature(shared);
  }
  else {
    ob_arm = skin.create_armature(m_bmain, scene, view_layer);  // once for every armature
  }

  /* enter armature edit mode */
  bArmature *armature = (bArmature *)ob_arm->data;
  ED_armature_to_edit(armature);

  totbone = 0;
  // bone_direction_row = 1; // TODO: don't default to Y but use asset and based on it decide on
  /* default row */

  /* create bones */
  /* TODO:
   * check if bones have already been created for a given joint */

  std::vector<COLLADAFW::Node *>::iterator ri;
  for (ri = root_joints.begin(); ri != root_joints.end(); ri++) {
    COLLADAFW::Node *node = *ri;
    /* for shared armature check if bone tree is already created */
    if (shared && std::find(skin_root_joints.begin(), skin_root_joints.end(), node) !=
                      skin_root_joints.end()) {
      continue;
    }

    /* since root_joints may contain joints for multiple controllers, we need to filter */
    if (skin.uses_joint_or_descendant(node)) {

      create_bone(
          &skin, node, NULL, node->getChildNodes().getCount(), NULL, armature, layer_labels);

      if (joint_parent_map.find(node->getUniqueId()) != joint_parent_map.end() &&
          !skin.get_parent()) {
        skin.set_parent(joint_parent_map[node->getUniqueId()]);
      }
    }
  }

  /* exit armature edit mode to populate the Armature object */
  ED_armature_from_edit(bmain, armature);
  ED_armature_edit_free(armature);

  for (ri = root_joints.begin(); ri != root_joints.end(); ri++) {
    COLLADAFW::Node *node = *ri;
    set_bone_transformation_type(node, ob_arm);
  }

  ED_armature_to_edit(armature);
  if (this->import_settings->find_chains) {
    connect_bone_chains(armature, (Bone *)armature->bonebase.first, UNLIMITED_CHAIN_MAX);
  }
  fix_leaf_bone_hierarchy(
      armature, (Bone *)armature->bonebase.first, this->import_settings->fix_orientation);
  ED_armature_from_edit(bmain, armature);
  ED_armature_edit_free(armature);

  DEG_id_tag_update(&ob_arm->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);

  return ob_arm;
}

void ArmatureImporter::set_bone_transformation_type(const COLLADAFW::Node *node, Object *ob_arm)
{
  bPoseChannel *pchan = BKE_pose_channel_find_name(ob_arm->pose, bc_get_joint_name(node));
  if (pchan) {
    pchan->rotmode = (node_is_decomposed(node)) ? ROT_MODE_EUL : ROT_MODE_QUAT;
  }

  COLLADAFW::NodePointerArray childnodes = node->getChildNodes();
  for (int index = 0; index < childnodes.getCount(); index++) {
    node = childnodes[index];
    set_bone_transformation_type(node, ob_arm);
  }
}

void ArmatureImporter::set_pose(Object *ob_arm,
                                COLLADAFW::Node *root_node,
                                const char *parentname,
                                float parent_mat[4][4])
{
  const char *bone_name = bc_get_joint_name(root_node);
  float mat[4][4];
  float obmat[4][4];

  /* object-space */
  get_node_mat(obmat, root_node, NULL, NULL);
  bool is_decomposed = node_is_decomposed(root_node);

  // if (*edbone)
  bPoseChannel *pchan = BKE_pose_channel_find_name(ob_arm->pose, bone_name);
  pchan->rotmode = (is_decomposed) ? ROT_MODE_EUL : ROT_MODE_QUAT;

  // else fprintf ( "",

  /* get world-space */
  if (parentname) {
    mul_m4_m4m4(mat, parent_mat, obmat);
    bPoseChannel *parchan = BKE_pose_channel_find_name(ob_arm->pose, parentname);

    mul_m4_m4m4(pchan->pose_mat, parchan->pose_mat, mat);
  }
  else {

    copy_m4_m4(mat, obmat);
    float invObmat[4][4];
    invert_m4_m4(invObmat, ob_arm->obmat);
    mul_m4_m4m4(pchan->pose_mat, invObmat, mat);
  }

#if 0
  float angle = 0.0f;
  mat4_to_axis_angle(ax, &angle, mat);
  pchan->bone->roll = angle;
#endif

  COLLADAFW::NodePointerArray &children = root_node->getChildNodes();
  for (unsigned int i = 0; i < children.getCount(); i++) {
    set_pose(ob_arm, children[i], bone_name, mat);
  }
}

bool ArmatureImporter::node_is_decomposed(const COLLADAFW::Node *node)
{
  const COLLADAFW::TransformationPointerArray &nodeTransforms = node->getTransformations();
  for (unsigned int i = 0; i < nodeTransforms.getCount(); i++) {
    COLLADAFW::Transformation *transform = nodeTransforms[i];
    COLLADAFW::Transformation::TransformationType tm_type = transform->getTransformationType();
    if (tm_type == COLLADAFW::Transformation::MATRIX) {
      return false;
    }
  }
  return true;
}

/**
 * root - if this joint is the top joint in hierarchy, if a joint
 * is a child of a node (not joint), root should be true since
 * this is where we build armature bones from
 */
void ArmatureImporter::add_root_joint(COLLADAFW::Node *node, Object *parent)
{
  root_joints.push_back(node);
  if (parent) {
    joint_parent_map[node->getUniqueId()] = parent;
  }
}

#if 0
void ArmatureImporter::add_root_joint(COLLADAFW::Node *node)
{
  // root_joints.push_back(node);
  Object *ob_arm = find_armature(node);
  if (ob_arm) {
    get_armature_joints(ob_arm).root_joints.push_back(node);
  }
#  ifdef COLLADA_DEBUG
  else {
    fprintf(stderr, "%s cannot be added to armature.\n", get_joint_name(node));
  }
#  endif
}
#endif

/* here we add bones to armatures, having armatures previously created in write_controller */
void ArmatureImporter::make_armatures(bContext *C, std::vector<Object *> &objects_to_scale)
{
  Main *bmain = CTX_data_main(C);
  std::vector<Object *> ob_arms;
  std::map<COLLADAFW::UniqueId, SkinInfo>::iterator it;

  leaf_bone_length =
      FLT_MAX; /*TODO: Make this work for more than one armature in the import file*/

  for (it = skin_by_data_uid.begin(); it != skin_by_data_uid.end(); it++) {

    SkinInfo &skin = it->second;

    Object *ob_arm = create_armature_bones(bmain, skin);

    /* link armature with a mesh object */
    const COLLADAFW::UniqueId &uid = skin.get_controller_uid();
    const COLLADAFW::UniqueId *guid = get_geometry_uid(uid);
    if (guid != NULL) {
      Object *ob = mesh_importer->get_object_by_geom_uid(*guid);
      if (ob) {
        skin.link_armature(C, ob, joint_by_uid, this);

        std::vector<Object *>::iterator ob_it = std::find(
            objects_to_scale.begin(), objects_to_scale.end(), ob);

        if (ob_it != objects_to_scale.end()) {
          int index = ob_it - objects_to_scale.begin();
          objects_to_scale.erase(objects_to_scale.begin() + index);
        }

        if (std::find(objects_to_scale.begin(), objects_to_scale.end(), ob_arm) ==
            objects_to_scale.end()) {
          objects_to_scale.push_back(ob_arm);
        }

        if (std::find(ob_arms.begin(), ob_arms.end(), ob_arm) == ob_arms.end()) {
          ob_arms.push_back(ob_arm);
        }
      }
      else {
        fprintf(stderr, "Cannot find object to link armature with.\n");
      }
    }
    else {
      fprintf(stderr, "Cannot find geometry to link armature with.\n");
    }

    /* set armature parent if any */
    Object *par = skin.get_parent();
    if (par) {
      bc_set_parent(skin.BKE_armature_from_object(), par, C, false);
    }

    /* free memory stolen from SkinControllerData */
    skin.free();
  }

  /* for bones without skins */
  create_armature_bones(bmain, ob_arms);

  /* Fix bone relations */
  std::vector<Object *>::iterator ob_arm_it;
  for (ob_arm_it = ob_arms.begin(); ob_arm_it != ob_arms.end(); ob_arm_it++) {

    Object *ob_arm = *ob_arm_it;
    bArmature *armature = (bArmature *)ob_arm->data;

    /* and step back to edit mode to fix the leaf nodes */
    ED_armature_to_edit(armature);

    fix_parent_connect(armature, (Bone *)armature->bonebase.first);

    ED_armature_from_edit(bmain, armature);
    ED_armature_edit_free(armature);
  }
}

#if 0
/* link with meshes, create vertex groups, assign weights */
void ArmatureImporter::link_armature(Object *ob_arm,
                                     const COLLADAFW::UniqueId &geom_id,
                                     const COLLADAFW::UniqueId &controller_data_id)
{
  Object *ob = mesh_importer->get_object_by_geom_uid(geom_id);

  if (!ob) {
    fprintf(stderr, "Cannot find object by geometry UID.\n");
    return;
  }

  if (skin_by_data_uid.find(controller_data_id) == skin_by_data_uid.end()) {
    fprintf(stderr, "Cannot find skin info by controller data UID.\n");
    return;
  }

  SkinInfo &skin = skin_by_data_uid[conroller_data_id];

  /* create vertex groups */
}
#endif

bool ArmatureImporter::write_skin_controller_data(const COLLADAFW::SkinControllerData *data)
{
  /* at this stage we get vertex influence info that should go into me->verts and ob->defbase
   * there's no info to which object this should be long so we associate it with
   * skin controller data UID. */

  /* don't forget to call defgroup_unique_name before we copy */

  /* controller data uid -> [armature] -> joint data,
   * [mesh object] */

  SkinInfo skin(unit_converter);
  skin.borrow_skin_controller_data(data);

  /* store join inv bind matrix to use it later in armature construction */
  const COLLADAFW::Matrix4Array &inv_bind_mats = data->getInverseBindMatrices();
  for (unsigned int i = 0; i < data->getJointsCount(); i++) {
    skin.add_joint(inv_bind_mats[i]);
  }

  skin_by_data_uid[data->getUniqueId()] = skin;

  return true;
}

bool ArmatureImporter::write_controller(const COLLADAFW::Controller *controller)
{
  /* - create and store armature object */
  const COLLADAFW::UniqueId &con_id = controller->getUniqueId();

  if (controller->getControllerType() == COLLADAFW::Controller::CONTROLLER_TYPE_SKIN) {
    COLLADAFW::SkinController *co = (COLLADAFW::SkinController *)controller;
    /* to be able to find geom id by controller id */
    geom_uid_by_controller_uid[con_id] = co->getSource();

    const COLLADAFW::UniqueId &data_uid = co->getSkinControllerData();
    if (skin_by_data_uid.find(data_uid) == skin_by_data_uid.end()) {
      fprintf(stderr, "Cannot find skin by controller data UID.\n");
      return true;
    }

    skin_by_data_uid[data_uid].set_controller(co);
  }
  /* morph controller */
  else if (controller->getControllerType() == COLLADAFW::Controller::CONTROLLER_TYPE_MORPH) {
    COLLADAFW::MorphController *co = (COLLADAFW::MorphController *)controller;
    /* to be able to find geom id by controller id */
    geom_uid_by_controller_uid[con_id] = co->getSource();
    /* Shape keys are applied in DocumentImporter->finish() */
    morph_controllers.push_back(co);
  }

  return true;
}

void ArmatureImporter::make_shape_keys(bContext *C)
{
  Main *bmain = CTX_data_main(C);
  std::vector<COLLADAFW::MorphController *>::iterator mc;
  float weight;

  for (mc = morph_controllers.begin(); mc != morph_controllers.end(); mc++) {
    /* Controller data */
    COLLADAFW::UniqueIdArray &morphTargetIds = (*mc)->getMorphTargets();
    COLLADAFW::FloatOrDoubleArray &morphWeights = (*mc)->getMorphWeights();

    /* Prereq: all the geometries must be imported and mesh objects must be made */
    Object *source_ob = this->mesh_importer->get_object_by_geom_uid((*mc)->getSource());

    if (source_ob) {

      Mesh *source_me = (Mesh *)source_ob->data;
      /* insert key to source mesh */
      Key *key = source_me->key = BKE_key_add(bmain, (ID *)source_me);
      key->type = KEY_RELATIVE;
      KeyBlock *kb;

      /* insert basis key */
      kb = BKE_keyblock_add_ctime(key, "Basis", false);
      BKE_keyblock_convert_from_mesh(source_me, key, kb);

      /* insert other shape keys */
      for (int i = 0; i < morphTargetIds.getCount(); i++) {
        /* better to have a separate map of morph objects,
         * This'll do for now since only mesh morphing is imported */

        Mesh *me = this->mesh_importer->get_mesh_by_geom_uid(morphTargetIds[i]);

        if (me) {
          me->key = key;
          std::string morph_name = *this->mesh_importer->get_geometry_name(me->id.name);

          kb = BKE_keyblock_add_ctime(key, morph_name.c_str(), false);
          BKE_keyblock_convert_from_mesh(me, key, kb);

          /* apply weights */
          weight = morphWeights.getFloatValues()->getData()[i];
          kb->curval = weight;
        }
        else {
          fprintf(stderr, "Morph target geometry not found.\n");
        }
      }
    }
    else {
      fprintf(stderr, "Morph target object not found.\n");
    }
  }
}

COLLADAFW::UniqueId *ArmatureImporter::get_geometry_uid(const COLLADAFW::UniqueId &controller_uid)
{
  if (geom_uid_by_controller_uid.find(controller_uid) == geom_uid_by_controller_uid.end()) {
    return NULL;
  }

  return &geom_uid_by_controller_uid[controller_uid];
}

Object *ArmatureImporter::get_armature_for_joint(COLLADAFW::Node *node)
{
  std::map<COLLADAFW::UniqueId, SkinInfo>::iterator it;
  for (it = skin_by_data_uid.begin(); it != skin_by_data_uid.end(); it++) {
    SkinInfo &skin = it->second;

    if (skin.uses_joint_or_descendant(node)) {
      return skin.BKE_armature_from_object();
    }
  }

  std::map<COLLADAFW::UniqueId, Object *>::iterator arm;
  for (arm = unskinned_armature_map.begin(); arm != unskinned_armature_map.end(); arm++) {
    if (arm->first == node->getUniqueId()) {
      return arm->second;
    }
  }
  return NULL;
}

void ArmatureImporter::set_tags_map(TagsMap &tagsMap)
{
  this->uid_tags_map = tagsMap;
}

void ArmatureImporter::get_rna_path_for_joint(COLLADAFW::Node *node,
                                              char *joint_path,
                                              size_t count)
{
  BLI_snprintf(joint_path, count, "pose.bones[\"%s\"]", bc_get_joint_name(node));
}

/* gives a world-space mat */
bool ArmatureImporter::get_joint_bind_mat(float m[4][4], COLLADAFW::Node *joint)
{
  std::map<COLLADAFW::UniqueId, SkinInfo>::iterator it;
  bool found = false;
  for (it = skin_by_data_uid.begin(); it != skin_by_data_uid.end(); it++) {
    SkinInfo &skin = it->second;
    if ((found = skin.get_joint_inv_bind_matrix(m, joint))) {
      invert_m4(m);
      break;
    }
  }

  return found;
}

BoneExtended &ArmatureImporter::add_bone_extended(EditBone *bone,
                                                  COLLADAFW::Node *node,
                                                  int sibcount,
                                                  std::vector<std::string> &layer_labels,
                                                  BoneExtensionMap &extended_bones)
{
  BoneExtended *be = new BoneExtended(bone);
  extended_bones[bone->name] = be;

  TagsMap::iterator etit;
  ExtraTags *et = 0;
  etit = uid_tags_map.find(node->getUniqueId().toAscii());

  bool has_connect = false;
  int connect_type = -1;

  if (etit != uid_tags_map.end()) {

    float tail[3] = {FLT_MAX, FLT_MAX, FLT_MAX};
    float roll = 0;
    std::string layers;

    et = etit->second;

    bool has_tail = false;
    has_tail |= et->setData("tip_x", &tail[0]);
    has_tail |= et->setData("tip_y", &tail[1]);
    has_tail |= et->setData("tip_z", &tail[2]);

    has_connect = et->setData("connect", &connect_type);
    bool has_roll = et->setData("roll", &roll);

    layers = et->setData("layer", layers);

    if (has_tail && !has_connect) {
      /* got a bone tail definition but no connect info -> bone is not connected */
      has_connect = true;
      connect_type = 0;
    }

    be->set_bone_layers(layers, layer_labels);
    if (has_tail) {
      be->set_tail(tail);
    }
    if (has_roll) {
      be->set_roll(roll);
    }
  }

  if (!has_connect && this->import_settings->auto_connect) {
    /* auto connect only whyen parent has exactly one child*/
    connect_type = sibcount == 1;
  }

  be->set_use_connect(connect_type);
  be->set_leaf_bone(true);

  return *be;
}
