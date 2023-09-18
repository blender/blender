/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "usd_writer_abstract.h"
#include "usd_hierarchy_iterator.h"
#include "usd_writer_material.h"

#include <pxr/base/tf/stringUtils.h>
#include <pxr/usd/kind/registry.h>
#include <pxr/usd/usd/modelAPI.h>
#include <pxr/usd/usdGeom/bboxCache.h>

#include "BKE_customdata.h"
#include "BLI_assert.h"
#include "DNA_mesh_types.h"

#include "WM_api.hh"

/* TfToken objects are not cheap to construct, so we do it once. */
namespace usdtokens {
/* Materials */
static const pxr::TfToken diffuse_color("diffuseColor", pxr::TfToken::Immortal);
static const pxr::TfToken metallic("metallic", pxr::TfToken::Immortal);
static const pxr::TfToken preview_shader("previewShader", pxr::TfToken::Immortal);
static const pxr::TfToken preview_surface("UsdPreviewSurface", pxr::TfToken::Immortal);
static const pxr::TfToken roughness("roughness", pxr::TfToken::Immortal);
static const pxr::TfToken surface("surface", pxr::TfToken::Immortal);
static const pxr::TfToken blenderName("userProperties:blenderName", pxr::TfToken::Immortal);
}  // namespace usdtokens

namespace {

template<typename VECT>
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
    printf("WARNING: Couldn't USD attribute for array property %s.\n",
           prop_token.GetString().c_str());
    return false;
  }

  VECT vec_value(static_cast<typename VECT::ScalarType *>(prop->data.pointer));

  return vec_attr.Set(vec_value, timecode);
}

}  // anonymous namespace

namespace blender::io::usd {

static std::string get_mesh_active_uvlayer_name(const Object *ob)
{
  if (!ob || ob->type != OB_MESH || !ob->data) {
    return "";
  }

  const Mesh *me = static_cast<Mesh *>(ob->data);

  const char *name = CustomData_get_active_layer_name(&me->loop_data, CD_PROP_FLOAT2);

  return name ? name : "";
}

static void create_vector_attrib(const pxr::UsdPrim &prim,
                                 const IDProperty *prop,
                                 const pxr::TfToken &prop_token,
                                 const pxr::UsdTimeCode &timecode)
{
  if (!prim || !prop || prop_token.IsEmpty()) {
    return;
  }

  if (prop->type != IDP_ARRAY) {
    printf(
        "WARNING: Property %s is not an array type and can't be converted to a vector "
        "attribute.\n",
        prop_token.GetString().c_str());
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
    printf("WARNING: Couldn't determine USD type name for array property %s.\n",
           prop_token.GetString().c_str());
    return;
  }

  if (!success) {
    printf("WARNING: Couldn't set USD attribute from array property %s.\n",
           prop_token.GetString().c_str());
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

void USDAbstractWriter::set_iterator(const USDHierarchyIterator *iter)
{
  hierarchy_iterator_ = iter;
}

bool USDAbstractWriter::is_prototype(const Object *obj) const
{
  if (hierarchy_iterator_) {
    return hierarchy_iterator_->is_prototype(obj);
  }

  return false;
}


pxr::SdfPath USDAbstractWriter::get_material_library_path(const HierarchyContext &context) const
{
  std::string material_library_path;

  /* For instance prototypes, create the material beneath the prototyp prim. */
  if (usd_export_context_.export_params.use_instancing && this->is_prototype(context.object)) {

    material_library_path += std::string(usd_export_context_.export_params.root_prim_path);
    if (context.object->data) {
      material_library_path += context.higher_up_export_path;
    }
    else {
      material_library_path += context.export_path;
    }
    material_library_path += "/Looks";
  }

  if (material_library_path.empty()) {
    material_library_path = this->usd_export_context_.export_params.material_prim_path;
  }

  return pxr::SdfPath(material_library_path);
}

pxr::UsdShadeMaterial USDAbstractWriter::ensure_usd_material(const HierarchyContext &context,
                                                             Material *material)
{
  pxr::UsdStageRefPtr stage = usd_export_context_.stage;

  /* Construct the material. */
  pxr::TfToken material_name(pxr::TfMakeValidIdentifier(material->id.name + 2));
  pxr::SdfPath usd_path = get_material_library_path(context).AppendChild(material_name);

  pxr::UsdShadeMaterial usd_material = pxr::UsdShadeMaterial::Get(stage, usd_path);
  if (usd_material) {
    return usd_material;
  }

  std::string active_uv = get_mesh_active_uvlayer_name(context.object);

  usd_material = create_usd_material(usd_export_context_, usd_path, material, active_uv);

  if (usd_export_context_.export_params.export_custom_properties && material) {
    auto prim = usd_material.GetPrim();
    write_id_properties(prim, material->id, get_export_time_code());
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

void USDAbstractWriter::write_kind(pxr::UsdPrim &prim, pxr::TfToken kind)
{
  pxr::UsdModelAPI api(prim);
  api.SetKind(kind);
}

bool USDAbstractWriter::mark_as_instance(const HierarchyContext &context, const pxr::UsdPrim &prim)
{
  BLI_assert(context.is_instance());

  if (context.export_path == context.original_export_path) {
    printf("USD ref error: export path is reference path: %s\n", context.export_path.c_str());
    BLI_assert_msg(0, "USD reference error");
    return false;
  }

  std::string ref_path_str(usd_export_context_.export_params.root_prim_path);
  ref_path_str += context.original_export_path;

  pxr::SdfPath ref_path(ref_path_str);

  /* To avoid USD errors, make sure the referenced path exists. */
  usd_export_context_.stage->DefinePrim(ref_path);

  if (!prim.GetReferences().AddInternalReference(ref_path)) {
    /* See this URL for a description for why referencing may fail"
     * https://graphics.pixar.com/usd/docs/api/class_usd_references.html#Usd_Failing_References
     */
    printf("USD Export warning: unable to add reference from %s to %s, not instancing object\n",
           context.export_path.c_str(),
           context.original_export_path.c_str());
    return false;
  }

  prim.SetInstanceable(true);

  return true;
}

void USDAbstractWriter::write_id_properties(pxr::UsdPrim &prim,
                                            const ID &id,
                                            pxr::UsdTimeCode timecode)
{
  if (usd_export_context_.export_params.author_blender_name) {
    if (GS(id.name) == ID_OB) {
      // Author property of original blenderName
      prim.CreateAttribute(pxr::TfToken(usdtokens::blenderName.GetString() + ":object"),
                           pxr::SdfValueTypeNames->String,
                           true)
          .Set<std::string>(std::string(id.name + 2));
    }
    else {
      prim.CreateAttribute(pxr::TfToken(usdtokens::blenderName.GetString() + ":data"),
                           pxr::SdfValueTypeNames->String,
                           true)
          .Set<std::string>(std::string(id.name + 2));
    }
  }

  if (id.properties)
    write_user_properties(prim, (IDProperty *)id.properties, timecode);
}

void USDAbstractWriter::write_user_properties(pxr::UsdPrim &prim,
                                              IDProperty *properties,
                                              pxr::UsdTimeCode timecode)
{
  if (properties == nullptr) {
    return;
  }

  if (properties->type != IDP_GROUP) {
    return;
  }

  const StringRef kind_identifier = "usdkind";

  IDProperty *prop;
  for (prop = (IDProperty *)properties->data.group.first; prop; prop = prop->next) {
    if (kind_identifier == prop->name) {
      if (prop->type == IDP_STRING && usd_export_context_.export_params.export_usd_kind &&
          prop->data.pointer)
      {
        write_kind(prim, pxr::TfToken(static_cast<char *>(prop->data.pointer)));
      }
      continue;
    }

    std::string prop_name = pxr::TfMakeValidIdentifier(prop->name);

    std::string full_prop_name;
    if (usd_export_context_.export_params.add_properties_namespace) {
      full_prop_name = "userProperties:";
    }
    full_prop_name += prop_name;

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
