/* SPDX-License-Identifier: GPL-2.0-or-later
 * SPDX-FileCopyrightText: 2011-2022 Blender Foundation */

#include "curves.h"

#include <pxr/base/gf/vec2f.h>
#include <pxr/imaging/hd/tokens.h>

#include "BKE_customdata.h"
#include "BKE_material.h"

#include "BKE_curves.hh"

#include "hydra_scene_delegate.h"

namespace blender::io::hydra {

CurvesData::CurvesData(HydraSceneDelegate *scene_delegate,
                       const Object *object,
                       pxr::SdfPath const &prim_id)
    : ObjectData(scene_delegate, object, prim_id)
{
}

void CurvesData::init()
{
  ID_LOGN(1, "");

  const Object *object = (const Object *)id;
  write_curves((const Curves *)object->data);
  write_transform();
  write_materials();
}

void CurvesData::insert()
{
  ID_LOGN(1, "");
  scene_delegate_->GetRenderIndex().InsertRprim(
      pxr::HdPrimTypeTokens->basisCurves, scene_delegate_, prim_id);
}

void CurvesData::remove()
{
  ID_LOG(1, "");
  scene_delegate_->GetRenderIndex().RemoveRprim(prim_id);
}

void CurvesData::update()
{
  const Object *object = (const Object *)id;
  pxr::HdDirtyBits bits = pxr::HdChangeTracker::Clean;
  if ((id->recalc & ID_RECALC_GEOMETRY) || (((ID *)object->data)->recalc & ID_RECALC_GEOMETRY)) {
    init();
    bits = pxr::HdChangeTracker::AllDirty;
  }
  if (id->recalc & ID_RECALC_SHADING) {
    write_materials();
    bits |= pxr::HdChangeTracker::DirtyMaterialId | pxr::HdChangeTracker::DirtyDoubleSided;
  }
  if (id->recalc & ID_RECALC_TRANSFORM) {
    write_transform();
    bits |= pxr::HdChangeTracker::DirtyTransform;
  }

  if (bits == pxr::HdChangeTracker::Clean) {
    return;
  }

  scene_delegate_->GetRenderIndex().GetChangeTracker().MarkRprimDirty(prim_id, bits);
  ID_LOGN(1, "");
}

pxr::VtValue CurvesData::get_data(pxr::TfToken const &key) const
{
  if (key == pxr::HdTokens->points) {
    return pxr::VtValue(vertices_);
  }
  else if (key == pxr::HdPrimvarRoleTokens->textureCoordinate) {
    return pxr::VtValue(uvs_);
  }
  else if (key == pxr::HdTokens->widths) {
    return pxr::VtValue(widths_);
  }
  return pxr::VtValue();
}

pxr::SdfPath CurvesData::material_id() const
{
  if (!mat_data_) {
    return pxr::SdfPath();
  }
  return mat_data_->prim_id;
}

void CurvesData::available_materials(Set<pxr::SdfPath> &paths) const
{
  if (mat_data_ && !mat_data_->prim_id.IsEmpty()) {
    paths.add(mat_data_->prim_id);
  }
}

pxr::HdBasisCurvesTopology CurvesData::topology() const
{
  return pxr::HdBasisCurvesTopology(pxr::HdTokens->linear,
                                    pxr::TfToken(),
                                    pxr::HdTokens->nonperiodic,
                                    curve_vertex_counts_,
                                    pxr::VtIntArray());
}

pxr::HdPrimvarDescriptorVector CurvesData::primvar_descriptors(
    pxr::HdInterpolation interpolation) const
{
  pxr::HdPrimvarDescriptorVector primvars;
  if (interpolation == pxr::HdInterpolationVertex) {
    if (!vertices_.empty()) {
      primvars.emplace_back(pxr::HdTokens->points, interpolation, pxr::HdPrimvarRoleTokens->point);
    }
    if (!widths_.empty()) {
      primvars.emplace_back(pxr::HdTokens->widths, interpolation, pxr::HdPrimvarRoleTokens->none);
    }
  }
  else if (interpolation == pxr::HdInterpolationConstant) {
    if (!uvs_.empty()) {
      primvars.emplace_back(pxr::HdPrimvarRoleTokens->textureCoordinate,
                            interpolation,
                            pxr::HdPrimvarRoleTokens->textureCoordinate);
    }
  }
  return primvars;
}

void CurvesData::write_materials()
{
  const Object *object = (const Object *)id;
  const Material *mat = nullptr;
  /* TODO: Using only first material. Add support for multimaterial. */
  if (BKE_object_material_count_eval(object) > 0) {
    mat = BKE_object_material_get_eval(const_cast<Object *>(object), 0);
  }
  mat_data_ = get_or_create_material(mat);
}

void CurvesData::write_curves(const Curves *curves)
{
  curve_vertex_counts_.clear();
  widths_.clear();
  vertices_.clear();

  const float *radii = (const float *)CustomData_get_layer_named(
      &curves->geometry.point_data, CD_PROP_FLOAT, "radius");
  const float(*positions)[3] = (const float(*)[3])CustomData_get_layer_named(
      &curves->geometry.point_data, CD_PROP_FLOAT3, "position");

  vertices_.reserve(curves->geometry.curve_num);

  for (int i = 0; i < curves->geometry.curve_num; i++) {
    int first_point_index = *(curves->geometry.curve_offsets + i);
    int num_points = *(curves->geometry.curve_offsets + i + 1) - first_point_index;
    curve_vertex_counts_.push_back(num_points);

    /* Set radius similar to Cycles if isn't set */
    for (int j = 0; j < num_points; j++) {
      int ind = first_point_index + j;
      widths_.push_back(radii ? radii[ind] * 2 : 0.01f);
      vertices_.push_back(pxr::GfVec3f(positions[ind][0], positions[ind][1], positions[ind][2]));
    }
  }
  write_uv_maps(curves);
}

void CurvesData::write_uv_maps(const Curves *curves)
{
  uvs_.clear();

  const float(*uvs)[2] = (const float(*)[2])CustomData_get_layer_named(
      &curves->geometry.curve_data, CD_PROP_FLOAT2, "surface_uv_coordinate");
  if (uvs) {
    for (int i = 0; i < curves->geometry.curve_num; i++) {
      uvs_.push_back(pxr::GfVec2f(uvs[i][0], uvs[i][1]));
    }
  }
}

}  // namespace blender::io::hydra
