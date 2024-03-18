/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "usd_armature_utils.hh"

#include "BKE_armature.hh"
#include "BKE_modifier.hh"
#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_query.hh"
#include "DNA_armature_types.h"
#include "ED_armature.hh"

#include "WM_api.hh"

namespace blender::io::usd {

/* Recursively invoke the 'visitor' function on the given bone and its children. */
static void visit_bones(const Bone *bone, FunctionRef<void(const Bone *)> visitor)
{
  if (!(bone && visitor)) {
    return;
  }

  visitor(bone);

  LISTBASE_FOREACH (const Bone *, child, &bone->childbase) {
    visit_bones(child, visitor);
  }
}

const ModifierData *get_enabled_modifier(const Object &obj,
                                         ModifierType type,
                                         const Depsgraph *depsgraph)
{
  BLI_assert(depsgraph);

  Scene *scene = DEG_get_input_scene(depsgraph);
  eEvaluationMode mode = DEG_get_mode(depsgraph);

  LISTBASE_FOREACH (ModifierData *, md, &obj.modifiers) {

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
static const ArmatureModifierData *get_armature_modifier(const Object &obj,
                                                         const Depsgraph *depsgraph)
{
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

  LISTBASE_FOREACH (const Bone *, bone, &armature->bonebase) {
    visit_bones(bone, visitor);
  }
}

void get_armature_bone_names(const Object *ob_arm,
                             const bool use_deform,
                             Vector<std::string> &r_names)
{
  Map<StringRef, const Bone *> deform_map;
  if (use_deform) {
    init_deform_bones_map(ob_arm, &deform_map);
  }

  auto visitor = [&](const Bone *bone) {
    if (use_deform && !deform_map.contains(bone->name)) {
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
                        const Object &obj,
                        const Map<StringRef, const Bone *> *deform_map)
{
  BLI_assert(obj.pose);

  pxr::VtTokenArray joints;

  const bPose *pose = obj.pose;

  LISTBASE_FOREACH (const bPoseChannel *, pchan, &pose->chanbase) {
    if (pchan->bone) {
      if (deform_map && !deform_map->contains(pchan->bone->name)) {
        /* If deform_map is passed in, assume we're going deform-only.
         * Bones not found in the map should be skipped. */
        continue;
      }

      joints.push_back(build_usd_joint_path(pchan->bone));
    }
  }

  skel_anim.GetJointsAttr().Set(joints);
}

const Object *get_armature_modifier_obj(const Object &obj, const Depsgraph *depsgraph)
{
  const ArmatureModifierData *mod = get_armature_modifier(obj, depsgraph);
  return mod ? mod->object : nullptr;
}

bool is_armature_modifier_bone_name(const Object &obj,
                                    const StringRefNull name,
                                    const Depsgraph *depsgraph)
{
  const ArmatureModifierData *arm_mod = get_armature_modifier(obj, depsgraph);

  if (!arm_mod || !arm_mod->object || !arm_mod->object->data) {
    return false;
  }

  bArmature *arm = static_cast<bArmature *>(arm_mod->object->data);

  return BKE_armature_find_bone_name(arm, name.c_str());
}

bool can_export_skinned_mesh(const Object &obj, const Depsgraph *depsgraph)
{
  return get_enabled_modifier(obj, eModifierType_Armature, depsgraph) != nullptr;
}

void init_deform_bones_map(const Object *obj, Map<StringRef, const Bone *> *deform_map)
{
  if (!deform_map) {
    return;
  }

  deform_map->clear();

  auto deform_visitor = [&](const Bone *bone) {
    if (!bone) {
      return;
    }

    const bool deform = !(bone->flag & BONE_NO_DEFORM);
    if (deform) {
      deform_map->add(bone->name, bone);
    }
  };

  visit_bones(obj, deform_visitor);

  /* Get deform parents */
  for (const auto &item : deform_map->items()) {
    BLI_assert(item.value);
    for (const Bone *parent = item.value->parent; parent; parent = parent->parent) {
      deform_map->add(parent->name, parent);
    }
  }
}

}  // namespace blender::io::usd
