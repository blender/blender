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
 * The Original Code is Copyright (C) 2021 NVIDIA Corporation.
 * All rights reserved.
 */

#include "usd_reader_skeleton.h"
#include "usd_skel_convert.h"

#include "BKE_idprop.h"
#include "BKE_action.h"
#include "BKE_armature.h"
#include "BKE_fcurve.h"
#include "BKE_object.h"

#include "BLI_math.h"
#include "BLI_string.h"
#include "BLI_string_utils.h"

#include "DNA_armature_types.h"
#include "DNA_object_types.h"

#include "ED_armature.h"
#include "ED_keyframing.h"

#include "MEM_guardedalloc.h"

#include "WM_api.h"

#include <pxr/pxr.h>
#include <pxr/usd/usdSkel/cache.h>
#include <pxr/usd/usdSkel/skeletonQuery.h>

#include <iostream>

namespace blender::io::usd {

/* Debugging utility to print the given skeleton's joint paths. */
static void print_joints(pxr::UsdSkelSkeleton &skel,
                         const double motionSampleTime)
{
  if (!skel) {
    return;
  }

  if (pxr::UsdAttribute joints_attr = skel.GetJointsAttr()) {
    pxr::VtArray<pxr::TfToken> joints;
    if (joints_attr.Get(&joints, motionSampleTime)) {
      std::cout << "Num joints " << joints.size() << std::endl;
      for (pxr::TfToken &joint : joints) {
        pxr::SdfPath joint_path(joint);
        std::cout << joint_path << std::endl;
      }
    }
  }
}

/* Code from the Collada importer's AnimationImporter class.  */
 static FCurve *create_fcurve(int array_index, const char *rna_path)
{
  FCurve *fcu = BKE_fcurve_create();
  fcu->flag = (FCURVE_VISIBLE | FCURVE_SELECTED);
  fcu->rna_path = BLI_strdupn(rna_path, strlen(rna_path));
  fcu->array_index = array_index;
  return fcu;
}

 /* Code from the Collada importer's AnimationImporter class.  */
static void add_bezt(FCurve *fcu,
                     float frame,
                     float value,
                     eBezTriple_Interpolation ipo = BEZT_IPO_LIN)
{
  BezTriple bez;
  memset(&bez, 0, sizeof(BezTriple));
  bez.vec[1][0] = frame;
  bez.vec[1][1] = value;
  bez.ipo = ipo; /* use default interpolation mode here... */
  bez.f1 = bez.f2 = bez.f3 = SELECT;
  bez.h1 = bez.h2 = HD_AUTO;
  insert_bezt_fcurve(fcu, &bez, INSERTKEY_NOFLAGS);
  BKE_fcurve_handles_recalc(fcu);
}

/* Example code for adding a bone and creating curves for
 * animating its translation. */
static void test_add_bone_anim(Main *bmain, Object *obj)
{
  if (!obj || !obj->data || !bmain) {
    return;
  }

  bArmature *arm = static_cast<bArmature *>(obj->data);

  ED_armature_to_edit(arm);

  const char *bone_name = "Bone";

  EditBone * bone = ED_armature_ebone_add(arm, bone_name);

  /* Blender will cull zero-length bones, so we give
   * the bone an arbitrary size. */
  float v0[3] = { 0.0f, 0.0f, 0.0f };
  float v1[3] = { 0.0f, 1.0f, 0.0f };
  copy_v3_v3(bone->head, v0);
  copy_v3_v3(bone->tail, v1);

  ED_armature_from_edit(bmain, arm);
  ED_armature_edit_free(arm);

  /* As an arbitrary test, translate the bone in Y for 60 frames. */

  bAction *act = ED_id_action_ensure(bmain, (ID *)&obj->id);

  bActionGroup *grp = action_groups_add_new(act, bone_name);
  const char *rna_path = "pose.bones[\"Bone\"].location";
  FCurve *fcu0 = create_fcurve(0, rna_path);
  fcu0->totvert = 60;
  FCurve *fcu1 = create_fcurve(1, rna_path);
  fcu1->totvert = 60;
  FCurve *fcu2 = create_fcurve(2, rna_path);
  fcu2->totvert = 60;

  for (int i = 1; i < 61; ++i) {
    add_bezt(fcu0, static_cast<float>(i), 0.0f);
    add_bezt(fcu1, static_cast<float>(i), static_cast<float>(i) * 0.1f);
    add_bezt(fcu2, static_cast<float>(i), 0.0f);
  }

  action_groups_add_channel(act, grp, fcu0);
  action_groups_add_channel(act, grp, fcu1);
  action_groups_add_channel(act, grp, fcu2);
}

bool USDSkeletonReader::valid() const
{
  return skel_ && USDXformReader::valid();
}

void USDSkeletonReader::create_object(Main *bmain, const double /* motionSampleTime */)
{
  object_ = BKE_object_add_only_object(bmain, OB_ARMATURE, name_.c_str());

  bArmature *arm = BKE_armature_add(bmain, name_.c_str());
  object_->data = arm;
}

void USDSkeletonReader::read_object_data(Main *bmain, const double motionSampleTime)
{
  if (!object_ || !object_->data || !skel_) {
    return;
  }

  pxr::UsdSkelCache skel_cache;
  pxr::UsdSkelSkeletonQuery skel_query = skel_cache.GetSkelQuery(skel_);

  if (!skel_query.IsValid()) {
    std::cout << "WARNING: couldn't query skeleton " << skel_.GetPath() << std::endl;
    return;
  }

  const pxr::UsdSkelTopology &skel_topology = skel_query.GetTopology();

  pxr::VtTokenArray joint_order = skel_query.GetJointOrder();

  if (joint_order.size() != skel_topology.size()) {
    std::cout << "WARNING: skel topology and joint order size mismatch\n";
    return;
  }

  bArmature *arm = static_cast<bArmature *>(object_->data);

  ED_armature_to_edit(arm);

  /* The bones we create, stored in the skeleton's joint order. */
  std::vector<EditBone *> edit_bones;

  size_t num_joints = skel_topology.GetNumJoints();

  /* Keep track of the bones we create for each joint. */
  std::map<pxr::TfToken, std::string> joint_to_bone_map;

  /* Create the bones. */
  for (const pxr::TfToken &joint : joint_order) {
    std::string name = pxr::SdfPath(joint).GetName();
    EditBone * bone = ED_armature_ebone_add(arm, name.c_str());
    if (!bone) {
      std::cout << "WARNING: couldn't add bone for joint " << joint << std::endl;
      edit_bones.push_back(nullptr);
      continue;
    }
    joint_to_bone_map.insert(std::make_pair(joint, bone->name));
    edit_bones.push_back(bone);
  }

  /* Sanity check: we should have created a bone for each joint. */

  if (edit_bones.size() != num_joints) {
    std::cout << "WARNING: mismatch in bone and joint counts for skeleton " << skel_.GetPath() << std::endl;
    return;
  }

  /* Record the child bone indices per parent bone. */
  std::vector<std::vector<int>> child_bones(num_joints);

  /* Set bone parenting. */
  for (size_t i = 0; i < num_joints; ++i) {
    int parent_idx = skel_topology.GetParent(i);
    if (parent_idx < 0) {
      continue;
    }
    if (parent_idx >= edit_bones.size()) {
      std::cout << "WARNING: out of bounds parent index for bone " << pxr::SdfPath(joint_order[i])
                << " for skeleton " << skel_.GetPath() << std::endl;
      continue;
    }

    child_bones[parent_idx].push_back(i);
    if (edit_bones[i] && edit_bones[parent_idx]) {
      edit_bones[i]->parent = edit_bones[parent_idx];
    }
  }

  /* Skeleton-space joint bind transforms. */
  pxr::VtMatrix4dArray bind_xforms;
  if (!compute_skel_space_bind_transforms(skel_query, bind_xforms, 0.0f)) {
    std::cout << "WARNING: couldn't get skeleton space bind xforms for skeleton "
              << skel_.GetPath() << std::endl;
    return;
  }

  if (bind_xforms.size() != num_joints) {
    std::cout << "WARNING: mismatch in local space rest xforms and joint counts for skeleton " << skel_.GetPath() << std::endl;
    return;
  }

  /* Check if any bone natrices have negative determinants,
   * indicating negative scales, possibly due to mirroring
   * operations.  Such matrices can't be propery converted
   * to Blender's axis/roll bone representation (see
   * https://developer.blender.org/T82930).  If we detect
   * such matrices, we will flag an error and won't try
   * to import the animation, since the rotations would
   * be incorrect in such cases.  Unfortunately, the Pixar
   * UsdSkel examples of the "HumanFemale" suffer from
   * this issue. */
  bool negative_determinant = false;

  /* Set bone rest transforms. */
  for (size_t i = 0; i < num_joints; ++i) {
    EditBone *ebone = edit_bones[i];

    if (!ebone) {
      continue;
    }

    pxr::GfMatrix4f mat(bind_xforms[i]);

    float mat4[4][4];
    mat.Get(mat4);

    pxr::GfVec3f head(0.0f, 0.0f, 0.0f);
    pxr::GfVec3f tail(0.0f, 1.0f, 0.0f);

    copy_v3_v3(ebone->head, head.data());
    copy_v3_v3(ebone->tail, tail.data());

    ED_armature_ebone_from_mat4(ebone, mat4);

    if (mat.GetDeterminant() < 0.0) {
      negative_determinant = true;
    }
  }

  bool valid_skeleton = true;
  if (negative_determinant) {
    valid_skeleton = false;
    WM_reportf(RPT_WARNING,
               "USD Skeleton Import: bone matrices with negative determinants detected in prim %s."
               "Such matrices may indicate negative scales, possibly due to mirroring operations, "
               "and can't currently be converted to Blender's bone representation.  "
               "The skeletal animation won't be imported", prim_.GetPath().GetAsString().c_str());
  }

  /* Scale bones to account for separation between parents and
   * children, so that the bone size is in proportion with the
   * overall skeleton hierarchy.  USD skeletons are composed of
   * joints which we imperfectly represent as bones. */

  float avg_len_scale = 0;
  for (size_t i = 0; i < num_joints; ++i) {

    /* If the bone has any children, scale its length
     * by the distance between this bone's head
     * and the average head location of its children. */

    if (child_bones[i].empty()) {
      continue;
    }

    EditBone *parent = edit_bones[i];
    if (!parent) {
      continue;
    }

    pxr::GfVec3f avg_child_head(0);
    for (int j : child_bones[i]) {
      EditBone *child = edit_bones[j];
      if (!child) {
        continue;
      }
      pxr::GfVec3f child_head(child->head);
      avg_child_head += child_head;
    }

    avg_child_head /= child_bones[i].size();

    pxr::GfVec3f parent_head(parent->head);
    pxr::GfVec3f parent_tail(parent->tail);

    float new_len = (avg_child_head - parent_head).GetLength();

    /* Be sure not to scale by zero. */
    if (new_len > .00001) {
      parent_tail = parent_head + (parent_tail - parent_head).GetNormalized() * new_len;
      copy_v3_v3(parent->tail, parent_tail.data());
      avg_len_scale += new_len;
    }
  }

  /* Scale terminal bones by the average length scale. */
  avg_len_scale /= num_joints;

  if (avg_len_scale > .00001) {
    for (size_t i = 0; i < num_joints; ++i) {
      if (!child_bones[i].empty()) {
        continue;
      }
      EditBone *bone = edit_bones[i];
      if (!bone) {
        continue;
      }
      pxr::GfVec3f head(bone->head);
      pxr::GfVec3f tail(bone->tail);
      tail = head + (tail - head).GetNormalized() * avg_len_scale;
      copy_v3_v3(bone->tail, tail.data());
    }
  }

  ED_armature_from_edit(bmain, arm);
  ED_armature_edit_free(arm);

  if (valid_skeleton) {
    create_skeleton_curves(bmain, object_, skel_query, joint_to_bone_map);
  }

  USDXformReader::read_object_data(bmain, motionSampleTime);
}

}  // namespace blender::io::usd
