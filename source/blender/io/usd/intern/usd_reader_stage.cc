/* SPDX-FileCopyrightText: 2021 Tangent Animation and. NVIDIA Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "usd_reader_stage.h"
#include "usd_reader_camera.h"
#include "usd_reader_curve.h"
#include "usd_reader_light.h"
#include "usd_reader_material.h"
#include "usd_reader_mesh.h"
#include "usd_reader_nurbs.h"
#include "usd_reader_prim.h"
#include "usd_reader_shape.h"
#include "usd_reader_skeleton.h"
#include "usd_reader_volume.h"
#include "usd_reader_xform.h"

#include <pxr/pxr.h>
#include <pxr/usd/usd/primRange.h>
#include <pxr/usd/usdGeom/camera.h>
#include <pxr/usd/usdGeom/capsule.h>
#include <pxr/usd/usdGeom/cone.h>
#include <pxr/usd/usdGeom/cube.h>
#include <pxr/usd/usdGeom/curves.h>
#include <pxr/usd/usdGeom/cylinder.h>
#include <pxr/usd/usdGeom/mesh.h>
#include <pxr/usd/usdGeom/nurbsCurves.h>
#include <pxr/usd/usdGeom/scope.h>
#include <pxr/usd/usdGeom/sphere.h>
#include <pxr/usd/usdGeom/xform.h>
#include <pxr/usd/usdShade/material.h>

#if PXR_VERSION >= 2111
#  include <pxr/usd/usdLux/boundableLightBase.h>
#  include <pxr/usd/usdLux/nonboundableLightBase.h>
#else
#  include <pxr/usd/usdLux/light.h>
#endif

#include <iostream>

#include "BLI_sort.hh"
#include "BLI_string.h"

#include "BKE_lib_id.h"
#include "BKE_modifier.h"

#include "DNA_material_types.h"

#include "WM_api.hh"

namespace blender::io::usd {

USDStageReader::USDStageReader(pxr::UsdStageRefPtr stage,
                               const USDImportParams &params,
                               const ImportSettings &settings)
    : stage_(stage), params_(params), settings_(settings)
{
}

USDStageReader::~USDStageReader()
{
  clear_readers();
}

bool USDStageReader::valid() const
{
  return stage_;
}

bool USDStageReader::is_primitive_prim(const pxr::UsdPrim &prim) const
{
  return (prim.IsA<pxr::UsdGeomCapsule>() || prim.IsA<pxr::UsdGeomCylinder>() ||
          prim.IsA<pxr::UsdGeomCone>() || prim.IsA<pxr::UsdGeomCube>() ||
          prim.IsA<pxr::UsdGeomSphere>());
}

USDPrimReader *USDStageReader::create_reader_if_allowed(const pxr::UsdPrim &prim)
{
  if (params_.import_shapes && is_primitive_prim(prim)) {
    return new USDShapeReader(prim, params_, settings_);
  }
  if (params_.import_cameras && prim.IsA<pxr::UsdGeomCamera>()) {
    return new USDCameraReader(prim, params_, settings_);
  }
  if (params_.import_curves && prim.IsA<pxr::UsdGeomBasisCurves>()) {
    return new USDCurvesReader(prim, params_, settings_);
  }
  if (params_.import_curves && prim.IsA<pxr::UsdGeomNurbsCurves>()) {
    return new USDNurbsReader(prim, params_, settings_);
  }
  if (params_.import_meshes && prim.IsA<pxr::UsdGeomMesh>()) {
    return new USDMeshReader(prim, params_, settings_);
  }
#if PXR_VERSION >= 2111
  if (params_.import_lights &&
      (prim.IsA<pxr::UsdLuxBoundableLightBase>() || prim.IsA<pxr::UsdLuxNonboundableLightBase>()))
  {
#else
  if (params_.import_lights && prim.IsA<pxr::UsdLuxLight>()) {
#endif
    return new USDLightReader(prim, params_, settings_);
  }
  if (params_.import_volumes && prim.IsA<pxr::UsdVolVolume>()) {
    return new USDVolumeReader(prim, params_, settings_);
  }
  if (params_.import_skeletons && prim.IsA<pxr::UsdSkelSkeleton>()) {
    return new USDSkeletonReader(prim, params_, settings_);
  }
  if (prim.IsA<pxr::UsdGeomImageable>()) {
    return new USDXformReader(prim, params_, settings_);
  }

  return nullptr;
}

USDPrimReader *USDStageReader::create_reader(const pxr::UsdPrim &prim)
{
  if (is_primitive_prim(prim)) {
    return new USDShapeReader(prim, params_, settings_);
  }
  if (prim.IsA<pxr::UsdGeomCamera>()) {
    return new USDCameraReader(prim, params_, settings_);
  }
  if (prim.IsA<pxr::UsdGeomBasisCurves>()) {
    return new USDCurvesReader(prim, params_, settings_);
  }
  if (prim.IsA<pxr::UsdGeomNurbsCurves>()) {
    return new USDNurbsReader(prim, params_, settings_);
  }
  if (prim.IsA<pxr::UsdGeomMesh>()) {
    return new USDMeshReader(prim, params_, settings_);
  }
#if PXR_VERSION >= 2111
  if (prim.IsA<pxr::UsdLuxBoundableLightBase>() || prim.IsA<pxr::UsdLuxNonboundableLightBase>()) {
#else
  if (prim.IsA<pxr::UsdLuxLight>()) {
#endif
    return new USDLightReader(prim, params_, settings_);
  }
  if (prim.IsA<pxr::UsdVolVolume>()) {
    return new USDVolumeReader(prim, params_, settings_);
  }
  if (prim.IsA<pxr::UsdSkelSkeleton>()) {
    return new USDSkeletonReader(prim, params_, settings_);
  }
  if (prim.IsA<pxr::UsdGeomImageable>()) {
    return new USDXformReader(prim, params_, settings_);
  }
  return nullptr;
}

bool USDStageReader::include_by_visibility(const pxr::UsdGeomImageable &imageable) const
{
  if (!params_.import_visible_only) {
    /* Invisible prims are allowed. */
    return true;
  }

  pxr::UsdAttribute visibility_attr = imageable.GetVisibilityAttr();

  if (!visibility_attr) {
    /* No visibility attribute, so allow. */
    return true;
  }

  /* Include if the prim has an animating visibility attribute or is not invisible. */

  if (visibility_attr.ValueMightBeTimeVarying()) {
    return true;
  }

  pxr::TfToken visibility;
  visibility_attr.Get(&visibility);
  return visibility != pxr::UsdGeomTokens->invisible;
}

bool USDStageReader::include_by_purpose(const pxr::UsdGeomImageable &imageable) const
{
  if (params_.import_skeletons && imageable.GetPrim().IsA<pxr::UsdSkelSkeleton>()) {
    /* Always include skeletons, if requested by the user, regardless of purpose. */
    return true;
  }

  if (params_.import_guide && params_.import_proxy && params_.import_render) {
    /* The options allow any purpose, so we trivially include the prim. */
    return true;
  }

  pxr::UsdAttribute purpose_attr = imageable.GetPurposeAttr();

  if (!purpose_attr) {
    /* No purpose attribute, so trivially include the prim. */
    return true;
  }

  pxr::TfToken purpose;
  purpose_attr.Get(&purpose);

  if (purpose == pxr::UsdGeomTokens->guide) {
    return params_.import_guide;
  }
  if (purpose == pxr::UsdGeomTokens->proxy) {
    return params_.import_proxy;
  }
  if (purpose == pxr::UsdGeomTokens->render) {
    return params_.import_render;
  }

  return true;
}

/* Determine if the given reader can use the parent of the encapsulated USD prim
 * to compute the Blender object's transform. If so, the reader is appropriately
 * flagged and the function returns true. Otherwise, the function returns false. */
static bool merge_with_parent(USDPrimReader *reader)
{
  USDXformReader *xform_reader = dynamic_cast<USDXformReader *>(reader);

  if (!xform_reader) {
    return false;
  }

  /* Check if the Xform reader is already merged. */
  if (xform_reader->use_parent_xform()) {
    return false;
  }

  /* Only merge if the parent is an Xform. */
  if (!xform_reader->prim().GetParent().IsA<pxr::UsdGeomXform>()) {
    return false;
  }

  /* Don't merge Xform and Scope prims. */
  if (xform_reader->prim().IsA<pxr::UsdGeomXform>() ||
      xform_reader->prim().IsA<pxr::UsdGeomScope>()) {
    return false;
  }

  /* Don't merge if the prim has authored transform ops. */
  if (xform_reader->prim_has_xform_ops()) {
    return false;
  }

  /* Flag the Xform reader as merged. */
  xform_reader->set_use_parent_xform(true);

  return true;
}

USDPrimReader *USDStageReader::collect_readers(Main *bmain, const pxr::UsdPrim &prim)
{
  if (prim.IsA<pxr::UsdGeomImageable>()) {
    pxr::UsdGeomImageable imageable(prim);

    if (!include_by_purpose(imageable)) {
      return nullptr;
    }

    if (!include_by_visibility(imageable)) {
      return nullptr;
    }
  }

  pxr::Usd_PrimFlagsPredicate filter_predicate = pxr::UsdPrimDefaultPredicate;

  if (params_.import_instance_proxies) {
    filter_predicate = pxr::UsdTraverseInstanceProxies(filter_predicate);
  }

  pxr::UsdPrimSiblingRange children = prim.GetFilteredChildren(filter_predicate);

  std::vector<USDPrimReader *> child_readers;

  for (const auto &childPrim : children) {
    if (USDPrimReader *child_reader = collect_readers(bmain, childPrim)) {
      child_readers.push_back(child_reader);
    }
  }

  if (prim.IsPseudoRoot()) {
    return nullptr;
  }

  /* Check if we can merge an Xform with its child prim. */
  if (child_readers.size() == 1) {

    USDPrimReader *child_reader = child_readers.front();

    if (merge_with_parent(child_reader)) {
      return child_reader;
    }
  }

  if (prim.IsA<pxr::UsdShadeMaterial>()) {
    /* Record material path for later processing, if needed,
     * e.g., when importing all materials. */
    material_paths_.push_back(prim.GetPath().GetAsString());

    /* We don't create readers for materials, so return early. */
    return nullptr;
  }

  USDPrimReader *reader = create_reader_if_allowed(prim);

  if (!reader) {
    return nullptr;
  }

  readers_.push_back(reader);
  reader->incref();

  /* Set each child reader's parent. */
  for (USDPrimReader *child_reader : child_readers) {
    child_reader->parent(reader);
  }

  return reader;
}

void USDStageReader::collect_readers(Main *bmain)
{
  if (!valid()) {
    return;
  }

  clear_readers();

  /* Iterate through the stage. */
  pxr::UsdPrim root = stage_->GetPseudoRoot();

  stage_->SetInterpolationType(pxr::UsdInterpolationType::UsdInterpolationTypeHeld);
  collect_readers(bmain, root);
}

void USDStageReader::process_armature_modifiers() const
{
  /* Iteratate over the skeleton readers to create the
   * armature object map, which maps a USD skeleton prim
   * path to the corresponding armature object. */
  std::map<std::string, Object *> usd_path_to_armature;
  for (const USDPrimReader *reader : readers_) {
    if (dynamic_cast<const USDSkeletonReader *>(reader) && reader->object()) {
      usd_path_to_armature.insert(std::make_pair(reader->prim_path(), reader->object()));
    }
  }

  /* Iterate over the mesh readers and set armature objects on armature modifiers. */
  for (const USDPrimReader *reader : readers_) {
    if (!reader->object()) {
      continue;
    }
    const USDMeshReader *mesh_reader = dynamic_cast<const USDMeshReader *>(reader);
    if (!mesh_reader) {
      continue;
    }
    /* Check if the mesh object has an armature modifier. */
    ModifierData *md = BKE_modifiers_findby_type(reader->object(), eModifierType_Armature);
    if (!md) {
      continue;
    }

    ArmatureModifierData *amd = reinterpret_cast<ArmatureModifierData *>(md);

    /* Assign the armature based on the bound USD skeleton path of the skinned mesh. */
    std::string skel_path = mesh_reader->get_skeleton_path();
    std::map<std::string, Object *>::const_iterator it = usd_path_to_armature.find(skel_path);
    if (it == usd_path_to_armature.end()) {
      WM_reportf(RPT_WARNING,
                 "%s: Couldn't find armature object corresponding to USD skeleton %s",
                 __func__,
                 skel_path.c_str());
    }
    amd->object = it->second;
  }
}

void USDStageReader::import_all_materials(Main *bmain)
{
  BLI_assert(valid());

  /* Build the material name map if it's not built yet. */
  if (settings_.mat_name_to_mat.empty()) {
    build_material_map(bmain, &settings_.mat_name_to_mat);
  }

  USDMaterialReader mtl_reader(params_, bmain);

  for (const std::string &mtl_path : material_paths_) {
    pxr::UsdPrim prim = stage_->GetPrimAtPath(pxr::SdfPath(mtl_path));

    pxr::UsdShadeMaterial usd_mtl(prim);
    if (!usd_mtl) {
      continue;
    }

    if (blender::io::usd::find_existing_material(
            prim.GetPath(), params_, settings_.mat_name_to_mat, settings_.usd_path_to_mat_name))
    {
      /* The material already exists. */
      continue;
    }

    /* Add the material now. */
    Material *new_mtl = mtl_reader.add_material(usd_mtl);
    BLI_assert_msg(new_mtl, "Failed to create material");

    const std::string mtl_name = pxr::TfMakeValidIdentifier(new_mtl->id.name + 2);
    settings_.mat_name_to_mat[mtl_name] = new_mtl;

    if (params_.mtl_name_collision_mode == USD_MTL_NAME_COLLISION_MAKE_UNIQUE) {
      /* Record the unique name of the Blender material we created for the USD material
       * with the given path, so we don't import the material again when assigning
       * materials to objects elsewhere in the code. */
      settings_.usd_path_to_mat_name[prim.GetPath().GetAsString()] = mtl_name;
    }
  }
}

void USDStageReader::fake_users_for_unused_materials()
{
  /* Iterate over the imported materials and set a fake user for any unused
   * materials. */
  for (const std::pair<const std::string, std::string> &path_mat_pair :
       settings_.usd_path_to_mat_name)
  {

    std::map<std::string, Material *>::iterator mat_it = settings_.mat_name_to_mat.find(
        path_mat_pair.second);

    if (mat_it == settings_.mat_name_to_mat.end()) {
      continue;
    }

    Material *mat = mat_it->second;
    if (mat->id.us == 0) {
      id_fake_user_set(&mat->id);
    }
  }
}

void USDStageReader::clear_readers()
{
  for (USDPrimReader *reader : readers_) {
    if (!reader) {
      continue;
    }

    reader->decref();

    if (reader->refcount() == 0) {
      delete reader;
    }
  }

  readers_.clear();
}

void USDStageReader::sort_readers()
{
  blender::parallel_sort(
      readers_.begin(), readers_.end(), [](const USDPrimReader *a, const USDPrimReader *b) {
        const char *na = a ? a->name().c_str() : "";
        const char *nb = b ? b->name().c_str() : "";
        return BLI_strcasecmp(na, nb) < 0;
      });
}

}  // Namespace blender::io::usd
