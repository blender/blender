/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Adapted from the Blender Alembic importer implementation.
 * Modifications Copyright 2021 Tangent Animation and
 * NVIDIA Corporation. All rights reserved. */

#include "usd_reader_mesh.hh"
#include "usd.hh"
#include "usd_attribute_utils.hh"
#include "usd_hash_types.hh"
#include "usd_mesh_utils.hh"
#include "usd_reader_material.hh"
#include "usd_skel_convert.hh"
#include "usd_utils.hh"

#include "BKE_attribute.hh"
#include "BKE_customdata.hh"
#include "BKE_geometry_set.hh"
#include "BKE_main.hh"
#include "BKE_material.h"
#include "BKE_mesh.hh"
#include "BKE_object.hh"
#include "BKE_report.hh"

#include "BLI_array.hh"
#include "BLI_map.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_span.hh"

#include "DNA_customdata_types.h"
#include "DNA_material_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_windowmanager_types.h"

#include <pxr/base/gf/matrix4f.h>
#include <pxr/base/vt/array.h>
#include <pxr/base/vt/types.h>
#include <pxr/usd/sdf/types.h>
#include <pxr/usd/usdGeom/primvarsAPI.h>
#include <pxr/usd/usdGeom/subset.h>
#include <pxr/usd/usdShade/materialBindingAPI.h>
#include <pxr/usd/usdShade/tokens.h>
#include <pxr/usd/usdSkel/bindingAPI.h>

#include <algorithm>

#include "CLG_log.h"
static CLG_LogRef LOG = {"io.usd"};

namespace usdtokens {
/* Materials */
static const pxr::TfToken st("st", pxr::TfToken::Immortal);
static const pxr::TfToken UVMap("UVMap", pxr::TfToken::Immortal);
static const pxr::TfToken normalsPrimvar("normals", pxr::TfToken::Immortal);
}  // namespace usdtokens

namespace utils {
using namespace blender::io::usd;
static pxr::UsdShadeMaterial compute_bound_material(const pxr::UsdPrim &prim,
                                                    eUSDMtlPurpose mtl_purpose)
{
  const pxr::UsdShadeMaterialBindingAPI api = pxr::UsdShadeMaterialBindingAPI(prim);

  /* See the following documentation for material resolution behavior:
   * https://openusd.org/release/api/class_usd_shade_material_binding_a_p_i.html#UsdShadeMaterialBindingAPI_MaterialResolution
   */

  pxr::UsdShadeMaterial mtl;
  switch (mtl_purpose) {
    case USD_MTL_PURPOSE_FULL:
      mtl = api.ComputeBoundMaterial(pxr::UsdShadeTokens->full);
      if (!mtl) {
        /* Add an additional Blender-specific fallback to help with oddly authored USD files. */
        mtl = api.ComputeBoundMaterial(pxr::UsdShadeTokens->preview);
      }
      break;
    case USD_MTL_PURPOSE_PREVIEW:
      mtl = api.ComputeBoundMaterial(pxr::UsdShadeTokens->preview);
      break;
    case USD_MTL_PURPOSE_ALL:
      mtl = api.ComputeBoundMaterial(pxr::UsdShadeTokens->allPurpose);
      break;
  }

  return mtl;
}

static void assign_materials(Main *bmain,
                             Object *ob,
                             const blender::Map<pxr::SdfPath, int> &mat_index_map,
                             const blender::io::usd::USDImportParams &params,
                             pxr::UsdStageRefPtr stage,
                             blender::Map<std::string, Material *> &mat_name_to_mat,
                             blender::Map<std::string, std::string> &usd_path_to_mat_name)
{
  using namespace blender::io::usd;
  if (!(stage && bmain && ob)) {
    return;
  }

  if (mat_index_map.size() > MAXMAT) {
    return;
  }

  USDMaterialReader mat_reader(params, bmain);

  for (const auto item : mat_index_map.items()) {
    Material *assigned_mat = blender::io::usd::find_existing_material(
        item.key, params, mat_name_to_mat, usd_path_to_mat_name);
    if (!assigned_mat) {
      /* Blender material doesn't exist, so create it now. */

      /* Look up the USD material. */
      pxr::UsdPrim prim = stage->GetPrimAtPath(item.key);
      pxr::UsdShadeMaterial usd_mat(prim);

      if (!usd_mat) {
        CLOG_WARN(
            &LOG, "Couldn't construct USD material from prim %s", item.key.GetAsString().c_str());
        continue;
      }

      /* Add the Blender material. */
      assigned_mat = mat_reader.add_material(usd_mat);

      if (!assigned_mat) {
        CLOG_WARN(&LOG,
                  "Couldn't create Blender material from USD material %s",
                  item.key.GetAsString().c_str());
        continue;
      }

      const std::string mat_name = make_safe_name(assigned_mat->id.name + 2, true);
      mat_name_to_mat.lookup_or_add_default(mat_name) = assigned_mat;

      if (params.mtl_name_collision_mode == USD_MTL_NAME_COLLISION_MAKE_UNIQUE) {
        /* Record the name of the Blender material we created for the USD material
         * with the given path. */
        usd_path_to_mat_name.lookup_or_add_default(item.key.GetAsString()) = mat_name;
      }
    }

    if (assigned_mat) {
      BKE_object_material_assign_single_obdata(bmain, ob, assigned_mat, item.value);
    }
    else {
      /* This shouldn't happen. */
      CLOG_WARN(&LOG, "Couldn't assign material %s", item.key.GetAsString().c_str());
    }
  }
  if (ob->totcol > 0) {
    ob->actcol = 1;
  }
}

}  // namespace utils

namespace blender::io::usd {

USDMeshReader::USDMeshReader(const pxr::UsdPrim &prim,
                             const USDImportParams &import_params,
                             const ImportSettings &settings)
    : USDGeomReader(prim, import_params, settings),
      mesh_prim_(prim),
      is_left_handed_(false),
      is_time_varying_(false),
      is_initial_load_(false)
{
}

void USDMeshReader::create_object(Main *bmain, const double /*motionSampleTime*/)
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

  if (mesh_prim_.GetPointsAttr().ValueMightBeTimeVarying() ||
      mesh_prim_.GetVelocitiesAttr().ValueMightBeTimeVarying())
  {
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
    import_blendshapes(bmain, object_, prim_, reports());
  }

  if (import_params_.import_skeletons) {
    import_mesh_skel_bindings(bmain, object_, prim_, reports());
  }

  USDXformReader::read_object_data(bmain, motionSampleTime);
}

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

  return positions_.size() != existing_mesh->verts_num ||
         face_counts_.size() != existing_mesh->faces_num ||
         face_indices_.size() != existing_mesh->corners_num;
}

void USDMeshReader::read_mpolys(Mesh *mesh) const
{
  MutableSpan<int> face_offsets = mesh->face_offsets_for_write();
  MutableSpan<int> corner_verts = mesh->corner_verts_for_write();

  int loop_index = 0;

  for (int i = 0; i < face_counts_.size(); i++) {
    const int face_size = face_counts_[i];

    face_offsets[i] = loop_index;

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

  bke::mesh_calc_edges(*mesh, false, false);
}

void USDMeshReader::read_uv_data_primvar(Mesh *mesh,
                                         const pxr::UsdGeomPrimvar &primvar,
                                         const double motionSampleTime)
{
  const StringRef primvar_name(
      pxr::UsdGeomPrimvar::StripPrimvarsName(primvar.GetName()).GetString());

  pxr::VtArray<pxr::GfVec2f> usd_uvs = get_primvar_array<pxr::GfVec2f>(primvar, motionSampleTime);
  if (usd_uvs.empty()) {
    return;
  }

  const pxr::TfToken varying_type = primvar.GetInterpolation();
  BLI_assert(ELEM(varying_type,
                  pxr::UsdGeomTokens->vertex,
                  pxr::UsdGeomTokens->faceVarying,
                  pxr::UsdGeomTokens->varying));

  if ((varying_type == pxr::UsdGeomTokens->faceVarying && usd_uvs.size() != mesh->corners_num) ||
      (varying_type == pxr::UsdGeomTokens->vertex && usd_uvs.size() != mesh->verts_num) ||
      (varying_type == pxr::UsdGeomTokens->varying && usd_uvs.size() != mesh->verts_num))
  {
    BKE_reportf(reports(),
                RPT_WARNING,
                "USD Import: UV attribute value '%s' count inconsistent with interpolation type",
                primvar.GetName().GetText());
    return;
  }

  bke::MutableAttributeAccessor attributes = mesh->attributes_for_write();
  bke::SpanAttributeWriter<float2> uv_data = attributes.lookup_or_add_for_write_only_span<float2>(
      primvar_name, bke::AttrDomain::Corner);

  if (!uv_data) {
    BKE_reportf(reports(),
                RPT_WARNING,
                "USD Import: couldn't add UV attribute '%s'",
                primvar.GetBaseName().GetText());
    return;
  }

  if (varying_type == pxr::UsdGeomTokens->faceVarying) {
    if (is_left_handed_) {
      /* Reverse the index order. */
      const OffsetIndices faces = mesh->faces();
      for (const int i : faces.index_range()) {
        const IndexRange face = faces[i];
        for (int j : face.index_range()) {
          const int rev_index = face.last(j);
          uv_data.span[face.start() + j] = float2(usd_uvs[rev_index][0], usd_uvs[rev_index][1]);
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
    BLI_assert(mesh->verts_num == usd_uvs.size());
    for (int i = 0; i < uv_data.span.size(); ++i) {
      /* Get the vertex index for this corner. */
      int vi = corner_verts[i];
      uv_data.span[i] = float2(usd_uvs[vi][0], usd_uvs[vi][1]);
    }
  }

  uv_data.finish();
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
  if (corner_indices.size() > mesh->verts_num) {
    CLOG_WARN(&LOG, "Too many vertex creases for mesh %s", prim_path_.c_str());
    return;
  }

  if (corner_indices.size() != corner_sharpnesses.size()) {
    CLOG_WARN(
        &LOG, "Vertex crease and sharpnesses count mismatch for mesh %s", prim_path_.c_str());
    return;
  }

  bke::MutableAttributeAccessor attributes = mesh->attributes_for_write();
  bke::SpanAttributeWriter creases = attributes.lookup_or_add_for_write_only_span<float>(
      "crease_vert", bke::AttrDomain::Point);

  for (size_t i = 0; i < corner_indices.size(); i++) {
    creases.span[corner_indices[i]] = corner_sharpnesses[i];
  }
  creases.finish();
}

void USDMeshReader::read_velocities(Mesh *mesh, const double motionSampleTime)
{
  pxr::VtVec3fArray velocities;
  mesh_prim_.GetVelocitiesAttr().Get(&velocities, motionSampleTime);

  if (!velocities.empty()) {
    bke::MutableAttributeAccessor attributes = mesh->attributes_for_write();
    bke::SpanAttributeWriter<float3> velocity =
        attributes.lookup_or_add_for_write_only_span<float3>("velocity", bke::AttrDomain::Point);

    Span<pxr::GfVec3f> usd_data(velocities.data(), velocities.size());
    velocity.span.copy_from(usd_data.cast<float3>());
    velocity.finish();
  }
}

void USDMeshReader::process_normals_vertex_varying(Mesh *mesh)
{
  if (normals_.empty()) {
    return;
  }

  if (normals_.size() != mesh->verts_num) {
    CLOG_WARN(&LOG, "Vertex varying normals count mismatch for mesh '%s'", prim_path_.c_str());
    return;
  }

  BLI_STATIC_ASSERT(sizeof(normals_[0]) == sizeof(float3), "Expected float3 normals size");
  BKE_mesh_set_custom_normals_from_verts(mesh, reinterpret_cast<float(*)[3]>(normals_.data()));
}

void USDMeshReader::process_normals_face_varying(Mesh *mesh) const
{
  if (normals_.empty()) {
    return;
  }

  /* Check for normals count mismatches to prevent crashes. */
  if (normals_.size() != mesh->corners_num) {
    CLOG_WARN(&LOG, "Loop normal count mismatch for mesh '%s'", prim_path_.c_str());
    return;
  }

  Array<float3> corner_normals(mesh->corners_num);

  const OffsetIndices faces = mesh->faces();
  for (const int i : faces.index_range()) {
    const IndexRange face = faces[i];
    for (int j : face.index_range()) {
      const int corner = face.start() + j;

      int usd_index = face.start();
      if (is_left_handed_) {
        usd_index += face.size() - 1 - j;
      }
      else {
        usd_index += j;
      }

      corner_normals[corner] = detail::convert_value<pxr::GfVec3f, float3>(normals_[usd_index]);
    }
  }

  BKE_mesh_set_custom_normals(mesh, reinterpret_cast<float(*)[3]>(corner_normals.data()));
}

void USDMeshReader::process_normals_uniform(Mesh *mesh) const
{
  if (normals_.empty()) {
    return;
  }

  /* Check for normals count mismatches to prevent crashes. */
  if (normals_.size() != mesh->faces_num) {
    CLOG_WARN(&LOG, "Uniform normal count mismatch for mesh '%s'", prim_path_.c_str());
    return;
  }

  Array<float3> corner_normals(mesh->corners_num);

  const OffsetIndices faces = mesh->faces();
  for (const int i : faces.index_range()) {
    for (const int corner : faces[i]) {
      corner_normals[corner] = detail::convert_value<pxr::GfVec3f, float3>(normals_[i]);
    }
  }

  BKE_mesh_set_custom_normals(mesh, reinterpret_cast<float(*)[3]>(corner_normals.data()));
}

void USDMeshReader::read_mesh_sample(ImportSettings *settings,
                                     Mesh *mesh,
                                     const double motionSampleTime,
                                     const bool new_mesh)
{
  /* Note that for new meshes we always want to read verts and faces,
   * regardless of the value of the read_flag, to avoid a crash downstream
   * in code that expect this data to be there. */

  if (new_mesh || (settings->read_flag & MOD_MESHSEQ_READ_VERT) != 0) {
    MutableSpan<float3> vert_positions = mesh->vert_positions_for_write();
    vert_positions.copy_from(Span(positions_.data(), positions_.size()).cast<float3>());
    mesh->tag_positions_changed();

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

  /* Process point normals after reading faces. */
  if ((settings->read_flag & MOD_MESHSEQ_READ_VERT) != 0 &&
      normal_interpolation_ == pxr::UsdGeomTokens->vertex)
  {
    process_normals_vertex_varying(mesh);
  }

  /* Custom Data layers. */
  if ((settings->read_flag & MOD_MESHSEQ_READ_VERT) ||
      (settings->read_flag & MOD_MESHSEQ_READ_COLOR) ||
      (settings->read_flag & MOD_MESHSEQ_READ_ATTRIBUTES))
  {
    read_velocities(mesh, motionSampleTime);
    read_custom_data(settings, mesh, motionSampleTime, new_mesh);
  }
}

void USDMeshReader::read_custom_data(const ImportSettings *settings,
                                     Mesh *mesh,
                                     const double motionSampleTime,
                                     const bool new_mesh)
{
  if (!(mesh && mesh_prim_ && mesh->corners_num > 0)) {
    return;
  }

  pxr::UsdGeomPrimvarsAPI pv_api = pxr::UsdGeomPrimvarsAPI(mesh_prim_);
  std::vector<pxr::UsdGeomPrimvar> primvars = pv_api.GetPrimvarsWithValues();

  pxr::TfToken active_color_name;
  pxr::TfToken active_uv_set_name;

  /* Convert primvars to custom layer data. */
  for (pxr::UsdGeomPrimvar &pv : primvars) {
    if (!pv.HasValue()) {
      BKE_reportf(reports(),
                  RPT_WARNING,
                  "Skipping primvar %s, mesh %s -- no value",
                  pv.GetName().GetText(),
                  &mesh->id.name[2]);
      continue;
    }

    if (!pv.GetAttr().GetTypeName().IsArray()) {
      /* Non-array attributes are technically improper USD. */
      continue;
    }

    const pxr::SdfValueTypeName type = pv.GetTypeName();
    const pxr::TfToken varying_type = pv.GetInterpolation();
    const pxr::TfToken name = pxr::UsdGeomPrimvar::StripPrimvarsName(pv.GetPrimvarName());

    /* To avoid unnecessarily reloading static primvars during animation,
     * early out if not first load and this primvar isn't animated. */
    if (!new_mesh && primvar_varying_map_.contains(name) && !primvar_varying_map_.lookup(name)) {
      continue;
    }

    /* We handle the non-standard primvar:velocity elsewhere. */
    if (ELEM(name, "velocity")) {
      continue;
    }

    if (ELEM(type,
             pxr::SdfValueTypeNames->StringArray,
             pxr::SdfValueTypeNames->QuatfArray,
             pxr::SdfValueTypeNames->QuatdArray,
             pxr::SdfValueTypeNames->QuathArray))
    {
      /* Skip creating known unsupported types, and avoid noisy error prints. */
      continue;
    }

    /* Read Color primvars. */
    if (convert_usd_type_to_blender(type) == CD_PROP_COLOR) {
      if ((settings->read_flag & MOD_MESHSEQ_READ_COLOR) != 0) {
        /* Set the active color name to 'displayColor', if a color primvar
         * with this name exists.  Otherwise, use the name of the first
         * color primvar we find for the active color. */
        if (active_color_name.IsEmpty() || name == usdtokens::displayColor) {
          active_color_name = name;
        }

        read_generic_mesh_primvar(mesh, pv, motionSampleTime, is_left_handed_);
      }
    }

    /* Read UV primvars. */
    else if (ELEM(varying_type,
                  pxr::UsdGeomTokens->vertex,
                  pxr::UsdGeomTokens->faceVarying,
                  pxr::UsdGeomTokens->varying) &&
             convert_usd_type_to_blender(type) == CD_PROP_FLOAT2)
    {
      if ((settings->read_flag & MOD_MESHSEQ_READ_UV) != 0) {
        /* Set the active uv set name to 'st', if a uv set primvar
         * with this name exists.  Otherwise, use the name of the first
         * uv set primvar we find for the active uv set. */
        if (active_uv_set_name.IsEmpty() || name == usdtokens::st) {
          active_uv_set_name = name;
        }
        this->read_uv_data_primvar(mesh, pv, motionSampleTime);
      }
    }

    /* Read all other primvars. */
    else {
      if ((settings->read_flag & MOD_MESHSEQ_READ_ATTRIBUTES) != 0) {
        read_generic_mesh_primvar(mesh, pv, motionSampleTime, is_left_handed_);
      }
    }

    /* Record whether the primvar attribute might be time varying. */
    if (!primvar_varying_map_.contains(name)) {
      bool might_be_time_varying = pv.ValueMightBeTimeVarying();
      primvar_varying_map_.add(name, might_be_time_varying);
      if (might_be_time_varying) {
        is_time_varying_ = true;
      }
    }
  } /* End primvar attribute loop. */

  if (!active_color_name.IsEmpty()) {
    BKE_id_attributes_default_color_set(&mesh->id, active_color_name.GetText());
    BKE_id_attributes_active_color_set(&mesh->id, active_color_name.GetText());
  }

  if (!active_uv_set_name.IsEmpty()) {
    int layer_index = CustomData_get_named_layer_index(
        &mesh->corner_data, CD_PROP_FLOAT2, active_uv_set_name.GetText());
    if (layer_index > -1) {
      CustomData_set_layer_active_index(&mesh->corner_data, CD_PROP_FLOAT2, layer_index);
      CustomData_set_layer_render_index(&mesh->corner_data, CD_PROP_FLOAT2, layer_index);
    }
  }
}

void USDMeshReader::assign_facesets_to_material_indices(double motionSampleTime,
                                                        MutableSpan<int> material_indices,
                                                        blender::Map<pxr::SdfPath, int> *r_mat_map)
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
      pxr::UsdPrim subset_prim = subset.GetPrim();
      pxr::UsdShadeMaterial subset_mtl = utils::compute_bound_material(subset_prim,
                                                                       import_params_.mtl_purpose);
      if (!subset_mtl) {
        continue;
      }

      pxr::SdfPath subset_mtl_path = subset_mtl.GetPath();
      if (subset_mtl_path.IsEmpty()) {
        continue;
      }

      pxr::TfToken element_type;
      subset.GetElementTypeAttr().Get(&element_type, motionSampleTime);
      if (element_type != pxr::UsdGeomTokens->face) {
        CLOG_WARN(&LOG,
                  "UsdGeomSubset '%s' uses unsupported elementType: %s",
                  subset_prim.GetName().GetText(),
                  element_type.GetText());
        continue;
      }

      const int mat_idx = r_mat_map->lookup_or_add(subset_mtl_path, 1 + current_mat++);
      const int max_element_idx = std::max(0, int(material_indices.size() - 1));

      pxr::VtIntArray indices;
      subset.GetIndicesAttr().Get(&indices, motionSampleTime);

      int bad_element_count = 0;
      for (const int element_idx : indices) {
        const int safe_element_idx = std::clamp(element_idx, 0, max_element_idx);
        bad_element_count += (safe_element_idx != element_idx) ? 1 : 0;
        material_indices[safe_element_idx] = mat_idx - 1;
      }

      if (bad_element_count > 0) {
        CLOG_WARN(&LOG,
                  "UsdGeomSubset '%s' contains invalid indices; material assignment may be "
                  "incorrect (%d were out of range)",
                  subset_prim.GetName().GetText(),
                  bad_element_count);
      }
    }
  }

  if (r_mat_map->is_empty()) {
    pxr::UsdShadeMaterial mtl = utils::compute_bound_material(prim_, import_params_.mtl_purpose);
    if (mtl) {
      pxr::SdfPath mtl_path = mtl.GetPath();

      if (!mtl_path.IsEmpty()) {
        r_mat_map->add(mtl.GetPath(), 1);
      }
    }
  }
}

void USDMeshReader::readFaceSetsSample(Main *bmain, Mesh *mesh, const double motionSampleTime)
{
  if (!import_params_.import_materials) {
    return;
  }

  blender::Map<pxr::SdfPath, int> mat_map;

  bke::MutableAttributeAccessor attributes = mesh->attributes_for_write();
  bke::SpanAttributeWriter<int> material_indices = attributes.lookup_or_add_for_write_span<int>(
      "material_index", bke::AttrDomain::Face);
  this->assign_facesets_to_material_indices(motionSampleTime, material_indices.span, &mat_map);
  material_indices.finish();
  /* Build material name map if it's not built yet. */
  if (this->settings_->mat_name_to_mat.is_empty()) {
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
                               const char ** /*r_err_str*/)
{
  if (!mesh_prim_) {
    return existing_mesh;
  }

  mesh_prim_.GetOrientationAttr().Get(&orientation_);
  if (orientation_ == pxr::UsdGeomTokens->leftHanded) {
    is_left_handed_ = true;
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
  }

  read_mesh_sample(
      &settings, active_mesh, params.motion_sample_time, new_mesh || is_initial_load_);

  if (new_mesh) {
    /* Here we assume that the number of materials doesn't change, i.e. that
     * the material slots that were created when the object was loaded from
     * USD are still valid now. */
    if (active_mesh->faces_num != 0 && import_params_.import_materials) {
      blender::Map<pxr::SdfPath, int> mat_map;
      bke::MutableAttributeAccessor attributes = active_mesh->attributes_for_write();
      bke::SpanAttributeWriter<int> material_indices =
          attributes.lookup_or_add_for_write_span<int>("material_index", bke::AttrDomain::Face);
      assign_facesets_to_material_indices(
          params.motion_sample_time, material_indices.span, &mat_map);
      material_indices.finish();
    }
  }

  if (settings.validate_meshes) {
    if (BKE_mesh_validate(active_mesh, false, false)) {
      BKE_reportf(reports(), RPT_INFO, "Fixed mesh for prim: %s", mesh_prim_.GetPath().GetText());
    }
  }

  return active_mesh;
}

void USDMeshReader::read_geometry(bke::GeometrySet &geometry_set,
                                  const USDMeshReadParams params,
                                  const char **r_err_str)
{
  Mesh *existing_mesh = geometry_set.get_mesh_for_write();
  Mesh *new_mesh = read_mesh(existing_mesh, params, r_err_str);

  if (new_mesh != existing_mesh) {
    geometry_set.replace_mesh(new_mesh);
  }
}

std::string USDMeshReader::get_skeleton_path() const
{
  /* Make sure we can apply UsdSkelBindingAPI to the prim.
   * Attempting to apply the API to instance proxies generates
   * a USD error. */
  if (!prim_ || prim_.IsInstanceProxy()) {
    return "";
  }

  pxr::UsdSkelBindingAPI skel_api(prim_);

  if (pxr::UsdSkelSkeleton skel = skel_api.GetInheritedSkeleton()) {
    return skel.GetPath().GetAsString();
  }

  return "";
}

std::optional<XformResult> USDMeshReader::get_local_usd_xform(const float time) const
{
  if (!import_params_.import_skeletons || prim_.IsInstanceProxy()) {
    /* Use the standard transform computation, since we are ignoring
     * skinning data. Note that applying the UsdSkelBinding API to an
     * instance proxy generates a USD error. */
    return USDXformReader::get_local_usd_xform(time);
  }

  pxr::UsdSkelBindingAPI skel_api = pxr::UsdSkelBindingAPI(prim_);
  if (pxr::UsdAttribute xf_attr = skel_api.GetGeomBindTransformAttr()) {
    if (xf_attr.HasAuthoredValue()) {
      pxr::GfMatrix4d bind_xf;
      if (skel_api.GetGeomBindTransformAttr().Get(&bind_xf)) {
        /* The USD bind transform is a matrix of doubles,
         * but we cast it to GfMatrix4f because Blender expects
         * a matrix of floats. Also, we assume the transform
         * is constant over time. */
        return XformResult(pxr::GfMatrix4f(bind_xf), true);
      }
      else {
        BKE_reportf(reports(),
                    RPT_WARNING,
                    "%s: Couldn't compute geom bind transform for %s",
                    __func__,
                    prim_.GetPath().GetAsString().c_str());
      }
    }
  }

  return USDXformReader::get_local_usd_xform(time);
}

}  // namespace blender::io::usd
