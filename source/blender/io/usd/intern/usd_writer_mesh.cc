/* SPDX-FileCopyrightText: 2019 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "usd_writer_mesh.h"
#include "usd_hierarchy_iterator.h"

#include <pxr/usd/usdGeom/mesh.h>
#include <pxr/usd/usdGeom/primvarsAPI.h>
#include <pxr/usd/usdShade/material.h>
#include <pxr/usd/usdShade/materialBindingAPI.h>

#include "BLI_assert.h"
#include "BLI_math_vector_types.hh"

#include "BKE_attribute.h"
#include "BKE_customdata.h"
#include "BKE_lib_id.h"
#include "BKE_library.h"
#include "BKE_material.h"
#include "BKE_mesh.hh"
#include "BKE_mesh_runtime.h"
#include "BKE_mesh_wrapper.h"
#include "BKE_modifier.h"
#include "BKE_object.h"

#include "bmesh.h"
#include "bmesh_tools.h"

#include "DEG_depsgraph.h"
#include "DNA_layer_types.h"
#include "DNA_mesh_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_fluidsim_types.h"
#include "DNA_object_types.h"
#include "DNA_particle_types.h"

#include "WM_api.h"

#include <iostream>

namespace blender::io::usd {

const pxr::TfToken empty_token;

/* TfToken objects are not cheap to construct, so we do it once. */
namespace usdtokens {
static const pxr::TfToken blenderName("userProperties:blenderName", pxr::TfToken::Immortal);
static const pxr::TfToken blenderNameNS("userProperties:blenderName:", pxr::TfToken::Immortal);
static const pxr::TfToken blenderObject("object", pxr::TfToken::Immortal);
static const pxr::TfToken blenderObjectNS("object:", pxr::TfToken::Immortal);
static const pxr::TfToken blenderData("data", pxr::TfToken::Immortal);
static const pxr::TfToken blenderDataNS("data:", pxr::TfToken::Immortal);
}  // namespace usdtokens

/* check if the mesh is a subsurf, ignoring disabled modifiers and
 * displace if it's after subsurf. */
static ModifierData *get_subsurf_modifier(Object *ob, ModifierMode mode)
{
  ModifierData *md = static_cast<ModifierData *>(ob->modifiers.last);

  for (; md; md = md->prev) {
    if (!BKE_modifier_is_enabled(nullptr, md, mode)) {
      continue;
    }

    if (md->type == eModifierType_Subsurf) {
      SubsurfModifierData *smd = reinterpret_cast<SubsurfModifierData *>(md);

      if (smd->subdivType == ME_CC_SUBSURF) {
        return md;
      }
    }

    /* mesh is not a subsurf. break */
    if ((md->type != eModifierType_Displace) && (md->type != eModifierType_ParticleSystem)) {
      return NULL;
    }
  }

  return NULL;
}

USDGenericMeshWriter::USDGenericMeshWriter(const USDExporterContext &ctx) : USDAbstractWriter(ctx)
{
}

bool USDGenericMeshWriter::is_supported(const HierarchyContext *context) const
{
  // TODO(makowalski) -- Check if we should be calling is_object_visible() below.
  // if (usd_export_context_.export_params.visible_objects_only) {
  // return context->is_object_visible(usd_export_context_.export_params.evaluation_mode);
  // }

  if (!usd_export_context_.export_params.visible_objects_only) {
    // We can skip the visibility test.
    return true;
  }

  Object *object = context->object;
  bool is_dupli = context->duplicator != nullptr;
  int base_flag;

  if (is_dupli) {
    /* Construct the object's base flags from its dupliparent, just like is done in
     * deg_objects_dupli_iterator_next(). Without this, the visiblity check below will fail. Doing
     * this here, instead of a more suitable location in AbstractHierarchyIterator, prevents
     * copying the Object for every dupli. */
    base_flag = object->base_flag;
    object->base_flag = context->duplicator->base_flag | BASE_FROM_DUPLI;
  }

  int visibility = BKE_object_visibility(object,
                                         usd_export_context_.export_params.evaluation_mode);

  if (is_dupli) {
    object->base_flag = base_flag;
  }

  return (visibility & OB_VISIBLE_SELF) != 0;
}

void USDGenericMeshWriter::do_write(HierarchyContext &context)
{
  Object *object_eval = context.object;

  const ModifierMode mode = usd_export_context_.export_params.evaluation_mode ==
                                    DAG_EVAL_VIEWPORT ?
                                eModifierMode_Realtime :
                                eModifierMode_Render;

  m_subsurf_mod = get_subsurf_modifier(context.object, mode);
  const bool should_disable_temporary = m_subsurf_mod && !usd_export_context_.export_params.apply_subdiv;

  if (should_disable_temporary) {
    m_subsurf_mod->mode |= eModifierMode_DisableTemporary;
  }

  bool needsfree = false;
  Mesh *mesh = get_export_mesh(object_eval, needsfree);

  if (mesh == nullptr) {
    return;
  }

  if (usd_export_context_.export_params.triangulate_meshes) {
    const bool tag_only = false;
    const int quad_method = usd_export_context_.export_params.quad_method;
    const int ngon_method = usd_export_context_.export_params.ngon_method;

    BMeshCreateParams bmesh_create_params{};
    BMeshFromMeshParams bmesh_from_mesh_params{};
    bmesh_from_mesh_params.calc_face_normal = true;
    bmesh_from_mesh_params.calc_vert_normal = true;
    BMesh *bm = BKE_mesh_to_bmesh_ex(mesh, &bmesh_create_params, &bmesh_from_mesh_params);

    BM_mesh_triangulate(bm, quad_method, ngon_method, 4, tag_only, nullptr, nullptr, nullptr);

    Mesh *triangulated_mesh = BKE_mesh_from_bmesh_for_eval_nomain(bm, nullptr, mesh);
    BM_mesh_free(bm);

    if (needsfree) {
      free_export_mesh(mesh);
    }
    mesh = triangulated_mesh;
    needsfree = true;
  }

  try {
    write_mesh(context, mesh);

    if (needsfree) {
      free_export_mesh(mesh);
    }
  }
  catch (...) {
    if (needsfree) {
      free_export_mesh(mesh);
    }
    throw;
  }

  auto prim = usd_export_context_.stage->GetPrimAtPath(usd_export_context_.usd_path);
  if (prim.IsValid() && object_eval)
    prim.SetActive((object_eval->duplicator_visibility_flag & OB_DUPLI_FLAG_RENDER) != 0);

  if (usd_export_context_.export_params.export_custom_properties && mesh)
    write_id_properties(prim, mesh->id, get_export_time_code());

  if (should_disable_temporary) {
    m_subsurf_mod->mode &= ~eModifierMode_DisableTemporary;
  }
}

void USDGenericMeshWriter::write_custom_data(const Mesh *mesh, pxr::UsdGeomMesh usd_mesh)
{
  const bke::AttributeAccessor attributes = mesh->attributes();

  char *active_set_name = nullptr;
  const int active_uv_set_index = CustomData_get_render_layer_index(&mesh->ldata, CD_PROP_FLOAT2);
  if (active_uv_set_index != -1) {
    active_set_name = mesh->ldata.layers[active_uv_set_index].name;
  }

  attributes.for_all(
      [&](const bke::AttributeIDRef &attribute_id, const bke::AttributeMetaData &meta_data) {
        /* Skipping "internal" Blender properties. Also skipping
         * material_index as it's dealt with elsewhere. */
        if (attribute_id.name()[0] == '.' ||
            ELEM(attribute_id.name(), "position", "material_index")) {
          return true;
        }

        /* UV Data. */
        if (meta_data.domain == ATTR_DOMAIN_CORNER && meta_data.data_type == CD_PROP_FLOAT2) {
          if (usd_export_context_.export_params.export_uvmaps) {
            write_uv_data(mesh, usd_mesh, attribute_id, meta_data, active_set_name);
          }
        }

        /* Color data. */
        else if (ELEM(meta_data.domain, ATTR_DOMAIN_CORNER, ATTR_DOMAIN_POINT) &&
                 ELEM(meta_data.data_type, CD_PROP_BYTE_COLOR, CD_PROP_COLOR))
        {
          if (usd_export_context_.export_params.export_vertex_colors) {
            write_color_data(mesh, usd_mesh, attribute_id, meta_data);
          }
        }

        else {
          if (usd_export_context_.export_params.export_mesh_attributes) {
            write_generic_data(mesh, usd_mesh, attribute_id, meta_data);
          }
        }

        return true;
      });
}

static pxr::SdfValueTypeName convert_blender_type_to_usd(const eCustomDataType blender_type)
{
  switch (blender_type) {
    case CD_PROP_FLOAT:
      return pxr::SdfValueTypeNames->FloatArray;
    case CD_PROP_INT8:
    case CD_PROP_INT32:
      return pxr::SdfValueTypeNames->IntArray;
    case CD_PROP_FLOAT2:
      return pxr::SdfValueTypeNames->Float2Array;
    case CD_PROP_FLOAT3:
      return pxr::SdfValueTypeNames->Float3Array;
    case CD_PROP_STRING:
      return pxr::SdfValueTypeNames->StringArray;
    case CD_PROP_BOOL:
      return pxr::SdfValueTypeNames->BoolArray;
    case CD_PROP_QUATERNION:
      return pxr::SdfValueTypeNames->QuatfArray;
    default:
      WM_reportf(RPT_WARNING, "Unsupported domain for mesh data.");
      return pxr::SdfValueTypeNames->Opaque;
  }
}

static const pxr::TfToken convert_blender_domain_to_usd(const eAttrDomain blender_domain)
{
  switch (blender_domain) {
    case ATTR_DOMAIN_CORNER:
      return pxr::UsdGeomTokens->faceVarying;
    case ATTR_DOMAIN_POINT:
      return pxr::UsdGeomTokens->vertex;
    case ATTR_DOMAIN_FACE:
      return pxr::UsdGeomTokens->uniform;

    /* Notice: Edge types are not supported in USD! */
    default:
      WM_reportf(RPT_WARNING, "Unsupported domain for mesh data.");
      return pxr::TfToken();
  }
}

template<typename T>
const VArray<T> get_attribute_buffer(const Mesh *mesh,
                                     const bke::AttributeIDRef &attribute_id,
                                     const bke::AttributeMetaData &meta_data,
                                     const T default_value)
{
  return *mesh->attributes().lookup_or_default<T>(attribute_id, meta_data.domain, default_value);
}

template<typename T, typename U>
void USDGenericMeshWriter::copy_blender_buffer_to_prim(const VArray<T> &buffer,
                                                       const pxr::UsdTimeCode timecode,
                                                       pxr::UsdGeomPrimvar attribute_pv)
{
  pxr::VtArray<U> data;
  for (const auto index : buffer.index_range()) {
    U value = buffer.get(index);
    data.push_back(value);
  }
  attribute_pv.Set(data, timecode);

  const pxr::UsdAttribute &prim_attr = attribute_pv.GetAttr();
  usd_value_writer_.SetAttribute(prim_attr, pxr::VtValue(data), timecode);
}

template<typename T, typename U>
void USDGenericMeshWriter::copy_blender_buffer_to_prim2(const VArray<T> &buffer,
                                                        const pxr::UsdTimeCode timecode,
                                                        pxr::UsdGeomPrimvar attribute_pv)
{
  pxr::VtArray<U> data;
  for (const auto index : buffer.index_range()) {
    T value = buffer.get(index);
    data.push_back({value.x, value.y});
  }

  if (!attribute_pv.HasValue() && timecode != pxr::UsdTimeCode::Default()) {
    attribute_pv.Set(data, pxr::UsdTimeCode::Default());
  }
  attribute_pv.Set(data, timecode);

  const pxr::UsdAttribute &prim_attr = attribute_pv.GetAttr();
  usd_value_writer_.SetAttribute(prim_attr, pxr::VtValue(data), timecode);
}

template<typename T, typename U>
void USDGenericMeshWriter::copy_blender_buffer_to_prim3(const VArray<T> &buffer,
                                                        const pxr::UsdTimeCode timecode,
                                                        pxr::UsdGeomPrimvar attribute_pv)
{
  pxr::VtArray<U> data;
  for (const auto index : buffer.index_range()) {
    T buffer_value = buffer.get(index);
    /* Colors store as rgb, so recast */
    float3 value = static_cast<float3>(buffer_value);
    data.push_back({value.x, value.y, value.z});
  }
  attribute_pv.Set(data, timecode);

  const pxr::UsdAttribute &prim_attr = attribute_pv.GetAttr();
  usd_value_writer_.SetAttribute(prim_attr, pxr::VtValue(data), timecode);
}

template<typename T, typename U>
void USDGenericMeshWriter::copy_blender_buffer_to_prim_quat(const VArray<T> &buffer,
                                                            const pxr::UsdTimeCode timecode,
                                                            pxr::UsdGeomPrimvar attribute_pv)
{
  pxr::VtArray<U> data;
  for (const auto index : buffer.index_range()) {
    T value = buffer.get(index);
    /* Note for the future: We may need a separate _prim4 function as
     * this function deliberately puts the W value first. */
    data.push_back({value.w, value.x, value.y, value.z});
  }
  attribute_pv.Set(data, timecode);

  const pxr::UsdAttribute &prim_attr = attribute_pv.GetAttr();
  usd_value_writer_.SetAttribute(prim_attr, pxr::VtValue(data), timecode);
}

void USDGenericMeshWriter::write_generic_data(const Mesh *mesh,
                                              pxr::UsdGeomMesh usd_mesh,
                                              const bke::AttributeIDRef &attribute_id,
                                              const bke::AttributeMetaData &meta_data)
{
  /* Skipping some Blender-specific attributes that have no conversion */
  if (ELEM(attribute_id.name(), "crease_edge")) {
    return;
  }

  pxr::UsdTimeCode timecode = get_export_time_code();
  const std::string name = attribute_id.name();
  pxr::TfToken primvar_name(pxr::TfMakeValidIdentifier(name));
  const pxr::UsdGeomPrimvarsAPI pvApi = pxr::UsdGeomPrimvarsAPI(usd_mesh);

  /* Varying type depends on original domain. */
  const pxr::TfToken prim_varying = convert_blender_domain_to_usd(meta_data.domain);
  const pxr::SdfValueTypeName prim_attr_type = convert_blender_type_to_usd(meta_data.data_type);

  if (prim_varying == empty_token || prim_attr_type == pxr::SdfValueTypeNames->Opaque) {
    WM_reportf(RPT_WARNING,
               "Mesh %s, Attribute %s cannot be converted to USD.",
               &mesh->id.name[2],
               attribute_id.name().data());
    return;
  }

  pxr::UsdGeomPrimvar attribute_pv = pvApi.CreatePrimvar(
      primvar_name, prim_attr_type, prim_varying);

  switch (meta_data.data_type) {
    case CD_PROP_FLOAT: {
      auto buffer = get_attribute_buffer<float>(mesh, attribute_id, meta_data, 0);
      copy_blender_buffer_to_prim<float, float>(buffer, timecode, attribute_pv);
      break;
    }
    case CD_PROP_INT8: {
      auto buffer = get_attribute_buffer<int8_t>(mesh, attribute_id, meta_data, 0);
      copy_blender_buffer_to_prim<int8_t, int>(buffer, timecode, attribute_pv);
      break;
    }
    case CD_PROP_INT32: {
      auto buffer = get_attribute_buffer<int32_t>(mesh, attribute_id, meta_data, 0);
      copy_blender_buffer_to_prim<int32_t, int32_t>(buffer, timecode, attribute_pv);
      break;
    }
    case CD_PROP_FLOAT2: {
      auto buffer = get_attribute_buffer<float2>(mesh, attribute_id, meta_data, {0.0f, 0.0f});
      copy_blender_buffer_to_prim2<float2, pxr::GfVec2f>(buffer, timecode, attribute_pv);
      break;
    }
    case CD_PROP_FLOAT3: {
      auto buffer = get_attribute_buffer<float3>(
          mesh, attribute_id, meta_data, {0.0f, 0.0f, 0.0f});
      copy_blender_buffer_to_prim3<float3, pxr::GfVec3f>(buffer, timecode, attribute_pv);
      break;
    }
    case CD_PROP_BOOL: {
      auto buffer = get_attribute_buffer<bool>(mesh, attribute_id, meta_data, false);
      copy_blender_buffer_to_prim<bool, bool>(buffer, timecode, attribute_pv);
      break;
    }
    /*
     * // This seems to be unsupported so far?
    case CD_PROP_QUATERNION: {
      auto buffer = get_attribute_buffer<float4>(
          // Note that GfQuatf takes in values in wxyz order.
          mesh, attribute_id, meta_data, {1.0f, 0.0f, 0.0f, 0.0f});
      copy_blender_buffer_to_prim_quat<float4, pxr::GfQuatf>(buffer, timecode, attribute_pv);
      break;
    }
    */
    default:
      BLI_assert_msg(0, "Unsupported domain for mesh data.");
  }
}

void USDGenericMeshWriter::write_uv_data(const Mesh *mesh,
                                         pxr::UsdGeomMesh usd_mesh,
                                         const bke::AttributeIDRef &attribute_id,
                                         const bke::AttributeMetaData &meta_data,
                                         const char *active_set_name)
{
  pxr::UsdTimeCode timecode = get_export_time_code();
  const pxr::UsdGeomPrimvarsAPI pvApi = pxr::UsdGeomPrimvarsAPI(usd_mesh);

  const blender::StringRef active_ref(active_set_name);

  /* Optionally, rename the active UV set to "st".
   *
   * Because primvars don't have a notion of "active" for data like
   * UVs, but a specific UV set may be considered "active" by target
   * applications, the convention is to name the active set "st". */
  bool rename_to_st = this->usd_export_context_.export_params.convert_uv_to_st &&
                      active_set_name && (active_ref == attribute_id.name());

  const std::string name = rename_to_st ? "st" : attribute_id.name();

  pxr::TfToken primvar_name(pxr::TfMakeValidIdentifier(name));

  pxr::UsdGeomPrimvar uv_pv = pvApi.CreatePrimvar(
      primvar_name, pxr::SdfValueTypeNames->TexCoord2fArray, pxr::UsdGeomTokens->faceVarying);

  const VArray<float2> buffer = *mesh->attributes().lookup_or_default<float2>(
      attribute_id, meta_data.domain, {0.0f, 0.0f});
  copy_blender_buffer_to_prim2<float2, pxr::GfVec2f>(buffer, timecode, uv_pv);
}

void USDGenericMeshWriter::write_color_data(const Mesh *mesh,
                                            pxr::UsdGeomMesh usd_mesh,
                                            const bke::AttributeIDRef &attribute_id,
                                            const bke::AttributeMetaData &meta_data)
{
  pxr::UsdTimeCode timecode = get_export_time_code();
  const std::string name = attribute_id.name();
  pxr::TfToken primvar_name(pxr::TfMakeValidIdentifier(name));
  const pxr::UsdGeomPrimvarsAPI pvApi = pxr::UsdGeomPrimvarsAPI(usd_mesh);

  /* Varying type depends on original domain. */
  const pxr::TfToken prim_varying = meta_data.domain == ATTR_DOMAIN_CORNER ?
                                        pxr::UsdGeomTokens->faceVarying :
                                        pxr::UsdGeomTokens->vertex;

  pxr::UsdGeomPrimvar colors_pv = pvApi.CreatePrimvar(
      primvar_name, pxr::SdfValueTypeNames->Color3fArray, prim_varying);

  const VArray<ColorGeometry4f> buffer = get_attribute_buffer<ColorGeometry4f>(
      mesh, attribute_id, meta_data, {0.0f, 0.0f, 0.0f, 1.0f});

  switch (meta_data.domain) {
    case ATTR_DOMAIN_CORNER:
    case ATTR_DOMAIN_POINT:
      copy_blender_buffer_to_prim3<ColorGeometry4f, pxr::GfVec3f>(buffer, timecode, colors_pv);
      break;

    default:
      BLI_assert_msg(0, "Invalid domain for mesh color data.");
      return;
  }
}

void USDGenericMeshWriter::free_export_mesh(Mesh *mesh)
{
  BKE_id_free(nullptr, mesh);
}

pxr::UsdTimeCode USDGenericMeshWriter::get_mesh_export_time_code() const
{
  return get_export_time_code();
}

struct USDMeshData {
  pxr::VtArray<pxr::GfVec3f> points;
  pxr::VtIntArray face_vertex_counts;
  pxr::VtIntArray face_indices;
  std::map<short, pxr::VtIntArray> face_groups;

  /* The length of this array specifies the number of creases on the surface. Each element gives
   * the number of (must be adjacent) vertices in each crease, whose indices are linearly laid out
   * in the 'creaseIndices' attribute. Since each crease must be at least one edge long, each
   * element of this array should be greater than one. */
  pxr::VtIntArray crease_lengths;
  /* The indices of all vertices forming creased edges. The size of this array must be equal to the
   * sum of all elements of the 'creaseLengths' attribute. */
  pxr::VtIntArray crease_vertex_indices;
  /* The per-crease or per-edge sharpness for all creases (Usd.Mesh.SHARPNESS_INFINITE for a
   * perfectly sharp crease). Since 'creaseLengths' encodes the number of vertices in each crease,
   * the number of elements in this array will be either 'len(creaseLengths)' or the sum over all X
   * of '(creaseLengths[X] - 1)'. Note that while the RI spec allows each crease to have either a
   * single sharpness or a value per-edge, USD will encode either a single sharpness per crease on
   * a mesh, or sharpness's for all edges making up the creases on a mesh. */
  pxr::VtFloatArray crease_sharpnesses;

  /* The lengths of this array specifies the number of sharp corners (or vertex crease) on the
   * surface. Each value is the index of a vertex in the mesh's vertex list. */
  pxr::VtIntArray corner_indices;
  /* The per-vertex sharpnesses. The lengths of this array must match that of `corner_indices`. */
  pxr::VtFloatArray corner_sharpnesses;
};

void USDGenericMeshWriter::write_vertex_groups(const Object *ob,
                                               const Mesh *mesh,
                                               pxr::UsdGeomMesh usd_mesh,
                                               bool as_point_groups)
{
  if (!ob)
    return;

  pxr::UsdTimeCode timecode = get_export_time_code();

  int i, j;
  bDeformGroup *def = nullptr;
  std::vector<pxr::UsdGeomPrimvar> pv_groups;
  std::vector<pxr::VtArray<float>> pv_data;

  // Create vertex groups primvars
  for (def = (bDeformGroup *)ob->defbase.first, i = 0, j = 0; def; def = def->next, ++i) {
    if (!def) {
      continue;
    }

    pxr::TfToken primvar_name(pxr::TfMakeValidIdentifier(def->name));
    pxr::TfToken primvar_interpolation = (as_point_groups) ? pxr::UsdGeomTokens->vertex :
                                                             pxr::UsdGeomTokens->faceVarying;

    pxr::UsdGeomPrimvarsAPI primvarsAPI(usd_mesh.GetPrim());

    pv_groups.push_back(primvarsAPI.CreatePrimvar(
        primvar_name, pxr::SdfValueTypeNames->FloatArray, primvar_interpolation));

    size_t primvar_size = 0;

    if (as_point_groups) {
      primvar_size = mesh->totvert;
    }
    else {
      primvar_size = mesh->corner_verts().size();
    }
    pv_data.push_back(pxr::VtArray<float>(primvar_size, 0.0f));
  }

  size_t num_groups = pv_groups.size();

  if (num_groups == 0) {
    return;
  }

  const blender::Span<MDeformVert> dverts = mesh->deform_verts();

  // Extract vertex groups
  if (as_point_groups) {
    for (i = 0; i < dverts.size(); ++i) {
      for (j = 0; j < dverts[i].totweight; ++j) {
        uint idx = dverts[i].dw[j].def_nr;
        float w = dverts[i].dw[j].weight;
        /* This out of bounds check is necessary because MDeformVert.totweight can be
        larger than the number of bDeformGroup structs in Object.defbase. It appears to be
        a Blender bug that can cause this scenario.*/
        if (idx < num_groups) {
          pv_data[idx][i] = w;
        }
      }
    }
  }
  else {
    const OffsetIndices polys = mesh->polys();
    const Span<int> corner_verts = mesh->corner_verts();
    int p_idx = 0;

    for (const int i : polys.index_range()) {
      const IndexRange poly = polys[i];
      for (const int vert : corner_verts.slice(poly)) {
        const MDeformVert &dvert = dverts[vert];
        for (j = 0; j < dvert.totweight; ++j) {
          uint idx = dvert.dw[j].def_nr;
          float w = dvert.dw[j].weight;
          /* This out of bounds check is necessary because MDeformVert.totweight can be
           * larger than the number of bDeformGroup structs in Object.defbase. Appears to be
           * a Blender bug that can cause this scenario. */
          if (idx < num_groups) {
            pv_data[idx][p_idx] = w;
          }
        }
        ++p_idx;
      }
    }
  }

  // Store data in usd
  for (i = 0; i < num_groups; i++) {
    pv_groups[i].Set(pv_data[i], timecode);

    const pxr::UsdAttribute &vertex_colors_attr = pv_groups[i].GetAttr();
    usd_value_writer_.SetAttribute(vertex_colors_attr, pxr::VtValue(pv_data[i]), timecode);
  }
}

void USDGenericMeshWriter::write_mesh(HierarchyContext &context, Mesh *mesh)
{
  pxr::UsdTimeCode timecode = get_mesh_export_time_code();
  pxr::UsdStageRefPtr stage = usd_export_context_.stage;

  pxr::UsdGeomMesh usd_mesh =
      (usd_export_context_.export_params.export_as_overs) ?
          pxr::UsdGeomMesh(usd_export_context_.stage->OverridePrim(usd_export_context_.usd_path)) :
          pxr::UsdGeomMesh::Define(usd_export_context_.stage, usd_export_context_.usd_path);

  write_visibility(context, timecode, usd_mesh);

  USDMeshData usd_mesh_data;
  /* Ensure data exists if currently in edit mode. */
  BKE_mesh_wrapper_ensure_mdata(mesh);
  get_geometry_data(mesh, usd_mesh_data);

  if (usd_export_context_.export_params.export_vertices) {
    pxr::UsdAttribute attr_points = usd_mesh.CreatePointsAttr(pxr::VtValue(), true);
    pxr::UsdAttribute attr_face_vertex_counts = usd_mesh.CreateFaceVertexCountsAttr(pxr::VtValue(),
                                                                                    true);

    pxr::UsdAttribute attr_face_vertex_indices = usd_mesh.CreateFaceVertexIndicesAttr(
        pxr::VtValue(), true);

    // NOTE (Marcelo Sercheli): Code to set values at default time was removed since
    // `timecode` will be default time in case of non-animation exports. For animated
    // exports, USD will inter/extrapolate values linearly.
    usd_value_writer_.SetAttribute(attr_points, pxr::VtValue(usd_mesh_data.points), timecode);
    usd_value_writer_.SetAttribute(
        attr_face_vertex_counts, pxr::VtValue(usd_mesh_data.face_vertex_counts), timecode);
    usd_value_writer_.SetAttribute(
        attr_face_vertex_indices, pxr::VtValue(usd_mesh_data.face_indices), timecode);

    if (!usd_mesh_data.crease_lengths.empty()) {
      pxr::UsdAttribute attr_crease_lengths = usd_mesh.CreateCreaseLengthsAttr(pxr::VtValue(),
                                                                               true);
      pxr::UsdAttribute attr_crease_indices = usd_mesh.CreateCreaseIndicesAttr(pxr::VtValue(),
                                                                               true);
      pxr::UsdAttribute attr_crease_sharpness = usd_mesh.CreateCreaseSharpnessesAttr(
          pxr::VtValue(), true);

      // NOTE (Marcelo Sercheli): Code to set values at default time was removed since
      // `timecode` will be default time in case of non-animation exports. For animated
      // exports, USD will inter/extrapolate values linearly.
      usd_value_writer_.SetAttribute(
          attr_crease_lengths, pxr::VtValue(usd_mesh_data.crease_lengths), timecode);
      usd_value_writer_.SetAttribute(
          attr_crease_indices, pxr::VtValue(usd_mesh_data.crease_vertex_indices), timecode);
      usd_value_writer_.SetAttribute(
          attr_crease_sharpness, pxr::VtValue(usd_mesh_data.crease_sharpnesses), timecode);
    }
  }

  write_custom_data(mesh, usd_mesh);

  if (usd_export_context_.export_params.export_vertex_groups) {
    write_vertex_groups(context.object,
                        mesh,
                        usd_mesh,
                        !usd_export_context_.export_params.vertex_data_as_face_varying);
  }

  if (!usd_mesh_data.corner_indices.empty() &&
      usd_mesh_data.corner_indices.size() == usd_mesh_data.corner_sharpnesses.size())
  {
    pxr::UsdAttribute attr_corner_indices = usd_mesh.CreateCornerIndicesAttr(pxr::VtValue(), true);
    pxr::UsdAttribute attr_corner_sharpnesses = usd_mesh.CreateCornerSharpnessesAttr(
        pxr::VtValue(), true);

    if (!attr_corner_indices.HasValue()) {
      attr_corner_indices.Set(usd_mesh_data.corner_indices, timecode);
      attr_corner_sharpnesses.Set(usd_mesh_data.corner_sharpnesses, timecode);
    }

    usd_value_writer_.SetAttribute(
        attr_corner_indices, pxr::VtValue(usd_mesh_data.corner_indices), timecode);
    usd_value_writer_.SetAttribute(
        attr_corner_sharpnesses, pxr::VtValue(usd_mesh_data.crease_sharpnesses), timecode);
  }

  write_custom_data(mesh, usd_mesh);

  if (usd_export_context_.export_params.export_normals) {
    write_normals(mesh, usd_mesh);
  }
  write_surface_velocity(mesh, usd_mesh);

  /* TODO(Sybren): figure out what happens when the face groups change. */
  if (frame_has_been_written_) {
    return;
  }

  if (usd_export_context_.export_params.export_vertices) {
    usd_mesh.CreateSubdivisionSchemeAttr().Set(
        (m_subsurf_mod == NULL) ? pxr::UsdGeomTokens->none : pxr::UsdGeomTokens->catmullClark);
  }

  if (usd_export_context_.export_params.export_materials) {
    assign_materials(context, usd_mesh, usd_mesh_data.face_groups);
  }

  /* Blender grows its bounds cache to cover animated meshes, so only author once. */
  if (const std::optional<Bounds<float3>> bounds = mesh->bounds_min_max()) {
    pxr::VtArray<pxr::GfVec3f> extent{
        pxr::GfVec3f{bounds->min[0], bounds->min[1], bounds->min[2]},
        pxr::GfVec3f{bounds->max[0], bounds->max[1], bounds->max[2]}};
    usd_mesh.CreateExtentAttr().Set(extent);
  }
}

static void get_vertices(const Mesh *mesh, USDMeshData &usd_mesh_data)
{
  usd_mesh_data.points.reserve(mesh->totvert);

  const Span<float3> positions = mesh->vert_positions();
  for (const int i : positions.index_range()) {
    const float3 &position = positions[i];
    usd_mesh_data.points.push_back(pxr::GfVec3f(position.x, position.y, position.z));
  }
}

static void get_loops_polys(const Mesh *mesh, USDMeshData &usd_mesh_data)
{
  /* Only construct face groups (a.k.a. geometry subsets) when we need them for material
   * assignments. */
  const bke::AttributeAccessor attributes = mesh->attributes();
  const VArray<int> material_indices = *attributes.lookup_or_default<int>(
      "material_index", ATTR_DOMAIN_FACE, 0);
  if (!material_indices.is_single() && mesh->totcol > 1) {
    const VArraySpan<int> indices_span(material_indices);
    for (const int i : indices_span.index_range()) {
      usd_mesh_data.face_groups[indices_span[i]].push_back(i);
    }
  }

  usd_mesh_data.face_vertex_counts.reserve(mesh->totpoly);
  usd_mesh_data.face_indices.reserve(mesh->totloop);

  const OffsetIndices polys = mesh->polys();
  const Span<int> corner_verts = mesh->corner_verts();

  for (const int i : polys.index_range()) {
    const IndexRange poly = polys[i];
    usd_mesh_data.face_vertex_counts.push_back(poly.size());
    for (const int vert : corner_verts.slice(poly)) {
      usd_mesh_data.face_indices.push_back(vert);
    }
  }
}

static void get_edge_creases(const Mesh *mesh, USDMeshData &usd_mesh_data)
{
  const bke::AttributeAccessor attributes = mesh->attributes();
  const bke::AttributeReader attribute = attributes.lookup<float>("crease_edge", ATTR_DOMAIN_EDGE);
  if (!attribute) {
    return;
  }
  const VArraySpan creases(*attribute);
  const Span<int2> edges = mesh->edges();
  for (const int i : edges.index_range()) {
    const float crease = creases[i];
    if (crease == 0.0f) {
      continue;
    }

    const float sharpness = crease >= 1.0f ? pxr::UsdGeomMesh::SHARPNESS_INFINITE : crease;

    usd_mesh_data.crease_vertex_indices.push_back(edges[i][0]);
    usd_mesh_data.crease_vertex_indices.push_back(edges[i][1]);
    usd_mesh_data.crease_lengths.push_back(2);
    usd_mesh_data.crease_sharpnesses.push_back(sharpness);
  }
}

static void get_vert_creases(const Mesh *mesh, USDMeshData &usd_mesh_data)
{
  const bke::AttributeAccessor attributes = mesh->attributes();
  const bke::AttributeReader attribute = attributes.lookup<float>("crease_vert",
                                                                  ATTR_DOMAIN_POINT);
  if (!attribute) {
    return;
  }
  const VArraySpan creases(*attribute);
  for (const int i : creases.index_range()) {
    const float sharpness = creases[i];

    if (sharpness != 0.0f) {
      usd_mesh_data.corner_indices.push_back(i);
      usd_mesh_data.corner_sharpnesses.push_back(sharpness);
    }
  }
}

void USDGenericMeshWriter::get_geometry_data(const Mesh *mesh, USDMeshData &usd_mesh_data)
{
  get_vertices(mesh, usd_mesh_data);
  get_loops_polys(mesh, usd_mesh_data);
  get_edge_creases(mesh, usd_mesh_data);
  get_vert_creases(mesh, usd_mesh_data);
}

void USDGenericMeshWriter::assign_materials(const HierarchyContext &context,
                                            pxr::UsdGeomMesh usd_mesh,
                                            const MaterialFaceGroups &usd_face_groups)
{
  if (context.object->totcol == 0) {
    return;
  }

  /* Binding a material to a geometry subset isn't supported by the Hydra GL viewport yet,
   * which is why we always bind the first material to the entire mesh. See
   * https://github.com/PixarAnimationStudios/USD/issues/542 for more info. */
  bool mesh_material_bound = false;
  auto mesh_prim = usd_mesh.GetPrim();
  pxr::UsdShadeMaterialBindingAPI material_binding_api(mesh_prim);
  for (int mat_num = 0; mat_num < context.object->totcol; mat_num++) {
    Material *material = BKE_object_material_get(context.object, mat_num + 1);
    if (material == nullptr) {
      continue;
    }

    pxr::UsdShadeMaterial usd_material = ensure_usd_material(context, material);
    material_binding_api.Bind(usd_material);

    /* USD seems to support neither per-material nor per-face-group double-sidedness, so we just
     * use the flag from the first non-empty material slot. */
    usd_mesh.CreateDoubleSidedAttr(
        pxr::VtValue((material->blend_flag & MA_BL_CULL_BACKFACE) == 0));

    mesh_material_bound = true;
    break;
  }

  if (mesh_material_bound) {
    /* USD will require that prims with material bindings have the #MaterialBindingAPI applied
     * schema. While Bind() above will create the binding attribute, Apply() needs to be called as
     * well to add the #MaterialBindingAPI schema to the prim itself. */
    material_binding_api.Apply(mesh_prim);
  }
  else {
    /* Blender defaults to double-sided, but USD to single-sided. */
    usd_mesh.CreateDoubleSidedAttr(pxr::VtValue(true));
  }

  if (!mesh_material_bound || usd_face_groups.size() < 2) {
    /* Either all material slots were empty or there is only one material in use. As geometry
     * subsets are only written when actually used to assign a material, and the mesh already has
     * the material assigned, there is no need to continue. */
    return;
  }

  /* Define a geometry subset per material. */
  for (const MaterialFaceGroups::value_type &face_group : usd_face_groups) {
    short material_number = face_group.first;
    const pxr::VtIntArray &face_indices = face_group.second;

    Material *material = BKE_object_material_get(context.object, material_number + 1);
    if (material == nullptr) {
      continue;
    }

    pxr::UsdShadeMaterial usd_material = ensure_usd_material(context, material);
    pxr::TfToken material_name = usd_material.GetPath().GetNameToken();

    pxr::UsdGeomSubset usd_face_subset = material_binding_api.CreateMaterialBindSubset(
        material_name, face_indices);
    auto subset_prim = usd_face_subset.GetPrim();
    auto subset_material_api = pxr::UsdShadeMaterialBindingAPI(subset_prim);
    subset_material_api.Bind(usd_material);
    /* Apply the #MaterialBindingAPI applied schema, as required by USD. */
    subset_material_api.Apply(subset_prim);
  }
}

void USDGenericMeshWriter::write_normals(const Mesh *mesh, pxr::UsdGeomMesh usd_mesh)
{
  pxr::UsdTimeCode timecode = get_export_time_code();
  const float(*lnors)[3] = static_cast<const float(*)[3]>(
      CustomData_get_layer(&mesh->ldata, CD_NORMAL));
  const OffsetIndices polys = mesh->polys();
  const Span<int> corner_verts = mesh->corner_verts();

  pxr::VtVec3fArray loop_normals;
  loop_normals.reserve(mesh->totloop);

  if (lnors != nullptr) {
    /* Export custom loop normals. */
    for (int loop_idx = 0, totloop = mesh->totloop; loop_idx < totloop; ++loop_idx) {
      loop_normals.push_back(pxr::GfVec3f(lnors[loop_idx]));
    }
  }
  else {
    /* Compute the loop normals based on the 'smooth' flag. */
    bke::AttributeAccessor attributes = mesh->attributes();
    const Span<float3> vert_normals = mesh->vert_normals();
    const Span<float3> poly_normals = mesh->poly_normals();
    const VArray<bool> sharp_faces = *attributes.lookup_or_default<bool>(
        "sharp_face", ATTR_DOMAIN_FACE, false);
    for (const int i : polys.index_range()) {
      const IndexRange poly = polys[i];
      if (sharp_faces[i]) {
        /* Flat shaded, use common normal for all verts. */
        pxr::GfVec3f pxr_normal(&poly_normals[i].x);
        for (int loop_idx = 0; loop_idx < poly.size(); ++loop_idx) {
          loop_normals.push_back(pxr_normal);
        }
      }
      else {
        /* Smooth shaded, use individual vert normals. */
        for (const int vert : corner_verts.slice(poly)) {
          loop_normals.push_back(pxr::GfVec3f(&vert_normals[vert].x));
        }
      }
    }
  }

  pxr::UsdAttribute attr_normals = usd_mesh.CreateNormalsAttr(pxr::VtValue(), true);
  if (!attr_normals.HasValue()) {
    attr_normals.Set(loop_normals, pxr::UsdTimeCode::Default());
  }
  usd_value_writer_.SetAttribute(attr_normals, pxr::VtValue(loop_normals), timecode);
  usd_mesh.SetNormalsInterpolation(pxr::UsdGeomTokens->faceVarying);
}

void USDGenericMeshWriter::write_surface_velocity(const Mesh *mesh, pxr::UsdGeomMesh usd_mesh)
{
  /* Export velocity attribute output by fluid sim, sequence cache modifier
   * and geometry nodes. */
  CustomDataLayer *velocity_layer = BKE_id_attribute_find(
      &mesh->id, "velocity", CD_PROP_FLOAT3, ATTR_DOMAIN_POINT);

  if (velocity_layer == nullptr) {
    return;
  }

  const float(*velocities)[3] = reinterpret_cast<float(*)[3]>(velocity_layer->data);

  /* Export per-vertex velocity vectors. */
  pxr::VtVec3fArray usd_velocities;
  usd_velocities.reserve(mesh->totvert);

  for (int vertex_idx = 0, totvert = mesh->totvert; vertex_idx < totvert; ++vertex_idx) {
    usd_velocities.push_back(pxr::GfVec3f(velocities[vertex_idx]));
  }

  pxr::UsdTimeCode timecode = get_export_time_code();
  usd_mesh.CreateVelocitiesAttr().Set(usd_velocities, timecode);
}

USDMeshWriter::USDMeshWriter(const USDExporterContext &ctx) : USDGenericMeshWriter(ctx) {}

Mesh *USDMeshWriter::get_export_mesh(Object *object_eval, bool & /*r_needsfree*/)
{
  Scene *scene = DEG_get_evaluated_scene(usd_export_context_.depsgraph);
  // Assumed safe because the original depsgraph was nonconst in usd_capi...
  Depsgraph *dg = const_cast<Depsgraph *>(usd_export_context_.depsgraph);

  const Object *ob_src_eval = DEG_get_evaluated_object(dg, object_eval);
  return BKE_object_get_evaluated_mesh(ob_src_eval);
}

}  // namespace blender::io::usd
