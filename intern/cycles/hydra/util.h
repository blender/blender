/* SPDX-FileCopyrightText: 2022 NVIDIA Corporation
 * SPDX-FileCopyrightText: 2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "hydra/config.h"
#include "scene/attribute.h"

#include <pxr/base/vt/value.h>
#include <pxr/imaging/hd/dataSource.h>
#include <pxr/imaging/hd/enums.h>
#include <pxr/imaging/hd/primvarsSchema.h>
#include <pxr/imaging/hd/sceneIndex.h>
#include <pxr/imaging/hd/timeSampleArray.h>
#include <pxr/imaging/hd/types.h>

PXR_NAMESPACE_OPEN_SCOPE
class HdSceneDelegate;
class SdfPath;
class TfToken;
PXR_NAMESPACE_CLOSE_SCOPE

HDCYCLES_NAMESPACE_OPEN_SCOPE

/* Prim data source from the scene index, or empty if none is reachable. */
PXR_NS::HdSceneIndexPrim GetPrim(PXR_NS::HdSceneDelegate *delegate, const PXR_NS::SdfPath &id);

/* Typed child data source, or null if missing or of a different type. */
template<typename T>
typename PXR_NS::HdTypedSampledDataSource<T>::Handle GetTypedDataSource(
    const PXR_NS::HdContainerDataSourceHandle &container, const PXR_NS::TfToken &name)
{
  if (!container) {
    return nullptr;
  }
  return PXR_NS::HdTypedSampledDataSource<T>::Cast(container->Get(name));
}

/* Typed value at shutter offset 0, or `fallback` if missing or of a different type. */
template<typename T>
T GetTypedValue(const PXR_NS::HdContainerDataSourceHandle &container,
                const PXR_NS::TfToken &name,
                const T &fallback = T())
{
  if (!container) {
    return fallback;
  }
  const auto base = container->Get(name);
  if (const auto typed = PXR_NS::HdTypedSampledDataSource<T>::Cast(base)) {
    return typed->GetTypedValue(0.0f);
  }
  if (const auto sampled = PXR_NS::HdSampledDataSource::Cast(base)) {
    PXR_NS::VtValue v = sampled->GetValue(0.0f);
    if (v.IsHolding<T>()) {
      return v.UncheckedGet<T>();
    }
  }
  return fallback;
}

/* Sample a typed data source over the shutter into `out`. */
template<typename T, unsigned int Capacity>
void SampleTyped(const typename PXR_NS::HdTypedSampledDataSource<T>::Handle &ds,
                 float shutterOpen,
                 float shutterClose,
                 PXR_NS::HdTimeSampleArray<T, Capacity> *out)
{
  if (!ds) {
    out->Resize(0);
    return;
  }
  std::vector<float> times;
  const bool hasSamples = ds->GetContributingSampleTimesForInterval(
      shutterOpen, shutterClose, &times);
  if (!hasSamples || times.empty()) {
    out->Resize(1);
    out->times[0] = 0.0f;
    out->values[0] = ds->GetTypedValue(0.0f);
    return;
  }
  out->Resize(times.size());
  for (size_t i = 0; i < times.size(); ++i) {
    out->times[i] = times[i];
    out->values[i] = ds->GetTypedValue(times[i]);
  }
}

/* Read a primvar's value at shutter offset 0. */
PXR_NS::VtValue ReadPrimvar(const PXR_NS::HdPrimvarsSchema &primvars, const PXR_NS::TfToken &name);

/* Read a primvar's interpolation. */
PXR_NS::HdInterpolation ReadPrimvarInterpolation(const PXR_NS::HdPrimvarsSchema &primvars,
                                                 const PXR_NS::TfToken &name);

/* Read a primvar's role token. */
PXR_NS::TfToken ReadPrimvarRole(const PXR_NS::HdPrimvarsSchema &primvars,
                                const PXR_NS::TfToken &name);

/* Enumerate primvar names whose interpolation matches. */
PXR_NS::TfTokenVector PrimvarNamesAtInterpolation(const PXR_NS::HdPrimvarsSchema &primvars,
                                                  PXR_NS::HdInterpolation interpolation);

/* Convert a Hydra primvar value to a Cycles attribute. */
void ApplyPrimvars(CCL_NS::AttributeSet &attributes,
                   const CCL_NS::ustring &name,
                   PXR_NS::VtValue value,
                   CCL_NS::AttributeElement elem,
                   CCL_NS::AttributeStandard std);

HDCYCLES_NAMESPACE_CLOSE_SCOPE
