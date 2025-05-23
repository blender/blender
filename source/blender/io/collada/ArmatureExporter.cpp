/* SPDX-FileCopyrightText: 2010-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup collada
 */

#include "COLLADASWInstanceController.h"

#include "BKE_armature.hh"

#include "ED_armature.hh"

#include "BLI_listbase.h"
#include "BLI_math_matrix.h"

#include "ArmatureExporter.h"
#include "SceneExporter.h"

#include "collada_utils.h"

void ArmatureExporter::add_bone_collections(Object *ob_arm, COLLADASW::Node &node)
{
  bArmature *armature = (bArmature *)ob_arm->data;

  /* Because our importer assumes that "extras" tags have a unique name, it's not possible to
   * export a `<bonecollection>` element per bone collection. This is why all the names are stored
   * in one element, newline-separated. */

  std::stringstream collection_stream;
  std::stringstream visible_stream;
  for (const BoneCollection *bcoll : armature->collections_span()) {
    collection_stream << bcoll->name << "\n";

    if (bcoll->flags & BONE_COLLECTION_VISIBLE) {
      visible_stream << bcoll->name << "\n";
    }
  }

  std::string collection_names = collection_stream.str();
  if (collection_names.length() > 1) {
    collection_names.pop_back(); /* Pop off the last `\n`. */
    node.addExtraTechniqueParameter("blender", "collections", collection_names);
  }

  std::string visible_names = visible_stream.str();
  if (visible_names.length() > 1) {
    visible_names.pop_back(); /* Pop off the last `\n`. */
    node.addExtraTechniqueParameter("blender", "visible_collections", visible_names);
  }

  if (armature->runtime.active_collection) {
    node.addExtraTechniqueParameter(
        "blender", "active_collection", std::string(armature->active_collection_name));
  }
}

void ArmatureExporter::add_armature_bones(Object *ob_arm,
                                          ViewLayer *view_layer,
                                          SceneExporter *se,
                                          std::vector<Object *> &child_objects)

{
  /* write bone nodes */

  bArmature *armature = (bArmature *)ob_arm->data;
  bool is_edited = armature->edbo != nullptr;

  if (!is_edited) {
    ED_armature_to_edit(armature);
  }

  LISTBASE_FOREACH (Bone *, bone, &armature->bonebase) {
    add_bone_node(bone, ob_arm, se, child_objects);
  }

  if (!is_edited) {
    ED_armature_edit_free(armature);
  }
}

void ArmatureExporter::write_bone_URLs(COLLADASW::InstanceController &ins,
                                       Object *ob_arm,
                                       Bone *bone)
{
  if (bc_is_root_bone(bone, this->export_settings.get_deform_bones_only())) {
    std::string joint_id = translate_id(id_name(ob_arm) + "_" + bone->name);
    ins.addSkeleton(COLLADABU::URI(COLLADABU::Utils::EMPTY_STRING, joint_id));
  }
  else {
    LISTBASE_FOREACH (Bone *, child, &bone->childbase) {
      write_bone_URLs(ins, ob_arm, child);
    }
  }
}

bool ArmatureExporter::add_instance_controller(Object *ob)
{
  Object *ob_arm = bc_get_assigned_armature(ob);
  bArmature *arm = (bArmature *)ob_arm->data;

  const std::string &controller_id = get_controller_id(ob_arm, ob);

  COLLADASW::InstanceController ins(mSW);
  ins.setUrl(COLLADASW::URI(COLLADABU::Utils::EMPTY_STRING, controller_id));

  Mesh *mesh = (Mesh *)ob->data;
  if (mesh->deform_verts().is_empty()) {
    return false;
  }

  /* write root bone URLs */
  LISTBASE_FOREACH (Bone *, bone, &arm->bonebase) {
    write_bone_URLs(ins, ob_arm, bone);
  }

  InstanceWriter::add_material_bindings(
      ins.getBindMaterial(), ob, this->export_settings.get_active_uv_only());

  ins.add();
  return true;
}

#if 0
void ArmatureExporter::operator()(Object *ob)
{
  Object *ob_arm = bc_get_assigned_armature(ob);
}

bool ArmatureExporter::already_written(Object *ob_arm)
{
  return std::find(written_armatures.begin(), written_armatures.end(), ob_arm) !=
         written_armatures.end();
}

void ArmatureExporter::wrote(Object *ob_arm)
{
  written_armatures.push_back(ob_arm);
}

void ArmatureExporter::find_objects_using_armature(Object *ob_arm,
                                                   std::vector<Object *> &objects,
                                                   Scene *sce)
{
  objects.clear();

  Base *base = (Base *)sce->base.first;
  while (base) {
    Object *ob = base->object;

    if (ob->type == OB_MESH && get_assigned_armature(ob) == ob_arm) {
      objects.push_back(ob);
    }

    base = base->next;
  }
}
#endif

void ArmatureExporter::add_bone_node(Bone *bone,
                                     Object *ob_arm,
                                     SceneExporter *se,
                                     std::vector<Object *> &child_objects)
{
  if (can_export(bone)) {
    std::string node_id = translate_id(id_name(ob_arm) + "_" + bone->name);
    std::string node_name = std::string(bone->name);
    std::string node_sid = get_joint_sid(bone);

    COLLADASW::Node node(mSW);

    node.setType(COLLADASW::Node::JOINT);
    node.setNodeId(node_id);
    node.setNodeName(node_name);
    node.setNodeSid(node_sid);

    if (this->export_settings.get_use_blender_profile()) {
      if (!is_export_root(bone)) {
        if (bone->flag & BONE_CONNECTED) {
          node.addExtraTechniqueParameter("blender", "connect", true);
        }
      }

      std::string collection_names;
      LISTBASE_FOREACH (const BoneCollectionReference *, bcoll_ref, &bone->runtime.collections) {
        collection_names += std::string(bcoll_ref->bcoll->name) + "\n";
      }
      if (collection_names.length() > 1) {
        collection_names.pop_back(); /* Pop off the last `\n`. */
        node.addExtraTechniqueParameter("blender", "", collection_names, "", "collections");
      }

      bArmature *armature = (bArmature *)ob_arm->data;
      EditBone *ebone = bc_get_edit_bone(armature, bone->name);
      if (ebone && ebone->roll != 0) {
        node.addExtraTechniqueParameter("blender", "roll", ebone->roll);
      }
      if (bc_is_leaf_bone(bone)) {
        Vector head, tail;
        const BCMatrix &global_transform = this->export_settings.get_global_transform();
        if (this->export_settings.get_apply_global_orientation()) {
          bc_add_global_transform(head, bone->arm_head, global_transform);
          bc_add_global_transform(tail, bone->arm_tail, global_transform);
        }
        else {
          copy_v3_v3(head, bone->arm_head);
          copy_v3_v3(tail, bone->arm_tail);
        }
        node.addExtraTechniqueParameter("blender", "tip_x", tail[0] - head[0]);
        node.addExtraTechniqueParameter("blender", "tip_y", tail[1] - head[1]);
        node.addExtraTechniqueParameter("blender", "tip_z", tail[2] - head[2]);
      }
    }

    node.start();

    add_bone_transform(ob_arm, bone, node);

    /* Write nodes of child-objects, remove written objects from list. */
    std::vector<Object *>::iterator iter = child_objects.begin();

    while (iter != child_objects.end()) {
      Object *ob = *iter;
      if (ob->partype == PARBONE && STREQ(ob->parsubstr, bone->name)) {
        float backup_parinv[4][4];
        copy_m4_m4(backup_parinv, ob->parentinv);

        /* Crude, temporary change to parentinv
         * so transform gets exported correctly. */

        /* Add bone tail- translation... don't know why
         * bone parenting is against the tail of a bone
         * and not its head, seems arbitrary. */
        ob->parentinv[3][1] += bone->length;

        /* OPEN_SIM_COMPATIBILITY
         * TODO: when such objects are animated as
         * single matrix the tweak must be applied
         * to the result. */
        if (export_settings.get_open_sim()) {
          /* Tweak objects parent-inverse to match compatibility. */
          float temp[4][4];

          copy_m4_m4(temp, bone->arm_mat);
          temp[3][0] = temp[3][1] = temp[3][2] = 0.0f;

          mul_m4_m4m4(ob->parentinv, temp, ob->parentinv);
        }

        se->writeNode(ob);
        copy_m4_m4(ob->parentinv, backup_parinv);
        iter = child_objects.erase(iter);
      }
      else {
        iter++;
      }
    }

    LISTBASE_FOREACH (Bone *, child, &bone->childbase) {
      add_bone_node(child, ob_arm, se, child_objects);
    }
    node.end();
  }
  else {
    LISTBASE_FOREACH (Bone *, child, &bone->childbase) {
      add_bone_node(child, ob_arm, se, child_objects);
    }
  }
}

bool ArmatureExporter::is_export_root(Bone *bone)
{
  Bone *entry = bone->parent;
  while (entry) {
    if (can_export(entry)) {
      return false;
    }
    entry = entry->parent;
  }
  return can_export(bone);
}

void ArmatureExporter::add_bone_transform(Object *ob_arm, Bone *bone, COLLADASW::Node &node)
{
  // bPoseChannel *pchan = BKE_pose_channel_find_name(ob_arm->pose, bone->name);

  float mat[4][4];
  float bone_rest_mat[4][4];   /* derived from bone->arm_mat */
  float parent_rest_mat[4][4]; /* derived from bone->parent->arm_mat */

  bool has_restmat = bc_get_property_matrix(bone, "rest_mat", mat);

  if (!has_restmat) {

    /* Have no rest-pose matrix stored, try old style <= Blender 2.78. */

    bc_create_restpose_mat(this->export_settings, bone, bone_rest_mat, bone->arm_mat, true);

    if (is_export_root(bone)) {
      copy_m4_m4(mat, bone_rest_mat);
    }
    else {
      Matrix parent_inverse;
      bc_create_restpose_mat(
          this->export_settings, bone->parent, parent_rest_mat, bone->parent->arm_mat, true);

      invert_m4_m4(parent_inverse, parent_rest_mat);
      mul_m4_m4m4(mat, parent_inverse, bone_rest_mat);
    }

    /* OPEN_SIM_COMPATIBILITY */

    if (export_settings.get_open_sim()) {
      /* Remove rotations vs armature from transform
       * parent_rest_rot * mat * irest_rot */
      Matrix workmat;
      copy_m4_m4(workmat, bone_rest_mat);

      workmat[3][0] = workmat[3][1] = workmat[3][2] = 0.0f;
      invert_m4(workmat);

      mul_m4_m4m4(mat, mat, workmat);

      if (!is_export_root(bone)) {
        copy_m4_m4(workmat, parent_rest_mat);
        workmat[3][0] = workmat[3][1] = workmat[3][2] = 0.0f;

        mul_m4_m4m4(mat, workmat, mat);
      }
    }
  }

  if (this->export_settings.get_limit_precision()) {
    BCMatrix::sanitize(mat, LIMITTED_PRECISION);
  }

  TransformWriter::add_joint_transform(node, mat, nullptr, this->export_settings, has_restmat);
}

std::string ArmatureExporter::get_controller_id(Object *ob_arm, Object *ob)
{
  return translate_id(id_name(ob_arm)) + "_" + translate_id(id_name(ob)) +
         SKIN_CONTROLLER_ID_SUFFIX;
}
