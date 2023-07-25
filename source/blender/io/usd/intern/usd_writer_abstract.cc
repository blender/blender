/* SPDX-FileCopyrightText: 2019 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "usd_writer_abstract.h"
#include "usd_hierarchy_iterator.h"
#include "usd_writer_material.h"

#include <pxr/base/tf/stringUtils.h>
#include <pxr/usd/usdGeom/bboxCache.h>

#include "BKE_customdata.h"
#include "BLI_assert.h"

#include "DNA_mesh_types.h"

#include "WM_api.h"

/* TfToken objects are not cheap to construct, so we do it once. */
namespace usdtokens {
/* Materials */
static const pxr::TfToken diffuse_color("diffuseColor", pxr::TfToken::Immortal);
static const pxr::TfToken metallic("metallic", pxr::TfToken::Immortal);
static const pxr::TfToken preview_shader("previewShader", pxr::TfToken::Immortal);
static const pxr::TfToken preview_surface("UsdPreviewSurface", pxr::TfToken::Immortal);
static const pxr::TfToken roughness("roughness", pxr::TfToken::Immortal);
static const pxr::TfToken surface("surface", pxr::TfToken::Immortal);
}  // namespace usdtokens

static std::string get_mesh_active_uvlayer_name(const Object *ob)
{
  if (!ob || ob->type != OB_MESH || !ob->data) {
    return "";
  }

  const Mesh *me = static_cast<Mesh *>(ob->data);

  const char *name = CustomData_get_active_layer_name(&me->loop_data, CD_PROP_FLOAT2);

  return name ? name : "";
}

namespace blender::io::usd {

USDAbstractWriter::USDAbstractWriter(const USDExporterContext &usd_export_context)
    : usd_export_context_(usd_export_context), frame_has_been_written_(false), is_animated_(false)
{
}

bool USDAbstractWriter::is_supported(const HierarchyContext * /*context*/) const
{
  return true;
}

std::string USDAbstractWriter::get_export_file_path() const
{
  return usd_export_context_.hierarchy_iterator->get_export_file_path();
}

pxr::UsdTimeCode USDAbstractWriter::get_export_time_code() const
{
  if (is_animated_) {
    return usd_export_context_.hierarchy_iterator->get_export_time_code();
  }
  /* By using the default timecode USD won't even write a single `timeSample` for non-animated
   * data. Instead, it writes it as non-timesampled. */
  static pxr::UsdTimeCode default_timecode = pxr::UsdTimeCode::Default();
  return default_timecode;
}

void USDAbstractWriter::write(HierarchyContext &context)
{
  if (!frame_has_been_written_) {
    is_animated_ = usd_export_context_.export_params.export_animation &&
                   check_is_animated(context);
  }
  else if (!is_animated_) {
    /* A frame has already been written, and without animation one frame is enough. */
    return;
  }

  do_write(context);

  frame_has_been_written_ = true;
}

const pxr::SdfPath &USDAbstractWriter::usd_path() const
{
  return usd_export_context_.usd_path;
}

pxr::SdfPath USDAbstractWriter::get_material_library_path() const
{
  static std::string material_library_path("/_materials");

  const char *root_prim_path = usd_export_context_.export_params.root_prim_path;

  if (root_prim_path[0] != '\0') {
    return pxr::SdfPath(root_prim_path + material_library_path);
  }

  return pxr::SdfPath(material_library_path);
}

pxr::UsdShadeMaterial USDAbstractWriter::ensure_usd_material(const HierarchyContext &context,
                                                             Material *material)
{
  pxr::UsdStageRefPtr stage = usd_export_context_.stage;

  /* Construct the material. */
  pxr::TfToken material_name(usd_export_context_.hierarchy_iterator->get_id_name(&material->id));
  pxr::SdfPath usd_path = get_material_library_path().AppendChild(material_name);
  pxr::UsdShadeMaterial usd_material = pxr::UsdShadeMaterial::Get(stage, usd_path);
  if (usd_material) {
    return usd_material;
  }
  usd_material = pxr::UsdShadeMaterial::Define(stage, usd_path);

  if (material->use_nodes && this->usd_export_context_.export_params.generate_preview_surface) {
    std::string active_uv = get_mesh_active_uvlayer_name(context.object);
    create_usd_preview_surface_material(
        this->usd_export_context_, material, usd_material, active_uv);
  }
  else {
    create_usd_viewport_material(this->usd_export_context_, material, usd_material);
  }

  return usd_material;
}

void USDAbstractWriter::write_visibility(const HierarchyContext &context,
                                         const pxr::UsdTimeCode timecode,
                                         pxr::UsdGeomImageable &usd_geometry)
{
  pxr::UsdAttribute attr_visibility = usd_geometry.CreateVisibilityAttr(pxr::VtValue(), true);

  const bool is_visible = context.is_object_visible(
      usd_export_context_.export_params.evaluation_mode);
  const pxr::TfToken visibility = is_visible ? pxr::UsdGeomTokens->inherited :
                                               pxr::UsdGeomTokens->invisible;

  usd_value_writer_.SetAttribute(attr_visibility, pxr::VtValue(visibility), timecode);
}

bool USDAbstractWriter::mark_as_instance(const HierarchyContext &context, const pxr::UsdPrim &prim)
{
  BLI_assert(context.is_instance());

  if (context.export_path == context.original_export_path) {
    printf("USD ref error: export path is reference path: %s\n", context.export_path.c_str());
    BLI_assert_msg(0, "USD reference error");
    return false;
  }

  pxr::SdfPath ref_path(context.original_export_path);
  if (!prim.GetReferences().AddInternalReference(ref_path)) {
    /* See this URL for a description for why referencing may fail"
     * https://graphics.pixar.com/usd/docs/api/class_usd_references.html#Usd_Failing_References
     */
    printf("USD Export warning: unable to add reference from %s to %s, not instancing object\n",
           context.export_path.c_str(),
           context.original_export_path.c_str());
    return false;
  }

  return true;
}

void USDAbstractWriter::author_extent(const pxr::UsdTimeCode timecode, pxr::UsdGeomBoundable &prim)
{
  /* Do not use any existing `extentsHint` that may be authored, instead recompute the extent when
   * authoring it. */
  const bool useExtentsHint = false;
  const pxr::TfTokenVector includedPurposes{pxr::UsdGeomTokens->default_};
  pxr::UsdGeomBBoxCache bboxCache(timecode, includedPurposes, useExtentsHint);
  pxr::GfBBox3d bounds = bboxCache.ComputeLocalBound(prim.GetPrim());
  if (pxr::GfBBox3d() == bounds) {
    /* This will occur, for example, if a mesh does not have any vertices. */
    WM_reportf(RPT_WARNING,
               "USD Export: no bounds could be computed for %s",
               prim.GetPrim().GetName().GetText());
    return;
  }

  pxr::VtArray<pxr::GfVec3f> extent{(pxr::GfVec3f)bounds.GetRange().GetMin(),
                                    (pxr::GfVec3f)bounds.GetRange().GetMax()};
  prim.CreateExtentAttr().Set(extent);
}

}  // namespace blender::io::usd
