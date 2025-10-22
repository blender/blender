/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "usd_writer_abstract.hh"
#include "usd_attribute_utils.hh"
#include "usd_hierarchy_iterator.hh"
#include "usd_utils.hh"
#include "usd_writer_material.hh"

#include <pxr/base/tf/stringUtils.h>
#include <pxr/usd/usdGeom/bboxCache.h>
#include <pxr/usd/usdGeom/scope.h>

#include "BKE_customdata.hh"

#include "BLI_assert.h"
#include "BLI_bounds_types.hh"

#include "DNA_material_types.h"
#include "DNA_mesh_types.h"

#include "CLG_log.h"
static CLG_LogRef LOG = {"io.usd"};

/* TfToken objects are not cheap to construct, so we do it once. */
namespace usdtokens {
static const pxr::TfToken blender_ns("userProperties:blender", pxr::TfToken::Immortal);
}  // namespace usdtokens

static std::string get_mesh_active_uvlayer_name(const Object *ob)
{
  if (!ob || ob->type != OB_MESH || !ob->data) {
    return "";
  }

  const Mesh *mesh = static_cast<Mesh *>(ob->data);
  return mesh->active_uv_map_name();
}

template<typename USDT>
bool set_vec_attrib(const pxr::UsdPrim &prim,
                    const IDProperty *prop,
                    const pxr::TfToken &prop_token,
                    const pxr::SdfValueTypeName &type_name,
                    const pxr::UsdTimeCode &time)
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

  return vec_attr.Set(vec_value, time);
}

namespace blender::io::usd {

static void create_vector_attrib(const pxr::UsdPrim &prim,
                                 const IDProperty *prop,
                                 const pxr::TfToken &prop_token,
                                 const pxr::UsdTimeCode &time)
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
      success = set_vec_attrib<pxr::GfVec2f>(prim, prop, prop_token, type_name, time);
    }
    else if (prop->len == 3) {
      type_name = pxr::SdfValueTypeNames->Float3;
      success = set_vec_attrib<pxr::GfVec3f>(prim, prop, prop_token, type_name, time);
    }
    else if (prop->len == 4) {
      type_name = pxr::SdfValueTypeNames->Float4;
      success = set_vec_attrib<pxr::GfVec4f>(prim, prop, prop_token, type_name, time);
    }
  }
  else if (prop->subtype == IDP_DOUBLE) {
    if (prop->len == 2) {
      type_name = pxr::SdfValueTypeNames->Double2;
      success = set_vec_attrib<pxr::GfVec2d>(prim, prop, prop_token, type_name, time);
    }
    else if (prop->len == 3) {
      type_name = pxr::SdfValueTypeNames->Double3;
      success = set_vec_attrib<pxr::GfVec3d>(prim, prop, prop_token, type_name, time);
    }
    else if (prop->len == 4) {
      type_name = pxr::SdfValueTypeNames->Double4;
      success = set_vec_attrib<pxr::GfVec4d>(prim, prop, prop_token, type_name, time);
    }
  }
  else if (prop->subtype == IDP_INT) {
    if (prop->len == 2) {
      type_name = pxr::SdfValueTypeNames->Int2;
      success = set_vec_attrib<pxr::GfVec2i>(prim, prop, prop_token, type_name, time);
    }
    else if (prop->len == 3) {
      type_name = pxr::SdfValueTypeNames->Int3;
      success = set_vec_attrib<pxr::GfVec3i>(prim, prop, prop_token, type_name, time);
    }
    else if (prop->len == 4) {
      type_name = pxr::SdfValueTypeNames->Int4;
      success = set_vec_attrib<pxr::GfVec4i>(prim, prop, prop_token, type_name, time);
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
  return pxr::UsdTimeCode::Default();
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

  const std::string &root_prim_path = usd_export_context_.export_params.root_prim_path;

  if (!root_prim_path.empty()) {
    return pxr::SdfPath(root_prim_path + material_library_path);
  }

  return pxr::SdfPath(material_library_path);
}

pxr::SdfPath USDAbstractWriter::get_proto_material_root_path(const HierarchyContext &context) const
{
  static std::string material_library_path("/_materials");

  std::string path_prefix(usd_export_context_.export_params.root_prim_path);

  path_prefix += context.higher_up_export_path;

  return pxr::SdfPath(path_prefix + material_library_path);
}

pxr::UsdShadeMaterial USDAbstractWriter::ensure_usd_material_created(
    const HierarchyContext &context, Material *material) const
{
  pxr::UsdStageRefPtr stage = usd_export_context_.stage;

  /* Construct the material. */
  pxr::TfToken material_name(
      make_safe_name(material->id.name + 2, usd_export_context_.export_params.allow_unicode));
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
  add_to_prim_map(prim.GetPath(), &material->id);
  write_id_properties(prim, material->id, get_export_time_code());

  return usd_material;
}

pxr::UsdShadeMaterial USDAbstractWriter::ensure_usd_material(const HierarchyContext &context,
                                                             Material *material) const
{
  pxr::UsdShadeMaterial library_material = ensure_usd_material_created(context, material);

  /* If instancing is enabled and the object is an instancing prototype, create a material
   * under the prototype root referencing the library material. This is considered a best
   * practice and is required for certain renderers (e.g., karma). */

  if (!(usd_export_context_.export_params.use_instancing && context.is_prototype())) {
    /* We don't need to handle the material for the prototype. */
    return library_material;
  }

  /* Create the prototype material. */

  pxr::UsdStageRefPtr stage = usd_export_context_.stage;

  pxr::SdfPath usd_path = pxr::UsdGeomScope::Define(stage, get_proto_material_root_path(context))
                              .GetPath()
                              .AppendChild(library_material.GetPath().GetNameToken());

  pxr::UsdShadeMaterial proto_material = pxr::UsdShadeMaterial::Define(stage, usd_path);

  if (!proto_material.GetPrim().GetReferences().AddInternalReference(library_material.GetPath())) {
    CLOG_WARN(&LOG,
              "Unable to add a material reference from %s to %s for prototype %s",
              proto_material.GetPath().GetAsString().c_str(),
              library_material.GetPath().GetAsString().c_str(),
              context.export_path.c_str());
    return library_material;
  }

  return proto_material;
}

void USDAbstractWriter::write_visibility(const HierarchyContext &context,
                                         const pxr::UsdTimeCode time,
                                         const pxr::UsdGeomImageable &usd_geometry)
{
  pxr::UsdAttribute attr_visibility = usd_geometry.CreateVisibilityAttr(pxr::VtValue(), true);

  const bool is_visible = context.is_object_visible(
      usd_export_context_.export_params.evaluation_mode);
  const pxr::TfToken visibility = is_visible ? pxr::UsdGeomTokens->inherited :
                                               pxr::UsdGeomTokens->invisible;

  usd_value_writer_.SetAttribute(attr_visibility, pxr::VtValue(visibility), time);
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

  BLI_assert(!context.original_export_path.empty());
  BLI_assert(context.original_export_path.front() == '/');

  std::string ref_path_str(usd_export_context_.export_params.root_prim_path);
  ref_path_str += context.original_export_path;

  pxr::SdfPath ref_path(ref_path_str);

  /* To avoid USD errors, make sure the referenced path exists. */
  usd_export_context_.stage->DefinePrim(ref_path);

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

  prim.SetInstanceable(true);

  return true;
}

void USDAbstractWriter::write_id_properties(const pxr::UsdPrim &prim,
                                            const ID &id,
                                            pxr::UsdTimeCode time) const
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
    write_user_properties(prim, id.properties, time);
  }
}

void USDAbstractWriter::write_user_properties(const pxr::UsdPrim &prim,
                                              IDProperty *properties,
                                              pxr::UsdTimeCode time) const
{
  if (properties == nullptr) {
    return;
  }

  if (properties->type != IDP_GROUP) {
    return;
  }

  const StringRef displayName_identifier = "displayName";

  const std::string default_namespace(
      usd_export_context_.export_params.custom_properties_namespace);

  for (IDProperty *prop = (IDProperty *)properties->data.group.first; prop; prop = prop->next) {
    if (displayName_identifier == prop->name) {
      if (prop->type == IDP_STRING && prop->data.pointer) {
        prim.SetDisplayName(static_cast<char *>(prop->data.pointer));
      }
      continue;
    }

    std::vector<std::string> path_names = pxr::TfStringTokenize(prop->name, ":");

    /* If the path does not already have a namespace prefix, prepend the default namespace
     * specified by the user, if any. */
    if (!default_namespace.empty() && path_names.size() < 2) {
      path_names.insert(path_names.begin(), default_namespace);
    }

    std::vector<std::string> safe_names;
    for (const std::string &name : path_names) {
      safe_names.push_back(make_safe_name(name, usd_export_context_.export_params.allow_unicode));
    }

    std::string full_prop_name = pxr::SdfPath::JoinIdentifier(safe_names);
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
          int_attr.Set<int>(prop->data.val, time);
        }
        break;
      case IDP_FLOAT:
        if (pxr::UsdAttribute float_attr = prim.CreateAttribute(
                prop_token, pxr::SdfValueTypeNames->Float, true))
        {
          float_attr.Set<float>(*reinterpret_cast<float *>(&prop->data.val), time);
        }
        break;
      case IDP_DOUBLE:
        if (pxr::UsdAttribute double_attr = prim.CreateAttribute(
                prop_token, pxr::SdfValueTypeNames->Double, true))
        {
          double_attr.Set<double>(*reinterpret_cast<double *>(&prop->data.val), time);
        }
        break;
      case IDP_STRING:
        if (pxr::UsdAttribute str_attr = prim.CreateAttribute(
                prop_token, pxr::SdfValueTypeNames->String, true))
        {
          str_attr.Set<std::string>(static_cast<const char *>(prop->data.pointer), time);
        }
        break;
      case IDP_BOOLEAN:
        if (pxr::UsdAttribute bool_attr = prim.CreateAttribute(
                prop_token, pxr::SdfValueTypeNames->Bool, true))
        {
          bool_attr.Set<bool>(prop->data.val, time);
        }
        break;
      case IDP_ARRAY:
        create_vector_attrib(prim, prop, prop_token, time);
        break;
    }
  }
}

void USDAbstractWriter::author_extent(const pxr::UsdGeomBoundable &boundable,
                                      const pxr::UsdTimeCode time)
{
  /* Do not use any existing `extentsHint` that may be authored, instead recompute the extent when
   * authoring it. */
  const bool useExtentsHint = false;
  const pxr::TfTokenVector includedPurposes{pxr::UsdGeomTokens->default_};
  pxr::UsdGeomBBoxCache bboxCache(time, includedPurposes, useExtentsHint);
  pxr::GfBBox3d bounds = bboxCache.ComputeLocalBound(boundable.GetPrim());

  /* Note: An empty 'bounds' is still valid (e.g. a mesh with no vertices). */
  pxr::VtArray<pxr::GfVec3f> extent{pxr::GfVec3f(bounds.GetRange().GetMin()),
                                    pxr::GfVec3f(bounds.GetRange().GetMax())};

  pxr::UsdAttribute attr_extent = boundable.CreateExtentAttr(pxr::VtValue(), true);
  set_attribute(attr_extent, extent, time, usd_value_writer_);
}

void USDAbstractWriter::author_extent(const pxr::UsdGeomBoundable &boundable,
                                      const std::optional<Bounds<float3>> &bounds,
                                      const pxr::UsdTimeCode time)
{
  pxr::VtArray<pxr::GfVec3f> extent(2);
  if (bounds) {
    extent[0].Set(bounds->min);
    extent[1].Set(bounds->max);
  }

  pxr::UsdAttribute attr_extent = boundable.CreateExtentAttr(pxr::VtValue(), true);
  set_attribute(attr_extent, extent, time, usd_value_writer_);
}

void USDAbstractWriter::add_to_prim_map(const pxr::SdfPath &usd_path, const ID *id) const
{
  if (usd_export_context_.hierarchy_iterator) {
    usd_export_context_.hierarchy_iterator->add_to_prim_map(usd_path, id);
  }
}

}  // namespace blender::io::usd
