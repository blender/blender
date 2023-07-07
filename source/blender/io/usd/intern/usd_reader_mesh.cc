/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Adapted from the Blender Alembic importer implementation.
 * Modifications Copyright 2021 Tangent Animation and
 * NVIDIA Corporation. All rights reserved. */

#include "usd_reader_mesh.h"
#include "usd_reader_material.h"
#include "usd_skel_convert.h"

#include "BKE_attribute.hh"
#include "BKE_customdata.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_mesh.hh"
#include "BKE_object.h"

#include "BLI_math_vector_types.hh"
#include "BLI_span.hh"
#include "BLI_string.h"

#include "DNA_customdata_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"

#include "WM_api.h"

#include "MEM_guardedalloc.h"

#include "WM_api.h"

#include <pxr/base/vt/array.h>
#include <pxr/base/vt/types.h>
#include <pxr/base/vt/value.h>
#include <pxr/usd/sdf/types.h>
#include <pxr/usd/usdGeom/primvarsAPI.h>
#include <pxr/usd/usdGeom/subset.h>
#include <pxr/usd/usdShade/materialBindingAPI.h>
#include <pxr/usd/usdSkel/bindingAPI.h>

#include <iostream>

namespace usdtokens {
/* Materials */
static const pxr::TfToken st("st", pxr::TfToken::Immortal);
static const pxr::TfToken UVMap("UVMap", pxr::TfToken::Immortal);
static const pxr::TfToken Cd("Cd", pxr::TfToken::Immortal);
static const pxr::TfToken displayColor("displayColor", pxr::TfToken::Immortal);
static const pxr::TfToken normalsPrimvar("normals", pxr::TfToken::Immortal);
}  // namespace usdtokens

namespace utils {

static pxr::UsdShadeMaterial compute_bound_material(const pxr::UsdPrim &prim)
{
  pxr::UsdShadeMaterialBindingAPI api = pxr::UsdShadeMaterialBindingAPI(prim);

  /* Compute generically bound ('allPurpose') materials. */
  pxr::UsdShadeMaterial mtl = api.ComputeBoundMaterial();

  /* If no generic material could be resolved, also check for 'preview' and
   * 'full' purpose materials as fallbacks. */
  if (!mtl) {
    mtl = api.ComputeBoundMaterial(pxr::UsdShadeTokens->preview);
  }

  if (!mtl) {
    mtl = api.ComputeBoundMaterial(pxr::UsdShadeTokens->full);
  }

  return mtl;
}

static void assign_materials(Main *bmain,
                             Object *ob,
                             const std::map<pxr::SdfPath, int> &mat_index_map,
                             const USDImportParams &params,
                             pxr::UsdStageRefPtr stage,
                             std::map<std::string, Material *> &mat_name_to_mat,
                             std::map<std::string, std::string> &usd_path_to_mat_name)
{
  if (!(stage && bmain && ob)) {
    return;
  }

  if (mat_index_map.size() > MAXMAT) {
    return;
  }

  blender::io::usd::USDMaterialReader mat_reader(params, bmain);

  for (std::map<pxr::SdfPath, int>::const_iterator it = mat_index_map.begin();
       it != mat_index_map.end();
       ++it)
  {

    Material *assigned_mat = blender::io::usd::find_existing_material(
        it->first, params, mat_name_to_mat, usd_path_to_mat_name);
    if (!assigned_mat) {
      /* Blender material doesn't exist, so create it now. */

      /* Look up the USD material. */
      pxr::UsdPrim prim = stage->GetPrimAtPath(it->first);
      pxr::UsdShadeMaterial usd_mat(prim);

      if (!usd_mat) {
        std::cout << "WARNING: Couldn't construct USD material from prim " << it->first
                  << std::endl;
        continue;
      }

      /* Add the Blender material. */
      assigned_mat = mat_reader.add_material(usd_mat);

      if (!assigned_mat) {
        std::cout << "WARNING: Couldn't create Blender material from USD material " << it->first
                  << std::endl;
        continue;
      }

      const std::string mat_name = pxr::TfMakeValidIdentifier(assigned_mat->id.name + 2);
      mat_name_to_mat[mat_name] = assigned_mat;

      if (params.mtl_name_collision_mode == USD_MTL_NAME_COLLISION_MAKE_UNIQUE) {
        /* Record the name of the Blender material we created for the USD material
         * with the given path. */
        usd_path_to_mat_name[it->first.GetAsString()] = mat_name;
      }
    }

    if (assigned_mat) {
      BKE_object_material_assign_single_obdata(bmain, ob, assigned_mat, it->second);
    }
    else {
      /* This shouldn't happen. */
      std::cout << "WARNING: Couldn't assign material " << it->first << std::endl;
    }
  }
  if (ob->totcol > 0) {
    ob->actcol = 1;
  }
}

}  // namespace utils

static void *add_customdata_cb(Mesh *mesh, const char *name, const int data_type)
{
  eCustomDataType cd_data_type = static_cast<eCustomDataType>(data_type);
  void *cd_ptr;
  CustomData *loopdata;
  int numloops;

  /* unsupported custom data type -- don't do anything. */
  if (!ELEM(cd_data_type, CD_PROP_FLOAT2, CD_PROP_BYTE_COLOR)) {
    return nullptr;
  }

  loopdata = &mesh->ldata;
  cd_ptr = CustomData_get_layer_named_for_write(loopdata, cd_data_type, name, mesh->totloop);
  if (cd_ptr != nullptr) {
    /* layer already exists, so just return it. */
    return cd_ptr;
  }

  /* Create a new layer. */
  numloops = mesh->totloop;
  cd_ptr = CustomData_add_layer_named(loopdata, cd_data_type, CD_SET_DEFAULT, numloops, name);
  return cd_ptr;
}

namespace blender::io::usd {

USDMeshReader::USDMeshReader(const pxr::UsdPrim &prim,
                             const USDImportParams &import_params,
                             const ImportSettings &settings)
    : USDGeomReader(prim, import_params, settings),
      mesh_prim_(prim),
      is_left_handed_(false),
      has_uvs_(false),
      is_time_varying_(false),
      is_initial_load_(false)
{
}

static const int convert_usd_type_to_blender(const std::string usd_type)
{
  static std::unordered_map<std::string, int> type_map{
      {pxr::SdfValueTypeNames->FloatArray.GetAsToken().GetString(), CD_PROP_FLOAT},
      {pxr::SdfValueTypeNames->Double.GetAsToken().GetString(), CD_PROP_FLOAT},
      {pxr::SdfValueTypeNames->IntArray.GetAsToken().GetString(), CD_PROP_INT32},
      {pxr::SdfValueTypeNames->Float2Array.GetAsToken().GetString(), CD_PROP_FLOAT2},
      {pxr::SdfValueTypeNames->TexCoord2dArray.GetAsToken().GetString(), CD_PROP_FLOAT2},
      {pxr::SdfValueTypeNames->TexCoord2fArray.GetAsToken().GetString(), CD_PROP_FLOAT2},
      {pxr::SdfValueTypeNames->TexCoord2hArray.GetAsToken().GetString(), CD_PROP_FLOAT2},
      {pxr::SdfValueTypeNames->TexCoord3dArray.GetAsToken().GetString(), CD_PROP_FLOAT2},
      {pxr::SdfValueTypeNames->TexCoord3fArray.GetAsToken().GetString(), CD_PROP_FLOAT2},
      {pxr::SdfValueTypeNames->TexCoord3hArray.GetAsToken().GetString(), CD_PROP_FLOAT2},
      {pxr::SdfValueTypeNames->Float3Array.GetAsToken().GetString(), CD_PROP_FLOAT3},
      {pxr::SdfValueTypeNames->Color3fArray.GetAsToken().GetString(), CD_PROP_COLOR},
      {pxr::SdfValueTypeNames->Color3hArray.GetAsToken().GetString(), CD_PROP_COLOR},
      {pxr::SdfValueTypeNames->Color3dArray.GetAsToken().GetString(), CD_PROP_COLOR},
      {pxr::SdfValueTypeNames->TokenArray.GetAsToken().GetString(), CD_PROP_STRING},
      {pxr::SdfValueTypeNames->StringArray.GetAsToken().GetString(), CD_PROP_STRING},
      {pxr::SdfValueTypeNames->BoolArray.GetAsToken().GetString(), CD_PROP_BOOL},
      {pxr::SdfValueTypeNames->QuatfArray.GetAsToken().GetString(), CD_PROP_QUATERNION},
  };

  auto value = type_map.find(usd_type);
  if (value == type_map.end()) {
    WM_reportf(RPT_WARNING, "Unsupported type for mesh data.");
    return 0;
  }

  return value->second;
}

static const int convert_usd_varying_to_blender(const std::string usd_domain)
{
  static std::unordered_map<std::string, int> domain_map{
      {pxr::UsdGeomTokens->faceVarying.GetString(), ATTR_DOMAIN_CORNER},
      {pxr::UsdGeomTokens->vertex.GetString(), ATTR_DOMAIN_POINT},
      {pxr::UsdGeomTokens->varying.GetString(), ATTR_DOMAIN_POINT},
      {pxr::UsdGeomTokens->face.GetString(), ATTR_DOMAIN_FACE},
      /* As there's no "constant" type in Blender, for now we're
       * translating into a point Attribute. */
      {pxr::UsdGeomTokens->constant.GetString(), ATTR_DOMAIN_POINT},
      {pxr::UsdGeomTokens->uniform.GetString(), ATTR_DOMAIN_FACE},
      /* Notice: Edge types are not supported! */
  };

  auto value = domain_map.find(usd_domain);
  if (value == domain_map.end()) {
    WM_reportf(RPT_WARNING, "Unsupported domain for mesh data type %s.", usd_domain.c_str());
    return 0;
  }

  return value->second;
}

void USDMeshReader::create_object(Main *bmain, const double /* motionSampleTime */)
{
  Mesh *mesh = BKE_mesh_add(bmain, name_.c_str());

  object_ = BKE_object_add_only_object(bmain, OB_MESH, name_.c_str());
  object_->data = mesh;
}

void USDMeshReader::read_object_data(Main *bmain, const double motionSampleTime)
{
  Mesh *mesh = (Mesh *)object_->data;

  is_initial_load_ = true;
  const USDMeshReadParams params = create_mesh_read_params(motionSampleTime,
                                                           import_params_.mesh_read_flag);

  Mesh *read_mesh = this->read_mesh(mesh, params, nullptr);

  is_initial_load_ = false;
  if (read_mesh != mesh) {
    BKE_mesh_nomain_to_mesh(read_mesh, mesh, object_);
  }

  readFaceSetsSample(bmain, mesh, motionSampleTime);

  if (mesh_prim_.GetPointsAttr().ValueMightBeTimeVarying()) {
    is_time_varying_ = true;
  }

  if (is_time_varying_) {
    add_cache_modifier();
  }

  if (import_params_.import_subdiv) {
    pxr::TfToken subdivScheme;
    mesh_prim_.GetSubdivisionSchemeAttr().Get(&subdivScheme, motionSampleTime);

    if (subdivScheme == pxr::UsdGeomTokens->catmullClark) {
      add_subdiv_modifier();
    }
  }

  if (import_params_.import_blendshapes) {
    import_blendshapes(bmain, object_, prim_);
  }

  if (import_params_.import_skeletons) {
    import_skel_bindings(bmain, object_, prim_);
  }

  USDXformReader::read_object_data(bmain, motionSampleTime);
}  // namespace blender::io::usd

bool USDMeshReader::valid() const
{
  return bool(mesh_prim_);
}

bool USDMeshReader::topology_changed(const Mesh *existing_mesh, const double motionSampleTime)
{
  /* TODO(makowalski): Is it the best strategy to cache the mesh
   * geometry in this function?  This needs to be revisited. */

  mesh_prim_.GetFaceVertexIndicesAttr().Get(&face_indices_, motionSampleTime);
  mesh_prim_.GetFaceVertexCountsAttr().Get(&face_counts_, motionSampleTime);
  mesh_prim_.GetPointsAttr().Get(&positions_, motionSampleTime);

  pxr::UsdGeomPrimvarsAPI primvarsAPI(mesh_prim_);

  /* TODO(makowalski): Reading normals probably doesn't belong in this function,
   * as this is not required to determine if the topology has changed. */

  /* If 'normals' and 'primvars:normals' are both specified, the latter has precedence. */
  pxr::UsdGeomPrimvar primvar = primvarsAPI.GetPrimvar(usdtokens::normalsPrimvar);
  if (primvar.HasValue()) {
    primvar.ComputeFlattened(&normals_, motionSampleTime);
    normal_interpolation_ = primvar.GetInterpolation();
  }
  else {
    mesh_prim_.GetNormalsAttr().Get(&normals_, motionSampleTime);
    normal_interpolation_ = mesh_prim_.GetNormalsInterpolation();
  }

  return positions_.size() != existing_mesh->totvert ||
         face_counts_.size() != existing_mesh->totpoly ||
         face_indices_.size() != existing_mesh->totloop;
}

void USDMeshReader::read_mpolys(Mesh *mesh)
{
  MutableSpan<int> poly_offsets = mesh->poly_offsets_for_write();
  MutableSpan<int> corner_verts = mesh->corner_verts_for_write();

  int loop_index = 0;

  std::vector<int> degenerate_faces;

  for (int i = 0; i < face_counts_.size(); i++) {
    const int face_size = face_counts_[i];

    /* Check for faces with the same vertex specified twice in a row. */
    if (face_indices_[loop_index] == face_indices_[loop_index+face_size-1]) {
      /* Loop below does not test first to last. */
      degenerate_faces.push_back(i);
    }
    else {
      for (int j = loop_index+1; j < loop_index + face_size; j++) {
        if (face_indices_[j] == face_indices_[j-1]) {
          degenerate_faces.push_back(i);
          break;
        }
      }
    }

    poly_offsets[i] = loop_index;

    /* Polygons are always assumed to be smooth-shaded. If the mesh should be flat-shaded,
     * this is encoded in custom loop normals. */

    if (is_left_handed_) {
      int loop_end_index = loop_index + (face_size - 1);
      for (int f = 0; f < face_size; ++f, ++loop_index) {
        corner_verts[loop_index] = face_indices_[loop_end_index - f];
      }
    }
    else {
      for (int f = 0; f < face_size; ++f, ++loop_index) {
        corner_verts[loop_index] = face_indices_[loop_index];
      }
    }
  }

  BKE_mesh_calc_edges(mesh, false, false);
}

void USDMeshReader::read_color_data_primvar(Mesh *mesh,
                                            const pxr::UsdGeomPrimvar &primvar,
                                            const double motionSampleTime)
{
  if (!(mesh && primvar && primvar.HasValue())) {
    return;
  }

  if (primvar_varying_map_.find(primvar.GetPrimvarName()) == primvar_varying_map_.end()) {
    bool might_be_time_varying = primvar.ValueMightBeTimeVarying();
    primvar_varying_map_.insert(std::make_pair(primvar.GetPrimvarName(), might_be_time_varying));
    if (might_be_time_varying) {
      is_time_varying_ = true;
    }
  }

  pxr::VtArray<pxr::GfVec3f> usd_colors;

  if (!primvar.ComputeFlattened(&usd_colors, motionSampleTime)) {
    WM_reportf(RPT_WARNING,
               "USD Import: couldn't compute values for color attribute '%s'",
               primvar.GetName().GetText());
    return;
  }

  pxr::TfToken interp = primvar.GetInterpolation();

  if ((interp == pxr::UsdGeomTokens->faceVarying && usd_colors.size() != mesh->totloop) ||
      (interp == pxr::UsdGeomTokens->varying && usd_colors.size() != mesh->totloop) ||
      (interp == pxr::UsdGeomTokens->vertex && usd_colors.size() != mesh->totvert) ||
      (interp == pxr::UsdGeomTokens->constant && usd_colors.size() != 1) ||
      (interp == pxr::UsdGeomTokens->uniform && usd_colors.size() != mesh->totpoly))
  {
    WM_reportf(RPT_WARNING,
               "USD Import: color attribute value '%s' count inconsistent with interpolation type",
               primvar.GetName().GetText());
    return;
  }

  const StringRef primvar_name(primvar.GetBaseName().GetString());
  bke::MutableAttributeAccessor attributes = mesh->attributes_for_write();

  eAttrDomain color_domain = ATTR_DOMAIN_POINT;

  if (ELEM(interp,
           pxr::UsdGeomTokens->varying,
           pxr::UsdGeomTokens->faceVarying,
           pxr::UsdGeomTokens->uniform))
  {
    color_domain = ATTR_DOMAIN_CORNER;
  }

  bke::SpanAttributeWriter<ColorGeometry4f> color_data;
  color_data = attributes.lookup_or_add_for_write_only_span<ColorGeometry4f>(primvar_name,
                                                                             color_domain);
  if (!color_data) {
    WM_reportf(RPT_WARNING,
               "USD Import: couldn't add color attribute '%s'",
               primvar.GetBaseName().GetText());
    return;
  }

  if (ELEM(interp, pxr::UsdGeomTokens->constant, pxr::UsdGeomTokens->uniform)) {
    /* For situations where there's only a single item, flood fill the object. */
    color_data.span.fill(
        ColorGeometry4f(usd_colors[0][0], usd_colors[0][1], usd_colors[0][2], 1.0f));
  }
  else {
    /* Check for situations that allow for a straight-forward copy by index. */
    if ((ELEM(interp, pxr::UsdGeomTokens->vertex)) ||
        (color_domain == ATTR_DOMAIN_CORNER && !is_left_handed_))
    {
      for (int i = 0; i < usd_colors.size(); i++) {
        ColorGeometry4f color = ColorGeometry4f(
            usd_colors[i][0], usd_colors[i][1], usd_colors[i][2], 1.0f);
        color_data.span[i] = color;
      }
    }

    /* Special case: expand uniform color into corner color.
     * Uniforms in USD come through as single colors, face-varying. Since Blender does not
     * support this particular combination for paintable color attributes, we convert the type
     * here to make sure that the user gets the same visual result.
     * */
    else if (ELEM(interp, pxr::UsdGeomTokens->uniform)) {
      for (int i = 0; i < usd_colors.size(); i++) {
        const ColorGeometry4f color = ColorGeometry4f(
            usd_colors[i][0], usd_colors[i][1], usd_colors[i][2], 1.0f);
        color_data.span[i * 4] = color;
        color_data.span[i * 4 + 1] = color;
        color_data.span[i * 4 + 2] = color;
        color_data.span[i * 4 + 3] = color;
      }
    }

    else {
      const OffsetIndices polys = mesh->polys();
      const Span<int> corner_verts = mesh->corner_verts();
      for (const int i : polys.index_range()) {
        const IndexRange &poly = polys[i];
        for (int j = 0; j < poly.size(); ++j) {
          int loop_index = poly[j];

          /* Default for constant varying interpolation. */
          int usd_index = 0;

          if (interp == pxr::UsdGeomTokens->vertex) {
            usd_index = corner_verts[loop_index];
          }
          else if (interp == pxr::UsdGeomTokens->faceVarying) {
            usd_index = poly.start();
            if (is_left_handed_) {
              usd_index += poly.size() - 1 - j;
            }
            else {
              usd_index += j;
            }
          }
          else if (interp == pxr::UsdGeomTokens->uniform) {
            /* Uniform varying uses the poly index. */
            usd_index = i;
          }

          if (usd_index >= usd_colors.size()) {
            continue;
          }

          ColorGeometry4f color = ColorGeometry4f(
              usd_colors[usd_index][0], usd_colors[usd_index][1], usd_colors[usd_index][2], 1.0f);
          color_data.span[usd_index] = color;
        }
      }
    }
  }

  color_data.finish();
}

void USDMeshReader::read_uv_data_primvar(Mesh *mesh,
                                         const pxr::UsdGeomPrimvar &primvar,
                                         const double motionSampleTime)
{
  const StringRef primvar_name(primvar.StripPrimvarsName(primvar.GetName()).GetString());

  pxr::VtValue usd_uvs_val;
  if (!primvar.ComputeFlattened(&usd_uvs_val, motionSampleTime)) {
    WM_reportf(RPT_WARNING,
               "USD Import: couldn't compute values for uv attribute '%s'",
               primvar.GetName().GetText());
    return;
  }

  if (!usd_uvs_val.CanCast<pxr::VtVec2fArray>()) {
    WM_reportf(RPT_WARNING,
               "USD Import: can't cast uv attribute '%s' to float2 array",
               primvar.GetName().GetText());
    return;
  }

  pxr::VtVec2fArray usd_uvs =
      usd_uvs_val.Cast<pxr::VtVec2fArray>().UncheckedGet<pxr::VtVec2fArray>();

  bke::MutableAttributeAccessor attributes = mesh->attributes_for_write();
  bke::SpanAttributeWriter<float2> uv_data;
  uv_data = attributes.lookup_or_add_for_write_only_span<float2>(primvar_name, ATTR_DOMAIN_CORNER);

  if (!uv_data) {
    WM_reportf(RPT_WARNING,
               "USD Import: couldn't add UV attribute '%s'",
               primvar.GetBaseName().GetText());
    return;
  }

  const pxr::TfToken varying_type = primvar.GetInterpolation();
  BLI_assert(ELEM(varying_type, pxr::UsdGeomTokens->vertex, pxr::UsdGeomTokens->faceVarying));

  if (varying_type == pxr::UsdGeomTokens->faceVarying) {
    if (is_left_handed_) {
      /* Reverse the index order. */
      const OffsetIndices polys = mesh->polys();
      for (const int i : polys.index_range()) {
        const IndexRange poly = polys[i];
        for (int j = 0; j < poly.size(); j++) {
          const int rev_index = poly.start() + poly.size() - 1 - j;
          uv_data.span[poly.start() + j] = float2(usd_uvs[rev_index][0], usd_uvs[rev_index][1]);
        }
      }
    }
    else {
      for (int i = 0; i < uv_data.span.size(); ++i) {
        uv_data.span[i] = float2(usd_uvs[i][0], usd_uvs[i][1]);
      }
    }
  }
  else {
    /* Handle vertex interpolation. */
    const Span<int> corner_verts = mesh->corner_verts();
    BLI_assert(mesh->totvert == usd_uvs.size());
    for (int i = 0; i < uv_data.span.size(); ++i) {
      /* Get the vertex index for this corner. */
      int vi = corner_verts[i];
      uv_data.span[i] = float2(usd_uvs[vi][0], usd_uvs[vi][1]);
    }
  }

  uv_data.finish();
}

template<typename T>
bke::SpanAttributeWriter<T> get_attribute_buffer_for_write(Mesh *mesh,
                                                           const pxr::TfToken name,
                                                           const int domain)
{
  bke::MutableAttributeAccessor attributes = mesh->attributes_for_write();
  bke::SpanAttributeWriter<T> data = attributes.lookup_or_add_for_write_only_span<T>(
      name.GetText(), static_cast<eAttrDomain>(domain));
  return std::move(data);
}

template<typename T, typename U>
void copy_prim_array_to_blender_buffer(const pxr::UsdGeomPrimvar &primvar,
                                       bke::SpanAttributeWriter<U> &buffer,
                                       const double motionSampleTime)
{
  pxr::VtArray<T> primvar_array;

  if (primvar.ComputeFlattened(&primvar_array, motionSampleTime)
      && primvar_array.size() == buffer.span.size()) {
    int64_t index = 0;
    for (const auto value : primvar_array) {
      buffer.span[index] = value;
      index += 1;
    }
    /* handle size mismatches, such as constant colors */
    auto last_value = primvar_array[primvar_array.size() - 1];
    for (; index < buffer.span.size(); index += 1) {
      buffer.span[index] = last_value;
    }
  }
  else {
    WM_reportf(
        RPT_WARNING, "Unable to get array values for primvar %s", primvar.GetName().GetText());
  }

  buffer.finish();
}

template<typename T, typename U>
void copy_prim_array_to_blender_buffer2(const pxr::UsdGeomPrimvar &primvar,
                                        bke::SpanAttributeWriter<U> &buffer,
                                        const double motionSampleTime)
{
  pxr::VtArray<T> primvar_array;

  if (primvar.ComputeFlattened(&primvar_array, motionSampleTime)
      && primvar_array.size() == buffer.span.size()) {
    int64_t index = 0;
    for (const auto value : primvar_array) {
      buffer.span[index] = {value[0], value[1]};
      index += 1;
    }
    /* handle size mismatches, such as constant colors */
    auto last_value = primvar_array[primvar_array.size() - 1];
    for (; index < buffer.span.size(); index += 1) {
      buffer.span[index] = {last_value[0], last_value[1]};
    }
  }
  else {
    WM_reportf(
        RPT_WARNING, "Unable to get array values for primvar %s", primvar.GetName().GetText());
  }

  buffer.finish();
}

template<typename T, typename U>
void copy_prim_array_to_blender_buffer3(const pxr::UsdGeomPrimvar &primvar,
                                        bke::SpanAttributeWriter<U> &buffer,
                                        const double motionSampleTime)
{
  pxr::VtArray<T> primvar_array;

  if (primvar.ComputeFlattened(&primvar_array, motionSampleTime)
      && primvar_array.size() == buffer.span.size()) {
    int64_t index = 0;
    for (const auto value : primvar_array) {
      buffer.span[index] = {value[0], value[1], value[2]};
      index += 1;
    }
    /* handle size mismatches, such as constant colors */
    auto last_value = primvar_array[primvar_array.size() - 1];
    for (; index < buffer.span.size(); index += 1) {
      buffer.span[index] = {last_value[0], last_value[1], last_value[2]};
    }
  }
  else {
    WM_reportf(
        RPT_WARNING, "Unable to get array values for primvar %s", primvar.GetName().GetText());
  }

  buffer.finish();
}

template<typename T, typename U>
void copy_prim_array_to_blender_buffer_color(const pxr::UsdGeomPrimvar &primvar,
                                             bke::SpanAttributeWriter<U> &buffer,
                                             const double motionSampleTime)
{
  pxr::VtArray<T> primvar_array;

  if (primvar.ComputeFlattened(&primvar_array, motionSampleTime)
      && primvar_array.size() == buffer.span.size()) {
    int64_t index = 0;
    for (const auto value : primvar_array) {
      buffer.span[index] = {value[0], value[1], value[2], 1.0f};
      index += 1;
    }
    /* handle size mismatches, such as constant colors */
    auto last_value = primvar_array[primvar_array.size() - 1];
    for (; index < buffer.span.size(); index += 1) {
      buffer.span[index] = {last_value[0], last_value[1], last_value[2], 1.0f};
    }
  }
  else {
    WM_reportf(
        RPT_WARNING, "Unable to get array values for primvar %s", primvar.GetName().GetText());
  }

  buffer.finish();
}

void USDMeshReader::read_generic_data_primvar(Mesh *mesh,
                                              const pxr::UsdGeomPrimvar &primvar,
                                              const double motionSampleTime)
{
  const pxr::SdfValueTypeName sdf_type = primvar.GetTypeName();
  const pxr::TfToken varying_type = primvar.GetInterpolation();
  const pxr::TfToken name = primvar.StripPrimvarsName(primvar.GetPrimvarName());

  const int domain = convert_usd_varying_to_blender(varying_type.GetString());
  const int type = convert_usd_type_to_blender(sdf_type.GetAsToken().GetString());

  switch (type) {
    case CD_PROP_FLOAT: {
      pxr::VtArray<float> primvar_array;
      bke::SpanAttributeWriter<float> buffer = get_attribute_buffer_for_write<float>(
          mesh, name, domain);
      copy_prim_array_to_blender_buffer<float, float>(primvar, buffer, motionSampleTime);
      break;
    }
    case CD_PROP_INT32: {
      pxr::VtArray<int32_t> primvar_array;
      bke::SpanAttributeWriter<int32_t> buffer = get_attribute_buffer_for_write<int32_t>(
          mesh, name, domain);
      copy_prim_array_to_blender_buffer<int32_t, int32_t>(primvar, buffer, motionSampleTime);
      break;
    }
    case CD_PROP_FLOAT2: {
      pxr::VtArray<pxr::GfVec2f> primvar_array;
      bke::SpanAttributeWriter<float2> buffer = get_attribute_buffer_for_write<float2>(
          mesh, name, domain);
      copy_prim_array_to_blender_buffer2<pxr::GfVec2f, float2>(primvar, buffer, motionSampleTime);
      break;
    }
    case CD_PROP_FLOAT3: {
      pxr::VtArray<pxr::GfVec3f> primvar_array;
      bke::SpanAttributeWriter<float3> buffer = get_attribute_buffer_for_write<float3>(
          mesh, name, domain);
      copy_prim_array_to_blender_buffer3<pxr::GfVec3f, float3>(primvar, buffer, motionSampleTime);
      break;
    }
    case CD_PROP_COLOR: {
      pxr::VtArray<pxr::GfVec3f> primvar_array;
      bke::SpanAttributeWriter<ColorGeometry4f> buffer =
          get_attribute_buffer_for_write<ColorGeometry4f>(mesh, name, domain);
      copy_prim_array_to_blender_buffer_color<pxr::GfVec3f, ColorGeometry4f>(
          primvar, buffer, motionSampleTime);
      break;
    }
    case CD_PROP_BOOL: {
      pxr::VtArray<bool> primvar_array;
      bke::SpanAttributeWriter<bool> buffer = get_attribute_buffer_for_write<bool>(
          mesh, name, domain);
      copy_prim_array_to_blender_buffer<bool, bool>(primvar, buffer, motionSampleTime);
      break;
    }
    default:
      WM_reportf(RPT_ERROR,
                 "Generic primvar %s: invalid type %s.",
                 primvar.GetName().GetText(),
                 sdf_type.GetAsToken().GetText());
      break;
  }
}

void USDMeshReader::read_vertex_creases(Mesh *mesh, const double motionSampleTime)
{
  pxr::VtIntArray corner_indices;
  if (!mesh_prim_.GetCornerIndicesAttr().Get(&corner_indices, motionSampleTime)) {
    return;
  }

  pxr::VtIntArray corner_sharpnesses;
  if (!mesh_prim_.GetCornerSharpnessesAttr().Get(&corner_sharpnesses, motionSampleTime)) {
    return;
  }

  /* It is fine to have fewer indices than vertices, but never the other way other. */
  if (corner_indices.size() > mesh->totvert) {
    std::cerr << "WARNING: too many vertex crease for mesh " << prim_path_ << std::endl;
    return;
  }

  if (corner_indices.size() != corner_sharpnesses.size()) {
    std::cerr << "WARNING: vertex crease indices and sharpnesses count mismatch for mesh "
              << prim_path_ << std::endl;
    return;
  }

  bke::MutableAttributeAccessor attributes = mesh->attributes_for_write();
  bke::SpanAttributeWriter creases = attributes.lookup_or_add_for_write_span<float>(
      "crease_vert", ATTR_DOMAIN_POINT);

  for (size_t i = 0; i < corner_indices.size(); i++) {
    creases.span[corner_indices[i]] = corner_sharpnesses[i];
  }
  creases.finish();
}

void USDMeshReader::process_normals_vertex_varying(Mesh *mesh)
{
  if (!mesh) {
    return;
  }

  if (normals_.empty()) {
    return;
  }

  if (normals_.size() != mesh->totvert) {
    std::cerr << "WARNING: vertex varying normals count mismatch for mesh " << prim_path_
              << std::endl;
    return;
  }

  MutableSpan vert_normals{(float3 *)BKE_mesh_vert_normals_for_write(mesh), mesh->totvert};
  BLI_STATIC_ASSERT(sizeof(normals_[0]) == sizeof(float3), "Expected float3 normals size");
  vert_normals.copy_from({(float3 *)normals_.data(), int64_t(normals_.size())});
  BKE_mesh_vert_normals_clear_dirty(mesh);
}

void USDMeshReader::process_normals_face_varying(Mesh *mesh)
{
  if (normals_.empty()) {
    return;
  }

  /* Check for normals count mismatches to prevent crashes. */
  if (normals_.size() != mesh->totloop) {
    std::cerr << "WARNING: loop normal count mismatch for mesh " << mesh->id.name << std::endl;
    return;
  }

  mesh->flag |= ME_AUTOSMOOTH;

  long int loop_count = normals_.size();

  float(*lnors)[3] = static_cast<float(*)[3]>(
      MEM_malloc_arrayN(loop_count, sizeof(float[3]), "USD::FaceNormals"));

  const OffsetIndices polys = mesh->polys();
  for (const int i : polys.index_range()) {
    const IndexRange poly = polys[i];
    for (int j = 0; j < poly.size(); j++) {
      int blender_index = poly.start() + j;

      int usd_index = poly.start();
      if (is_left_handed_) {
        usd_index += poly.size() - 1 - j;
      }
      else {
        usd_index += j;
      }

      lnors[blender_index][0] = normals_[usd_index][0];
      lnors[blender_index][1] = normals_[usd_index][1];
      lnors[blender_index][2] = normals_[usd_index][2];
    }
  }
  BKE_mesh_set_custom_normals(mesh, lnors);

  MEM_freeN(lnors);
}

void USDMeshReader::process_normals_uniform(Mesh *mesh)
{
  if (normals_.empty()) {
    return;
  }

  /* Check for normals count mismatches to prevent crashes. */
  if (normals_.size() != mesh->totpoly) {
    std::cerr << "WARNING: uniform normal count mismatch for mesh " << mesh->id.name << std::endl;
    return;
  }

  float(*lnors)[3] = static_cast<float(*)[3]>(
      MEM_malloc_arrayN(mesh->totloop, sizeof(float[3]), "USD::FaceNormals"));

  const OffsetIndices polys = mesh->polys();
  for (const int i : polys.index_range()) {
    for (const int corner : polys[i]) {
      lnors[corner][0] = normals_[i][0];
      lnors[corner][1] = normals_[i][1];
      lnors[corner][2] = normals_[i][2];
    }
  }

  mesh->flag |= ME_AUTOSMOOTH;
  BKE_mesh_set_custom_normals(mesh, lnors);

  MEM_freeN(lnors);
}

void USDMeshReader::read_mesh_sample(ImportSettings *settings,
                                     Mesh *mesh,
                                     const double motionSampleTime,
                                     const bool new_mesh)
{
  /* Note that for new meshes we always want to read verts and polys,
   * regardless of the value of the read_flag, to avoid a crash downstream
   * in code that expect this data to be there. */

  if (new_mesh || (settings->read_flag & MOD_MESHSEQ_READ_VERT) != 0) {
    MutableSpan<float3> vert_positions = mesh->vert_positions_for_write();
    for (int i = 0; i < positions_.size(); i++) {
      vert_positions[i] = {positions_[i][0], positions_[i][1], positions_[i][2]};
    }
    BKE_mesh_tag_positions_changed(mesh);

    read_vertex_creases(mesh, motionSampleTime);
  }

  if (new_mesh || (settings->read_flag & MOD_MESHSEQ_READ_POLY) != 0) {
    read_mpolys(mesh);
    if (normal_interpolation_ == pxr::UsdGeomTokens->faceVarying) {
      process_normals_face_varying(mesh);
    }
    else if (normal_interpolation_ == pxr::UsdGeomTokens->uniform) {
      process_normals_uniform(mesh);
    }
  }

  /* Process point normals after reading polys. */
  if ((settings->read_flag & MOD_MESHSEQ_READ_VERT) != 0 &&
      normal_interpolation_ == pxr::UsdGeomTokens->vertex)
  {
    process_normals_vertex_varying(mesh);
  }

  /* Custom Data layers. */
  if ((settings->read_flag & MOD_MESHSEQ_READ_VERT) |
      (settings->read_flag & MOD_MESHSEQ_READ_COLOR)) {
    read_custom_data(settings, mesh, motionSampleTime);
  }
}

void USDMeshReader::read_custom_data(const ImportSettings *settings,
                                     Mesh *mesh,
                                     const double motionSampleTime)
{
  if (!(mesh && mesh_prim_ && mesh->totloop > 0)) {
    return;
  }

  pxr::UsdGeomPrimvarsAPI pv_api = pxr::UsdGeomPrimvarsAPI(mesh_prim_);
  std::vector<pxr::UsdGeomPrimvar> primvars = pv_api.GetPrimvarsWithValues();

  pxr::TfToken active_color_name;
  pxr::TfToken active_uv_set_name;

  /* Convert primvars to custom layer data. */
  for (pxr::UsdGeomPrimvar &pv : primvars) {
    if (!pv.HasValue()) {
      WM_reportf(RPT_WARNING,
                 "Skipping primvar %s, mesh %s -- no value.",
                 pv.GetName().GetText(),
                 &mesh->id.name[2]);
      continue;
    }

    const pxr::SdfValueTypeName type = pv.GetTypeName();
    const pxr::TfToken varying_type = pv.GetInterpolation();
    const pxr::TfToken name = pv.StripPrimvarsName(pv.GetPrimvarName());

    /* Read Color primvars. */
    if (ELEM(varying_type, pxr::UsdGeomTokens->vertex, pxr::UsdGeomTokens->faceVarying) &&
        ELEM(type,
             pxr::SdfValueTypeNames->Color3hArray,
             pxr::SdfValueTypeNames->Color3fArray,
             pxr::SdfValueTypeNames->Color3dArray))
    {
      if (((settings->read_flag & MOD_MESHSEQ_READ_VERT) != 0) ||
          ((settings->read_flag & MOD_MESHSEQ_READ_COLOR) != 0))
      {
        /* Set the active color name to 'displayColor', if a color primvar
         * with this name exists.  Otherwise, use the name of the first
         * color primvar we find for the active color. */
        if (active_color_name.IsEmpty() || name == usdtokens::displayColor) {
          active_color_name = name;
        }

        read_color_data_primvar(mesh, pv, motionSampleTime);
      }
    }

    /* Read UV primvars. */
    else if (ELEM(varying_type, pxr::UsdGeomTokens->vertex, pxr::UsdGeomTokens->faceVarying) &&
             ELEM(type,
                  pxr::SdfValueTypeNames->TexCoord2dArray,
                  pxr::SdfValueTypeNames->TexCoord2fArray,
                  pxr::SdfValueTypeNames->TexCoord2hArray,
                  pxr::SdfValueTypeNames->TexCoord3dArray,
                  pxr::SdfValueTypeNames->TexCoord3fArray,
                  pxr::SdfValueTypeNames->TexCoord3hArray,
                  pxr::SdfValueTypeNames->Float2Array))
    {
      if ((settings->read_flag & MOD_MESHSEQ_READ_UV) != 0) {
        /* Set the active uv set name to 'st', if a uv set primvar
         * with this name exists.  Otherwise, use the name of the first
         * uv set primvar we find for the active uv set. */
        if (active_uv_set_name.IsEmpty() || name == usdtokens::st) {
          active_uv_set_name = name;
        }
        read_uv_data_primvar(mesh, pv, motionSampleTime);
      }
    }

    /* Read all other primvars. */
    else {
      if ((settings->read_flag & MOD_MESHSEQ_READ_ATTRIBUTES) != 0) {
        read_generic_data_primvar(mesh, pv, motionSampleTime);
      }
    }

    /* Record whether the primvar attribute might be time varying. */
    if (primvar_varying_map_.find(name) == primvar_varying_map_.end()) {
      bool might_be_time_varying = pv.ValueMightBeTimeVarying();
      primvar_varying_map_.insert(std::make_pair(name, might_be_time_varying));
      if (might_be_time_varying) {
        is_time_varying_ = true;
      }
    }
  } /* End primvar attribute loop. */
}

void USDMeshReader::assign_facesets_to_material_indices(double motionSampleTime,
                                                        MutableSpan<int> material_indices,
                                                        std::map<pxr::SdfPath, int> *r_mat_map)
{
  if (r_mat_map == nullptr) {
    return;
  }

  /* Find the geom subsets that have bound materials.
   * We don't call #pxr::UsdShadeMaterialBindingAPI::GetMaterialBindSubsets()
   * because this function returns only those subsets that are in the 'materialBind'
   * family, but, in practice, applications (like Houdini) might export subsets
   * in different families that are bound to materials.
   * TODO(makowalski): Reassess if the above is the best approach. */
  const std::vector<pxr::UsdGeomSubset> subsets = pxr::UsdGeomSubset::GetAllGeomSubsets(
      mesh_prim_);

  int current_mat = 0;
  if (!subsets.empty()) {
    for (const pxr::UsdGeomSubset &subset : subsets) {

      pxr::UsdShadeMaterial subset_mtl = utils::compute_bound_material(subset.GetPrim());

      if (!subset_mtl) {
        continue;
      }

      pxr::SdfPath subset_mtl_path = subset_mtl.GetPath();

      if (subset_mtl_path.IsEmpty()) {
        continue;
      }

      if (r_mat_map->find(subset_mtl_path) == r_mat_map->end()) {
        (*r_mat_map)[subset_mtl_path] = 1 + current_mat++;
      }

      const int mat_idx = (*r_mat_map)[subset_mtl_path] - 1;

      pxr::UsdAttribute indicesAttribute = subset.GetIndicesAttr();
      pxr::VtIntArray indices;
      indicesAttribute.Get(&indices, motionSampleTime);

      for (const int i : indices) {
        material_indices[i] = mat_idx;
      }
    }
  }

  if (r_mat_map->empty()) {
    pxr::UsdShadeMaterial mtl = utils::compute_bound_material(prim_);
    if (mtl) {
      pxr::SdfPath mtl_path = mtl.GetPath();

      if (!mtl_path.IsEmpty()) {
        r_mat_map->insert(std::make_pair(mtl.GetPath(), 1));
      }
    }
  }
}

void USDMeshReader::readFaceSetsSample(Main *bmain, Mesh *mesh, const double motionSampleTime)
{
  if (!import_params_.import_materials) {
    return;
  }

  std::map<pxr::SdfPath, int> mat_map;

  bke::MutableAttributeAccessor attributes = mesh->attributes_for_write();
  bke::SpanAttributeWriter<int> material_indices = attributes.lookup_or_add_for_write_span<int>(
      "material_index", ATTR_DOMAIN_FACE);
  this->assign_facesets_to_material_indices(motionSampleTime, material_indices.span, &mat_map);
  material_indices.finish();

  /* Build material name map if it's not built yet. */
  if (this->settings_->mat_name_to_mat.empty()) {
    build_material_map(bmain, &this->settings_->mat_name_to_mat);
  }

  utils::assign_materials(bmain,
                          object_,
                          mat_map,
                          this->import_params_,
                          this->prim_.GetStage(),
                          this->settings_->mat_name_to_mat,
                          this->settings_->usd_path_to_mat_name);
}

Mesh *USDMeshReader::read_mesh(Mesh *existing_mesh,
                               const USDMeshReadParams params,
                               const char ** /* err_str */)
{
  if (!mesh_prim_) {
    return existing_mesh;
  }

  mesh_prim_.GetOrientationAttr().Get(&orientation_);
  if (orientation_ == pxr::UsdGeomTokens->leftHanded) {
    is_left_handed_ = true;
  }

  pxr::UsdGeomPrimvarsAPI primvarsAPI(mesh_prim_);

  std::vector<pxr::TfToken> uv_tokens;

  /* Currently we only handle UV primvars. */
  if (params.read_flags & MOD_MESHSEQ_READ_UV) {

    std::vector<pxr::UsdGeomPrimvar> primvars = primvarsAPI.GetPrimvars();

    for (pxr::UsdGeomPrimvar p : primvars) {

      pxr::TfToken name = p.GetPrimvarName();
      pxr::SdfValueTypeName type = p.GetTypeName();

      bool is_uv = false;

      /* Assume all UVs are stored in one of these primvar types */
      if (ELEM(type,
               pxr::SdfValueTypeNames->TexCoord2hArray,
               pxr::SdfValueTypeNames->TexCoord2fArray,
               pxr::SdfValueTypeNames->TexCoord2dArray))
      {
        is_uv = true;
      }
      /* In some cases, the st primvar is stored as float2 values. */
      else if (name == usdtokens::st && type == pxr::SdfValueTypeNames->Float2Array) {
        is_uv = true;
      }

      if (is_uv) {

        pxr::TfToken interp = p.GetInterpolation();

        if (!ELEM(interp, pxr::UsdGeomTokens->faceVarying, pxr::UsdGeomTokens->vertex)) {
          continue;
        }

        uv_tokens.push_back(p.GetBaseName());
        has_uvs_ = true;

        /* Record whether the UVs might be time varying. */
        if (primvar_varying_map_.find(name) == primvar_varying_map_.end()) {
          bool might_be_time_varying = p.ValueMightBeTimeVarying();
          primvar_varying_map_.insert(std::make_pair(name, might_be_time_varying));
          if (might_be_time_varying) {
            is_time_varying_ = true;
          }
        }
      }
    }
  }

  Mesh *active_mesh = existing_mesh;
  bool new_mesh = false;

  /* TODO(makowalski): implement the optimization of only updating the mesh points when
   * the topology is consistent, as in the Alembic importer. */

  ImportSettings settings;
  if (settings_) {
    settings.validate_meshes = settings_->validate_meshes;
  }

  settings.read_flag |= params.read_flags;

  if (topology_changed(existing_mesh, params.motion_sample_time)) {
    new_mesh = true;
    active_mesh = BKE_mesh_new_nomain_from_template(
        existing_mesh, positions_.size(), 0, face_counts_.size(), face_indices_.size());

    for (pxr::TfToken token : uv_tokens) {
      add_customdata_cb(active_mesh, token.GetText(), CD_PROP_FLOAT2);
    }
  }

  read_mesh_sample(
      &settings, active_mesh, params.motion_sample_time, new_mesh || is_initial_load_);

  if (new_mesh) {
    /* Here we assume that the number of materials doesn't change, i.e. that
     * the material slots that were created when the object was loaded from
     * USD are still valid now. */
    if (active_mesh->totpoly != 0 && import_params_.import_materials) {
      std::map<pxr::SdfPath, int> mat_map;
      bke::MutableAttributeAccessor attributes = active_mesh->attributes_for_write();
      bke::SpanAttributeWriter<int> material_indices =
          attributes.lookup_or_add_for_write_span<int>("material_index", ATTR_DOMAIN_FACE);
      assign_facesets_to_material_indices(
          params.motion_sample_time, material_indices.span, &mat_map);
      material_indices.finish();
    }
  }

  if (settings.validate_meshes) {
    if (BKE_mesh_validate(active_mesh, false, false)) {
      WM_reportf(RPT_INFO, "Fixed mesh for prim: %s", mesh_prim_.GetPath().GetText());
    }
  }

  return active_mesh;
}

std::string USDMeshReader::get_skeleton_path() const
{
  if (!prim_) {
    return "";
  }

   pxr::UsdSkelBindingAPI skel_api = pxr::UsdSkelBindingAPI::Apply(prim_);

  if (!skel_api) {
    return "";
  }

  if (pxr::UsdSkelSkeleton skel = skel_api.GetInheritedSkeleton()) {
    return skel.GetPath().GetAsString();
  }

  return "";
}

/* Return a local transform to place the mesh in its world bind position.
 * In some cases, the bind transform and prim world transform might be
 * be different, in which case we must adjust the local transform
 * to ensure the mesh is correctly aligned for bininding.  A use
 * case where this might be needed is if a skel animation is exported
 * from Blender and both the skeleton and mesh are transformed in Create
 * or another DCC, without modifying the original mesh bind transform. */
bool USDMeshReader::get_geom_bind_xform_correction(const pxr::GfMatrix4d &bind_xf,
                                                   pxr::GfMatrix4d *r_xform,
                                                   const float time) const
{
  if (!r_xform) {
    return false;
  }

  pxr::GfMatrix4d world_xf = get_world_matrix(prim_, time);

  if (pxr::GfIsClose(bind_xf, world_xf, .000000001)) {
    /* The world and bind matrices are equal, so we don't
     * need to correct the local transfor.  Get the transform
     * with the standard API.  */
    pxr::UsdGeomXformable xformable;

    if (use_parent_xform_) {
      xformable = pxr::UsdGeomXformable(prim_.GetParent());
    }
    else {
      xformable = pxr::UsdGeomXformable(prim_);
    }

    if (!xformable) {
      /* This shouldn't happen. */
      *r_xform = pxr::GfMatrix4d(1.0);
      return false;
    }

    bool reset_xform_stack;
    return xformable.GetLocalTransformation(r_xform, &reset_xform_stack, time);
  }

  /* If we got here, then the bind transform and prim
   * world transform differ, so we must adjust the local
   * transform to ensure the mesh is aligned in the correct
   * bind position */
  pxr::GfMatrix4d parent_world_xf(1.0);

  pxr::UsdPrim parent;

  if (use_parent_xform_) {
    if (prim_.GetParent()) {
      parent = prim_.GetParent().GetParent();
    }
  }
  else {
    parent = prim_.GetParent();
  }

  if (parent) {
    parent_world_xf = get_world_matrix(parent, time);
  }

  pxr::GfMatrix4d corrected_local_xf = bind_xf * parent_world_xf.GetInverse();
  *r_xform = corrected_local_xf;

  return true;
}

/* Override transform computation to account for the binding
 * transformation for skinned meshes. */
bool USDMeshReader::get_local_usd_xform(pxr::GfMatrix4d *r_xform,
                                        bool *r_is_constant,
                                        const float time) const
{
  if (!r_xform) {
    return false;
  }

  if (!import_params_.import_skeletons) {
    /* Use the standard transform computation, since we are ignoring
     * skinning data. */
    return USDXformReader::get_local_usd_xform(r_xform, r_is_constant, time);
  }

  if (!(prim_.IsInstanceProxy() || prim_.IsInPrototype())) {
    if (pxr::UsdSkelBindingAPI skel_api = pxr::UsdSkelBindingAPI::Apply(prim_)) {
      if (skel_api.GetGeomBindTransformAttr().HasAuthoredValue()) {
        pxr::GfMatrix4d bind_xf;
        if (skel_api.GetGeomBindTransformAttr().Get(&bind_xf)) {
          /* Assume that if a bind transform is defined, then the
           * transform is constant. */
          if (r_is_constant) {
            *r_is_constant = true;
          }
          return get_geom_bind_xform_correction(bind_xf, r_xform, time);
        }
        else {
          std::cout << "WARNING: couldn't compute geom bind transform for " << prim_.GetPath()
                    << std::endl;
        }
      }
    }
  }

  return USDXformReader::get_local_usd_xform(r_xform, r_is_constant, time);
}

}  // namespace blender::io::usd
