/* SPDX-FileCopyrightText: 2023 Nvidia. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_attribute.hh"
#include "BKE_geometry_set.hh"
#include "BKE_lib_id.hh"
#include "BKE_mesh.hh"
#include "BKE_object.hh"
#include "BKE_report.hh"

#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_windowmanager_types.h"

#include "usd_attribute_utils.hh"
#include "usd_mesh_utils.hh"
#include "usd_reader_shape.hh"

#include <pxr/usd/usdGeom/capsule.h>
#include <pxr/usd/usdGeom/cone.h>
#include <pxr/usd/usdGeom/cube.h>
#include <pxr/usd/usdGeom/cylinder.h>
#include <pxr/usd/usdGeom/sphere.h>
#include <pxr/usdImaging/usdImaging/capsuleAdapter.h>
#include <pxr/usdImaging/usdImaging/coneAdapter.h>
#include <pxr/usdImaging/usdImaging/cubeAdapter.h>
#include <pxr/usdImaging/usdImaging/cylinderAdapter.h>
#include <pxr/usdImaging/usdImaging/sphereAdapter.h>

namespace usdtokens {
/* Materials */
static const pxr::TfToken displayColor("displayColor", pxr::TfToken::Immortal);
}  // namespace usdtokens

namespace blender::io::usd {

USDShapeReader::USDShapeReader(const pxr::UsdPrim &prim,
                               const USDImportParams &import_params,
                               const ImportSettings &settings)
    : USDGeomReader(prim, import_params, settings)
{
}

void USDShapeReader::create_object(Main *bmain, double /*motionSampleTime*/)
{
  Mesh *mesh = BKE_mesh_add(bmain, name_.c_str());
  object_ = BKE_object_add_only_object(bmain, OB_MESH, name_.c_str());
  object_->data = mesh;
}

void USDShapeReader::read_object_data(Main *bmain, double motionSampleTime)
{
  const USDMeshReadParams params = create_mesh_read_params(motionSampleTime,
                                                           import_params_.mesh_read_flag);
  Mesh *mesh = (Mesh *)object_->data;
  Mesh *read_mesh = this->read_mesh(mesh, params, nullptr);

  if (read_mesh != mesh) {
    BKE_mesh_nomain_to_mesh(read_mesh, mesh, object_);
    if (is_time_varying()) {
      USDGeomReader::add_cache_modifier();
    }
  }

  USDXformReader::read_object_data(bmain, motionSampleTime);
}

template<typename Adapter>
void USDShapeReader::read_values(const double motionSampleTime,
                                 pxr::VtVec3fArray &positions,
                                 pxr::VtIntArray &face_indices,
                                 pxr::VtIntArray &face_counts) const
{
  Adapter adapter;
  pxr::VtValue points_val = adapter.GetPoints(prim_, motionSampleTime);

  if (points_val.IsHolding<pxr::VtVec3fArray>()) {
    positions = points_val.Get<pxr::VtVec3fArray>();
  }

  pxr::VtValue topology_val = adapter.GetTopology(prim_, pxr::SdfPath(), motionSampleTime);

  if (topology_val.IsHolding<pxr::HdMeshTopology>()) {
    const pxr::HdMeshTopology &topology = topology_val.Get<pxr::HdMeshTopology>();
    face_counts = topology.GetFaceVertexCounts();
    face_indices = topology.GetFaceVertexIndices();
  }
}

bool USDShapeReader::read_mesh_values(double motionSampleTime,
                                      pxr::VtVec3fArray &positions,
                                      pxr::VtIntArray &face_indices,
                                      pxr::VtIntArray &face_counts) const
{
  if (prim_.IsA<pxr::UsdGeomCapsule>()) {
    read_values<pxr::UsdImagingCapsuleAdapter>(
        motionSampleTime, positions, face_indices, face_counts);
    return true;
  }

  if (prim_.IsA<pxr::UsdGeomCylinder>()) {
    read_values<pxr::UsdImagingCylinderAdapter>(
        motionSampleTime, positions, face_indices, face_counts);
    return true;
  }

  if (prim_.IsA<pxr::UsdGeomCone>()) {
    read_values<pxr::UsdImagingConeAdapter>(
        motionSampleTime, positions, face_indices, face_counts);
    return true;
  }

  if (prim_.IsA<pxr::UsdGeomCube>()) {
    read_values<pxr::UsdImagingCubeAdapter>(
        motionSampleTime, positions, face_indices, face_counts);
    return true;
  }

  if (prim_.IsA<pxr::UsdGeomSphere>()) {
    read_values<pxr::UsdImagingSphereAdapter>(
        motionSampleTime, positions, face_indices, face_counts);
    return true;
  }

  BKE_reportf(reports(),
              RPT_ERROR,
              "Unhandled Gprim type: %s (%s)",
              prim_.GetTypeName().GetText(),
              prim_.GetPath().GetText());
  return false;
}

Mesh *USDShapeReader::read_mesh(Mesh *existing_mesh,
                                const USDMeshReadParams params,
                                const char ** /*err_str*/)
{
  pxr::VtIntArray face_indices;
  pxr::VtIntArray face_counts;

  if (!prim_) {
    return existing_mesh;
  }

  /* Should have a good set of data by this point-- copy over. */
  Mesh *active_mesh = mesh_from_prim(existing_mesh, params, face_indices, face_counts);

  if (active_mesh == existing_mesh) {
    return existing_mesh;
  }

  MutableSpan<int> face_offsets = active_mesh->face_offsets_for_write();
  for (const int i : IndexRange(active_mesh->faces_num)) {
    face_offsets[i] = face_counts[i];
  }
  offset_indices::accumulate_counts_to_offsets(face_offsets);

  /* Don't smooth-shade cubes; we're not worrying about sharpness for Gprims. */
  bke::mesh_smooth_set(*active_mesh, !prim_.IsA<pxr::UsdGeomCube>());

  MutableSpan<int> corner_verts = active_mesh->corner_verts_for_write();
  for (const int i : corner_verts.index_range()) {
    corner_verts[i] = face_indices[i];
  }

  bke::mesh_calc_edges(*active_mesh, false, false);
  return active_mesh;
}

void USDShapeReader::read_geometry(bke::GeometrySet &geometry_set,
                                   USDMeshReadParams params,
                                   const char **err_str)
{
  Mesh *existing_mesh = geometry_set.get_mesh_for_write();
  Mesh *new_mesh = read_mesh(existing_mesh, params, err_str);

  if (new_mesh != existing_mesh) {
    geometry_set.replace_mesh(new_mesh);
  }
}

void USDShapeReader::apply_primvars_to_mesh(Mesh *mesh, const double motionSampleTime) const
{
  /* TODO: also handle the displayOpacity primvar. */
  if (!mesh || !prim_) {
    return;
  }

  pxr::UsdGeomPrimvarsAPI pv_api = pxr::UsdGeomPrimvarsAPI(prim_);
  std::vector<pxr::UsdGeomPrimvar> primvars = pv_api.GetPrimvarsWithValues();

  pxr::TfToken active_color_name;

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

    const pxr::TfToken name = pv.StripPrimvarsName(pv.GetPrimvarName());

    /* Skip reading primvars that have been read before and are not time varying. */
    if (primvar_time_varying_map_.contains(name) && !primvar_time_varying_map_.lookup(name)) {
      continue;
    }

    const pxr::SdfValueTypeName sdf_type = pv.GetTypeName();

    const std::optional<eCustomDataType> type = convert_usd_type_to_blender(sdf_type);
    if (type == CD_PROP_COLOR) {
      /* Set the active color name to 'displayColor', if a color primvar
       * with this name exists.  Otherwise, use the name of the first
       * color primvar we find for the active color. */
      if (active_color_name.IsEmpty() || name == usdtokens::displayColor) {
        active_color_name = name;
      }

      read_color_data_primvar(mesh, pv, motionSampleTime, reports(), false);

      /* Record whether the primvar attribute might be time varying. */
      if (!primvar_time_varying_map_.contains(name)) {
        primvar_time_varying_map_.add(name, pv.ValueMightBeTimeVarying());
      }
    }
  }

  if (!active_color_name.IsEmpty()) {
    BKE_id_attributes_default_color_set(&mesh->id, active_color_name.GetText());
    BKE_id_attributes_active_color_set(&mesh->id, active_color_name.GetText());
  }
}

Mesh *USDShapeReader::mesh_from_prim(Mesh *existing_mesh,
                                     const USDMeshReadParams params,
                                     pxr::VtIntArray &face_indices,
                                     pxr::VtIntArray &face_counts) const
{
  pxr::VtVec3fArray positions;

  if (!read_mesh_values(params.motion_sample_time, positions, face_indices, face_counts)) {
    return existing_mesh;
  }

  const bool poly_counts_match = existing_mesh ? face_counts.size() == existing_mesh->faces_num :
                                                 false;
  const bool position_counts_match = existing_mesh ? positions.size() == existing_mesh->verts_num :
                                                     false;

  Mesh *active_mesh = nullptr;
  if (!position_counts_match || !poly_counts_match) {
    active_mesh = BKE_mesh_new_nomain_from_template(
        existing_mesh, positions.size(), 0, face_counts.size(), face_indices.size());
  }
  else {
    active_mesh = existing_mesh;
  }

  MutableSpan<float3> vert_positions = active_mesh->vert_positions_for_write();

  for (int i = 0; i < positions.size(); i++) {
    vert_positions[i][0] = positions[i][0];
    vert_positions[i][1] = positions[i][1];
    vert_positions[i][2] = positions[i][2];
  }

  if (params.read_flags & MOD_MESHSEQ_READ_COLOR) {
    if (active_mesh != existing_mesh) {
      /* Clear the primvar map to force attributes to be reloaded. */
      this->primvar_time_varying_map_.clear();
    }
    apply_primvars_to_mesh(active_mesh, params.motion_sample_time);
  }

  return active_mesh;
}

bool USDShapeReader::is_time_varying()
{
  for (const bool animating_flag : primvar_time_varying_map_.values()) {
    if (animating_flag) {
      return true;
    }
  }

  if (prim_.IsA<pxr::UsdGeomCapsule>()) {
    pxr::UsdGeomCapsule geom(prim_);
    return (geom.GetAxisAttr().ValueMightBeTimeVarying() ||
            geom.GetHeightAttr().ValueMightBeTimeVarying() ||
            geom.GetRadiusAttr().ValueMightBeTimeVarying());
  }

  if (prim_.IsA<pxr::UsdGeomCylinder>()) {
    pxr::UsdGeomCylinder geom(prim_);
    return (geom.GetAxisAttr().ValueMightBeTimeVarying() ||
            geom.GetHeightAttr().ValueMightBeTimeVarying() ||
            geom.GetRadiusAttr().ValueMightBeTimeVarying());
  }

  if (prim_.IsA<pxr::UsdGeomCone>()) {
    pxr::UsdGeomCone geom(prim_);
    return (geom.GetAxisAttr().ValueMightBeTimeVarying() ||
            geom.GetHeightAttr().ValueMightBeTimeVarying() ||
            geom.GetRadiusAttr().ValueMightBeTimeVarying());
  }

  if (prim_.IsA<pxr::UsdGeomCube>()) {
    pxr::UsdGeomCube geom(prim_);
    return geom.GetSizeAttr().ValueMightBeTimeVarying();
  }

  if (prim_.IsA<pxr::UsdGeomSphere>()) {
    pxr::UsdGeomSphere geom(prim_);
    return geom.GetRadiusAttr().ValueMightBeTimeVarying();
  }

  BKE_reportf(reports(),
              RPT_ERROR,
              "Unhandled Gprim type: %s (%s)",
              prim_.GetTypeName().GetText(),
              prim_.GetPath().GetText());
  return false;
}

}  // namespace blender::io::usd
