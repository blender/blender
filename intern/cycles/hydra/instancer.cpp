/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2022 NVIDIA Corporation
 * Copyright 2022 Blender Foundation */

#include "hydra/instancer.h"

#include <pxr/base/gf/quatd.h>
#include <pxr/imaging/hd/sceneDelegate.h>

HDCYCLES_NAMESPACE_OPEN_SCOPE

HdCyclesInstancer::HdCyclesInstancer(HdSceneDelegate *delegate,
                                     const SdfPath &instancerId
#if PXR_VERSION <= 2011
                                     ,
                                     const SdfPath &parentId
#endif
                                     )
    : HdInstancer(delegate,
                  instancerId
#if PXR_VERSION <= 2011
                  ,
                  parentId
#endif
      )
{
}

HdCyclesInstancer::~HdCyclesInstancer() {}

#if PXR_VERSION > 2011
void HdCyclesInstancer::Sync(HdSceneDelegate *sceneDelegate,
                             HdRenderParam *renderParam,
                             HdDirtyBits *dirtyBits)
{
  _UpdateInstancer(sceneDelegate, dirtyBits);

  if (HdChangeTracker::IsAnyPrimvarDirty(*dirtyBits, GetId())) {
    SyncPrimvars();
  }
}
#endif

void HdCyclesInstancer::SyncPrimvars()
{
  HdSceneDelegate *const sceneDelegate = GetDelegate();
  const HdDirtyBits dirtyBits =
      sceneDelegate->GetRenderIndex().GetChangeTracker().GetInstancerDirtyBits(GetId());

  for (const HdPrimvarDescriptor &desc :
       sceneDelegate->GetPrimvarDescriptors(GetId(), HdInterpolationInstance))
  {
    if (!HdChangeTracker::IsPrimvarDirty(dirtyBits, GetId(), desc.name)) {
      continue;
    }

    const VtValue value = sceneDelegate->Get(GetId(), desc.name);
    if (value.IsEmpty()) {
      continue;
    }

    if (desc.name == HdInstancerTokens->translate) {
      _translate = value.Get<VtVec3fArray>();
    }
    else if (desc.name == HdInstancerTokens->rotate) {
      _rotate = value.Get<VtVec4fArray>();
    }
    else if (desc.name == HdInstancerTokens->scale) {
      _scale = value.Get<VtVec3fArray>();
    }
    else if (desc.name == HdInstancerTokens->instanceTransform) {
      _instanceTransform = value.Get<VtMatrix4dArray>();
    }
  }

  sceneDelegate->GetRenderIndex().GetChangeTracker().MarkInstancerClean(GetId());
}

VtMatrix4dArray HdCyclesInstancer::ComputeInstanceTransforms(const SdfPath &prototypeId)
{
#if PXR_VERSION <= 2011
  SyncPrimvars();
#endif

  const VtIntArray instanceIndices = GetDelegate()->GetInstanceIndices(GetId(), prototypeId);
  const GfMatrix4d instanceTransform = GetDelegate()->GetInstancerTransform(GetId());

  VtMatrix4dArray transforms;
  transforms.reserve(instanceIndices.size());

  for (int index : instanceIndices) {
    GfMatrix4d transform = instanceTransform;

    if (index < _translate.size()) {
      GfMatrix4d translateMat(1);
      translateMat.SetTranslate(_translate[index]);
      transform *= translateMat;
    }

    if (index < _rotate.size()) {
      GfMatrix4d rotateMat(1);
      const GfVec4f &quat = _rotate[index];
      rotateMat.SetRotate(GfQuatd(quat[0], quat[1], quat[2], quat[3]));
      transform *= rotateMat;
    }

    if (index < _scale.size()) {
      GfMatrix4d scaleMat(1);
      scaleMat.SetScale(_scale[index]);
      transform *= scaleMat;
    }

    if (index < _instanceTransform.size()) {
      transform *= _instanceTransform[index];
    }

    transforms.push_back(transform);
  }

  VtMatrix4dArray resultTransforms;

  if (const auto instancer = static_cast<HdCyclesInstancer *>(
          GetDelegate()->GetRenderIndex().GetInstancer(GetParentId())))
  {
    for (const GfMatrix4d &parentTransform : instancer->ComputeInstanceTransforms(GetId())) {
      for (const GfMatrix4d &localTransform : transforms) {
        resultTransforms.push_back(parentTransform * localTransform);
      }
    }
  }
  else {
    resultTransforms = std::move(transforms);
  }

  return resultTransforms;
}

HDCYCLES_NAMESPACE_CLOSE_SCOPE
