/* SPDX-FileCopyrightText: 2022 NVIDIA Corporation
 * SPDX-FileCopyrightText: 2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "hydra/curves.h"
#include "hydra/geometry.inl"
#include "hydra/util.h"
#include "scene/hair.h"
#include "util/types_float3.h"

#include <pxr/imaging/hd/basisCurvesSchema.h>
#include <pxr/imaging/hd/basisCurvesTopologySchema.h>

HDCYCLES_NAMESPACE_OPEN_SCOPE

HdCyclesCurves::HdCyclesCurves(const SdfPath &rprimId) : HdCyclesGeometry(rprimId) {}

HdCyclesCurves::~HdCyclesCurves() = default;

HdDirtyBits HdCyclesCurves::GetInitialDirtyBitsMask() const
{
  HdDirtyBits bits = HdCyclesGeometry::GetInitialDirtyBitsMask();
  bits |= HdChangeTracker::DirtyPoints | HdChangeTracker::DirtyWidths |
          HdChangeTracker::DirtyPrimvar | HdChangeTracker::DirtyTopology;
  return bits;
}

HdDirtyBits HdCyclesCurves::_PropagateDirtyBits(HdDirtyBits bits) const
{
  if (bits & (HdChangeTracker::DirtyTopology)) {
    // Changing topology clears the geometry, so need to populate everything again
    bits |= HdChangeTracker::DirtyPoints | HdChangeTracker::DirtyWidths |
            HdChangeTracker::DirtyPrimvar;
  }

  return bits;
}

void HdCyclesCurves::Populate(HdSceneDelegate *sceneDelegate, HdDirtyBits dirtyBits, bool &rebuild)
{
  if (HdChangeTracker::IsTopologyDirty(dirtyBits, GetId())) {
    PopulateTopology(sceneDelegate);
  }

  if (dirtyBits & HdChangeTracker::DirtyPoints) {
    PopulatePoints(sceneDelegate);
  }

  if (dirtyBits & HdChangeTracker::DirtyWidths) {
    PopulateWidths(sceneDelegate);
  }

  if (dirtyBits & HdChangeTracker::DirtyPrimvar) {
    PopulatePrimvars(sceneDelegate);
  }

  rebuild = (_geom->position_is_modified()) || (_geom->radius_is_modified());
}

void HdCyclesCurves::PopulatePoints(HdSceneDelegate *sceneDelegate)
{
  const HdSceneIndexPrim prim = GetPrim(sceneDelegate, GetId());
  const HdPrimvarsSchema primvars = HdPrimvarsSchema::GetFromParent(prim.dataSource);
  const VtValue value = ReadPrimvar(primvars, HdTokens->points);

  if (!value.IsHolding<VtVec3fArray>()) {
    TF_WARN("Invalid points data for %s", GetId().GetText());
    return;
  }

  const auto &points = value.UncheckedGet<VtVec3fArray>();

  TF_VERIFY(points.size() >= _geom->num_keys());

  static_assert(sizeof(GfVec3f) == sizeof(packed_float3));

  std::copy_n(reinterpret_cast<const packed_float3 *>(points.data()),
              std::min(points.size(), _geom->num_keys()),
              _geom->get_position_for_write());
}

void HdCyclesCurves::PopulateWidths(HdSceneDelegate *sceneDelegate)
{
  const HdSceneIndexPrim prim = GetPrim(sceneDelegate, GetId());
  const HdPrimvarsSchema primvars = HdPrimvarsSchema::GetFromParent(prim.dataSource);
  const VtValue value = ReadPrimvar(primvars, HdTokens->widths);
  const HdInterpolation interpolation = ReadPrimvarInterpolation(primvars, HdTokens->widths);

  if (!value.IsHolding<VtFloatArray>()) {
    TF_WARN("Invalid widths data for %s", GetId().GetText());
    return;
  }

  const auto &widths = value.UncheckedGet<VtFloatArray>();
  float *radius = _geom->get_radius_for_write();

  if (interpolation == HdInterpolationConstant) {
    TF_VERIFY(widths.size() == 1);

    const float constantRadius = widths[0] * 0.5f;

    for (size_t i = 0; i < _geom->num_keys(); ++i) {
      radius[i] = constantRadius;
    }
  }
  else if (interpolation == HdInterpolationVertex) {
    TF_VERIFY(widths.size() == _geom->num_keys());

    for (size_t i = 0; i < _geom->num_keys(); ++i) {
      radius[i] = widths[i] * 0.5f;
    }
  }
}

void HdCyclesCurves::PopulatePrimvars(HdSceneDelegate *sceneDelegate)
{
  Scene *const scene = (Scene *)_geom->get_owner();

  const HdSceneIndexPrim prim = GetPrim(sceneDelegate, GetId());
  const HdPrimvarsSchema primvars = HdPrimvarsSchema::GetFromParent(prim.dataSource);

  const std::pair<HdInterpolation, AttributeElement> interpolations[] = {
      std::make_pair(HdInterpolationVertex, ATTR_ELEMENT_CURVE_KEY),
      std::make_pair(HdInterpolationVarying, ATTR_ELEMENT_CURVE_KEY),
      std::make_pair(HdInterpolationUniform, ATTR_ELEMENT_CURVE),
      std::make_pair(HdInterpolationConstant, ATTR_ELEMENT_OBJECT),
  };

  for (const auto &interpolation : interpolations) {
    for (const TfToken &primvarName : PrimvarNamesAtInterpolation(primvars, interpolation.first)) {
      // Skip special primvars that are handled separately
      if (primvarName == HdTokens->points || primvarName == HdTokens->widths) {
        continue;
      }

      const VtValue value = ReadPrimvar(primvars, primvarName);
      if (value.IsEmpty()) {
        continue;
      }

      const TfToken role = ReadPrimvarRole(primvars, primvarName);
      const ustring name(primvarName.GetString());

      AttributeStandard std = ATTR_STD_NONE;
      if (role == HdPrimvarRoleTokens->textureCoordinate) {
        std = ATTR_STD_UV;
      }
      else if (primvarName == HdTokens->normals && interpolation.first == HdInterpolationVertex) {
        std = ATTR_STD_VERTEX_NORMAL;
      }
      else if (primvarName == HdTokens->displayColor &&
               interpolation.first == HdInterpolationConstant)
      {
        if (value.IsHolding<VtVec3fArray>() && value.GetArraySize() == 1) {
          const GfVec3f color = value.UncheckedGet<VtVec3fArray>()[0];
          _instances[0]->set_color(make_float3(color[0], color[1], color[2]));
        }
      }

      // Skip attributes that are not needed
      if ((std != ATTR_STD_NONE && _geom->need_attribute(scene, std)) ||
          _geom->need_attribute(scene, name))
      {
        AttributeElement elem = interpolation.second;
        if (std == ATTR_STD_VERTEX_NORMAL) {
          elem = ATTR_ELEMENT_CURVE_KEY_NORMAL;
        }
        ApplyPrimvars(_geom->attributes, name, value, elem, std);
      }
    }
  }
}

void HdCyclesCurves::PopulateTopology(HdSceneDelegate *sceneDelegate)
{
  // Clear geometry before populating it again with updated topology
  _geom->clear(true);

  const HdSceneIndexPrim prim = GetPrim(sceneDelegate, GetId());
  const HdBasisCurvesTopologySchema topoSchema =
      HdBasisCurvesSchema::GetFromParent(prim.dataSource).GetTopology();

  TfToken curveType = HdTokens->linear;
  if (auto ds = topoSchema.GetType()) {
    curveType = ds->GetTypedValue(0.0f);
  }
  TfToken curveBasis = HdTokens->bezier;
  if (auto ds = topoSchema.GetBasis()) {
    curveBasis = ds->GetTypedValue(0.0f);
  }
  TfToken curveWrap = HdTokens->nonperiodic;
  if (auto ds = topoSchema.GetWrap()) {
    curveWrap = ds->GetTypedValue(0.0f);
  }
  VtIntArray curveVertexCounts;
  if (auto ds = topoSchema.GetCurveVertexCounts()) {
    curveVertexCounts = ds->GetTypedValue(0.0f);
  }
  VtIntArray curveIndices;
  if (auto ds = topoSchema.GetCurveIndices()) {
    curveIndices = ds->GetTypedValue(0.0f);
  }
  const HdBasisCurvesTopology topology(
      curveType, curveBasis, curveWrap, curveVertexCounts, curveIndices);

  _geom->resize_curves(topology.GetNumCurves(), topology.CalculateNeededNumberOfControlPoints());

  const VtIntArray vertCounts = topology.GetCurveVertexCounts();

  int *curve_first_key = _geom->get_curve_first_key().data();

  for (int curve = 0, key = 0; curve < topology.GetNumCurves(); ++curve) {
    // Always reference shader at index zero, which is the primitive material
    curve_first_key[curve] = key;

    key += vertCounts[curve];
  }

  std::ranges::fill(_geom->get_curve_shader(), 0);

  _geom->tag_curve_first_key_modified();
  _geom->tag_curve_shader_modified();
}

HDCYCLES_NAMESPACE_CLOSE_SCOPE
