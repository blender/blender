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
#include "usd_writer_blendshape_mesh.h"
#include "usd_hierarchy_iterator.h"

#include <pxr/base/gf/matrix4d.h>
#include <pxr/base/gf/matrix4f.h>
#include <pxr/usd/usdGeom/mesh.h>
#include <pxr/usd/usdSkel/animation.h>
#include <pxr/usd/usdSkel/blendShape.h>

#include "BKE_key.h"
#include "BKE_lib_id.h"
#include "BKE_mesh.hh"
#include "BKE_mesh_runtime.hh"
#include "BKE_modifier.h"
#include "BKE_object.h"

#include "BLI_math_vector.h"

#include "DNA_key_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_meta_types.h"

#include "WM_api.hh"
#include "WM_types.hh"

#include <string>

namespace usdtokens {
static const pxr::TfToken Anim("Anim", pxr::TfToken::Immortal);
static const pxr::TfToken Skel("Skel", pxr::TfToken::Immortal);
static const pxr::TfToken joint1("joint1", pxr::TfToken::Immortal);
}  // namespace usdtokens

namespace blender::io::usd {

static pxr::VtFloatArray get_blendshape_weights(const Key *key)
{
  pxr::VtFloatArray weights;

  LISTBASE_FOREACH (KeyBlock *, kb, &key->block) {
    if (kb == key->block.first) {
      // Skip the first key, which is the basis.
      continue;
    }
    weights.push_back(kb->curval);
  }

  return weights;
}

static const Key *get_shape_key(Object *obj)
{
  if (!(obj && obj->data)) {
    return nullptr;
  }

  if (obj->type != OB_MESH) {
    return nullptr;
  }

  const Mesh *mesh = static_cast<Mesh *>(obj->data);

  return mesh->key;
}

// static void print_blendshape_info(Object *obj)
//{
//  const Key *key = get_shape_key(obj);
//
//  if (!key) {
//    return;
//  }
//
//  printf("have shape key\n");
//  const int num_keys = key->totkey;
//  printf("num keys %d\n", num_keys);
//  printf("type %d\n", key->type);
//  printf("ctime: %f\n", key->ctime);
//  // BKE_keyblock_convert_to_mesh()
//  // BKE_keyblock_mesh_calc_normals()
//  // BKE_keyblock_element_count_from_shape()
//  LISTBASE_FOREACH (KeyBlock *, kb, &key->block) {
//    printf("%s %f %f\n", kb->name, kb->curval, kb->pos);
//  }
//
//  printf("anim pointer %p\n", key->adt);
//}

bool is_blendshape_mesh(Object *obj)
{
  const Key *key = get_shape_key(obj);

  return key && key->totkey > 0 && key->type == KEY_RELATIVE;
}

USDBlendShapeMeshWriter::USDBlendShapeMeshWriter(const USDExporterContext &ctx)
    : USDMeshWriter(ctx)
{
}

void USDBlendShapeMeshWriter::do_write(HierarchyContext &context)
{
  if (!this->frame_has_been_written_) {
    USDGenericMeshWriter::do_write(context);
  }

  write_blendshape(context);
}

bool USDBlendShapeMeshWriter::is_supported(const HierarchyContext *context) const
{
  return is_blendshape_mesh(context->object) && USDGenericMeshWriter::is_supported(context);
}

bool USDBlendShapeMeshWriter::check_is_animated(const HierarchyContext &context) const
{
  const Key *key = get_shape_key(context.object);

  return key && key->totkey > 0 && key->adt != nullptr;
}

void USDBlendShapeMeshWriter::write_blendshape(HierarchyContext &context) const
{
  /* A blendshape writer might be created even if
   * there are no blendshapes, so check that blendshapes
   * exist before continuting. */
  if (!is_blendshape_mesh(context.object)) {
    return;
  }

  const Key *key = get_shape_key(context.object);

  if (!key || !key->block.first) {
    WM_reportf(RPT_WARNING,
               "WARNING: couldn't get shape key for blendshape mesh prim %s",
               usd_export_context_.usd_path.GetString().c_str());
    return;
  }

  /* Validate the offset counts. */
  Mesh *src_mesh = static_cast<Mesh *>(context.object->data);
  KeyBlock *basis = reinterpret_cast<KeyBlock *>(src_mesh->key->block.first);
  if (src_mesh->totvert != basis->totelem) {
    /* No need for a warning, as we would have warned about
     * the vert count mismatch when creating the mesh. */
    return;
  }

  pxr::UsdSkelSkeleton skel = get_skeleton(context);

  if (!skel) {
    printf("WARNING: couldn't get skeleton for blendshape mesh prim %s\n",
           this->usd_export_context_.usd_path.GetString().c_str());
    return;
  }

  if (!this->frame_has_been_written_) {
    pxr::UsdPrim mesh_prim = usd_export_context_.stage->GetPrimAtPath(
        usd_export_context_.usd_path);

    if (!mesh_prim.IsValid()) {
      printf("WARNING: couldn't get valid mesh prim for blendshape mesh %s\n",
             this->usd_export_context_.usd_path.GetString().c_str());
      return;
    }

    create_blend_shapes(key, mesh_prim, skel);
  }

  if (exporting_anim(key)) {
    add_weights_sample(key, skel);
  }
}

void USDBlendShapeMeshWriter::create_blend_shapes(const Key *key,
                                                  const pxr::UsdPrim &mesh_prim,
                                                  const pxr::UsdSkelSkeleton &skel) const
{
  if (!(key && mesh_prim && skel)) {
    return;
  }

  pxr::UsdSkelBindingAPI skel_api = pxr::UsdSkelBindingAPI::Apply(mesh_prim);

  if (!skel_api) {
    printf("WARNING: couldn't apply UsdSkelBindingAPI to blendshape mesh prim %s\n",
           mesh_prim.GetPath().GetAsString().c_str());
    return;
  }

  skel_api.CreateSkeletonRel().AddTarget(skel.GetPath());

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
      // Skip the first key, which is the basis.
      continue;
    }

    pxr::TfToken name(pxr::TfMakeValidIdentifier(kb->name));
    blendshape_names.push_back(name);

    pxr::SdfPath path = usd_export_context_.usd_path.AppendChild(name);
    blendshape_paths.push_back(path);

    pxr::UsdSkelBlendShape blendshape =
        usd_export_context_.usd_define_or_over<pxr::UsdSkelBlendShape>(path);

    pxr::UsdAttribute offsets_attr = blendshape.CreateOffsetsAttr();

    /* Some applications, like Houdini, don't render blend shapes unless the point
     * indices are set, so we always create this attribute, even when every index
     * is included. */
    pxr::UsdAttribute point_indices_attr = blendshape.CreatePointIndicesAttr();

    pxr::VtVec3fArray offsets(kb->totelem);
    pxr::VtIntArray indices(kb->totelem);

    const float(*fp)[3] = static_cast<float(*)[3]>(kb->data);

    const float(*basis_fp)[3] = static_cast<float(*)[3]>(basis_key->data);

    for (int i = 0; i < kb->totelem; ++i) {
      /* Subtract the key positions from the
       * basis positions to get the offsets. */
      sub_v3_v3v3(offsets[i].data(), fp[i], basis_fp[i]);
      indices[i] = i;
    }

    offsets_attr.Set(offsets);
    point_indices_attr.Set(indices);
  }

  /* Set the blendshape names and targets on the shape. */
  pxr::UsdAttribute blendshape_attr = skel_api.CreateBlendShapesAttr();
  blendshape_attr.Set(blendshape_names);
  skel_api.CreateBlendShapeTargetsRel().SetTargets(blendshape_paths);

  /* Some DCCs seem to require joint indices and weights to
   * bind the skeleton for blendshapes, so we we create these
   * primvars, if needed. */

  if (!skel_api.GetJointIndicesAttr().HasAuthoredValue()) {
    pxr::VtArray<int> joint_indices(basis_totelem, 0);
    skel_api.CreateJointIndicesPrimvar(false, 1).GetAttr().Set(joint_indices);
  }

  if (!skel_api.GetJointWeightsAttr().HasAuthoredValue()) {
    pxr::VtArray<float> joint_weights(basis_totelem, 1.0f);
    skel_api.CreateJointWeightsPrimvar(false, 1).GetAttr().Set(joint_weights);
  }

  /* Create the skeleton animation. */
  pxr::SdfPath anim_path = skel.GetPath().AppendChild(usdtokens::Anim);
  const pxr::UsdSkelAnimation anim = usd_export_context_.usd_define_or_over<pxr::UsdSkelAnimation>(
      anim_path);

  if (anim) {
    /* Set the blendshape names on the animation. */
    pxr::UsdAttribute blendshape_attr = anim.CreateBlendShapesAttr();
    blendshape_attr.Set(blendshape_names);

    pxr::VtFloatArray weights = get_blendshape_weights(key);
    pxr::UsdAttribute weights_attr = anim.CreateBlendShapeWeightsAttr();
    weights_attr.Set(weights);
  }
}

void USDBlendShapeMeshWriter::add_weights_sample(const Key *key,
                                                 const pxr::UsdSkelSkeleton &skel) const
{
  /* Create the skeleton animation. */
  pxr::SdfPath anim_path = skel.GetPath().AppendChild(usdtokens::Anim);
  const pxr::UsdSkelAnimation anim = usd_export_context_.usd_define_or_over<pxr::UsdSkelAnimation>(
      anim_path);

  if (anim) {
    pxr::VtFloatArray weights = get_blendshape_weights(key);
    pxr::UsdAttribute weights_attr = anim.CreateBlendShapeWeightsAttr();
    pxr::UsdTimeCode timecode = get_export_time_code();
    weights_attr.Set(weights, timecode);
  }
}

pxr::UsdSkelSkeleton USDBlendShapeMeshWriter::get_skeleton(const HierarchyContext &context) const
{
  pxr::SdfPath skel_path = usd_export_context_.usd_path.GetParentPath().AppendChild(
      usdtokens::Skel);

  pxr::UsdSkelSkeleton skel = usd_export_context_.usd_define_or_over<pxr::UsdSkelSkeleton>(
      skel_path);

  /* Initialize the skeleton. */
  pxr::VtMatrix4dArray bind_transforms(1, pxr::GfMatrix4d(1.0));
  pxr::VtMatrix4dArray rest_transforms(1, pxr::GfMatrix4d(1.0));
  skel.CreateBindTransformsAttr().Set(bind_transforms);
  skel.GetRestTransformsAttr().Set(rest_transforms);

  /* Some DCCs seem to require joint names to bind the
   * skeleton to blendshapes. */
  pxr::VtTokenArray joints({usdtokens::joint1});
  skel.CreateJointsAttr().Set(joints);

  /* Specify the animation source on the skeleton. */
  pxr::UsdSkelBindingAPI skel_api(skel.GetPrim());
  skel_api.CreateAnimationSourceRel().AddTarget(pxr::SdfPath(usdtokens::Anim));

  return skel;
}

Mesh *USDBlendShapeMeshWriter::get_export_mesh(Object *object_eval, bool &r_needsfree)
{
  /* We must check if blendshapes are enabled before attempting to create the
   * blendshape mesh. */
  if (!(usd_export_context_.export_params.export_blendshapes && is_blendshape_mesh(object_eval))) {
    /* Get the default mesh.  */
    return USDMeshWriter::get_export_mesh(object_eval, r_needsfree);
  }

  if (object_eval->type != OB_MESH) {
    return nullptr;
  }

  Mesh *src_mesh = BKE_object_get_pre_modified_mesh(object_eval);

  if (!src_mesh || !src_mesh->key || !src_mesh->key->block.first) {
    return nullptr;
  }

  KeyBlock *basis = reinterpret_cast<KeyBlock *>(src_mesh->key->block.first);

  if (src_mesh->totvert != basis->totelem) {
    WM_reportf(
        RPT_WARNING,
        "USD Export: mesh %s can't be exported as a blendshape because the mesh vertex count %d "
        "doesn't match shape key number of elements %d'.  This may be because the mesh topology "
        "was "
        "changed by a modifier.  Exporting meshes with modifiers as blendshapes isn't currently "
        "supported",
        object_eval->id.name + 2,
        src_mesh->totvert,
        basis->totelem);

    return USDMeshWriter::get_export_mesh(object_eval, r_needsfree);
  }

  Mesh *temp_mesh = reinterpret_cast<Mesh *>(
      BKE_id_copy_ex(nullptr, &src_mesh->id, nullptr, LIB_ID_COPY_LOCALIZE));

  BKE_keyblock_convert_to_mesh(
      basis,
      reinterpret_cast<float(*)[3]>(temp_mesh->vert_positions_for_write().data()),
      temp_mesh->totvert);

  r_needsfree = true;

  return temp_mesh;
}

/* Blend shape meshes are never animated, but the blendshape writer itself
 * might be animating as it must add time samples to skeletal animations.
 * This function ensures that the mesh data is written as non-timesampled.
 * This is currently required to work around a bug in Create which causes
 * a crash if the blendhshape mesh is timesampled. */
pxr::UsdTimeCode USDBlendShapeMeshWriter::get_mesh_export_time_code() const
{
  /* By using the default timecode USD won't even write a single `timeSample` for non-animated
   * data. Instead, it writes it as non-timesampled. */
  static pxr::UsdTimeCode default_timecode = pxr::UsdTimeCode::Default();
  return default_timecode;
}

bool USDBlendShapeMeshWriter::exporting_anim(const Key *shape_key) const
{
  return usd_export_context_.export_params.export_animation && shape_key && shape_key->adt;
}

}  // namespace blender::io::usd
