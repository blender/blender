/* SPDX-FileCopyrightText: 2023 NVIDIA Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "usd_skel_convert.h"

#include "usd.h"

#include <pxr/usd/usdSkel/animation.h>
#include <pxr/usd/usdSkel/bindingAPI.h>
#include <pxr/usd/usdSkel/blendShape.h>
#include <pxr/usd/usdSkel/cache.h>
#include <pxr/usd/usdSkel/skeletonQuery.h>
#include <pxr/usd/usdSkel/utils.h>

#include "DNA_anim_types.h"
#include "DNA_armature_types.h"
#include "DNA_key_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_meta_types.h"
#include "DNA_scene_types.h"

#include "BKE_action.h"
#include "BKE_armature.h"
#include "BKE_deform.h"
#include "BKE_fcurve.h"
#include "BKE_key.h"
#include "BKE_lib_id.h"
#include "BKE_mesh.h"
#include "BKE_mesh_runtime.hh"
#include "BKE_modifier.h"
#include "BKE_object.h"
#include "BKE_object_deform.h"

#include "BLI_math_vector.h"

#include "ED_armature.hh"
#include "ED_keyframing.hh"
#include "ED_mesh.hh"

#include "WM_api.hh"

#include <iostream>
#include <string>
#include <vector>

namespace {

/* Utility: return the magnitude of the largest component
 * of the given vector. */
inline float max_mag_component(const pxr::GfVec3d &vec)
{
  return pxr::GfMax(pxr::GfAbs(vec[0]), pxr::GfAbs(vec[1]), pxr::GfAbs(vec[2]));
}

/* Utility: create curve at the given array index. */
FCurve *create_fcurve(const int array_index, const std::string &rna_path)
{
  FCurve *fcu = BKE_fcurve_create();
  fcu->flag = (FCURVE_VISIBLE | FCURVE_SELECTED);
  fcu->rna_path = BLI_strdup(rna_path.c_str());
  fcu->array_index = array_index;
  return fcu;
}

/* Utility: create curve at the given array index and
 * add it as a channel to a group. */
FCurve *create_chan_fcurve(bAction *act,
                           bActionGroup *grp,
                           const int array_index,
                           const std::string &rna_path,
                           const int totvert)
{
  FCurve *fcu = create_fcurve(array_index, rna_path);
  fcu->totvert = totvert;
  action_groups_add_channel(act, grp, fcu);
  return fcu;
}

/* Utility: add curve sample. */
void add_bezt(FCurve *fcu,
              const float frame,
              const float value,
              const eBezTriple_Interpolation ipo = BEZT_IPO_LIN)
{
  BezTriple bez;
  memset(&bez, 0, sizeof(BezTriple));
  bez.vec[1][0] = frame;
  bez.vec[1][1] = value;
  bez.ipo = ipo; /* use default interpolation mode here... */
  bez.f1 = bez.f2 = bez.f3 = SELECT;
  bez.h1 = bez.h2 = HD_AUTO;
  insert_bezt_fcurve(fcu, &bez, INSERTKEY_NOFLAGS);
}

/**
 * Import a USD skeleton animation as an action on the given armature object.
 * This assumes bones have already been created on the armature.
 *
 * \param bmain: Main pointer
 * \param arm_obj: Armature object to which the action will be added
 * \param skel_query: The USD skeleton query for reading the animation
 * \param joint_to_bone_map: Map a USD skeleton joint name to a bone name
 */
void import_skeleton_curves(Main *bmain,
                            Object *arm_obj,
                            const pxr::UsdSkelSkeletonQuery &skel_query,
                            const std::map<pxr::TfToken, std::string> &joint_to_bone_map)

{
  if (!(bmain && arm_obj && skel_query)) {
    return;
  }

  if (joint_to_bone_map.empty()) {
    return;
  }

  const pxr::UsdSkelAnimQuery &anim_query = skel_query.GetAnimQuery();

  if (!anim_query) {
    /* No animation is defined. */
    return;
  }

  std::vector<double> samples;
  anim_query.GetJointTransformTimeSamples(&samples);

  if (samples.empty()) {
    return;
  }

  const size_t num_samples = samples.size();

  /* Create the action on the armature. */
  bAction *act = ED_id_action_ensure(bmain, (ID *)&arm_obj->id);

  /* Create the curves. */

  /* Get the joint paths. */
  pxr::VtTokenArray joint_order = skel_query.GetJointOrder();

  std::vector<FCurve *> loc_curves;
  std::vector<FCurve *> rot_curves;
  std::vector<FCurve *> scale_curves;

  /* Iterate over the joints and create the corresponding curves for the bones. */
  for (const pxr::TfToken &joint : joint_order) {
    std::map<pxr::TfToken, std::string>::const_iterator it = joint_to_bone_map.find(joint);

    if (it == joint_to_bone_map.end()) {
      /* This joint doesn't correspond to any bone we created.
       * Add null placeholders for the channel curves. */
      loc_curves.push_back(nullptr);
      loc_curves.push_back(nullptr);
      loc_curves.push_back(nullptr);
      rot_curves.push_back(nullptr);
      rot_curves.push_back(nullptr);
      rot_curves.push_back(nullptr);
      rot_curves.push_back(nullptr);
      scale_curves.push_back(nullptr);
      scale_curves.push_back(nullptr);
      scale_curves.push_back(nullptr);
      continue;
    }

    bActionGroup *grp = action_groups_add_new(act, it->second.c_str());

    /* Add translation curves. */
    std::string rna_path = "pose.bones[\"" + it->second + "\"].location";
    loc_curves.push_back(create_chan_fcurve(act, grp, 0, rna_path, num_samples));
    loc_curves.push_back(create_chan_fcurve(act, grp, 1, rna_path, num_samples));
    loc_curves.push_back(create_chan_fcurve(act, grp, 2, rna_path, num_samples));

    /* Add rotation curves. */
    rna_path = "pose.bones[\"" + it->second + "\"].rotation_quaternion";
    rot_curves.push_back(create_chan_fcurve(act, grp, 0, rna_path, num_samples));
    rot_curves.push_back(create_chan_fcurve(act, grp, 1, rna_path, num_samples));
    rot_curves.push_back(create_chan_fcurve(act, grp, 2, rna_path, num_samples));
    rot_curves.push_back(create_chan_fcurve(act, grp, 3, rna_path, num_samples));

    /* Add scale curves. */
    rna_path = "pose.bones[\"" + it->second + "\"].scale";
    scale_curves.push_back(create_chan_fcurve(act, grp, 0, rna_path, num_samples));
    scale_curves.push_back(create_chan_fcurve(act, grp, 1, rna_path, num_samples));
    scale_curves.push_back(create_chan_fcurve(act, grp, 2, rna_path, num_samples));
  }

  /* Sanity checks: make sure we have a curve entry for each joint. */
  if (loc_curves.size() != joint_order.size() * 3) {
    std::cout << "PROGRAMMER ERROR: location curve count mismatch\n";
    return;
  }

  if (rot_curves.size() != joint_order.size() * 4) {
    std::cout << "PROGRAMMER ERROR: rotation curve count mismatch\n";
    return;
  }

  if (scale_curves.size() != joint_order.size() * 3) {
    std::cout << "PROGRAMMER ERROR: scale curve count mismatch\n";
    return;
  }

  /* The curve for each joint represents the transform relative
   * to the bind transform in joint-local space. I.e.,
   *
   * `jointLocalTransform * inv(jointLocalBindTransform)`
   *
   * There doesn't appear to be a way to query the joint-local
   * bind transform through the API, so we have to compute it
   * ourselves from the world bind transforms and the skeleton
   * topology.
   */

  /* Get the world space joint transforms at bind time. */
  pxr::VtMatrix4dArray bind_xforms;
  if (!skel_query.GetJointWorldBindTransforms(&bind_xforms)) {
    WM_reportf(RPT_WARNING,
               "%s: Couldn't get world bind transforms for skeleton %s",
               __func__,
               skel_query.GetSkeleton().GetPrim().GetPath().GetAsString().c_str());
    return;
  }

  if (bind_xforms.size() != joint_order.size()) {
    WM_reportf(RPT_WARNING,
               "%s: Number of bind transforms doesn't match the number of joints for skeleton %s",
               __func__,
               skel_query.GetSkeleton().GetPrim().GetPath().GetAsString().c_str());
    return;
  }

  const pxr::UsdSkelTopology &skel_topology = skel_query.GetTopology();

  pxr::VtMatrix4dArray joint_local_bind_xforms(bind_xforms.size());
  for (int i = 0; i < bind_xforms.size(); ++i) {
    const int parent_id = skel_topology.GetParent(i);

    if (parent_id >= 0) {
      /* This is a non-root joint.  Compute the bind transform of the joint
       * relative to its parent. */
      joint_local_bind_xforms[i] = bind_xforms[i] * bind_xforms[parent_id].GetInverse();
    }
    else {
      /* This is the root joint. */
      joint_local_bind_xforms[i] = bind_xforms[i];
    }
  }

  /* Set the curve samples. */
  for (const double frame : samples) {
    pxr::VtMatrix4dArray joint_local_xforms;
    if (!skel_query.ComputeJointLocalTransforms(&joint_local_xforms, frame)) {
      std::cout << "WARNING: couldn't compute joint local transforms on frame " << frame
                << std::endl;
      continue;
    }

    if (joint_local_xforms.size() != joint_order.size()) {
      std::cout << "WARNING: number of joint local transform entries " << joint_local_xforms.size()
                << " doesn't match the number of joints " << joint_order.size() << std::endl;
      continue;
    }

    for (int i = 0; i < joint_local_xforms.size(); ++i) {
      pxr::GfMatrix4d bone_xform = joint_local_xforms[i] * joint_local_bind_xforms[i].GetInverse();

      pxr::GfVec3f t;
      pxr::GfQuatf qrot;
      pxr::GfVec3h s;

      if (!pxr::UsdSkelDecomposeTransform(bone_xform, &t, &qrot, &s)) {
        std::cout << "WARNING: error decomposing matrix on frame " << frame << std::endl;
        continue;
      }

      const float re = qrot.GetReal();
      const pxr::GfVec3f &im = qrot.GetImaginary();

      for (int j = 0; j < 3; ++j) {
        const int k = 3 * i + j;
        if (k >= loc_curves.size()) {
          std::cout << "PROGRAMMER ERROR: out of bounds translation curve index." << std::endl;
          break;
        }
        if (FCurve *fcu = loc_curves[k]) {
          add_bezt(fcu, frame, t[j]);
        }
      }

      for (int j = 0; j < 4; ++j) {
        const int k = 4 * i + j;
        if (k >= rot_curves.size()) {
          std::cout << "PROGRAMMER ERROR: out of bounds rotation curve index." << std::endl;
          break;
        }
        if (FCurve *fcu = rot_curves[k]) {
          if (j == 0) {
            add_bezt(fcu, frame, re);
          }
          else {
            add_bezt(fcu, frame, im[j - 1]);
          }
        }
      }

      for (int j = 0; j < 3; ++j) {
        const int k = 3 * i + j;
        if (k >= scale_curves.size()) {
          std::cout << "PROGRAMMER ERROR: out of bounds scale curve index." << std::endl;
          break;
        }
        if (FCurve *fcu = scale_curves[k]) {
          add_bezt(fcu, frame, s[j]);
        }
      }
    }
  }

  /* Recalculate curve handles. */
  auto recalc_handles = [](FCurve *fcu) { BKE_fcurve_handles_recalc(fcu); };
  std::for_each(loc_curves.begin(), loc_curves.end(), recalc_handles);
  std::for_each(rot_curves.begin(), rot_curves.end(), recalc_handles);
  std::for_each(scale_curves.begin(), scale_curves.end(), recalc_handles);
}

}  // End anonymous namespace.

namespace blender::io::usd {

void import_blendshapes(Main *bmain,
                        Object *mesh_obj,
                        const pxr::UsdPrim &prim,
                        const bool import_anim)
{
  if (!(mesh_obj && mesh_obj->data && mesh_obj->type == OB_MESH && prim)) {
    return;
  }

  if (prim.IsInstanceProxy()) {
    /* Attempting to create a UsdSkelBindingAPI for
     * instance proxies generates USD errors. */
    return;
  }

  pxr::UsdSkelBindingAPI skel_api = pxr::UsdSkelBindingAPI::Apply(prim);

  if (!skel_api) {
    /* No skel binding. */
    return;
  }

  /* Get the blend shape targets, which are the USD paths to the
   * blend shape primitives. */

  if (!skel_api.GetBlendShapeTargetsRel().HasAuthoredTargets()) {
    /* No targets. */
    return;
  }

  pxr::SdfPathVector targets;
  if (!skel_api.GetBlendShapeTargetsRel().GetTargets(&targets)) {
    WM_reportf(RPT_WARNING,
               "%s: Couldn't get blendshape targets for prim %s",
               __func__,
               prim.GetPath().GetAsString().c_str());
    return;
  }

  if (targets.empty()) {
    return;
  }

  if (!skel_api.GetBlendShapesAttr().HasAuthoredValue()) {
    return;
  }

  /* Get the blend shape name tokens. */
  pxr::VtTokenArray blendshapes;
  if (!skel_api.GetBlendShapesAttr().Get(&blendshapes)) {
    return;
  }

  if (blendshapes.empty()) {
    return;
  }

  /* Sanity check. */
  if (targets.size() != blendshapes.size()) {
    WM_reportf(RPT_WARNING,
               "%s: Number of blendshapes doesn't match number of blendshape targets for prim %s",
               __func__,
               prim.GetPath().GetAsString().c_str());
    return;
  }

  pxr::UsdStageRefPtr stage = prim.GetStage();

  if (!stage) {
    WM_reportf(RPT_WARNING,
               "%s: Couldn't get stage for prim %s",
               __func__,
               prim.GetPath().GetAsString().c_str());
    return;
  }

  Mesh *mesh = static_cast<Mesh *>(mesh_obj->data);

  /* Insert key to source mesh. */
  Key *key = BKE_key_add(bmain, (ID *)mesh);
  key->type = KEY_RELATIVE;

  mesh->key = key;

  /* Insert basis key. */
  KeyBlock *kb = BKE_keyblock_add(key, "Basis");
  BKE_keyblock_convert_from_mesh(mesh, key, kb);

  /* Keep track of the shape-keys we're adding,
   * for validation when creating curves later. */
  std::set<pxr::TfToken> shapekey_names;

  for (int i = 0; i < targets.size(); ++i) {
    /* Get USD path to blend shape. */
    const pxr::SdfPath &path = targets[i];
    pxr::UsdSkelBlendShape blendshape(stage->GetPrimAtPath(path));

    if (!blendshape) {
      continue;
    }

    /* Get the blend shape offsets. */
    if (!blendshape.GetOffsetsAttr().HasAuthoredValue()) {
      /* Blend shape has no authored offsets. */
      continue;
    }

    pxr::VtVec3fArray offsets;
    if (!blendshape.GetOffsetsAttr().Get(&offsets)) {
      WM_reportf(RPT_WARNING,
                 "%s: Couldn't get offsets for blend shape %s",
                 __func__,
                 path.GetAsString().c_str());
      continue;
    }

    if (offsets.empty()) {
      WM_reportf(
          RPT_WARNING, "%s: No offsets for blend shape %s", __func__, path.GetAsString().c_str());
      continue;
    }

    shapekey_names.insert(blendshapes[i]);

    /* Add the key block. */
    kb = BKE_keyblock_add(key, blendshapes[i].GetString().c_str());
    BKE_keyblock_convert_from_mesh(mesh, key, kb);

    /* if authored, point indices are indices into the original mesh
     * that correspond to the values in the offsets array. */
    pxr::VtArray<int> point_indices;
    if (blendshape.GetPointIndicesAttr().HasAuthoredValue()) {
      blendshape.GetPointIndicesAttr().Get(&point_indices);
    }

    float *fp = static_cast<float *>(kb->data);

    if (point_indices.empty()) {
      /* Iterate over all key block elements and add the corresponding
       * offset to the key block point. */
      for (int a = 0; a < kb->totelem; ++a, fp += 3) {
        if (a >= offsets.size()) {
          WM_reportf(
              RPT_WARNING,
              "%s: Number of offsets greater than number of mesh vertices for blend shape %s",
              __func__,
              path.GetAsString().c_str());
          break;
        }
        add_v3_v3(fp, offsets[a].data());
      }
    }
    else {
      /* Iterate over the point indices and add the offset to the corresponding
       * key block point. */
      int a = 0;
      for (int i : point_indices) {
        if (i < 0 || i > kb->totelem) {
          std::cerr << "Out of bounds point index " << i << " for blendshape " << path
                    << std::endl;
          ++a;
          continue;
        }
        if (a >= offsets.size()) {
          WM_reportf(
              RPT_WARNING,
              "%s: Number of offsets greater than number of mesh vertices for blend shape %s",
              __func__,
              path.GetAsString().c_str());
          break;
        }
        add_v3_v3(&fp[3 * i], offsets[a].data());
        ++a;
      }
    }
  }

  if (!import_anim) {
    /* We're not importing animation, so we are done. */
    return;
  }

  /* Get the blend animation source from the skeleton. */

  pxr::UsdSkelSkeleton skel_prim = skel_api.GetInheritedSkeleton();

  if (!skel_prim) {
    return;
  }

  skel_api = pxr::UsdSkelBindingAPI::Apply(skel_prim.GetPrim());

  if (!skel_api) {
    return;
  }

  pxr::UsdPrim anim_prim = skel_api.GetInheritedAnimationSource();

  if (!anim_prim) {
    return;
  }

  pxr::UsdSkelAnimation skel_anim(anim_prim);

  if (!skel_anim) {
    return;
  }

  /* Check if a blend shape weight animation was authored. */
  if (!skel_anim.GetBlendShapesAttr().HasAuthoredValue()) {
    return;
  }

  pxr::UsdAttribute weights_attr = skel_anim.GetBlendShapeWeightsAttr();

  if (!(weights_attr && weights_attr.HasAuthoredValue())) {
    return;
  }

  /* Get the animation time samples. */
  std::vector<double> times;
  if (!weights_attr.GetTimeSamples(&times)) {
    return;
  }

  if (times.empty()) {
    return;
  }

  /* Get the blend shape name tokens. */
  if (!skel_anim.GetBlendShapesAttr().Get(&blendshapes)) {
    return;
  }

  if (blendshapes.empty()) {
    return;
  }

  const size_t num_samples = times.size();

  /* Create the animation and curves. */
  bAction *act = ED_id_action_ensure(bmain, (ID *)&key->id);
  std::vector<FCurve *> curves;

  for (auto blendshape_name : blendshapes) {
    if (shapekey_names.find(blendshape_name) == shapekey_names.end()) {
      /* We didn't create a shape-key for this blend-shape, so we don't
       * create a curve and insert a null placeholder in the curve array. */
      curves.push_back(nullptr);
      continue;
    }

    /* Create the curve for this shape key. */
    std::string rna_path = "key_blocks[\"" + blendshape_name.GetString() + "\"].value";
    FCurve *fcu = create_fcurve(0, rna_path);
    fcu->totvert = num_samples;
    curves.push_back(fcu);
    BLI_addtail(&act->curves, fcu);
  }

  /* Add the weight time samples to the curves. */
  for (double frame : times) {
    pxr::VtFloatArray weights;
    if (!weights_attr.Get(&weights, frame)) {
      std::cerr << "Couldn't get blendshape weights for time " << frame << std::endl;
      continue;
    }

    if (weights.size() != curves.size()) {
      std::cerr << "Programmer error: number of weight samples doesn't match number of shapekey "
                   "curve entries for frame "
                << frame << std::endl;
      continue;
    }

    for (int wi = 0; wi < weights.size(); ++wi) {
      if (curves[wi] != nullptr) {
        add_bezt(curves[wi], frame, weights[wi]);
      }
    }
  }

  /* Recalculate curve handles. */
  auto recalc_handles = [](FCurve *fcu) { BKE_fcurve_handles_recalc(fcu); };
  std::for_each(curves.begin(), curves.end(), recalc_handles);
}

void import_skeleton(Main *bmain,
                     Object *arm_obj,
                     const pxr::UsdSkelSkeleton &skel,
                     const bool import_anim)
{
  if (!(arm_obj && arm_obj->data && arm_obj->type == OB_ARMATURE)) {
    return;
  }

  pxr::UsdSkelCache skel_cache;
  pxr::UsdSkelSkeletonQuery skel_query = skel_cache.GetSkelQuery(skel);

  if (!skel_query.IsValid()) {
    WM_reportf(RPT_WARNING,
               "%s: Couldn't query skeleton %s",
               __func__,
               skel.GetPath().GetAsString().c_str());
    return;
  }

  const pxr::UsdSkelTopology &skel_topology = skel_query.GetTopology();

  pxr::VtTokenArray joint_order = skel_query.GetJointOrder();

  if (joint_order.size() != skel_topology.size()) {
    WM_reportf(RPT_WARNING,
               "%s: Topology and joint order size mismatch for skeleton %s",
               __func__,
               skel.GetPath().GetAsString().c_str());
    return;
  }

  bArmature *arm = static_cast<bArmature *>(arm_obj->data);

  /* Set the armature to edit mode when creating the bones. */
  ED_armature_to_edit(arm);

  /* The bones we create, stored in the skeleton's joint order. */
  std::vector<EditBone *> edit_bones;

  /* Keep track of the bones we create for each joint.
   * We'll need this when creating animation curves
   * later. */
  std::map<pxr::TfToken, std::string> joint_to_bone_map;

  /* Create the bones. */
  for (const pxr::TfToken &joint : joint_order) {
    std::string name = pxr::SdfPath(joint).GetName();
    EditBone *bone = ED_armature_ebone_add(arm, name.c_str());
    if (!bone) {
      WM_reportf(
          RPT_WARNING, "%s: Couldn't add bone for joint %s", __func__, joint.GetString().c_str());
      edit_bones.push_back(nullptr);
      continue;
    }
    joint_to_bone_map.insert(std::make_pair(joint, bone->name));
    edit_bones.push_back(bone);
  }

  /* Sanity check: we should have created a bone for each joint. */
  const size_t num_joints = skel_topology.GetNumJoints();
  if (edit_bones.size() != num_joints) {
    WM_reportf(RPT_WARNING,
               "%s: Mismatch in bone and joint counts for skeleton %s",
               __func__,
               skel.GetPath().GetAsString().c_str());
    return;
  }

  /* Get the world space joint transforms at bind time. */
  pxr::VtMatrix4dArray bind_xforms;
  if (!skel_query.GetJointWorldBindTransforms(&bind_xforms)) {
    WM_reportf(RPT_WARNING,
               "%s: Couldn't get world bind transforms for skeleton %s",
               __func__,
               skel.GetPath().GetAsString().c_str());
    return;
  }

  if (bind_xforms.size() != num_joints) {
    WM_reportf(RPT_WARNING,
               "%s:  Mismatch in bind xforms and joint counts for skeleton %s",
               __func__,
               skel.GetPath().GetAsString().c_str());
    return;
  }

  /* Check if any bone matrices have negative determinants,
   * indicating negative scales, possibly due to mirroring
   * operations.  Such matrices can't be properly converted
   * to Blender's axis/roll bone representation (see
   * https://projects.blender.org/blender/blender/issues/82930).
   * If we detect such matrices, we will flag an error and won't
   * try to import the animation, since the rotations would
   * be incorrect in such cases. Unfortunately, the Pixar
   * `UsdSkel` examples of the "HumanFemale" suffer from
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
               "The skeletal animation won't be imported",
               skel.GetPath().GetAsString().c_str());
  }

  /* Set bone parenting.  In addition, scale bones to account
   * for separation between parents and children, so that the
   * bone size is in proportion with the overall skeleton hierarchy.
   * USD skeletons are composed of joints which we imperfectly
   * represent as bones. */

  /* This will record the child bone indices per parent bone,
   * to simplify accessing children when computing lengths. */
  std::vector<std::vector<int>> child_bones(num_joints);

  for (size_t i = 0; i < num_joints; ++i) {
    const int parent_idx = skel_topology.GetParent(i);
    if (parent_idx < 0) {
      continue;
    }
    if (parent_idx >= edit_bones.size()) {
      std::cout << "WARNING: out of bounds parent index for bone " << pxr::SdfPath(joint_order[i])
                << " for skeleton " << skel.GetPath() << std::endl;
      continue;
    }

    child_bones[parent_idx].push_back(i);
    if (edit_bones[i] && edit_bones[parent_idx]) {
      edit_bones[i]->parent = edit_bones[parent_idx];
    }
  }

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

    const float new_len = (avg_child_head - parent_head).GetLength();

    /* Check for epsilon relative to the parent head before scaling. */
    if (new_len > .00001 * max_mag_component(parent_head)) {
      parent_tail = parent_head + (parent_tail - parent_head).GetNormalized() * new_len;
      copy_v3_v3(parent->tail, parent_tail.data());
      avg_len_scale += new_len;
    }
  }

  /* Scale terminal bones by the average length scale. */
  avg_len_scale /= num_joints;

  for (size_t i = 0; i < num_joints; ++i) {
    if (!child_bones[i].empty()) {
      /* Not a terminal bone. */
      continue;
    }
    EditBone *bone = edit_bones[i];
    if (!bone) {
      continue;
    }
    pxr::GfVec3f head(bone->head);

    /* Check for epsilon relative to the head before scaling. */
    if (avg_len_scale > .00001 * max_mag_component(head)) {
      pxr::GfVec3f tail(bone->tail);
      tail = head + (tail - head).GetNormalized() * avg_len_scale;
      copy_v3_v3(bone->tail, tail.data());
    }
  }

  /* Get out of edit mode. */
  ED_armature_from_edit(bmain, arm);
  ED_armature_edit_free(arm);

  if (import_anim && valid_skeleton) {
    import_skeleton_curves(bmain, arm_obj, skel_query, joint_to_bone_map);
  }
}

void import_mesh_skel_bindings(Main *bmain, Object *mesh_obj, const pxr::UsdPrim &prim)
{
  if (!(bmain && mesh_obj && mesh_obj->type == OB_MESH && prim)) {
    return;
  }

  if (prim.IsInstanceProxy()) {
    /* Attempting to create a UsdSkelBindingAPI for
     * instance proxies generates USD errors. */
    return;
  }

  pxr::UsdSkelBindingAPI skel_api = pxr::UsdSkelBindingAPI::Apply(prim);

  if (!skel_api) {
    return;
  }

  pxr::UsdSkelSkeleton skel = skel_api.GetInheritedSkeleton();

  if (!skel) {
    return;
  }

  /* Get the joint identifiers from the skeleton. We will
   * need these to construct deform groups. */
  pxr::VtArray<pxr::TfToken> joints;

  if (skel_api.GetJointsAttr().HasAuthoredValue()) {
    skel_api.GetJointsAttr().Get(&joints);
  }
  else if (skel.GetJointsAttr().HasAuthoredValue()) {
    skel.GetJointsAttr().Get(&joints);
  }

  if (joints.empty()) {
    return;
  }

  /* Get the joint indices, which specify which joints influence a given point. */
  pxr::UsdGeomPrimvar joint_indices_primvar = skel_api.GetJointIndicesPrimvar();
  if (!(joint_indices_primvar && joint_indices_primvar.HasAuthoredValue())) {
    return;
  }

  /* Get the weights, which specify the weight of a joint on a given point. */
  pxr::UsdGeomPrimvar joint_weights_primvar = skel_api.GetJointWeightsPrimvar();
  if (!(joint_weights_primvar && joint_weights_primvar.HasAuthoredValue())) {
    return;
  }

  /* Element size specifies the number of joints that might influence a given point.
   * This is the stride we take when accessing the indices and weights for a given point. */
  int joint_indices_elem_size = joint_indices_primvar.GetElementSize();
  int joint_weights_elem_size = joint_weights_primvar.GetElementSize();

  /* We expect the element counts to match. */
  if (joint_indices_elem_size != joint_weights_elem_size) {
    WM_reportf(RPT_WARNING,
               "%s: Joint weights and joint indices element size mismatch for prim %s",
               __func__,
               prim.GetPath().GetAsString().c_str());
    return;
  }

  /* Get the joint indices and weights. */
  pxr::VtIntArray joint_indices;
  joint_indices_primvar.ComputeFlattened(&joint_indices);

  pxr::VtFloatArray joint_weights;
  joint_weights_primvar.ComputeFlattened(&joint_weights);

  if (joint_indices.empty() || joint_weights.empty()) {
    return;
  }

  if (joint_indices.size() != joint_weights.size()) {
    WM_reportf(RPT_WARNING,
               "%s: Joint weights and joint indices size mismatch size mismatch for prim %s",
               __func__,
               prim.GetPath().GetAsString().c_str());
    return;
  }

  Mesh *mesh = static_cast<Mesh *>(mesh_obj->data);

  const pxr::TfToken interp = joint_weights_primvar.GetInterpolation();

  /* Sanity check: we expect only vertex or constant interpolation. */
  if (interp != pxr::UsdGeomTokens->vertex && interp != pxr::UsdGeomTokens->constant) {
    WM_reportf(RPT_WARNING,
               "%s: Unexpected joint weights interpolation type %s for prim %s",
               __func__,
               interp.GetString().c_str(),
               prim.GetPath().GetAsString().c_str());
    return;
  }

  /* Sanity check: make sure we have the expected number of values for the interpolation type. */
  if (interp == pxr::UsdGeomTokens->vertex &&
      joint_weights.size() != mesh->totvert * joint_weights_elem_size)
  {
    WM_reportf(RPT_WARNING,
               "%s: Joint weights of unexpected size for vertex interpolation for prim %s",
               __func__,
               prim.GetPath().GetAsString().c_str());
    return;
  }

  if (interp == pxr::UsdGeomTokens->constant && joint_weights.size() != joint_weights_elem_size) {
    WM_reportf(RPT_WARNING,
               "%s: Joint weights of unexpected size for constant interpolation for prim %s",
               __func__,
               prim.GetPath().GetAsString().c_str());
    return;
  }

  /* Determine which joint indices are used for skinning this prim. */
  std::vector<int> used_indices;
  for (int index : joint_indices) {
    if (std::find(used_indices.begin(), used_indices.end(), index) == used_indices.end()) {
      /* We haven't accounted for this index yet. */
      if (index < 0 || index >= joints.size()) {
        std::cerr << "Out of bound joint index " << index << std::endl;
        continue;
      }
      used_indices.push_back(index);
    }
  }

  if (used_indices.empty()) {
    return;
  }

  if (BKE_object_defgroup_data_create(static_cast<ID *>(mesh_obj->data)) == NULL) {
    WM_reportf(RPT_WARNING,
               "%s: Error creating deform group data for mesh %s",
               __func__,
               mesh_obj->id.name + 2);
    return;
  }

  /* Add the armature modifier, if one doesn't exist. */
  if (!BKE_modifiers_findby_type(mesh_obj, eModifierType_Armature)) {
    ModifierData *md = BKE_modifier_new(eModifierType_Armature);
    BLI_addtail(&mesh_obj->modifiers, md);
  }

  /* Create a deform group per joint. */
  std::vector<bDeformGroup *> joint_def_grps(joints.size(), nullptr);

  for (int idx : used_indices) {
    std::string joint_name = pxr::SdfPath(joints[idx]).GetName();
    if (!BKE_object_defgroup_find_name(mesh_obj, joint_name.c_str())) {
      bDeformGroup *def_grp = BKE_object_defgroup_add_name(mesh_obj, joint_name.c_str());
      joint_def_grps[idx] = def_grp;
    }
  }

  /* Set the deform group verts and weights. */
  for (int i = 0; i < mesh->totvert; ++i) {
    /* Offset into the weights array, which is
     * always 0 for constant interpolation. */
    int offset = 0;
    if (interp == pxr::UsdGeomTokens->vertex) {
      offset = i * joint_weights_elem_size;
    }
    for (int j = 0; j < joint_weights_elem_size; ++j) {
      const int k = offset + j;
      const float w = joint_weights[k];
      if (w < .00001) {
        /* No deform group if zero weight. */
        continue;
      }
      const int joint_idx = joint_indices[k];
      if (bDeformGroup *def_grp = joint_def_grps[joint_idx]) {
        ED_vgroup_vert_add(mesh_obj, def_grp, i, w, WEIGHT_REPLACE);
      }
    }
  }
}

}  // namespace blender::io::usd
