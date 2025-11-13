/* SPDX-FileCopyrightText: 2021 Tangent Animation and. NVIDIA Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "usd_reader_stage.hh"

#include "usd_hook.hh"
#include "usd_reader_camera.hh"
#include "usd_reader_curve.hh"
#include "usd_reader_instance.hh"
#include "usd_reader_light.hh"
#include "usd_reader_material.hh"
#include "usd_reader_mesh.hh"
#include "usd_reader_nurbs.hh"
#include "usd_reader_pointinstancer.hh"
#include "usd_reader_points.hh"
#include "usd_reader_prim.hh"
#include "usd_reader_shape.hh"
#include "usd_reader_skeleton.hh"
#include "usd_reader_volume.hh"
#include "usd_reader_xform.hh"
#include "usd_utils.hh"

#include <pxr/usd/usd/primRange.h>
#include <pxr/usd/usdGeom/camera.h>
#include <pxr/usd/usdGeom/capsule.h>
#include <pxr/usd/usdGeom/capsule_1.h>
#include <pxr/usd/usdGeom/cone.h>
#include <pxr/usd/usdGeom/cube.h>
#include <pxr/usd/usdGeom/cylinder.h>
#include <pxr/usd/usdGeom/cylinder_1.h>
#include <pxr/usd/usdGeom/mesh.h>
#include <pxr/usd/usdGeom/metrics.h>
#include <pxr/usd/usdGeom/nurbsCurves.h>
#include <pxr/usd/usdGeom/plane.h>
#include <pxr/usd/usdGeom/pointInstancer.h>
#include <pxr/usd/usdGeom/points.h>
#include <pxr/usd/usdGeom/scope.h>
#include <pxr/usd/usdGeom/sphere.h>
#include <pxr/usd/usdGeom/tokens.h>
#include <pxr/usd/usdGeom/xform.h>
#include <pxr/usd/usdLux/boundableLightBase.h>
#include <pxr/usd/usdLux/domeLight.h>
#include <pxr/usd/usdLux/domeLight_1.h>
#include <pxr/usd/usdLux/nonboundableLightBase.h>
#include <pxr/usd/usdShade/material.h>

#include "BLI_map.hh"
#include "BLI_math_base.h"
#include "BLI_math_matrix.h"
#include "BLI_math_rotation.h"
#include "BLI_sort.hh"
#include "BLI_string.h"

#include "BKE_collection.hh"
#include "BKE_lib_id.hh"
#include "BKE_modifier.hh"
#include "BKE_report.hh"

#include "CLG_log.h"

#include "DNA_collection_types.h"
#include "DNA_material_types.h"

#include <fmt/core.h>

static CLG_LogRef LOG = {"io.usd"};

namespace blender::io::usd {

static void decref(USDPrimReader *reader)
{
  if (!reader) {
    return;
  }

  reader->decref();

  if (reader->refcount() == 0) {
    delete reader;
  }
}

/**
 * Create a collection with the given parent and name.
 */
static Collection *create_collection(Main *bmain, Collection *parent, const char *name)
{
  if (!bmain) {
    return nullptr;
  }

  return BKE_collection_add(bmain, parent, name);
}

/**
 * Set the instance collection on the given instance reader.
 * The collection is assigned from the given map based on
 * the prototype prim path.
 */
static void set_instance_collection(
    USDInstanceReader *instance_reader,
    const blender::Map<pxr::SdfPath, Collection *> &proto_collection_map)
{
  if (!instance_reader) {
    return;
  }

  pxr::SdfPath proto_path = instance_reader->proto_path();

  Collection *collection = proto_collection_map.lookup_default(proto_path, nullptr);
  if (collection != nullptr) {
    instance_reader->set_instance_collection(collection);
  }
  else {
    CLOG_WARN(&LOG,
              "Couldn't find prototype collection for %s",
              instance_reader->prim_path().GetAsString().c_str());
  }
}

/* Update the given import settings with the global rotation matrix to orient
 * imported objects with Z-up, if necessary */
static void convert_to_z_up(pxr::UsdStageRefPtr stage, ImportSettings &settings)
{
  if (!stage || pxr::UsdGeomGetStageUpAxis(stage) == pxr::UsdGeomTokens->z) {
    return;
  }

  settings.do_convert_mat = true;

  /* Rotate 90 degrees about the X-axis. */
  float rmat[3][3];
  float axis[3] = {1.0f, 0.0f, 0.0f};
  axis_angle_normalized_to_mat3(rmat, axis, M_PI_2);

  unit_m4(settings.conversion_mat);
  copy_m4_m3(settings.conversion_mat, rmat);
}

/**
 * Find the lowest level of Blender generated roots
 * so that round tripping an export can be more invisible
 */
static void find_prefix_to_skip(pxr::UsdStageRefPtr stage, ImportSettings &settings)
{
  if (!stage) {
    return;
  }

  pxr::TfToken generated_key("Blender:generated");
  pxr::SdfPath path("/");
  auto prim = stage->GetPseudoRoot();
  while (true) {

    uint32_t child_count = 0;
    for (auto child : prim.GetChildren()) {
      if (child_count == 0) {
        prim = child.GetPrim();
      }
      ++child_count;
    }

    if (child_count != 1) {
      /* Our blender write out only supports a single root chain,
       * so whenever we encounter more than one child, we should
       * early exit */
      break;
    }

    /* We only care about prims that have the key and the value doesn't matter */
    if (!prim.HasCustomDataKey(generated_key)) {
      break;
    }
    path = path.AppendChild(prim.GetName());
  }

  /* Treat the root as empty */
  if (path == pxr::SdfPath("/")) {
    path = pxr::SdfPath();
  }

  settings.skip_prefix = path;
}

/**
 * Set compatibility flags if the Stage was written by Blender.
 */
static void determine_blender_compat(pxr::UsdStageRefPtr stage, ImportSettings &settings)
{
  const std::string doc = stage->GetRootLayer()->GetDocumentation();

  /* Was the incoming Stage written by Blender? If so, set some broad compatibility flags. */
  if (doc.find("Blender v", 0) == 0) {
    /* Set flag if the Blender Stage was from before version 4.4. */
    settings.blender_stage_version_prior_44 = doc < "Blender v4.4";
  }
}

USDStageReader::USDStageReader(pxr::UsdStageRefPtr stage,
                               const USDImportParams &params,
                               const std::function<CacheFile *()> &get_cache_file_fn)
    : stage_(stage), params_(params)
{
  determine_blender_compat(stage_, settings_);
  convert_to_z_up(stage_, settings_);
  find_prefix_to_skip(stage_, settings_);
  settings_.get_cache_file = get_cache_file_fn;
  settings_.stage_meters_per_unit = pxr::UsdGeomGetStageMetersPerUnit(stage);
  settings_.scene_scale = params.scale;
  if (params.apply_unit_conversion_scale) {
    settings_.scene_scale *= settings_.stage_meters_per_unit;
  }
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
  return (prim.IsA<pxr::UsdGeomCapsule>() || prim.IsA<pxr::UsdGeomCapsule_1>() ||
          prim.IsA<pxr::UsdGeomCylinder>() || prim.IsA<pxr::UsdGeomCylinder_1>() ||
          prim.IsA<pxr::UsdGeomCone>() || prim.IsA<pxr::UsdGeomCube>() ||
          prim.IsA<pxr::UsdGeomSphere>() || prim.IsA<pxr::UsdGeomPlane>());
}

USDPrimReader *USDStageReader::create_reader_if_allowed(const pxr::UsdPrim &prim)
{
  if (params_.support_scene_instancing && prim.IsInstance()) {
    return new USDInstanceReader(prim, params_, settings_);
  }
  if (params_.import_shapes && is_primitive_prim(prim)) {
    return new USDShapeReader(prim, params_, settings_);
  }
  if (prim.IsA<pxr::UsdGeomPointInstancer>()) {
    return new USDPointInstancerReader(prim, params_, settings_);
  }
  if (params_.import_cameras && prim.IsA<pxr::UsdGeomCamera>()) {
    return new USDCameraReader(prim, params_, settings_);
  }
  if (params_.import_curves && prim.IsA<pxr::UsdGeomBasisCurves>()) {
    return new USDBasisCurvesReader(prim, params_, settings_);
  }
  if (params_.import_curves && prim.IsA<pxr::UsdGeomNurbsCurves>()) {
    return new USDNurbsReader(prim, params_, settings_);
  }
  if (params_.import_meshes && prim.IsA<pxr::UsdGeomMesh>()) {
    return new USDMeshReader(prim, params_, settings_);
  }
  if (params_.import_lights &&
      (prim.IsA<pxr::UsdLuxDomeLight>() || prim.IsA<pxr::UsdLuxDomeLight_1>()))
  {
    /* Dome lights are handled elsewhere. */
    return nullptr;
  }
  if (params_.import_lights &&
      (prim.IsA<pxr::UsdLuxBoundableLightBase>() || prim.IsA<pxr::UsdLuxNonboundableLightBase>()))
  {
    return new USDLightReader(prim, params_, settings_);
  }
  if (params_.import_volumes && prim.IsA<pxr::UsdVolVolume>()) {
    return new USDVolumeReader(prim, params_, settings_);
  }
  if (params_.import_skeletons && prim.IsA<pxr::UsdSkelSkeleton>()) {
    return new USDSkeletonReader(prim, params_, settings_);
  }
  if (params_.import_points && prim.IsA<pxr::UsdGeomPoints>()) {
    return new USDPointsReader(prim, params_, settings_);
  }
  if (prim.IsA<pxr::UsdGeomImageable>()) {
    return new USDXformReader(prim, params_, settings_);
  }

  return nullptr;
}

USDPrimReader *USDStageReader::create_reader(const pxr::UsdPrim &prim)
{
  if (params_.support_scene_instancing && prim.IsInstance()) {
    return new USDInstanceReader(prim, params_, settings_);
  }
  if (is_primitive_prim(prim)) {
    return new USDShapeReader(prim, params_, settings_);
  }
  if (prim.IsA<pxr::UsdGeomCamera>()) {
    return new USDCameraReader(prim, params_, settings_);
  }
  if (prim.IsA<pxr::UsdGeomBasisCurves>()) {
    return new USDBasisCurvesReader(prim, params_, settings_);
  }
  if (prim.IsA<pxr::UsdGeomNurbsCurves>()) {
    return new USDNurbsReader(prim, params_, settings_);
  }
  if (prim.IsA<pxr::UsdGeomMesh>()) {
    return new USDMeshReader(prim, params_, settings_);
  }
  if (prim.IsA<pxr::UsdLuxDomeLight>() || prim.IsA<pxr::UsdLuxDomeLight_1>()) {
    /* We don't handle dome lights. */
    return nullptr;
  }
  if (prim.IsA<pxr::UsdLuxBoundableLightBase>() || prim.IsA<pxr::UsdLuxNonboundableLightBase>()) {
    return new USDLightReader(prim, params_, settings_);
  }
  if (prim.IsA<pxr::UsdVolVolume>()) {
    return new USDVolumeReader(prim, params_, settings_);
  }
  if (prim.IsA<pxr::UsdSkelSkeleton>()) {
    return new USDSkeletonReader(prim, params_, settings_);
  }
  if (prim.IsA<pxr::UsdGeomPoints>()) {
    return new USDPointsReader(prim, params_, settings_);
  }
  if (prim.IsA<pxr::UsdGeomPointInstancer>()) {
    return new USDPointInstancerReader(prim, params_, settings_);
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

bool USDStageReader::merge_with_parent(USDPrimReader *reader) const
{
  /* Don't merge if the param is set to false */
  if (!params_.merge_parent_xform) {
    return false;
  }

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
      xform_reader->prim().IsA<pxr::UsdGeomScope>())
  {
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

USDPrimReader *USDStageReader::collect_readers(const pxr::UsdPrim &prim,
                                               const UsdPathSet &pruned_prims,
                                               const bool defined_prims_only,
                                               blender::Vector<USDPrimReader *> &r_readers)
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

  if (prim.IsA<pxr::UsdLuxDomeLight>() || prim.IsA<pxr::UsdLuxDomeLight_1>()) {
    USDDomeLightReader *reader = new USDDomeLightReader(prim, params_, settings_);
    reader->incref();
    dome_light_readers_.append(reader);
  }

  pxr::Usd_PrimFlagsConjunction filter_flags = pxr::UsdPrimIsActive && pxr::UsdPrimIsLoaded &&
                                               !pxr::UsdPrimIsAbstract;

  if (defined_prims_only) {
    filter_flags &= pxr::UsdPrimIsDefined;
  }

  pxr::Usd_PrimFlagsPredicate filter_predicate(filter_flags);
  if (!params_.support_scene_instancing) {
    filter_predicate = pxr::UsdTraverseInstanceProxies(filter_predicate);
  }

  blender::Vector<USDPrimReader *> child_readers;

  pxr::UsdPrimSiblingRange children = prim.GetFilteredChildren(filter_predicate);

  for (const auto &child_prim : children) {
    if (pruned_prims.contains(child_prim.GetPath())) {
      continue;
    }
    if (USDPrimReader *child_reader = collect_readers(
            child_prim, pruned_prims, defined_prims_only, r_readers))
    {
      child_readers.append(child_reader);
    }
  }

  if (prim.IsPseudoRoot()) {
    return nullptr;
  }

  /* If we find prims that have been auto generated by Blender, we skip them on import
   * so that the imported scene can closely match the exported scene */
  if (!settings_.skip_prefix.IsEmpty()) {
    if (settings_.skip_prefix.HasPrefix(prim.GetPath())) {
      return nullptr;
    }
  }

  /* Check if we can merge an Xform with its child prim. */
  if (child_readers.size() == 1) {

    USDPrimReader *child_reader = child_readers.first();

    if (merge_with_parent(child_reader)) {
      return child_reader;
    }
  }

  if (prim.IsA<pxr::UsdShadeMaterial>()) {
    /* Record material path for later processing, if needed,
     * e.g., when importing all materials. */
    material_paths_.append(prim.GetPath());

    /* We don't create readers for materials, so return early. */
    return nullptr;
  }

  USDPrimReader *reader = create_reader_if_allowed(prim);

  if (!reader) {
    return nullptr;
  }
  if (!reader->valid()) {
    return nullptr;
  }

  r_readers.append(reader);
  reader->incref();

  /* Set each child reader's parent. */
  for (USDPrimReader *child_reader : child_readers) {
    child_reader->parent(reader);
  }

  return reader;
}

void USDStageReader::collect_readers()
{
  if (!valid()) {
    return;
  }

  clear_readers();

  /* Identify paths to point instancer prototypes, as these will be converted
   * in a separate pass over the stage. */
  UsdPathSet instancer_proto_paths = collect_point_instancer_proto_paths();

  /* Iterate through the stage. */
  pxr::UsdPrim root = stage_->GetPseudoRoot();

  stage_->SetInterpolationType(pxr::UsdInterpolationType::UsdInterpolationTypeHeld);

  /* Create readers, skipping over prototype prims in this pass. */
  collect_readers(root, instancer_proto_paths, params_.import_defined_only, readers_);

  if (params_.support_scene_instancing) {
    /* Collect the scene-graph instance prototypes. */
    std::vector<pxr::UsdPrim> protos = stage_->GetPrototypes();

    for (const pxr::UsdPrim &proto_prim : protos) {
      blender::Vector<USDPrimReader *> proto_readers;
      collect_readers(proto_prim, instancer_proto_paths, true, proto_readers);
      proto_readers_.add(proto_prim.GetPath(), proto_readers);

      for (USDPrimReader *reader : proto_readers) {
        readers_.append(reader);
        reader->incref();
      }
    }
  }

  if (!instancer_proto_paths.is_empty()) {
    create_point_instancer_proto_readers(instancer_proto_paths);
  }
}

void USDStageReader::process_armature_modifiers() const
{
  /* Iterate over the skeleton readers to create the
   * armature object map, which maps a USD skeleton prim
   * path to the corresponding armature object. */
  blender::Map<pxr::SdfPath, Object *> usd_path_to_armature;
  for (const USDPrimReader *reader : readers_) {
    if (dynamic_cast<const USDSkeletonReader *>(reader) && reader->object()) {
      usd_path_to_armature.add(reader->prim_path(), reader->object());
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
    pxr::SdfPath skel_path = mesh_reader->get_skeleton_path();
    Object *object = usd_path_to_armature.lookup_default(skel_path, nullptr);
    if (object == nullptr) {
      BKE_reportf(reports(),
                  RPT_WARNING,
                  "%s: Couldn't find armature object corresponding to USD skeleton %s",
                  __func__,
                  skel_path.GetAsString().c_str());
    }
    amd->object = object;
  }
}

void USDStageReader::import_all_materials(Main *bmain)
{
  BLI_assert(valid());

  /* Build the material name map if it's not built yet. */
  if (settings_.mat_name_to_mat.is_empty()) {
    build_material_map(bmain, settings_.mat_name_to_mat);
  }

  USDMaterialReader mtl_reader(params_, *bmain);
  for (const pxr::SdfPath &mtl_path : material_paths_) {
    pxr::UsdPrim prim = stage_->GetPrimAtPath(mtl_path);

    pxr::UsdShadeMaterial usd_mtl(prim);
    if (!usd_mtl) {
      continue;
    }

    if (blender::io::usd::find_existing_material(
            prim.GetPath(), params_, settings_.mat_name_to_mat, settings_.usd_path_to_mat))
    {
      /* The material already exists. */
      continue;
    }

    /* Can the material be handled by an import hook? */
    const bool have_import_hook = settings_.mat_import_hook_sources.contains(mtl_path);

    /* Add the Blender material. If we have an import hook which can handle this material
     * we don't import USD Preview Surface shaders. */
    Material *new_mtl = mtl_reader.add_material(usd_mtl, !have_import_hook);
    BLI_assert_msg(new_mtl, "Failed to create material");

    settings_.mat_name_to_mat.add_new(new_mtl->id.name + 2, new_mtl);

    if (params_.mtl_name_collision_mode == USD_MTL_NAME_COLLISION_MAKE_UNIQUE) {
      /* Record the Blender material we created for the USD material with the given path.
       * This is to prevent importing the material again when assigning materials to objects
       * elsewhere in the code. */
      settings_.usd_path_to_mat.add_new(mtl_path, new_mtl);
    }

    if (have_import_hook) {
      /* Defer invoking the hook to convert the material till we can do so from
       * the main thread. */
      settings_.usd_path_to_mat_for_hook.add_new(mtl_path, new_mtl);
    }
  }
}

void USDStageReader::fake_users_for_unused_materials()
{
  /* Iterate over the imported materials and set a fake user for any unused
   * materials. */
  for (Material *mat : settings_.usd_path_to_mat.values()) {
    if (mat->id.us == 0) {
      id_fake_user_set(&mat->id);
    }
  }
}

void USDStageReader::find_material_import_hook_sources()
{
  pxr::UsdPrimRange range = stage_->Traverse();
  for (pxr::UsdPrim prim : range) {
    if (prim.IsA<pxr::UsdShadeMaterial>()) {
      pxr::UsdShadeMaterial usd_mat(prim);
      if (have_material_import_hook(stage_, usd_mat, params_, reports())) {
        settings_.mat_import_hook_sources.add(prim.GetPath());
      }
    }
  }
}

void USDStageReader::call_material_import_hooks(Main *bmain) const
{
  if (settings_.usd_path_to_mat_for_hook.is_empty()) {
    /* No materials can be converted by a hook. */
    return;
  }

  for (const auto item : settings_.usd_path_to_mat_for_hook.items()) {
    pxr::UsdPrim prim = stage_->GetPrimAtPath(item.key);

    pxr::UsdShadeMaterial usd_mtl(prim);
    if (!usd_mtl) {
      continue;
    }

    bool success = blender::io::usd::call_material_import_hooks(
        stage_, item.value, usd_mtl, params_, reports());

    if (!success) {
      /* None of the hooks succeeded, so fall back on importing USD Preview Surface if possible. */
      CLOG_WARN(&LOG,
                "USD hook 'on_material_import' for material %s failed, attempting to convert USD "
                "Preview Surface material",
                usd_mtl.GetPath().GetAsString().c_str());

      USDMaterialReader mat_reader(this->params_, *bmain);
      mat_reader.import_usd_preview(item.value, usd_mtl);
    }
  }
}

void USDStageReader::clear_readers()
{
  for (USDPrimReader *reader : readers_) {
    decref(reader);
  }
  readers_.clear();

  for (const auto item : proto_readers_.items()) {
    for (USDPrimReader *reader : item.value) {
      decref(reader);
    }
  }
  proto_readers_.clear();

  for (const auto item : instancer_proto_readers_.items()) {
    for (USDPrimReader *reader : item.value) {
      decref(reader);
    }
  }
  instancer_proto_readers_.clear();

  for (USDDomeLightReader *reader : dome_light_readers_) {
    decref(reader);
  }
  dome_light_readers_.clear();
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

void USDStageReader::create_proto_collections(Main *bmain, Collection *parent_collection)
{
  if (proto_readers_.is_empty() && instancer_proto_readers_.is_empty()) {
    return;
  }

  Collection *all_protos_collection = create_collection(bmain, parent_collection, "prototypes");

  if (all_protos_collection) {
    all_protos_collection->flag |= COLLECTION_HIDE_VIEWPORT;
    all_protos_collection->flag |= COLLECTION_HIDE_RENDER;
    if (parent_collection) {
      DEG_id_tag_update(&parent_collection->id, ID_RECALC_HIERARCHY);
    }
  }

  blender::Map<pxr::SdfPath, Collection *> proto_collection_map;

  for (const pxr::SdfPath &path : proto_readers_.keys()) {
    Collection *proto_collection = create_collection(bmain, all_protos_collection, "proto");

    proto_collection_map.add(path, proto_collection);
  }

  /* Set the instance collections on the readers, including the prototype
   * readers (which are included in readers_), as instancing may be nested. */

  for (USDPrimReader *reader : readers_) {
    if (USDInstanceReader *instance_reader = dynamic_cast<USDInstanceReader *>(reader)) {
      set_instance_collection(instance_reader, proto_collection_map);
    }
  }

  /* Add the prototype objects to the collections. */
  for (const auto &item : proto_readers_.items()) {
    Collection *collection = proto_collection_map.lookup_default(item.key, nullptr);
    if (collection == nullptr) {
      CLOG_WARN(&LOG,
                "Couldn't find collection when adding objects for prototype %s",
                item.key.GetAsString().c_str());
      continue;
    }

    for (const USDPrimReader *reader : item.value) {
      Object *ob = reader->object();

      if (!ob) {
        continue;
      }

      BKE_collection_object_add(bmain, collection, ob);
    }
  }

  /* Create collections for the point instancer prototypes. */

  /* For every point instancer reader, create a "prototypes" collection and set it
   * on the Collection Info node referenced by the geometry nodes modifier created by
   * the reader.  We also create collections containing prototype geometry as children
   * of the "prototypes" collection.  These child collections will be indexed for
   * instancing by the Instance on Points geometry node.
   *
   * Note that the prototype collections will be ordered alphabetically by the Collection
   * Info node.  We must therefore take care to generate collection names that will maintain
   * the original prototype order, so that the prototype indices will remain valid.  We use
   * the naming convention proto_<index>, where the index suffix may be zero padded (e.g.,
   * "proto_00", "proto_01", "proto_02", etc.).
   */

  for (USDPrimReader *reader : readers_) {
    USDPointInstancerReader *instancer_reader = dynamic_cast<USDPointInstancerReader *>(reader);
    if (!instancer_reader) {
      continue;
    }

    pxr::SdfPathVector proto_paths = instancer_reader->proto_paths();
    const pxr::SdfPath &instancer_path = reader->prim().GetPath();
    Collection *instancer_protos_coll = create_collection(
        bmain, all_protos_collection, instancer_path.GetName().c_str());

    /* Determine the max number of digits we will need for the possibly zero-padded
     * string representing the prototype index. */
    const int max_index_digits = integer_digits_i(proto_paths.size());

    int proto_index = 0;

    for (const pxr::SdfPath &proto_path : proto_paths) {
      BLI_assert(max_index_digits > 0);

      /* Format the collection name to follow the proto_<index> pattern. */
      std::string coll_name = fmt::format("proto_{0:0{1}}", proto_index, max_index_digits);

      /* Create the collection and populate it with the prototype objects. */
      Collection *proto_coll = create_collection(bmain, instancer_protos_coll, coll_name.c_str());
      blender::Vector<USDPrimReader *> proto_readers = instancer_proto_readers_.lookup_default(
          proto_path, {});
      for (const USDPrimReader *proto : proto_readers) {
        Object *ob = proto->object();
        if (!ob) {
          continue;
        }
        BKE_collection_object_add(bmain, proto_coll, ob);
      }
      ++proto_index;
    }

    instancer_reader->set_collection(bmain, *instancer_protos_coll);
  }
}

void USDStageReader::create_point_instancer_proto_readers(const UsdPathSet &proto_paths)
{
  if (proto_paths.is_empty()) {
    return;
  }

  for (const pxr::SdfPath &path : proto_paths) {

    pxr::UsdPrim proto_prim = stage_->GetPrimAtPath(path);

    if (!proto_prim) {
      continue;
    }

    Vector<USDPrimReader *> proto_readers;

    /* Note that point instancer prototypes may be defined as overs, so
     * we must call collect readers with argument defined_prims_only = false. */
    collect_readers(proto_prim, proto_paths, false /* include undefined prims */, proto_readers);

    instancer_proto_readers_.add(path, proto_readers);

    for (USDPrimReader *reader : proto_readers) {
      reader->set_is_in_instancer_proto(true);
      readers_.append(reader);
      reader->incref();
    }
  }
}

void USDStageReader::collect_point_instancer_proto_paths(const pxr::UsdPrim &prim,
                                                         UsdPathSet &r_paths) const
{
  /* Note that we use custom filter flags to allow traversing undefined prims,
   * because prototype prims may be defined as overs which are skipped by the
   * default predicate. */
  pxr::Usd_PrimFlagsConjunction filter_flags = pxr::UsdPrimIsActive && pxr::UsdPrimIsLoaded &&
                                               !pxr::UsdPrimIsAbstract;

  pxr::UsdPrimSiblingRange children = prim.GetFilteredChildren(filter_flags);

  for (const auto &child_prim : children) {

    /* Note we allow undefined prims in case prototypes are defined as overs.
     * If the prim is defined, we apply additional checks for inclusion. */
    if (child_prim.IsDefined()) {
      const pxr::UsdGeomImageable imageable = pxr::UsdGeomImageable(child_prim);
      if (!imageable) {
        continue;
      }

      /* We should only traverse through a hierarchy, and any potential instancers, if they would
       * be included by our purpose and visibility checks, matching what is inside
       * #collect_readers. */
      if (!include_by_purpose(imageable)) {
        continue;
      }

      if (!include_by_visibility(imageable)) {
        continue;
      }
    }

    /* We should only consider potential point instancers if they would be included by the scene
     * instancing flags. */
    if (!params_.support_scene_instancing && child_prim.IsInPrototype()) {
      continue;
    }

    if (pxr::UsdGeomPointInstancer instancer = pxr::UsdGeomPointInstancer(child_prim)) {
      pxr::SdfPathVector paths;
      instancer.GetPrototypesRel().GetTargets(&paths);
      for (const pxr::SdfPath &path : paths) {
        r_paths.add(path);
      }
    }

    collect_point_instancer_proto_paths(child_prim, r_paths);
  }
}

UsdPathSet USDStageReader::collect_point_instancer_proto_paths() const
{
  UsdPathSet result;

  if (!stage_) {
    return result;
  }

  collect_point_instancer_proto_paths(stage_->GetPseudoRoot(), result);

  std::vector<pxr::UsdPrim> protos = stage_->GetPrototypes();

  for (const pxr::UsdPrim &proto_prim : protos) {
    collect_point_instancer_proto_paths(proto_prim, result);
  }

  return result;
}

}  // namespace blender::io::usd
