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
 * The Original Code is Copyright (C) 2021 Blender Foundation.
 * All rights reserved.
 */
#include "usd_writer_armature.h"
#include "usd_hierarchy_iterator.h"
#include "usd_writer_transform.h"

#include "BKE_armature.h"
#include "DNA_armature_types.h"

#include "ED_armature.hh"

#include <pxr/base/gf/matrix4d.h>
#include <pxr/base/gf/matrix4f.h>
#include <pxr/usd/usdSkel/animation.h>
#include <pxr/usd/usdSkel/bindingAPI.h>
#include <pxr/usd/usdSkel/skeleton.h>
#include <pxr/usd/usdSkel/tokens.h>

#include <iostream>
#include <string>
#include <vector>

namespace usdtokens {
static const pxr::TfToken Anim("Anim", pxr::TfToken::Immortal);
}  // namespace usdtokens

static std::string build_path(const Bone *bone)
{
  std::string path(pxr::TfMakeValidIdentifier(bone->name));

  const Bone *parent = bone->parent;
  while (parent) {
    path = pxr::TfMakeValidIdentifier(parent->name) + std::string("/") + path;
    parent = parent->parent;
  }

  return path;
}

namespace {

struct BoneVisitor {
 public:
  virtual void Visit(const Bone *bone) = 0;
};

struct BoneNameList : public BoneVisitor {
  std::vector<std::string> *names;

  BoneNameList(std::vector<std::string> *in_names) : names(in_names) {}

  void Visit(const Bone *bone) override
  {
    if (bone && names) {
      names->push_back(bone->name);
    }
  }
};

struct BoneDataBuilder : public BoneVisitor {

  std::vector<std::string> paths;

  pxr::VtArray<pxr::GfMatrix4d> bind_xforms;

  pxr::VtArray<pxr::GfMatrix4d> rest_xforms;

  pxr::GfMatrix4f world_mat;

  BoneDataBuilder(const pxr::GfMatrix4f &in_world_mat) : world_mat(in_world_mat) {}

  void Visit(const Bone *bone) override
  {
    if (!bone) {
      return;
    }
    paths.push_back(build_path(bone));

    pxr::GfMatrix4f arm_mat(bone->arm_mat);

    pxr::GfMatrix4f bind_xf = arm_mat * world_mat;

    bind_xforms.push_back(pxr::GfMatrix4d(bind_xf));

    if (bone->parent) {
      pxr::GfMatrix4f parent_arm_mat(bone->parent->arm_mat);
      pxr::GfMatrix4f rest_xf = arm_mat * parent_arm_mat.GetInverse();
      rest_xforms.push_back(pxr::GfMatrix4d(rest_xf));
    }
    else {
      rest_xforms.push_back(pxr::GfMatrix4d(arm_mat));
    }
  }
};

}  // End anonymous namespace

static void visit_bones(const Bone *bone, BoneVisitor *visitor)
{
  if (!(bone && visitor)) {
    return;
  }

  visitor->Visit(bone);

  for (Bone *child = (Bone *)bone->childbase.first; child; child = child->next) {
    visit_bones(child, visitor);
  }
}

static void visit_bones(const Object *ob_arm, BoneVisitor *visitor)
{
  bArmature *armature = (bArmature *)ob_arm->data;

  for (Bone *bone = (Bone *)armature->bonebase.first; bone; bone = bone->next) {
    visit_bones(bone, visitor);
  }
}

static void create_pose_joints(const pxr::UsdSkelAnimation &skel_anim, Object *obj)
{
  if (!(skel_anim && obj && obj->pose)) {
    return;
  }

  pxr::VtTokenArray joints;

  bPose *pose = obj->pose;

  LISTBASE_FOREACH (bPoseChannel *, pchan, &pose->chanbase) {
    if (pchan->bone) {
      joints.push_back(pxr::TfToken(build_path(pchan->bone)));
    }
  }

  skel_anim.GetJointsAttr().Set(joints);
}

static bPoseChannel *get_parent_pose_chan(bPose *pose, bPoseChannel *in_pchan)
{
  if (!(pose && in_pchan && in_pchan->bone && in_pchan->bone->parent)) {
    return nullptr;
  }

  Bone *parent = in_pchan->bone->parent;

  LISTBASE_FOREACH (bPoseChannel *, pchan, &pose->chanbase) {
    if (pchan->bone == parent) {
      return pchan;
    }
  }

  return nullptr;
}

static void add_anim_sample(const pxr::UsdSkelAnimation &skel_anim,
                            Object *obj,
                            pxr::UsdTimeCode time)
{
  if (!(skel_anim && obj && obj->pose)) {
    return;
  }

  pxr::VtArray<pxr::GfMatrix4d> xforms;

  bPose *pose = obj->pose;

  LISTBASE_FOREACH (bPoseChannel *, pchan, &pose->chanbase) {

    if (!pchan->bone) {
      printf("WARNING: pchan %s is missing bone.\n", pchan->name);
      continue;
    }

    pxr::GfMatrix4f pose_mat(pchan->pose_mat);

    if (bPoseChannel *parent_pchan = get_parent_pose_chan(pose, pchan)) {
      pxr::GfMatrix4f parent_pose_mat(parent_pchan->pose_mat);
      pxr::GfMatrix4f xf = pose_mat * parent_pose_mat.GetInverse();
      xforms.push_back(pxr::GfMatrix4d(xf));
    }
    else {
      xforms.push_back(pxr::GfMatrix4d(pose_mat));
    }
  }

  skel_anim.SetTransforms(xforms, time);
}

namespace blender::io::usd {

void USDArmatureWriter::get_armature_bone_names(Object *obj, std::vector<std::string> &r_names)
{
  if (!obj) {
    return;
  }

  BoneNameList name_list(&r_names);
  visit_bones(obj, &name_list);
}

USDArmatureWriter::USDArmatureWriter(const USDExporterContext &ctx) : USDAbstractWriter(ctx) {}

void USDArmatureWriter::do_write(HierarchyContext &context)
{
  if (!context.object) {
    printf("WARNING in USDArmatureWriter::do_write: null object\n");
    return;
  }

  if (context.object->type != OB_ARMATURE) {
    printf("WARNING in USDArmatureWriter::do_write: object is not an armature\n");
    return;
  }

  if (context.object->data == nullptr) {
    printf("WARNING in USDArmatureWriter::do_write: null object data\n");
    return;
  }

  pxr::UsdStageRefPtr stage = usd_export_context_.stage;
  pxr::UsdTimeCode timecode = get_export_time_code();

  pxr::UsdSkelSkeleton usd_skel = (usd_export_context_.export_params.export_as_overs) ?
                                      pxr::UsdSkelSkeleton(
                                          stage->OverridePrim(usd_export_context_.usd_path)) :
                                      pxr::UsdSkelSkeleton::Define(stage,
                                                                   usd_export_context_.usd_path);

  if (!usd_skel) {
    printf("WARNING: Couldn't define Skeleton %s\n",
           usd_export_context_.usd_path.GetString().c_str());
    return;
  }

  pxr::UsdSkelAnimation usd_skel_anim;

  if (usd_export_context_.export_params.export_animation) {

    /* Create the SkelAnimation primitive. */

    /* TODO: Right now there is a remote possibility that the SkelAnimation path will clash
     * with the USD path for another object in the scene.  Look into extending USDHierarchyIterator
     * with a function that will provide a USD path that's guranteed to be unique (e.g., by
     * examining paths of all the writers in the writer map).  The USDHierarchyIterator
     * can be accessed for such a query like this:
     * this->usd_export_context_.hierarchy_iterator */

    pxr::SdfPath anim_path = usd_export_context_.usd_path.AppendChild(usdtokens::Anim);

    usd_skel_anim = (usd_export_context_.export_params.export_as_overs) ?
                        pxr::UsdSkelAnimation(stage->OverridePrim(anim_path)) :
                        pxr::UsdSkelAnimation::Define(stage, anim_path);

    if (!usd_skel_anim) {
      printf("WARNING: Couldn't define SkelAnim %s\n", anim_path.GetString().c_str());
    }
  }

  if (!this->frame_has_been_written_) {

    pxr::GfMatrix4f world_mat(context.matrix_world);
    /* The context world matrix does not include the unit
     * conversion scaling or axis rotation that may be applied
     * to root primitives on export, so we must include those,
     * if necessary. */
    float convert_mat[4][4];
    get_export_conversion_matrix(usd_export_context_.export_params, convert_mat);

    BoneDataBuilder bone_data(world_mat * pxr::GfMatrix4f(convert_mat));

    visit_bones(context.object, &bone_data);

    if (!bone_data.paths.empty()) {
      pxr::VtTokenArray joints(bone_data.paths.size());

      for (int i = 0; i < bone_data.paths.size(); ++i) {
        joints[i] = pxr::TfToken(bone_data.paths[i]);
      }

      usd_skel.GetJointsAttr().Set(joints);
    }

    usd_skel.GetBindTransformsAttr().Set(bone_data.bind_xforms);
    usd_skel.GetRestTransformsAttr().Set(bone_data.rest_xforms);

    if (usd_skel_anim) {
      pxr::UsdSkelBindingAPI usd_skel_api = pxr::UsdSkelBindingAPI::Apply(usd_skel.GetPrim());
      usd_skel_api.CreateAnimationSourceRel().SetTargets(
          pxr::SdfPathVector({pxr::SdfPath(usdtokens::Anim)}));

      create_pose_joints(usd_skel_anim, context.object);
    }
  }

  if (usd_skel_anim) {
    add_anim_sample(usd_skel_anim, context.object, timecode);
  }
}

bool USDArmatureWriter::check_is_animated(const HierarchyContext &context) const
{
  const Object *obj = context.object;

  if (!(obj && obj->type == OB_ARMATURE)) {
    return false;
  }

  /* TODO(makowalski): Is this a sufficient check? */
  return obj->adt != nullptr;
}

}  // namespace blender::io::usd
