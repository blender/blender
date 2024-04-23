/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "usd_writer_abstract.hh"
#include "usd_writer_material.hh"

#include <pxr/base/tf/stringUtils.h>
#include <pxr/usd/usdGeom/bboxCache.h>
#include <pxr/usd/usdGeom/scope.h>

#include "BKE_customdata.hh"
#include "BKE_report.hh"

#include "BLI_assert.h"

#include "DNA_mesh_types.h"

#include "CLG_log.h"
static CLG_LogRef LOG = {"io.usd"};

/* TfToken objects are not cheap to construct, so we do it once. */
namespace usdtokens {
/* Materials */
static const pxr::TfToken diffuse_color("diffuseColor", pxr::TfToken::Immortal);
static const pxr::TfToken metallic("metallic", pxr::TfToken::Immortal);
static const pxr::TfToken preview_shader("previewShader", pxr::TfToken::Immortal);
static const pxr::TfToken preview_surface("UsdPreviewSurface", pxr::TfToken::Immortal);
static const pxr::TfToken roughness("roughness", pxr::TfToken::Immortal);
static const pxr::TfToken surface("surface", pxr::TfToken::Immortal);
static const pxr::TfToken blender_ns("userProperties:blender", pxr::TfToken::Immortal);
}  // namespace usdtokens

static std::string get_mesh_active_uvlayer_name(const Object *ob)
{
  if (!ob || ob->type != OB_MESH || !ob->data) {
    return "";
  }

  const Mesh *mesh = static_cast<Mesh *>(ob->data);

  const char *name = CustomData_get_active_layer_name(&mesh->corner_data, CD_PROP_FLOAT2);

  return name ? name : "";
}

template<typename USDT>
bool set_vec_attrib(const pxr::UsdPrim &prim,
                    const IDProperty *prop,
                    const pxr::TfToken &prop_token,
                    const pxr::SdfValueTypeName &type_name,
                    const pxr::UsdTimeCode &timecode)
{
  if (!prim || !prop || !prop->data.pointer || prop_token.IsEmpty() || !type_name) {
    return false;
  }

  pxr::UsdAttribute vec_attr = prim.CreateAttribute(prop_token, type_name, true);

  if (!vec_attr) {
    CLOG_WARN(&LOG,
              "Couldn't create USD attribute for array property %s",
              prop_token.GetString().c_str());
    return false;
  }

  USDT vec_value(static_cast<typename USDT::ScalarType *>(prop->data.pointer));

  return vec_attr.Set(vec_value, timecode);
}

namespace blender::io::usd {

static void create_vector_attrib(const pxr::UsdPrim &prim,
                                 const IDProperty *prop,
                                 const pxr::TfToken &prop_token,
                                 const pxr::UsdTimeCode &timecode)
{
  if (!prim || !prop || prop_token.IsEmpty()) {
    return;
  }

  if (prop->type != IDP_ARRAY) {
    CLOG_WARN(&LOG,
              "Property %s is not an array type and can't be converted to a vector attribute",
              prop->name);
    return;
  }

  pxr::SdfValueTypeName type_name;
  bool success = false;

  if (prop->subtype == IDP_FLOAT) {
    if (prop->len == 2) {
      type_name = pxr::SdfValueTypeNames->Float2;
      success = set_vec_attrib<pxr::GfVec2f>(prim, prop, prop_token, type_name, timecode);
    }
    else if (prop->len == 3) {
      type_name = pxr::SdfValueTypeNames->Float3;
      success = set_vec_attrib<pxr::GfVec3f>(prim, prop, prop_token, type_name, timecode);
    }
    else if (prop->len == 4) {
      type_name = pxr::SdfValueTypeNames->Float4;
      success = set_vec_attrib<pxr::GfVec4f>(prim, prop, prop_token, type_name, timecode);
    }
  }
  else if (prop->subtype == IDP_DOUBLE) {
    if (prop->len == 2) {
      type_name = pxr::SdfValueTypeNames->Double2;
      success = set_vec_attrib<pxr::GfVec2d>(prim, prop, prop_token, type_name, timecode);
    }
    else if (prop->len == 3) {
      type_name = pxr::SdfValueTypeNames->Double3;
      success = set_vec_attrib<pxr::GfVec3d>(prim, prop, prop_token, type_name, timecode);
    }
    else if (prop->len == 4) {
      type_name = pxr::SdfValueTypeNames->Double4;
      success = set_vec_attrib<pxr::GfVec4d>(prim, prop, prop_token, type_name, timecode);
    }
  }
  else if (prop->subtype == IDP_INT) {
    if (prop->len == 2) {
      type_name = pxr::SdfValueTypeNames->Int2;
      success = set_vec_attrib<pxr::GfVec2i>(prim, prop, prop_token, type_name, timecode);
    }
    else if (prop->len == 3) {
      type_name = pxr::SdfValueTypeNames->Int3;
      success = set_vec_attrib<pxr::GfVec3i>(prim, prop, prop_token, type_name, timecode);
    }
    else if (prop->len == 4) {
      type_name = pxr::SdfValueTypeNames->Int4;
      success = set_vec_attrib<pxr::GfVec4i>(prim, prop, prop_token, type_name, timecode);
    }
  }

  if (!type_name) {
    CLOG_WARN(&LOG,
              "Couldn't determine USD type name for array property %s",
              prop_token.GetString().c_str());
    return;
  }

  if (!success) {
    CLOG_WARN(
        &LOG, "Couldn't set USD attribute from array property %s", prop_token.GetString().c_str());
    return;
  }
}

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
  return usd_export_context_.export_file_path;
}

pxr::UsdTimeCode USDAbstractWriter::get_export_time_code() const
{
  if (is_animated_) {
    BLI_assert(usd_export_context_.get_time_code);
    return usd_export_context_.get_time_code();
  }
  /* By using the default time-code USD won't even write a single `timeSample` for non-animated
   * data. Instead, it writes it as non-time-sampled. */
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
  pxr::TfToken material_name(pxr::TfMakeValidIdentifier(material->id.name + 2));
  pxr::SdfPath usd_path = pxr::UsdGeomScope::Define(stage, get_material_library_path())
                              .GetPath()
                              .AppendChild(material_name);
  pxr::UsdShadeMaterial usd_material = pxr::UsdShadeMaterial::Get(stage, usd_path);
  if (usd_material) {
    return usd_material;
  }

  std::string active_uv = get_mesh_active_uvlayer_name(context.object);

  usd_material = create_usd_material(
      usd_export_context_, usd_path, material, active_uv, reports());

  auto prim = usd_material.GetPrim();
  write_id_properties(prim, material->id, get_export_time_code());

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
    CLOG_ERROR(&LOG,
               "Reference error: export path matches reference path: %s",
               context.export_path.c_str());
    BLI_assert_msg(0, "USD reference error");
    return false;
  }

  pxr::SdfPath ref_path(context.original_export_path);
  if (!prim.GetReferences().AddInternalReference(ref_path)) {
    /* See this URL for a description for why referencing may fail"
     * https://graphics.pixar.com/usd/docs/api/class_usd_references.html#Usd_Failing_References
     */
    CLOG_WARN(&LOG,
              "Unable to add reference from %s to %s, not instancing object for export",
              context.export_path.c_str(),
              context.original_export_path.c_str());
    return false;
  }

  return true;
}

void USDAbstractWriter::write_id_properties(const pxr::UsdPrim &prim,
                                            const ID &id,
                                            pxr::UsdTimeCode timecode) const
{
  if (!usd_export_context_.export_params.export_custom_properties) {
    return;
  }

  if (usd_export_context_.export_params.author_blender_name) {
    if (GS(id.name) == ID_OB) {
      /* Author property of original blender Object name. */
      prim.CreateAttribute(pxr::TfToken(usdtokens::blender_ns.GetString() + ":object_name"),
                           pxr::SdfValueTypeNames->String,
                           true)
          .Set<std::string>(std::string(id.name + 2));
    }
    else {
      prim.CreateAttribute(pxr::TfToken(usdtokens::blender_ns.GetString() + ":data_name"),
                           pxr::SdfValueTypeNames->String,
                           true)
          .Set<std::string>(std::string(id.name + 2));
    }
  }

  if (id.properties) {
    write_user_properties(prim, id.properties, timecode);
  }
}

void USDAbstractWriter::write_user_properties(const pxr::UsdPrim &prim,
                                              IDProperty *properties,
                                              pxr::UsdTimeCode timecode) const
{
  if (properties == nullptr) {
    return;
  }

  if (properties->type != IDP_GROUP) {
    return;
  }

  const StringRef displayName_identifier = "displayName";

  for (IDProperty *prop = (IDProperty *)properties->data.group.first; prop; prop = prop->next) {
    if (displayName_identifier == prop->name) {
      if (prop->type == IDP_STRING && prop->data.pointer) {
        prim.SetDisplayName(static_cast<char *>(prop->data.pointer));
      }
      continue;
    }

    std::string prop_name = pxr::TfMakeValidIdentifier(prop->name);
    std::string full_prop_name = "userProperties:" + prop_name;

    pxr::TfToken prop_token = pxr::TfToken(full_prop_name);

    if (prim.HasAttribute(prop_token)) {
      /* Don't overwrite existing attributes, as these may have been
       * created by the exporter logic and shouldn't be changed. */
      continue;
    }

    switch (prop->type) {
      case IDP_INT:
        if (pxr::UsdAttribute int_attr = prim.CreateAttribute(
                prop_token, pxr::SdfValueTypeNames->Int, true))
        {
          int_attr.Set<int>(prop->data.val, timecode);
        }
        break;
      case IDP_FLOAT:
        if (pxr::UsdAttribute float_attr = prim.CreateAttribute(
                prop_token, pxr::SdfValueTypeNames->Float, true))
        {
          float_attr.Set<float>(*reinterpret_cast<float *>(&prop->data.val), timecode);
        }
        break;
      case IDP_DOUBLE:
        if (pxr::UsdAttribute double_attr = prim.CreateAttribute(
                prop_token, pxr::SdfValueTypeNames->Double, true))
        {
          double_attr.Set<double>(*reinterpret_cast<double *>(&prop->data.val), timecode);
        }
        break;
      case IDP_STRING:
        if (pxr::UsdAttribute str_attr = prim.CreateAttribute(
                prop_token, pxr::SdfValueTypeNames->String, true))
        {
          str_attr.Set<std::string>(static_cast<const char *>(prop->data.pointer), timecode);
        }
        break;
      case IDP_BOOLEAN:
        if (pxr::UsdAttribute bool_attr = prim.CreateAttribute(
                prop_token, pxr::SdfValueTypeNames->Bool, true))
        {
          bool_attr.Set<bool>(prop->data.val, timecode);
        }
        break;
      case IDP_ARRAY:
        create_vector_attrib(prim, prop, prop_token, timecode);
        break;
    }
  }
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
    BKE_reportf(reports(),
                RPT_WARNING,
                "USD Export: no bounds could be computed for %s",
                prim.GetPrim().GetName().GetText());
    return;
  }

  pxr::VtArray<pxr::GfVec3f> extent{(pxr::GfVec3f)bounds.GetRange().GetMin(),
                                    (pxr::GfVec3f)bounds.GetRange().GetMax()};
  prim.CreateExtentAttr().Set(extent);
}

}  // namespace blender::io::usd
