/* SPDX-FileCopyrightText: 2023 NVIDIA Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "usd_blend_shape_utils.hh"
#include "usd_utils.hh"

#include <pxr/usd/usdGeom/primvarsAPI.h>
#include <pxr/usd/usdSkel/animMapper.h>
#include <pxr/usd/usdSkel/animation.h>
#include <pxr/usd/usdSkel/bindingAPI.h>
#include <pxr/usd/usdSkel/blendShape.h>

#include "DNA_key_types.h"
#include "DNA_mesh_types.h"

#include "BKE_key.hh"
#include "BKE_mesh.hh"
#include "BKE_object.hh"
#include "DNA_object_types.h"

#include "BLI_assert.h"
#include "BLI_listbase.h"
#include "BLI_math_vector.h"
#include "BLI_set.hh"
#include "BLI_vector.hh"

#include <string>
#include <vector>

#include "CLG_log.h"
static CLG_LogRef LOG = {"io.usd"};

namespace usdtokens {
static const pxr::TfToken Anim("Anim", pxr::TfToken::Immortal);
static const pxr::TfToken joint1("joint1", pxr::TfToken::Immortal);
static const pxr::TfToken Skel("Skel", pxr::TfToken::Immortal);
}  // namespace usdtokens

namespace {

/* Helper struct to facilitate merging blend shape weights time
 * samples from multiple meshes to a single skeleton animation. */
struct BlendShapeMergeInfo {
  pxr::VtTokenArray src_blend_shapes;
  pxr::UsdAttribute src_weights_attr;
  /* Remap blend shape weight array from the
   * source order to the destination order. */
  pxr::UsdSkelAnimMapper anim_map;

  void init_anim_map(const pxr::VtTokenArray &dst_blend_shapes)
  {
    anim_map = pxr::UsdSkelAnimMapper(src_blend_shapes, dst_blend_shapes);
  }
};

/* Helper function to avoid name collisions when merging blend shape names from
 * multiple meshes to a single skeleton.
 *
 * Attempt to add the given name to the 'names' set as a unique entry, modifying
 * the name with a numerical suffix if necessary, and return the unique name that
 * was added to the set. */
std::string add_unique_name(blender::Set<std::string> &names, const std::string &name)
{
  std::string unique_name = name;
  int suffix = 2;
  while (names.contains(unique_name)) {
    unique_name = name + std::to_string(suffix++);
  }
  names.add(unique_name);
  return unique_name;
}

}  // namespace

namespace blender::io::usd {

pxr::TfToken TempBlendShapeWeightsPrimvarName("temp:weights", pxr::TfToken::Immortal);

void ensure_blend_shape_skeleton(pxr::UsdStageRefPtr stage, pxr::UsdPrim &mesh_prim)
{
  if (!stage || !mesh_prim) {
    return;
  }

  pxr::UsdSkelBindingAPI skel_api = pxr::UsdSkelBindingAPI::Apply(mesh_prim);

  if (!skel_api) {
    CLOG_WARN(&LOG,
              "Couldn't apply UsdSkelBindingAPI to mesh prim %s",
              mesh_prim.GetPath().GetAsString().c_str());
    return;
  }

  pxr::UsdSkelSkeleton skel;
  if (!skel_api.GetSkeleton(&skel)) {
    pxr::SdfPath skel_path = mesh_prim.GetParent().GetPath().AppendChild(usdtokens::Skel);
    skel = pxr::UsdSkelSkeleton::Define(stage, skel_path);

    if (!skel) {
      CLOG_WARN(&LOG,
                "Couldn't find or create skeleton bound to mesh prim %s",
                mesh_prim.GetPath().GetAsString().c_str());
      return;
    }

    skel_api.CreateSkeletonRel().AddTarget(skel.GetPath());

    /* Initialize the skeleton. */
    pxr::VtMatrix4dArray bind_transforms(1, pxr::GfMatrix4d(1.0));
    pxr::VtMatrix4dArray rest_transforms(1, pxr::GfMatrix4d(1.0));
    skel.CreateBindTransformsAttr().Set(bind_transforms);
    skel.GetRestTransformsAttr().Set(rest_transforms);

    /* Some DCCs seem to require joint names to bind the
     * skeleton to blend-shapes. */
    pxr::VtTokenArray joints({usdtokens::joint1});
    skel.CreateJointsAttr().Set(joints);
  }

  pxr::UsdAttribute temp_weights_attr = pxr::UsdGeomPrimvarsAPI(mesh_prim).GetPrimvar(
      TempBlendShapeWeightsPrimvarName);

  if (!temp_weights_attr) {
    /* No need to create the animation. */
    return;
  }

  pxr::SdfPath anim_path = skel.GetPath().AppendChild(usdtokens::Anim);
  pxr::UsdSkelAnimation anim = pxr::UsdSkelAnimation::Define(stage, anim_path);

  if (!anim) {
    CLOG_WARN(&LOG, "Couldn't define animation at path %s", anim_path.GetAsString().c_str());
    return;
  }

  pxr::VtTokenArray blendshape_names;
  skel_api.GetBlendShapesAttr().Get(&blendshape_names);
  anim.CreateBlendShapesAttr().Set(blendshape_names);

  std::vector<double> times;
  temp_weights_attr.GetTimeSamples(&times);

  pxr::UsdAttribute anim_weights_attr = anim.CreateBlendShapeWeightsAttr();

  pxr::VtFloatArray weights;
  for (const double time : times) {
    if (temp_weights_attr.Get(&weights, time)) {
      anim_weights_attr.Set(weights, time);
    }
  }

  /* Next, set the animation source on the skeleton. */

  skel_api = pxr::UsdSkelBindingAPI::Apply(skel.GetPrim());

  if (!skel_api) {
    CLOG_WARN(&LOG,
              "Couldn't apply UsdSkelBindingAPI to skeleton prim %s",
              skel.GetPath().GetAsString().c_str());
    return;
  }

  if (!skel_api.CreateAnimationSourceRel().AddTarget(pxr::SdfPath(usdtokens::Anim))) {
    CLOG_WARN(&LOG,
              "Couldn't set animation source on skeleton %s",
              skel.GetPath().GetAsString().c_str());
  }

  pxr::UsdGeomPrimvarsAPI(mesh_prim).RemovePrimvar(TempBlendShapeWeightsPrimvarName);
}

const Key *get_mesh_shape_key(const Object *obj)
{
  BLI_assert(obj);

  if (!obj->data || obj->type != OB_MESH) {
    return nullptr;
  }

  const Mesh *mesh = static_cast<const Mesh *>(obj->data);

  return mesh->key;
}

bool is_mesh_with_shape_keys(const Object *obj)
{
  const Key *key = get_mesh_shape_key(obj);
  return key && key->totkey > 0 && key->type == KEY_RELATIVE;
}

void create_blend_shapes(pxr::UsdStageRefPtr stage,
                         const Object *obj,
                         const pxr::UsdPrim &mesh_prim,
                         bool allow_unicode)
{
  const Key *key = get_mesh_shape_key(obj);

  if (!(key && mesh_prim)) {
    return;
  }

  pxr::UsdSkelBindingAPI skel_api = pxr::UsdSkelBindingAPI::Apply(mesh_prim);

  if (!skel_api) {
    CLOG_WARN(&LOG,
              "Couldn't apply UsdSkelBindingAPI to mesh prim %s",
              mesh_prim.GetPath().GetAsString().c_str());
    return;
  }

  pxr::VtTokenArray blendshape_names;
  std::vector<pxr::SdfPath> blendshape_paths;

  /* Get the basis, which we'll use to calculate offsets. */
  KeyBlock *basis_key = static_cast<KeyBlock *>(key->block.first);

  if (!basis_key) {
    return;
  }

  int basis_totelem = basis_key->totelem;

  LISTBASE_FOREACH (KeyBlock *, kb, &key->block) {
    if (!kb) {
      continue;
    }

    if (kb == basis_key) {
      /* Skip the basis. */
      continue;
    }

    pxr::TfToken name(make_safe_name(kb->name, allow_unicode));
    blendshape_names.push_back(name);

    pxr::SdfPath path = mesh_prim.GetPath().AppendChild(name);
    blendshape_paths.push_back(path);

    pxr::UsdSkelBlendShape blendshape = pxr::UsdSkelBlendShape::Define(stage, path);

    pxr::UsdAttribute offsets_attr = blendshape.CreateOffsetsAttr();

    /* Some applications, like Houdini, don't render blend shapes unless the point
     * indices are set, so we always create this attribute, even when every index
     * is included. */
    pxr::UsdAttribute point_indices_attr = blendshape.CreatePointIndicesAttr();

    pxr::VtVec3fArray offsets(kb->totelem);
    pxr::VtIntArray indices(kb->totelem);
    std::iota(indices.begin(), indices.end(), 0);

    const float (*fp)[3] = static_cast<float (*)[3]>(kb->data);

    const float (*basis_fp)[3] = static_cast<float (*)[3]>(basis_key->data);

    for (int i = 0; i < kb->totelem; ++i) {
      /* Subtract the key positions from the
       * basis positions to get the offsets. */
      sub_v3_v3v3(offsets[i].data(), fp[i], basis_fp[i]);
    }

    offsets_attr.Set(offsets);
    point_indices_attr.Set(indices);
  }

  /* Set the blend-shape names and targets on the shape. */
  pxr::UsdAttribute blendshape_attr = skel_api.CreateBlendShapesAttr();
  blendshape_attr.Set(blendshape_names);
  skel_api.CreateBlendShapeTargetsRel().SetTargets(blendshape_paths);

  /* Some DCCs seem to require joint indices and weights to
   * bind the skeleton for blend-shapes, so we create these primvars, if needed. */

  if (!skel_api.GetJointIndicesAttr().HasAuthoredValue()) {
    pxr::VtArray<int> joint_indices(basis_totelem, 0);
    skel_api.CreateJointIndicesPrimvar(false, 1).GetAttr().Set(joint_indices);
  }

  if (!skel_api.GetJointWeightsAttr().HasAuthoredValue()) {
    pxr::VtArray<float> joint_weights(basis_totelem, 1.0f);
    skel_api.CreateJointWeightsPrimvar(false, 1).GetAttr().Set(joint_weights);
  }
}

pxr::VtFloatArray get_blendshape_weights(const Key *key)
{
  BLI_assert(key);

  pxr::VtFloatArray weights;

  LISTBASE_FOREACH (KeyBlock *, kb, &key->block) {
    if (kb == key->block.first) {
      /* Skip the first key, which is the basis. */
      continue;
    }
    weights.push_back(kb->curval);
  }

  return weights;
}

void remap_blend_shape_anim(pxr::UsdStageRefPtr stage,
                            const pxr::SdfPath &skel_path,
                            const pxr::SdfPathSet &mesh_paths)
{
  pxr::UsdSkelBindingAPI skel_api = pxr::UsdSkelBindingAPI::Get(stage, skel_path);

  if (!skel_api) {
    CLOG_WARN(&LOG, "Couldn't get skeleton from path %s", skel_path.GetAsString().c_str());
    return;
  }

  /* Use existing animation if possible, otherwise create a new one. */
  pxr::UsdPrim anim_prim;
  pxr::UsdSkelAnimation anim;
  if (skel_api.GetAnimationSource(&anim_prim)) {
    anim = pxr::UsdSkelAnimation(anim_prim);
  }
  else {
    pxr::SdfPath anim_path = skel_path.AppendChild(usdtokens::Anim);
    anim = pxr::UsdSkelAnimation::Define(stage, anim_path);
  }

  if (!anim) {
    CLOG_WARN(&LOG, "Couldn't get animation under skeleton %s", skel_path.GetAsString().c_str());
    return;
  }

  Vector<BlendShapeMergeInfo> merge_info;

  /* We are merging blend shape names and weights from multiple
   * meshes to a single animation. In case of name collisions,
   * we must generate unique blend shape names for the merged
   * result.  This set keeps track of the unique names that will
   * be combined on the animation. */
  Set<std::string> merged_names;

  /* Iterate over all the meshes, generate unique blend shape names in case of name
   * collisions and set up the information we will need to merge the results. */
  for (const pxr::SdfPath &mesh_path : mesh_paths) {

    pxr::UsdPrim mesh_prim = stage->GetPrimAtPath(mesh_path);
    pxr::UsdSkelBindingAPI mesh_skel_api = pxr::UsdSkelBindingAPI::Apply(mesh_prim);
    if (!mesh_skel_api) {
      CLOG_WARN(&LOG,
                "Couldn't apply UsdSkelBindingAPI to mesh prim %s",
                mesh_path.GetAsString().c_str());
      continue;
    }

    /* Get the blend shape names for this mesh. */
    pxr::UsdAttribute blend_shapes_attr = mesh_skel_api.GetBlendShapesAttr();

    if (!blend_shapes_attr) {
      continue;
    }

    pxr::VtTokenArray names;
    if (!mesh_skel_api.GetBlendShapesAttr().Get(&names)) {
      continue;
    }

    /* Ensure the names are unique. */
    pxr::VtTokenArray unique_names;

    for (const pxr::TfToken &name : names.AsConst()) {
      std::string unique = add_unique_name(merged_names, name.GetString());
      unique_names.push_back(pxr::TfToken(unique));
    }

    /* Set the unique names back on the mesh. */
    mesh_skel_api.GetBlendShapesAttr().Set(unique_names);

    /* Look up the temporary weights time sample we wrote to the mesh. */
    const pxr::UsdAttribute temp_weights_attr = pxr::UsdGeomPrimvarsAPI(mesh_prim).GetPrimvar(
        TempBlendShapeWeightsPrimvarName);

    if (!temp_weights_attr) {
      /* No need to create the animation. Shouldn't usually happen. */
      return;
    }

    /* Generate information we will need to merge the weight samples below. */
    merge_info.append(BlendShapeMergeInfo());
    merge_info.last().src_blend_shapes = unique_names;
    merge_info.last().src_weights_attr = temp_weights_attr;
  }

  if (merged_names.is_empty()) {
    /* No blend shape names were collected. Shouldn't usually happen. */
    return;
  }

  /* Copy the list of name strings to a list of tokens, since we need to work with tokens. */
  pxr::VtTokenArray skel_blend_shape_names;
  for (const std::string &name : merged_names) {
    skel_blend_shape_names.push_back(pxr::TfToken(name));
  }

  /* Initialize the merge info structs with the list of names on the merged animation. */
  for (BlendShapeMergeInfo &info : merge_info) {
    info.init_anim_map(skel_blend_shape_names);
  }

  /* Set the names on the animation prim. */
  anim.CreateBlendShapesAttr().Set(skel_blend_shape_names);

  pxr::UsdAttribute dst_weights_attr = anim.CreateBlendShapeWeightsAttr();

  /* Merge the weight time samples. */
  std::vector<double> times;
  merge_info.first().src_weights_attr.GetTimeSamples(&times);

  if (times.empty()) {
    /* Times may be empty if there is only a default value for the weights,
     * so we read the default. */
    times.push_back(pxr::UsdTimeCode::Default().GetValue());
  }

  pxr::VtFloatArray dst_weights;

  for (const double time : times) {
    for (const BlendShapeMergeInfo &info : merge_info) {
      pxr::VtFloatArray src_weights;
      if (info.src_weights_attr.Get(&src_weights, time)) {
        if (!info.anim_map.Remap(src_weights.AsConst(), &dst_weights)) {
          CLOG_WARN(&LOG, "Failed remapping blend shape weights");
        }
      }
    }
    /* Set the merged weights on the animation. */
    dst_weights_attr.Set(dst_weights, time);
  }
}

Mesh *get_shape_key_basis_mesh(Object *obj)
{
  if (!obj || !obj->data || obj->type != OB_MESH) {
    return nullptr;
  }

  /* If we're exporting blend shapes, we export the unmodified mesh with
   * the verts in the basis key positions. */
  const Mesh *mesh = BKE_object_get_pre_modified_mesh(obj);

  if (!mesh || !mesh->key || !mesh->key->block.first) {
    return nullptr;
  }

  const KeyBlock *basis = reinterpret_cast<KeyBlock *>(mesh->key->block.first);

  if (mesh->verts_num != basis->totelem) {
    CLOG_WARN(&LOG, "Vertex and shape key element count mismatch for mesh %s", obj->id.name + 2);
    return nullptr;
  }

  /* Make a copy of the mesh so we can update the verts to the basis shape. */
  Mesh *temp_mesh = BKE_mesh_copy_for_eval(*mesh);

  /* Update the verts. */
  BKE_keyblock_convert_to_mesh(basis, temp_mesh->vert_positions_for_write());

  return temp_mesh;
}

}  // namespace blender::io::usd
