/* SPDX-FileCopyrightText: 2022 NVIDIA Corporation
 * SPDX-FileCopyrightText: 2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "hydra/pointcloud.h"
#include "hydra/geometry.inl"
#include "scene/pointcloud.h"

#include <pxr/imaging/hd/extComputationUtils.h>

HDCYCLES_NAMESPACE_OPEN_SCOPE

HdCyclesPoints::HdCyclesPoints(const SdfPath &rprimId
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

HdCyclesPoints::~HdCyclesPoints() {}

HdDirtyBits HdCyclesPoints::GetInitialDirtyBitsMask() const
{
  HdDirtyBits bits = HdCyclesGeometry::GetInitialDirtyBitsMask();
  bits |= HdChangeTracker::DirtyPoints | HdChangeTracker::DirtyWidths |
          HdChangeTracker::DirtyPrimvar;
  return bits;
}

HdDirtyBits HdCyclesPoints::_PropagateDirtyBits(HdDirtyBits bits) const
{
  // Points and widths always have to be updated together
  if (bits & (HdChangeTracker::DirtyPoints | HdChangeTracker::DirtyWidths)) {
    bits |= HdChangeTracker::DirtyPoints | HdChangeTracker::DirtyWidths;
  }

  return bits;
}

void HdCyclesPoints::Populate(HdSceneDelegate *sceneDelegate, HdDirtyBits dirtyBits, bool &rebuild)
{
  if (dirtyBits & (HdChangeTracker::DirtyPoints | HdChangeTracker::DirtyWidths)) {
    const size_t numPoints = _geom->num_points();

    PopulatePoints(sceneDelegate);
    PopulateWidths(sceneDelegate);

    rebuild = _geom->num_points() != numPoints;

    array<int> shaders;
    shaders.reserve(_geom->num_points());
    for (size_t i = 0; i < _geom->num_points(); ++i) {
      shaders.push_back_reserved(0);
    }

    _geom->set_shader(shaders);
  }

  if (dirtyBits & HdChangeTracker::DirtyPrimvar) {
    PopulatePrimvars(sceneDelegate);
  }
}

void HdCyclesPoints::PopulatePoints(HdSceneDelegate *sceneDelegate)
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

  _geom->set_points(pointsDataCycles);
}

void HdCyclesPoints::PopulateWidths(HdSceneDelegate *sceneDelegate)
{
  VtValue value = GetPrimvar(sceneDelegate, HdTokens->widths);
  const HdInterpolation interpolation = GetPrimvarInterpolation(sceneDelegate, HdTokens->widths);

  if (!value.IsHolding<VtFloatArray>()) {
    TF_WARN("Invalid widths data for %s", GetId().GetText());
    return;
  }

  const auto &widths = value.UncheckedGet<VtFloatArray>();

  array<float> radiusDataCycles;
  radiusDataCycles.reserve(_geom->num_points());

  if (interpolation == HdInterpolationConstant) {
    TF_VERIFY(widths.size() == 1);

    const float constantRadius = widths[0] * 0.5f;

    for (size_t i = 0; i < _geom->num_points(); ++i) {
      radiusDataCycles.push_back_reserved(constantRadius);
    }
  }
  else if (interpolation == HdInterpolationVertex) {
    TF_VERIFY(widths.size() == _geom->num_points());

    for (size_t i = 0; i < _geom->num_points(); ++i) {
      radiusDataCycles.push_back_reserved(widths[i] * 0.5f);
    }
  }

  _geom->set_radius(radiusDataCycles);
}

void HdCyclesPoints::PopulatePrimvars(HdSceneDelegate *sceneDelegate)
{
  Scene *const scene = (Scene *)_geom->get_owner();

  const std::pair<HdInterpolation, AttributeElement> interpolations[] = {
      std::make_pair(HdInterpolationVertex, ATTR_ELEMENT_VERTEX),
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
      else if (interpolation.first == HdInterpolationVertex) {
        if (desc.name == HdTokens->displayColor || desc.role == HdPrimvarRoleTokens->color) {
          std = ATTR_STD_VERTEX_COLOR;
        }
        else if (desc.name == HdTokens->normals) {
          std = ATTR_STD_VERTEX_NORMAL;
        }
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

HDCYCLES_NAMESPACE_CLOSE_SCOPE
