/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "usd_armature_utils.h"

#include "BKE_armature.h"
#include "BKE_modifier.h"
#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"
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
static const ModifierData *get_enabled_modifier(const Object *obj,
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

void get_armature_bone_names(const Object *ob_arm, Vector<std::string> &r_names)
{
  auto visitor = [&r_names](const Bone *bone) { r_names.append(bone->name); };

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

void create_pose_joints(pxr::UsdSkelAnimation &skel_anim, const Object *obj)
{
  if (!(skel_anim && obj && obj->pose)) {
    return;
  }

  pxr::VtTokenArray joints;

  const bPose *pose = obj->pose;

  LISTBASE_FOREACH (const bPoseChannel *, pchan, &pose->chanbase) {
    if (pchan->bone) {
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
  Vector<ModifierData *> mods = get_enabled_modifiers(obj, depsgraph);

  /* We can export a skinned mesh if the object has an enabled
   * armature modifier and no other enabled modifiers. */
  return mods.size() == 1 && mods.first()->type == eModifierType_Armature;
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

}  // namespace blender::io::usd
