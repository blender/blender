/* SPDX-FileCopyrightText: 2023 Nvidia. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_attribute.h"
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
#include <pxr/usd/usdGeom/capsule_1.h>
#include <pxr/usd/usdGeom/cone.h>
#include <pxr/usd/usdGeom/cube.h>
#include <pxr/usd/usdGeom/cylinder.h>
#include <pxr/usd/usdGeom/cylinder_1.h>
#include <pxr/usd/usdGeom/plane.h>
#include <pxr/usd/usdGeom/sphere.h>
#include <pxr/usdImaging/usdImaging/capsuleAdapter.h>
#include <pxr/usdImaging/usdImaging/coneAdapter.h>
#include <pxr/usdImaging/usdImaging/cubeAdapter.h>
#include <pxr/usdImaging/usdImaging/cylinderAdapter.h>
#include <pxr/usdImaging/usdImaging/planeAdapter.h>
#include <pxr/usdImaging/usdImaging/sphereAdapter.h>

namespace blender::io::usd {

USDShapeReader::USDShapeReader(const pxr::UsdPrim &prim,
                               const USDImportParams &import_params,
                               const ImportSettings &settings)
    : USDGeomReader(prim, import_params, settings)
{
}

void USDShapeReader::create_object(Main *bmain)
{
  Mesh *mesh = BKE_mesh_add(bmain, name_.c_str());
  object_ = BKE_object_add_only_object(bmain, OB_MESH, name_.c_str());
  object_->data = mesh;
}

void USDShapeReader::read_object_data(Main *bmain, pxr::UsdTimeCode time)
{
  const USDMeshReadParams params = create_mesh_read_params(time.GetValue(),
                                                           import_params_.mesh_read_flag);
  Mesh *mesh = (Mesh *)object_->data;
  Mesh *read_mesh = this->read_mesh(mesh, params, nullptr);

  if (read_mesh != mesh) {
    BKE_mesh_nomain_to_mesh(read_mesh, mesh, object_);
    if (is_time_varying()) {
      USDGeomReader::add_cache_modifier();
    }
  }

  USDXformReader::read_object_data(bmain, time);
}

template<typename Adapter>
void USDShapeReader::read_values(const pxr::UsdTimeCode time,
                                 pxr::VtVec3fArray &positions,
                                 pxr::VtIntArray &face_indices,
                                 pxr::VtIntArray &face_counts) const
{
  Adapter adapter;
  pxr::VtValue points_val = adapter.GetPoints(prim_, time);

  if (points_val.IsHolding<pxr::VtVec3fArray>()) {
    positions = points_val.UncheckedGet<pxr::VtVec3fArray>();
  }

  pxr::VtValue topology_val = adapter.GetTopology(prim_, pxr::SdfPath(), time);

  if (topology_val.IsHolding<pxr::HdMeshTopology>()) {
    const pxr::HdMeshTopology &topology = topology_val.UncheckedGet<pxr::HdMeshTopology>();
    face_counts = topology.GetFaceVertexCounts();
    face_indices = topology.GetFaceVertexIndices();
  }
}

bool USDShapeReader::read_mesh_values(pxr::UsdTimeCode time,
                                      pxr::VtVec3fArray &positions,
                                      pxr::VtIntArray &face_indices,
                                      pxr::VtIntArray &face_counts) const
{
  if (prim_.IsA<pxr::UsdGeomCapsule>() || prim_.IsA<pxr::UsdGeomCapsule_1>()) {
    read_values<pxr::UsdImagingCapsuleAdapter>(time, positions, face_indices, face_counts);
    return true;
  }

  if (prim_.IsA<pxr::UsdGeomCylinder>() || prim_.IsA<pxr::UsdGeomCylinder_1>()) {
    read_values<pxr::UsdImagingCylinderAdapter>(time, positions, face_indices, face_counts);
    return true;
  }

  if (prim_.IsA<pxr::UsdGeomCone>()) {
    read_values<pxr::UsdImagingConeAdapter>(time, positions, face_indices, face_counts);
    return true;
  }

  if (prim_.IsA<pxr::UsdGeomCube>()) {
    read_values<pxr::UsdImagingCubeAdapter>(time, positions, face_indices, face_counts);
    return true;
  }

  if (prim_.IsA<pxr::UsdGeomSphere>()) {
    read_values<pxr::UsdImagingSphereAdapter>(time, positions, face_indices, face_counts);
    return true;
  }

  if (prim_.IsA<pxr::UsdGeomPlane>()) {
    read_values<pxr::UsdImagingPlaneAdapter>(time, positions, face_indices, face_counts);
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
                                const char ** /*r_err_str*/)
{
  if (!prim_) {
    return existing_mesh;
  }

  pxr::VtIntArray usd_face_indices;
  pxr::VtIntArray usd_face_counts;

  /* Should have a good set of data by this point-- copy over. */
  Mesh *active_mesh = mesh_from_prim(existing_mesh, params, usd_face_indices, usd_face_counts);

  if (active_mesh == existing_mesh) {
    return existing_mesh;
  }

  Span<int> face_indices = Span(usd_face_indices.cdata(), usd_face_indices.size());
  Span<int> face_counts = Span(usd_face_counts.cdata(), usd_face_counts.size());

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
                                   const char **r_err_str)
{
  Mesh *existing_mesh = geometry_set.get_mesh_for_write();
  Mesh *new_mesh = read_mesh(existing_mesh, params, r_err_str);

  if (new_mesh != existing_mesh) {
    geometry_set.replace_mesh(new_mesh);
  }
}

void USDShapeReader::apply_primvars_to_mesh(Mesh *mesh, const pxr::UsdTimeCode time) const
{
  /* TODO: also handle the displayOpacity primvar. */
  if (!mesh || !prim_) {
    return;
  }

  pxr::UsdGeomPrimvarsAPI pv_api = pxr::UsdGeomPrimvarsAPI(prim_);
  std::vector<pxr::UsdGeomPrimvar> primvars = pv_api.GetPrimvarsWithValues();

  pxr::TfToken active_color_name;

  for (const pxr::UsdGeomPrimvar &pv : primvars) {
    const pxr::SdfValueTypeName pv_type = pv.GetTypeName();
    if (!pv_type.IsArray()) {
      continue; /* Skip non-array primvar attributes. */
    }

    const pxr::TfToken name = pxr::UsdGeomPrimvar::StripPrimvarsName(pv.GetPrimvarName());

    /* Skip reading primvars that have been read before and are not time varying. */
    if (primvar_time_varying_map_.contains(name) && !primvar_time_varying_map_.lookup(name)) {
      continue;
    }

    const std::optional<bke::AttrType> type = convert_usd_type_to_blender(pv_type);
    if (type == bke::AttrType::ColorFloat) {
      /* Set the active color name to 'displayColor', if a color primvar
       * with this name exists.  Otherwise, use the name of the first
       * color primvar we find for the active color. */
      if (active_color_name.IsEmpty() || name == usdtokens::displayColor) {
        active_color_name = name;
      }
    }

    read_generic_mesh_primvar(mesh, pv, time, false);

    /* Record whether the primvar attribute might be time varying. */
    if (!primvar_time_varying_map_.contains(name)) {
      primvar_time_varying_map_.add(name, pv.ValueMightBeTimeVarying());
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
  vert_positions.copy_from(Span(positions.cdata(), positions.size()).cast<float3>());

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

  if (prim_.IsA<pxr::UsdGeomCapsule_1>()) {
    pxr::UsdGeomCapsule_1 geom(prim_);
    return (geom.GetAxisAttr().ValueMightBeTimeVarying() ||
            geom.GetHeightAttr().ValueMightBeTimeVarying() ||
            geom.GetRadiusTopAttr().ValueMightBeTimeVarying() ||
            geom.GetRadiusBottomAttr().ValueMightBeTimeVarying());
  }

  if (prim_.IsA<pxr::UsdGeomCylinder>()) {
    pxr::UsdGeomCylinder geom(prim_);
    return (geom.GetAxisAttr().ValueMightBeTimeVarying() ||
            geom.GetHeightAttr().ValueMightBeTimeVarying() ||
            geom.GetRadiusAttr().ValueMightBeTimeVarying());
  }

  if (prim_.IsA<pxr::UsdGeomCylinder_1>()) {
    pxr::UsdGeomCylinder_1 geom(prim_);
    return (geom.GetAxisAttr().ValueMightBeTimeVarying() ||
            geom.GetHeightAttr().ValueMightBeTimeVarying() ||
            geom.GetRadiusTopAttr().ValueMightBeTimeVarying() ||
            geom.GetRadiusBottomAttr().ValueMightBeTimeVarying());
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

  if (prim_.IsA<pxr::UsdGeomPlane>()) {
    pxr::UsdGeomPlane geom(prim_);
    return (geom.GetWidthAttr().ValueMightBeTimeVarying() ||
            geom.GetLengthAttr().ValueMightBeTimeVarying() ||
            geom.GetAxisAttr().ValueMightBeTimeVarying());
  }

  BKE_reportf(reports(),
              RPT_ERROR,
              "Unhandled Gprim type: %s (%s)",
              prim_.GetTypeName().GetText(),
              prim_.GetPath().GetText());
  return false;
}

}  // namespace blender::io::usd
