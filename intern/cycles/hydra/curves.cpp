/* SPDX-FileCopyrightText: 2022 NVIDIA Corporation
 * SPDX-FileCopyrightText: 2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "hydra/curves.h"
#include "hydra/geometry.inl"
#include "scene/hair.h"

#include <pxr/imaging/hd/extComputationUtils.h>

HDCYCLES_NAMESPACE_OPEN_SCOPE

HdCyclesCurves::HdCyclesCurves(const SdfPath &rprimId
#if PXR_VERSION < 2102
                               ,
                               const SdfPath &instancerId
#endif
                               )
    : HdCyclesGeometry(rprimId
#if PXR_VERSION < 2102
                       ,
                       instancerId
#endif
      )
{
}

HdCyclesCurves::~HdCyclesCurves() {}

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

  rebuild = (_geom->curve_keys_is_modified()) || (_geom->curve_radius_is_modified());
}

void HdCyclesCurves::PopulatePoints(HdSceneDelegate *sceneDelegate)
{
  VtValue value;

  for (const HdExtComputationPrimvarDescriptor &desc :
       sceneDelegate->GetExtComputationPrimvarDescriptors(GetId(), HdInterpolationVertex))
  {
    if (desc.name == HdTokens->points) {
      auto valueStore = HdExtComputationUtils::GetComputedPrimvarValues({desc}, sceneDelegate);
      const auto valueStoreIt = valueStore.find(desc.name);
      if (valueStoreIt != valueStore.end()) {
        value = std::move(valueStoreIt->second);
      }
      break;
    }
  }

  if (value.IsEmpty()) {
    value = GetPrimvar(sceneDelegate, HdTokens->points);
  }

  if (!value.IsHolding<VtVec3fArray>()) {
    TF_WARN("Invalid points data for %s", GetId().GetText());
    return;
  }

  const auto &points = value.UncheckedGet<VtVec3fArray>();

  array<float3> pointsDataCycles;
  pointsDataCycles.reserve(points.size());

  for (const GfVec3f &point : points) {
    pointsDataCycles.push_back_reserved(make_float3(point[0], point[1], point[2]));
  }

  _geom->set_curve_keys(pointsDataCycles);
}

void HdCyclesCurves::PopulateWidths(HdSceneDelegate *sceneDelegate)
{
  VtValue value = GetPrimvar(sceneDelegate, HdTokens->widths);
  const HdInterpolation interpolation = GetPrimvarInterpolation(sceneDelegate, HdTokens->widths);

  if (!value.IsHolding<VtFloatArray>()) {
    TF_WARN("Invalid widths data for %s", GetId().GetText());
    return;
  }

  const auto &widths = value.UncheckedGet<VtFloatArray>();

  array<float> radiusDataCycles;
  radiusDataCycles.reserve(widths.size());

  if (interpolation == HdInterpolationConstant) {
    TF_VERIFY(widths.size() == 1);

    const float constantRadius = widths[0] * 0.5f;

    for (size_t i = 0; i < _geom->num_keys(); ++i) {
      radiusDataCycles.push_back_reserved(constantRadius);
    }
  }
  else if (interpolation == HdInterpolationVertex) {
    TF_VERIFY(widths.size() == _geom->num_keys());

    for (size_t i = 0; i < _geom->num_keys(); ++i) {
      radiusDataCycles.push_back_reserved(widths[i] * 0.5f);
    }
  }

  _geom->set_curve_radius(radiusDataCycles);
}

void HdCyclesCurves::PopulatePrimvars(HdSceneDelegate *sceneDelegate)
{
  Scene *const scene = (Scene *)_geom->get_owner();

  const std::pair<HdInterpolation, AttributeElement> interpolations[] = {
      std::make_pair(HdInterpolationVertex, ATTR_ELEMENT_CURVE_KEY),
      std::make_pair(HdInterpolationVarying, ATTR_ELEMENT_CURVE_KEY),
      std::make_pair(HdInterpolationUniform, ATTR_ELEMENT_CURVE),
      std::make_pair(HdInterpolationConstant, ATTR_ELEMENT_OBJECT),
  };

  for (const auto &interpolation : interpolations) {
    for (const HdPrimvarDescriptor &desc :
         GetPrimvarDescriptors(sceneDelegate, interpolation.first)) {
      // Skip special primvars that are handled separately
      if (desc.name == HdTokens->points || desc.name == HdTokens->widths) {
        continue;
      }

      VtValue value = GetPrimvar(sceneDelegate, desc.name);
      if (value.IsEmpty()) {
        continue;
      }

      const ustring name(desc.name.GetString());

      AttributeStandard std = ATTR_STD_NONE;
      if (desc.role == HdPrimvarRoleTokens->textureCoordinate) {
        std = ATTR_STD_UV;
      }
      else if (desc.name == HdTokens->displayColor &&
               interpolation.first == HdInterpolationConstant) {
        if (value.IsHolding<VtVec3fArray>() && value.GetArraySize() == 1) {
          const GfVec3f color = value.UncheckedGet<VtVec3fArray>()[0];
          _instances[0]->set_color(make_float3(color[0], color[1], color[2]));
        }
      }

      // Skip attributes that are not needed
      if ((std != ATTR_STD_NONE && _geom->need_attribute(scene, std)) ||
          _geom->need_attribute(scene, name))
      {
        ApplyPrimvars(_geom->attributes, name, value, interpolation.second, std);
      }
    }
  }
}

void HdCyclesCurves::PopulateTopology(HdSceneDelegate *sceneDelegate)
{
  // Clear geometry before populating it again with updated topology
  _geom->clear(true);

  HdBasisCurvesTopology topology = GetBasisCurvesTopology(sceneDelegate);

  _geom->reserve_curves(topology.GetNumCurves(), topology.CalculateNeededNumberOfControlPoints());

  const VtIntArray vertCounts = topology.GetCurveVertexCounts();

  for (int curve = 0, key = 0; curve < topology.GetNumCurves(); ++curve) {
    // Always reference shader at index zero, which is the primitive material
    _geom->add_curve(key, 0);

    key += vertCounts[curve];
  }
}

HDCYCLES_NAMESPACE_CLOSE_SCOPE
