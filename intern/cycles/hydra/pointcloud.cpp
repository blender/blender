/* SPDX-FileCopyrightText: 2022 NVIDIA Corporation
 * SPDX-FileCopyrightText: 2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "hydra/pointcloud.h"
#include "hydra/geometry.inl"
#include "hydra/util.h"
#include "scene/pointcloud.h"

HDCYCLES_NAMESPACE_OPEN_SCOPE

HdCyclesPoints::HdCyclesPoints(const SdfPath &rprimId) : HdCyclesGeometry(rprimId) {}

HdCyclesPoints::~HdCyclesPoints() = default;

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
  const HdSceneIndexPrim prim = GetPrim(sceneDelegate, GetId());
  const HdPrimvarsSchema primvars = HdPrimvarsSchema::GetFromParent(prim.dataSource);
  const VtValue value = ReadPrimvar(primvars, HdTokens->points);

  if (!value.IsHolding<VtVec3fArray>()) {
    TF_WARN("Invalid points data for %s", GetId().GetText());
    return;
  }

  const auto &points = value.UncheckedGet<VtVec3fArray>();
  static_assert(sizeof(GfVec3f) == sizeof(packed_float3));

  _geom->resize(int(points.size()));

  std::copy_n(reinterpret_cast<const packed_float3 *>(points.data()),
              points.size(),
              _geom->get_position_for_write());
}

void HdCyclesPoints::PopulateWidths(HdSceneDelegate *sceneDelegate)
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

    for (size_t i = 0; i < _geom->num_points(); ++i) {
      radius[i] = constantRadius;
    }
  }
  else if (interpolation == HdInterpolationVertex) {
    TF_VERIFY(widths.size() == _geom->num_points());

    for (size_t i = 0; i < _geom->num_points(); ++i) {
      radius[i] = widths[i] * 0.5f;
    }
  }
}

void HdCyclesPoints::PopulatePrimvars(HdSceneDelegate *sceneDelegate)
{
  Scene *const scene = (Scene *)_geom->get_owner();

  const HdSceneIndexPrim prim = GetPrim(sceneDelegate, GetId());
  const HdPrimvarsSchema primvars = HdPrimvarsSchema::GetFromParent(prim.dataSource);

  const std::pair<HdInterpolation, AttributeElement> interpolations[] = {
      std::make_pair(HdInterpolationVertex, ATTR_ELEMENT_VERTEX),
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
      else if (interpolation.first == HdInterpolationVertex) {
        if (primvarName == HdTokens->displayColor || role == HdPrimvarRoleTokens->color) {
          std = ATTR_STD_VERTEX_COLOR;
        }
        else if (primvarName == HdTokens->normals) {
          std = ATTR_STD_VERTEX_NORMAL;
        }
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
        ApplyPrimvars(_geom->attributes, name, value, interpolation.second, std);
      }
    }
  }
}

HDCYCLES_NAMESPACE_CLOSE_SCOPE
