/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "usd_armature_utils.h"

#include "BKE_armature.h"
#include "BKE_modifier.h"
#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_query.hh"
#include "DNA_armature_types.h"
#include "ED_armature.hh"

#include "WM_api.hh"

namespace blender::io::usd {

/* Recursively invoke the 'visitor' function on the given bone and its children. */
static void visit_bones(const Bone *bone, blender::FunctionRef<void(const Bone *)> visitor)
{
  if (!(bone && visitor)) {
    return;
  }

  visitor(bone);

  LISTBASE_FOREACH (const Bone *, child, &bone->childbase) {
    visit_bones(child, visitor);
  }
}

/**
 * Return the modifier of the given type enabled for the given dependency graph's
 * evaluation mode (viewport or render).
 */
const ModifierData *get_enabled_modifier(const Object *obj,
                                         ModifierType type,
                                         const Depsgraph *depsgraph)
{
  BLI_assert(obj);
  BLI_assert(depsgraph);

  Scene *scene = DEG_get_input_scene(depsgraph);
  eEvaluationMode mode = DEG_get_mode(depsgraph);

  LISTBASE_FOREACH (ModifierData *, md, &obj->modifiers) {

    if (!BKE_modifier_is_enabled(scene, md, mode)) {
      continue;
    }

    if (md->type == type) {
      return md;
    }
  }

  return nullptr;
}

/* Return the armature modifier on the given object.  Return null if no armature modifier
 * can be found. */
static const ArmatureModifierData *get_armature_modifier(const Object *obj,
                                                         const Depsgraph *depsgraph)
{
  BLI_assert(obj);
  const ArmatureModifierData *mod = reinterpret_cast<const ArmatureModifierData *>(
      get_enabled_modifier(obj, eModifierType_Armature, depsgraph));
  return mod;
}

void visit_bones(const Object *ob_arm, FunctionRef<void(const Bone *)> visitor)
{
  if (!(ob_arm && ob_arm->type == OB_ARMATURE && ob_arm->data)) {
    return;
  }

  bArmature *armature = (bArmature *)ob_arm->data;

  for (Bone *bone = (Bone *)armature->bonebase.first; bone; bone = bone->next) {
    visit_bones(bone, visitor);
  }
}

void get_armature_bone_names(const Object *ob_arm,
                             Vector<std::string> &r_names,
                             const bool use_deform)
{
  std::unordered_map<const char *, const Bone *> deform_map;
  if (use_deform) {
    init_deform_bones_map(ob_arm, &deform_map);
  }

  auto visitor = [&](const Bone *bone) {
    if (use_deform && deform_map.find(bone->name) == deform_map.end()) {
      return;
    }

    r_names.append(bone->name);
  };

  visit_bones(ob_arm, visitor);
}

pxr::TfToken build_usd_joint_path(const Bone *bone)
{
  std::string path(pxr::TfMakeValidIdentifier(bone->name));

  const Bone *parent = bone->parent;
  while (parent) {
    path = pxr::TfMakeValidIdentifier(parent->name) + std::string("/") + path;
    parent = parent->parent;
  }

  return pxr::TfToken(path);
}

void create_pose_joints(pxr::UsdSkelAnimation &skel_anim,
                        const Object *obj,
                        std::unordered_map<const char *, const Bone *> *deform_map)
{
  if (!(skel_anim && obj && obj->pose)) {
    return;
  }

  pxr::VtTokenArray joints;

  const bPose *pose = obj->pose;

  LISTBASE_FOREACH (const bPoseChannel *, pchan, &pose->chanbase) {
    if (pchan->bone) {
      if (deform_map && deform_map->find(pchan->bone->name) == deform_map->end()) {
        /* If deform_map is passed in, assume we're going deform-only.
         * Bones not found in the map should be skipped. */
        continue;
      }

      joints.push_back(build_usd_joint_path(pchan->bone));
    }
  }

  skel_anim.GetJointsAttr().Set(joints);
}

const Object *get_armature_modifier_obj(const Object *obj, const Depsgraph *depsgraph)
{
  const ArmatureModifierData *mod = get_armature_modifier(obj, depsgraph);
  return mod ? mod->object : nullptr;
}

bool is_armature_modifier_bone_name(const Object *obj,
                                    const char *name,
                                    const Depsgraph *depsgraph)
{
  if (!obj || !name) {
    return false;
  }
  const ArmatureModifierData *arm_mod = get_armature_modifier(obj, depsgraph);

  if (!arm_mod || !arm_mod->object || !arm_mod->object->data) {
    return false;
  }

  bArmature *arm = static_cast<bArmature *>(arm_mod->object->data);

  return BKE_armature_find_bone_name(arm, name);
}

bool can_export_skinned_mesh(const Object *obj, const Depsgraph *depsgraph)
{
  return get_enabled_modifier(obj, eModifierType_Armature, depsgraph) != nullptr;
}

Vector<ModifierData *> get_enabled_modifiers(const Object *obj, const Depsgraph *depsgraph)
{
  BLI_assert(obj);
  BLI_assert(depsgraph);

  blender::Vector<ModifierData *> result;

  Scene *scene = DEG_get_input_scene(depsgraph);
  eEvaluationMode mode = DEG_get_mode(depsgraph);

  LISTBASE_FOREACH (ModifierData *, md, &obj->modifiers) {

    if (!BKE_modifier_is_enabled(scene, md, mode)) {
      continue;
    }

    result.append(md);
  }

  return result;
}

void init_deform_bones_map(const Object *obj,
                           std::unordered_map<const char *, const Bone *> *deform_map)
{
  if (!deform_map) {
    return;
  }

  size_t num_bones = 0;
  size_t num_deform_bones = 0;
  size_t num_deform_parent_bones = 0;

  deform_map->clear();

  auto deform_visitor = [&](const Bone *bone) {
    if (!bone) {
      return;
    }

    const bool deform = !(bone->flag & BONE_NO_DEFORM);
    if (deform && deform_map) {
      deform_map->insert_or_assign(bone->name, bone);
    }
    num_deform_bones += int(deform);
    num_bones += 1;
  };

  visit_bones(obj, deform_visitor);

  /* Get deform parents */
  std::unordered_map<const char *, const Bone *> deform_parent_bones;

  for (auto pair : *deform_map) {
    if (pair.second) {
      Bone *parent = const_cast<Bone *>(pair.second)->parent;
      while (parent) {
        if (deform_parent_bones.find(parent->name) == deform_parent_bones.end()) {
          num_deform_parent_bones += 1;
        }
        deform_parent_bones.insert_or_assign(parent->name, parent);
        parent = parent->parent;
      }
    }

    deform_map->merge(deform_parent_bones);
  }
}

}  // namespace blender::io::usd
