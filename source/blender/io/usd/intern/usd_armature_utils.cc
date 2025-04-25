/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "usd_armature_utils.hh"
#include "usd_utils.hh"

#include "ANIM_action.hh"
#include "ANIM_fcurve.hh"

#include "BKE_armature.hh"
#include "BKE_fcurve.hh"
#include "BKE_modifier.hh"

#include "BLI_listbase.h"
#include "BLI_string_ref.hh"
#include "BLI_vector.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_query.hh"
#include "DNA_action_types.h"
#include "DNA_armature_types.h"

namespace blender::io::usd {

/* Utility: create new fcurve and add it as a channel to a group. */
FCurve *create_fcurve(blender::animrig::Channelbag &channelbag,
                      const blender::animrig::FCurveDescriptor &fcurve_descriptor,
                      const int sample_count)
{
  FCurve *fcurve = channelbag.fcurve_create_unique(nullptr, fcurve_descriptor);
  BLI_assert_msg(fcurve, "The same F-Curve is being created twice, this is unexpected.");
  BKE_fcurve_bezt_resize(fcurve, sample_count);
  return fcurve;
}

/* Utility: fill in a single fcurve sample at the provided index. */
void set_fcurve_sample(FCurve *fcu, int64_t sample_index, const float frame, const float value)
{
  BLI_assert(sample_index >= 0 && sample_index < fcu->totvert);
  BezTriple &bez = fcu->bezt[sample_index];
  bez.vec[1][0] = frame;
  bez.vec[1][1] = value;
  bez.ipo = BEZT_IPO_LIN;
  bez.f1 = bez.f2 = bez.f3 = SELECT;
  bez.h1 = bez.h2 = HD_AUTO;
}

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

  const Scene *scene = DEG_get_input_scene(depsgraph);
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

  const bArmature *armature = (bArmature *)ob_arm->data;
  LISTBASE_FOREACH (const Bone *, bone, &armature->bonebase) {
    visit_bones(bone, visitor);
  }
}

void get_armature_bone_names(const Object *ob_arm,
                             const bool use_deform,
                             Vector<StringRef> &r_names)
{
  Map<StringRef, const Bone *> deform_map;
  if (use_deform) {
    init_deform_bones_map(ob_arm, &deform_map);
  }

  auto visitor = [&](const Bone *bone) {
    const StringRef bone_name(bone->name);
    if (use_deform && !deform_map.contains(bone_name)) {
      return;
    }

    r_names.append(bone_name);
  };

  visit_bones(ob_arm, visitor);
}

pxr::TfToken build_usd_joint_path(const Bone *bone, bool allow_unicode)
{
  std::string path(make_safe_name(bone->name, allow_unicode));

  const Bone *parent = bone->parent;
  while (parent) {
    path = make_safe_name(parent->name, allow_unicode) + '/' + path;
    parent = parent->parent;
  }

  return pxr::TfToken(path);
}

void create_pose_joints(pxr::UsdSkelAnimation &skel_anim,
                        const Object &obj,
                        const Map<StringRef, const Bone *> *deform_map,
                        bool allow_unicode)
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

      joints.push_back(build_usd_joint_path(pchan->bone, allow_unicode));
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
