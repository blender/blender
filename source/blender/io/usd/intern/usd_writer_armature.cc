/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "usd_writer_armature.h"
#include "usd_armature_utils.h"
#include "usd_hierarchy_iterator.h"
#include "usd_writer_transform.h"

<<<<<<< HEAD
#include "BKE_armature.hh"
=======
#include "BKE_action.h"
#include "BKE_armature.hh"

>>>>>>> main
#include "DNA_armature_types.h"

#include "ED_armature.hh"

<<<<<<< HEAD
#include "WM_api.hh"

=======
>>>>>>> main
#include <pxr/base/gf/matrix4d.h>
#include <pxr/base/gf/matrix4f.h>
#include <pxr/usd/usdSkel/animation.h>
#include <pxr/usd/usdSkel/bindingAPI.h>
#include <pxr/usd/usdSkel/skeleton.h>
#include <pxr/usd/usdSkel/tokens.h>

#include <functional>
#include <iostream>

<<<<<<< HEAD
=======
#include "CLG_log.h"
static CLG_LogRef LOG = {"io.usd"};

>>>>>>> main
namespace usdtokens {
static const pxr::TfToken Anim("Anim", pxr::TfToken::Immortal);
}  // namespace usdtokens

<<<<<<< HEAD
=======
/* Get the pose matrix for the given channe.  The matrix is computed
 * relative to its parent, if a parent exists.  The returned matrix
 * corresponds to the USD joint-local transform. */
static pxr::GfMatrix4d parent_relative_pose_mat(const bPoseChannel *pchan)
{
  /* Note that the float matrix will be returned as GfMatrix4d, because
   * USD requires doubles. */
  const pxr::GfMatrix4f pose_mat(pchan->pose_mat);

  if (pchan->parent) {
    const pxr::GfMatrix4f parent_pose_mat(pchan->parent->pose_mat);
    const pxr::GfMatrix4f xf = pose_mat * parent_pose_mat.GetInverse();
    return pxr::GfMatrix4d(xf);
  }

  /* No parent, so return the pose matrix directly. */
  return pxr::GfMatrix4d(pose_mat);
}

>>>>>>> main
/* Initialize the given skeleton and animation from
 * the given armature object. */
static void initialize(const Object *obj,
                       pxr::UsdSkelSkeleton &skel,
                       pxr::UsdSkelAnimation &skel_anim,
<<<<<<< HEAD
                       std::unordered_map<const char *, const Bone *> *deform_bones)
=======
                       const blender::Map<blender::StringRef, const Bone *> *deform_bones)
>>>>>>> main
{
  using namespace blender::io::usd;

  pxr::VtTokenArray joints;
  pxr::VtArray<pxr::GfMatrix4d> bind_xforms;
  pxr::VtArray<pxr::GfMatrix4d> rest_xforms;

<<<<<<< HEAD
=======
  /* Function to collect the bind and rest transforms from each bone. */
>>>>>>> main
  auto visitor = [&](const Bone *bone) {
    if (!bone) {
      return;
    }

<<<<<<< HEAD
    if (deform_bones && deform_bones->find(bone->name) == deform_bones->end()) {
=======
    if (deform_bones && !deform_bones->contains(bone->name)) {
>>>>>>> main
      /* If deform_map is passed in, assume we're going deform-only.
       * Bones not found in the map should be skipped. */
      return;
    }

    joints.push_back(build_usd_joint_path(bone));
    const pxr::GfMatrix4f arm_mat(bone->arm_mat);
    bind_xforms.push_back(pxr::GfMatrix4d(arm_mat));

<<<<<<< HEAD
    if (bone->parent) {
=======
    /* Set the rest transform to the parent-relative pose matrix, or the parent-relative
     * armature matrix, if no pose channel exists. */
    if (const bPoseChannel *pchan = BKE_pose_channel_find_name(obj->pose, bone->name)) {
      rest_xforms.push_back(parent_relative_pose_mat(pchan));
    }
    else if (bone->parent) {
>>>>>>> main
      pxr::GfMatrix4f parent_arm_mat(bone->parent->arm_mat);
      const pxr::GfMatrix4f rest_mat = arm_mat * parent_arm_mat.GetInverse();
      rest_xforms.push_back(pxr::GfMatrix4d(rest_mat));
    }
    else {
      rest_xforms.push_back(pxr::GfMatrix4d(arm_mat));
    }
  };

  visit_bones(obj, visitor);
  skel.GetJointsAttr().Set(joints);
  skel.GetBindTransformsAttr().Set(bind_xforms);
  skel.GetRestTransformsAttr().Set(rest_xforms);

  pxr::UsdSkelBindingAPI usd_skel_api = pxr::UsdSkelBindingAPI::Apply(skel.GetPrim());
<<<<<<< HEAD
  usd_skel_api.CreateAnimationSourceRel().SetTargets(
      pxr::SdfPathVector({pxr::SdfPath(usdtokens::Anim)}));

  create_pose_joints(skel_anim, obj, deform_bones);
}

static const bPoseChannel *get_parent_pose_chan(const bPose *pose, const bPoseChannel *in_pchan)
{
  if (!(pose && in_pchan && in_pchan->bone && in_pchan->bone->parent)) {
    return nullptr;
  }

  Bone *parent = in_pchan->bone->parent;

  LISTBASE_FOREACH (const bPoseChannel *, pchan, &pose->chanbase) {
    if (pchan->bone == parent) {
      return pchan;
    }
  }

  return nullptr;
=======

  if (skel_anim) {
    usd_skel_api.CreateAnimationSourceRel().SetTargets(
        pxr::SdfPathVector({pxr::SdfPath(usdtokens::Anim)}));
    create_pose_joints(skel_anim, *obj, deform_bones);
  }
>>>>>>> main
}

/* Add skeleton transform samples from the armature pose channels. */
static void add_anim_sample(pxr::UsdSkelAnimation &skel_anim,
                            const Object *obj,
                            const pxr::UsdTimeCode time,
<<<<<<< HEAD
                            std::unordered_map<const char *, const Bone *> *deform_map)
=======
                            const blender::Map<blender::StringRef, const Bone *> *deform_map)
>>>>>>> main
{
  if (!(skel_anim && obj && obj->pose)) {
    return;
  }

  pxr::VtArray<pxr::GfMatrix4d> xforms;

  const bPose *pose = obj->pose;

  LISTBASE_FOREACH (const bPoseChannel *, pchan, &pose->chanbase) {
<<<<<<< HEAD
    if (!pchan->bone) {
      printf("WARNING: pchan %s is missing bone.\n", pchan->name);
      continue;
    }

    if (deform_map && deform_map->find(pchan->bone->name) == deform_map->end()) {
=======

    BLI_assert(pchan->bone);

    if (deform_map && !deform_map->contains(pchan->bone->name)) {
>>>>>>> main
      /* If deform_map is passed in, assume we're going deform-only.
       * Bones not found in the map should be skipped. */
      continue;
    }

<<<<<<< HEAD
    const pxr::GfMatrix4f pose_mat(pchan->pose_mat);

    if (const bPoseChannel *parent_pchan = get_parent_pose_chan(pose, pchan)) {
      const pxr::GfMatrix4f parent_pose_mat(parent_pchan->pose_mat);
      const pxr::GfMatrix4f xf = pose_mat * parent_pose_mat.GetInverse();
      xforms.push_back(pxr::GfMatrix4d(xf));
    }
    else {
      xforms.push_back(pxr::GfMatrix4d(pose_mat));
    }
=======
    xforms.push_back(parent_relative_pose_mat(pchan));
>>>>>>> main
  }

  skel_anim.SetTransforms(xforms, time);
}

namespace blender::io::usd {

USDArmatureWriter::USDArmatureWriter(const USDExporterContext &ctx) : USDAbstractWriter(ctx) {}

void USDArmatureWriter::do_write(HierarchyContext &context)
{
  if (!(context.object && context.object->type == OB_ARMATURE && context.object->data)) {
    BLI_assert_unreachable();
    return;
  }

  /* Create the skeleton. */
  pxr::UsdStageRefPtr stage = usd_export_context_.stage;
  pxr::UsdSkelSkeleton skel = pxr::UsdSkelSkeleton::Define(stage, usd_export_context_.usd_path);

  if (!skel) {
<<<<<<< HEAD
    WM_reportf(RPT_WARNING,
               "%s: couldn't define UsdSkelSkeleton %s\n",
               __func__,
               usd_export_context_.usd_path.GetString().c_str());
    return;
  }

  /* Create the skeleton animation primitive as a child of the skeleton. */
  pxr::SdfPath anim_path = usd_export_context_.usd_path.AppendChild(usdtokens::Anim);
  pxr::UsdSkelAnimation skel_anim = pxr::UsdSkelAnimation::Define(stage, anim_path);

  if (!skel_anim) {
    WM_reportf(RPT_WARNING,
               "%s: couldn't define UsdSkelAnimation %s\n",
               __func__,
               anim_path.GetString().c_str());
    return;
  }

  std::unordered_map<const char *, const Bone *> *use_deform =
      usd_export_context_.export_params.use_deform ? &deform_map_ : nullptr;

  if (!this->frame_has_been_written_) {
    init_deform_bones_map(context.object, use_deform);
    initialize(context.object, skel, skel_anim, use_deform);
  }

  add_anim_sample(skel_anim, context.object, get_export_time_code(), use_deform);
=======
    CLOG_WARN(&LOG,
              "Couldn't define UsdSkelSkeleton %s",
              usd_export_context_.usd_path.GetString().c_str());
    return;
  }

  pxr::UsdSkelAnimation skel_anim;

  if (usd_export_context_.export_params.export_animation) {
    /* Create the skeleton animation primitive as a child of the skeleton. */
    pxr::SdfPath anim_path = usd_export_context_.usd_path.AppendChild(usdtokens::Anim);
    skel_anim = pxr::UsdSkelAnimation::Define(stage, anim_path);

    if (!skel_anim) {
      CLOG_WARN(&LOG, "Couldn't define UsdSkelAnimation %s", anim_path.GetString().c_str());
      return;
    }
  }

  Map<StringRef, const Bone *> *deform_map = usd_export_context_.export_params.only_deform_bones ?
                                                 &deform_map_ :
                                                 nullptr;

  if (!this->frame_has_been_written_) {
    init_deform_bones_map(context.object, deform_map);
    initialize(context.object, skel, skel_anim, deform_map);
  }

  if (usd_export_context_.export_params.export_animation) {
    add_anim_sample(skel_anim, context.object, get_export_time_code(), deform_map);
  }
>>>>>>> main
}

bool USDArmatureWriter::check_is_animated(const HierarchyContext &context) const
{
  const Object *obj = context.object;

  if (!(obj && obj->type == OB_ARMATURE)) {
    return false;
  }

  return obj->adt != nullptr;
}

}  // namespace blender::io::usd
