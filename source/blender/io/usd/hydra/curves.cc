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
  /* TODO: Using only first material. Add support for multi-material. */
  if (BKE_object_material_count_eval(object) > 0) {
    mat = BKE_object_material_get_eval(const_cast<Object *>(object), 0);
  }
  mat_data_ = get_or_create_material(mat);
}

void CurvesData::write_curves(const Curves *curves_id)
{
  const bke::CurvesGeometry &curves = curves_id->geometry.wrap();

  curve_vertex_counts_.resize(curves.curves_num());
  offset_indices::copy_group_sizes(
      curves.points_by_curve(),
      curves.curves_range(),
      MutableSpan(curve_vertex_counts_.data(), curve_vertex_counts_.size()));

  const Span<float3> positions = curves.positions();
  vertices_.resize(curves.points_num());
  MutableSpan(vertices_.data(), vertices_.size()).copy_from(positions.cast<pxr::GfVec3f>());

  const VArray<float> radii = *curves.attributes().lookup_or_default<float>(
      "radius", ATTR_DOMAIN_POINT, 0.01f);
  widths_.resize(curves.points_num());
  for (const int i : curves.points_range()) {
    widths_[i] = radii[i] * 2.0f;
  }

  write_uv_maps(curves_id);
}

void CurvesData::write_uv_maps(const Curves *curves_id)
{
  const bke::CurvesGeometry &curves = curves_id->geometry.wrap();
  const Span<float2> surface_uv_coords = curves.surface_uv_coords();
  if (surface_uv_coords.is_empty()) {
    uvs_.clear();
    return;
  }

  uvs_.resize(curves.curves_num());
  MutableSpan(uvs_.data(), uvs_.size()).copy_from(surface_uv_coords.cast<pxr::GfVec2f>());
}

}  // namespace blender::io::hydra
