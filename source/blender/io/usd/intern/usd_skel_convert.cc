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
 * The Original Code is Copyright (C) 2022 NVIDIA Corporation.
 * All rights reserved.
 */

#include "usd_skel_convert.h"

#include "usd.h"

#include <pxr/usd/usdSkel/animation.h>
#include <pxr/usd/usdSkel/blendShape.h>
#include <pxr/usd/usdSkel/bindingAPI.h>
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
#include "BKE_mesh_runtime.h"
#include "BKE_modifier.h"
#include "BKE_object.h"
#include "BKE_object_deform.h"

#include "BLI_math_vector.h"

#include "ED_keyframing.h"
#include "ED_mesh.h"

#include <string>
#include <vector>

namespace usdtokens {
// Attribute names.
//static const pxr::TfToken color("color", pxr::TfToken::Immortal);
}  // namespace usdtokens

namespace {

FCurve *create_fcurve(int array_index, const char *rna_path)
{
  FCurve *fcu = BKE_fcurve_create();
  fcu->flag = (FCURVE_VISIBLE | FCURVE_SELECTED);
  fcu->rna_path = BLI_strdupn(rna_path, strlen(rna_path));
  fcu->array_index = array_index;
  return fcu;
}

FCurve *create_chan_fcurve(bAction *act,
                           bActionGroup *grp,
                           int array_index,
                           const char *rna_path,
                           int totvert)
{
  FCurve *fcu = create_fcurve(array_index, rna_path);
  fcu->totvert = totvert;
  action_groups_add_channel(act, grp, fcu);
  return fcu;
}

void add_bezt(FCurve *fcu,
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

/* Generate dummy curve samples for testing. */
//void add_dummy_samples(FCurve *fcu)
//{
//  int totvert = fcu->totvert;
//  for (int i = 0; i < totvert; ++i) {
//    add_bezt(fcu, static_cast<float>(i+1), static_cast<float>(i) * 0.1f);
//  }
//}

}  // End anonymous namespace.

namespace blender::io::usd {

pxr::GfMatrix4d get_world_matrix(const pxr::UsdPrim &prim, pxr::UsdTimeCode time)
{
  pxr::GfMatrix4d local_xf(1.0f);

  if (!prim) {
    return local_xf;
  }

  pxr::UsdGeomXformable xformable(prim);

  if (xformable) {
    bool reset_xform_stack = false;
    if (!xformable.GetLocalTransformation(&local_xf, &reset_xform_stack, time)) {
      std::cout << "WARNING: couldn't get local xform for prim " << prim.GetPath() << std::endl;
      return local_xf;
    }
  }

  return local_xf * get_world_matrix(prim.GetParent(), time);
}

void test_create_shapekeys(Main *bmain, Object *obj)
{
  if (!(obj && obj->data && obj->type == OB_MESH)) {
    return;
  }

  Mesh *mesh = static_cast<Mesh *>(obj->data);

  /* insert key to source mesh */
  Key *key = BKE_key_add(bmain, (ID *)mesh);
  key->type = KEY_RELATIVE;

  mesh->key = key;

  /* insert basis key */
  KeyBlock *kb = BKE_keyblock_add(key, "Basis");
  BKE_keyblock_convert_from_mesh(mesh, key, kb);

  kb = BKE_keyblock_add(key, "Key1");
  BKE_keyblock_convert_from_mesh(mesh, key, kb);

  float offsets[] = { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 };
  BKE_keyblock_update_from_offset(obj, kb, (float(*)[3])&offsets);

  bAction *act = ED_id_action_ensure(bmain, (ID *)&key->id);

  FCurve *fcu = create_fcurve(0, "key_blocks[\"Key1\"].value");
  fcu->totvert = 3;

  add_bezt(fcu, 0.f, 0.f);
  add_bezt(fcu, 30.f, 1.f);
  add_bezt(fcu, 60.f, 0.3f);

  BLI_addtail(&act->curves, fcu);
}

void import_blendshapes(Main *bmain, Object *obj, pxr::UsdPrim prim)
{
  if (!(obj && obj->data && obj->type == OB_MESH && prim)) {
    return;
  }

  if (prim.IsInstanceProxy() || prim.IsInPrototype()) {
    /* Attempting to create a UsdSkelBindingAPI for
     * instance proxies and prototypes generates USD errors. */
    return;
  }

  pxr::UsdSkelBindingAPI skel_api = pxr::UsdSkelBindingAPI::Apply(prim);

  if (!skel_api) {
    return;
  }

  if (!skel_api.GetBlendShapeTargetsRel().HasAuthoredTargets()) {
    return;
  }

  pxr::SdfPathVector targets;
  if (!skel_api.GetBlendShapeTargetsRel().GetTargets(&targets)) {
    std::cout << "Couldn't get blendshape targets for prim " << prim.GetPath() << std::endl;
    return;
  }

  if (targets.empty()) {
    return;
  }

  if (!skel_api.GetBlendShapesAttr().HasAuthoredValue()) {
    return;
  }

  pxr::VtTokenArray blendshapes;
  if (!skel_api.GetBlendShapesAttr().Get(&blendshapes)) {
    return;
  }

  if (blendshapes.empty()) {
    return;
  }

  if (targets.size() != blendshapes.size()) {
    std::cout << "Number of blendshapes doesn't match number of blendshape targets for prim " << prim.GetPath() << std::endl;
    return;
  }

  Mesh *mesh = static_cast<Mesh *>(obj->data);

  /* insert key to source mesh */
  Key *key = BKE_key_add(bmain, (ID *)mesh);
  key->type = KEY_RELATIVE;

  mesh->key = key;

  /* insert basis key */
  KeyBlock *kb = BKE_keyblock_add(key, "Basis");
  BKE_keyblock_convert_from_mesh(mesh, key, kb);

  pxr::UsdStageRefPtr stage = prim.GetStage();

  if (!stage) {
    return;
  }

  /* Keep track of the shapkeys we're adding, for
   * validation when creating curves later. */
  std::set<pxr::TfToken> shapekey_names;

  for (int i = 0; i < targets.size(); ++i) {

    const pxr::SdfPath &path = targets[i];

    pxr::UsdSkelBlendShape blendshape(stage->GetPrimAtPath(path));

    if (!blendshape) {
      continue;
    }

    if (!blendshape.GetOffsetsAttr().HasAuthoredValue()) {
      continue;
    }

    pxr::VtVec3fArray offsets;
    if (!blendshape.GetOffsetsAttr().Get(&offsets)) {
      std::cout << "Couldn't get offsets for blendshape " << path << std::endl;
      continue;
    }

    if (offsets.empty()) {
      std::cout << "No offsets for blendshape " << path << std::endl;
      continue;
    }

    shapekey_names.insert(blendshapes[i]);

    kb = BKE_keyblock_add(key, blendshapes[i].GetString().c_str());
    BKE_keyblock_convert_from_mesh(mesh, key, kb);

    pxr::VtArray<int> point_indices;
    if (blendshape.GetPointIndicesAttr().HasAuthoredValue()) {
      blendshape.GetPointIndicesAttr().Get(&point_indices);
    }

    float *fp = static_cast<float *>(kb->data);

    if (point_indices.empty()) {
      for (int a = 0; a < kb->totelem; ++a, fp += 3) {
        if (a >= offsets.size()) {
          std::cout << "Number of offsets greater than number of mesh vertices for blendshape "
                    << path << std::endl;
          break;
        }
        add_v3_v3(fp, offsets[a].data());
      }
    }
    else {
      int a = 0;
      for (int i : point_indices) {
        if (i < 0 || i > kb->totelem) {
          std::cout << "Out of bounds point index " << i << " for blendshape " << path << std::endl;
          ++a;
          continue;
        }
        if (a >= offsets.size()) {
          std::cout << "Number of offsets greater than number of mesh vertices for blendshape " << path << std::endl;
          break;
        }
        add_v3_v3(&fp[3 * i], offsets[a].data());
        ++a;
      }
    }
  }

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

  if (!skel_anim.GetBlendShapesAttr().HasAuthoredValue()) {
    return;
  }

  pxr::UsdAttribute weights_attr = skel_anim.GetBlendShapeWeightsAttr();

  if (!(weights_attr && weights_attr.HasAuthoredValue())) {
    return;
  }

  std::vector<double> times;
  if (!weights_attr.GetTimeSamples(&times)) {
    return;
  }

  if (times.empty()) {
    return;
  }

  if (!skel_anim.GetBlendShapesAttr().Get(&blendshapes)) {
    return;
  }

  if (blendshapes.empty()) {
    return;
  }

  size_t num_samples = times.size();

  /* Create the animation and curves. */
  bAction *act = ED_id_action_ensure(bmain, (ID *)&key->id);
  std::vector<FCurve *> curves;

  for (auto blendshape_name : blendshapes) {

    if (shapekey_names.find(blendshape_name) == shapekey_names.end()) {
      /* We didn't create a shapekey fo this blendshape, so we don't
       * create a curve and insert a null placeholder in the curve array. */
      curves.push_back(nullptr);
      continue;
    }

    std::string rna_path = "key_blocks[\"" + blendshape_name.GetString() + "\"].value";
    FCurve *fcu = create_fcurve(0, rna_path.c_str());
    fcu->totvert = num_samples;
    curves.push_back(fcu);
    BLI_addtail(&act->curves, fcu);
  }

  for (double frame : times) {
    pxr::VtFloatArray weights;
    if (!weights_attr.Get(&weights, frame)) {
      std::cout << "Couldn't get blendshape weights for time " << frame << std::endl;
      continue;
    }

    if (weights.size() != curves.size()) {
      std::cout << "Programmer error: number of weight samples doesn't match number of shapekey curve entries for frame " << frame << std::endl;
      continue;
    }

    for (int wi = 0; wi < weights.size(); ++wi) {
      if (curves[wi] != nullptr) {
        add_bezt(curves[wi], frame, weights[wi]);
      }
    }
  }

}

void create_skeleton_curves(Main *bmain,
                            Object *obj,
                            const pxr::UsdSkelSkeletonQuery &skel_query,
                            const std::map<pxr::TfToken, std::string> &joint_to_bone_map)

{
  if (!(bmain && obj && skel_query)) {
    return;
  }

  if (joint_to_bone_map.empty()) {
    return;
  }

  const pxr::UsdSkelAnimQuery &anim_query = skel_query.GetAnimQuery();

  if (!anim_query) {
    return;
  }

  std::vector<double> samples;
  anim_query.GetJointTransformTimeSamples(&samples);

  if (samples.empty()) {
    return;
  }

  size_t num_samples = samples.size();

  bAction *act = ED_id_action_ensure(bmain, (ID *)&obj->id);

  pxr::VtTokenArray joint_order = skel_query.GetJointOrder();

  std::vector<FCurve *> loc_curves;
  std::vector<FCurve *> rot_curves;
  std::vector<FCurve *> scale_curves;

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
    loc_curves.push_back(create_chan_fcurve(act, grp, 0, rna_path.c_str(), num_samples));
    loc_curves.push_back(create_chan_fcurve(act, grp, 1, rna_path.c_str(), num_samples));
    loc_curves.push_back(create_chan_fcurve(act, grp, 2, rna_path.c_str(), num_samples));

    /* Add rotation curves. */
    rna_path = "pose.bones[\"" + it->second + "\"].rotation_quaternion";
    rot_curves.push_back(create_chan_fcurve(act, grp, 0, rna_path.c_str(), num_samples));
    rot_curves.push_back(create_chan_fcurve(act, grp, 1, rna_path.c_str(), num_samples));
    rot_curves.push_back(create_chan_fcurve(act, grp, 2, rna_path.c_str(), num_samples));
    rot_curves.push_back(create_chan_fcurve(act, grp, 3, rna_path.c_str(), num_samples));

    /* Add scale curves. */
    rna_path = "pose.bones[\"" + it->second + "\"].scale";
    scale_curves.push_back(create_chan_fcurve(act, grp, 0, rna_path.c_str(), num_samples));
    scale_curves.push_back(create_chan_fcurve(act, grp, 1, rna_path.c_str(), num_samples));
    scale_curves.push_back(create_chan_fcurve(act, grp, 2, rna_path.c_str(), num_samples));
  }

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

  /* Skeleton-space joint bind transforms. */
  pxr::VtMatrix4dArray bind_xforms;
  if (!compute_skel_space_bind_transforms(skel_query, bind_xforms, 0.0f)) {
    std::cout << "WARNING: couldn't get skeleton space bind xforms for skeleton query "
              << skel_query.GetPrim().GetPath() << std::endl;
    return;
  }

  if (bind_xforms.size() != joint_order.size()) {
    std::cout << "WARNING: number of bind transforms doesn't match the number of joints\n";
    return;
  }

  const pxr::UsdSkelTopology &skel_topology = skel_query.GetTopology();

  /* This will store the inverse of the parent-relative bind xforms. */
  pxr::VtMatrix4dArray inv_bind_xforms(bind_xforms.size());

  for (int i = 0; i < bind_xforms.size(); ++i) {
    int parent_id = skel_topology.GetParent(i);

    if (parent_id >= 0) {
      /* This is a non-root bone.  Compute the transform of the joint
       * relative to its parent. */
      pxr::GfMatrix4d parent_relative_xf = bind_xforms[i] * bind_xforms[parent_id].GetInverse();
      inv_bind_xforms[i] = parent_relative_xf.GetInverse();
    } else {
      inv_bind_xforms[i] = bind_xforms[i].GetInverse();
    }
  }

  for (double frame : samples) {
    pxr::VtMatrix4dArray joint_local_xforms;

    if (!skel_query.ComputeJointLocalTransforms(&joint_local_xforms, frame)) {
      std::cout << "WARNING: couldn't compute joint local transforms on frame " << frame << std::endl;
      continue;
    }

    if (joint_local_xforms.size() != joint_order.size()) {
      std::cout << "WARNING: number of joint local transform entries " << joint_local_xforms.size()
                << " doesn't match the number of joints " << joint_order.size() << std::endl;
      continue;
    }

    for (int i = 0; i < joint_local_xforms.size(); ++i) {

      pxr::GfMatrix4d bind_relative_xf = joint_local_xforms[i] * inv_bind_xforms[i];

      pxr::GfVec3f t;
      pxr::GfQuatf qrot;
      pxr::GfVec3h s;

      if (!pxr::UsdSkelDecomposeTransform(bind_relative_xf, &t, &qrot, &s)) {
        std::cout << "WARNING: error decomposing matrix on frame " << frame << std::endl;
        continue;
      }

      float re = qrot.GetReal();
      pxr::GfVec3f im = qrot.GetImaginary();

      for (int j = 0; j < 3; ++j) {
        int k = 3 * i + j;
        if (k >= loc_curves.size()) {
          std::cout << "PROGRAMMER ERROR: out of bounds translation curve index." << std::endl;
          break;
        }
        if (FCurve *fcu = loc_curves[k]) {
          add_bezt(fcu, frame, t[j]);
        }
      }

      for (int j = 0; j < 4; ++j) {
        int k = 4 * i + j;
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
        int k = 3 * i + j;
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
}

void import_skel_bindings(Main *bmain, Object *mesh_obj, pxr::UsdPrim prim)
{
  if (!(bmain && mesh_obj && prim)) {
    return;
  }

  if (prim.IsInstanceProxy() || prim.IsInPrototype()) {
    /* Attempting to create a UsdSkelBindingAPI for
     * instance proxies and prototypes generates USD errors. */
    return;
  }

  if (mesh_obj->type != OB_MESH) {
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

  pxr::UsdGeomPrimvar joint_indices_primvar = skel_api.GetJointIndicesPrimvar();

  if (!(joint_indices_primvar && joint_indices_primvar.HasAuthoredValue())) {
    return;
  }

  pxr::UsdGeomPrimvar joint_weights_primvar = skel_api.GetJointWeightsPrimvar();

  if (!(joint_weights_primvar && joint_weights_primvar.HasAuthoredValue())) {
    return;
  }

  int joint_indices_elem_size = joint_indices_primvar.GetElementSize();
  int joint_weights_elem_size = joint_weights_primvar.GetElementSize();

  if (joint_indices_elem_size != joint_weights_elem_size) {
    std::cout << "WARNING: joint weights and joint indices element size mismatch." << std::endl;
    return;
  }

  /* The set of unique joint indices referenced in the joint indices
  *  attribute. */
  pxr::VtIntArray joint_indices;
  joint_indices_primvar.ComputeFlattened(&joint_indices);

  pxr::VtFloatArray joint_weights;
  joint_weights_primvar.ComputeFlattened(&joint_weights);

  if (joint_indices.empty() || joint_weights.empty()) {
    return;
  }

  if (joint_indices.size() != joint_weights.size()) {
    std::cout << "WARNING: joint weights and joint indices size mismatch." << std::endl;
    return;
  }

  Mesh *mesh = static_cast<Mesh *>(mesh_obj->data);

  pxr::TfToken interp = joint_weights_primvar.GetInterpolation();

  if (interp != pxr::UsdGeomTokens->vertex && interp != pxr::UsdGeomTokens->constant) {
    std::cout << "WARNING: unexpected joint weights interpolation type " << interp
              << std::endl;
    return;
  }

  if (interp == pxr::UsdGeomTokens->vertex && joint_weights.size() != mesh->totvert * joint_weights_elem_size) {
    std::cout << "WARNING: joint weights of unexpected size for vertex interpolation." << std::endl;
    return;
  }

  if (interp == pxr::UsdGeomTokens->constant && joint_weights.size() != joint_weights_elem_size) {
    std::cout << "WARNING: joint weights of unexpected size for constant interpolation."
              << std::endl;
    return;
  }

  std::vector<int> used_indices;
  for (int index : joint_indices) {
    if (std::find(used_indices.begin(), used_indices.end(), index) == used_indices.end()) {
      /* We haven't accounted for this index yet. */
      if (index < 0 || index >= joints.size()) {
        std::cout << "Out of bound joint index " << index << std::endl;
        continue;
      }
      used_indices.push_back(index);
    }
  }

  if (used_indices.empty()) {
    return;
  }

  if (BKE_object_defgroup_data_create(static_cast<ID *>(mesh_obj->data)) == NULL) {
    return;
  }

  /* Add the armature modifier, if one doesn't exist. */
  if (!BKE_modifiers_findby_type(mesh_obj, eModifierType_Armature)) {
    ModifierData *md = BKE_modifier_new(eModifierType_Armature);
    BLI_addtail(&mesh_obj->modifiers, md);
  }

  std::vector<bDeformGroup *> joint_def_grps(joints.size(), nullptr);

  for (int idx : used_indices) {
    std::string joint_name = pxr::SdfPath(joints[idx]).GetName();
    if (!BKE_object_defgroup_find_name(mesh_obj, joint_name.c_str())) {
      bDeformGroup *def_grp = BKE_object_defgroup_add_name(mesh_obj, joint_name.c_str());
      joint_def_grps[idx] = def_grp;
    }
  }

  for (int i = 0; i < mesh->totvert; ++i) {
    /* Offset into the weights array, which is
     * always 0 for constant interpolation. */
    int offset = 0;
    if (interp == pxr::UsdGeomTokens->vertex) {
      offset = i * joint_weights_elem_size;
    }
    for (int j = 0; j < joint_weights_elem_size; ++j) {
      int k = offset + j;
      float w = joint_weights[k];
      if (w < .00001) {
        /* No deform group if zero weight. */
        continue;
      }
      int joint_idx = joint_indices[k];
      bDeformGroup *def_grp = joint_def_grps[joint_idx];
      if (def_grp) {
        ED_vgroup_vert_add(mesh_obj, def_grp, i, w, WEIGHT_REPLACE);
      }
    }
  }
}


bool compute_skel_space_bind_transforms(const pxr::UsdSkelSkeletonQuery &skel_query,
                                        pxr::VtMatrix4dArray &out_xforms,
                                        pxr::UsdTimeCode time)
{
  if (!skel_query) {
    return false;
  }

  pxr::GfMatrix4d skel_mat_inv = get_world_matrix(skel_query.GetSkeleton().GetPrim(), time).GetInverse();

  if (!skel_query.GetJointWorldBindTransforms(&out_xforms)) {
    std::cout << "WARNING: couldn't get local xform for prim "
              << skel_query.GetSkeleton().GetPrim().GetPath() << std::endl;
    return false;
  }

  for (int i = 0; i < out_xforms.size(); ++i) {
    out_xforms[i] = out_xforms[i] * skel_mat_inv;
  }

  return true;
}

}  // namespace blender::io::usd
