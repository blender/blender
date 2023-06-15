/* SPDX-FileCopyrightText: 2022 NVIDIA Corporation
 * SPDX-FileCopyrightText: 2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "hydra/config.h"

#include <pxr/base/gf/matrix4d.h>
#include <pxr/base/gf/vec3f.h>
#include <pxr/base/gf/vec4f.h>
#include <pxr/base/vt/array.h>
#include <pxr/imaging/hd/instancer.h>

HDCYCLES_NAMESPACE_OPEN_SCOPE

class HdCyclesInstancer final : public PXR_NS::HdInstancer {
 public:
  HdCyclesInstancer(PXR_NS::HdSceneDelegate *delegate,
                    const PXR_NS::SdfPath &instancerId
#if PXR_VERSION <= 2011
                    ,
                    const PXR_NS::SdfPath &parentId
#endif
  );
  ~HdCyclesInstancer() override;

#if PXR_VERSION > 2011
  void Sync(PXR_NS::HdSceneDelegate *sceneDelegate,
            PXR_NS::HdRenderParam *renderParam,
            PXR_NS::HdDirtyBits *dirtyBits) override;
#endif

  PXR_NS::VtMatrix4dArray ComputeInstanceTransforms(const PXR_NS::SdfPath &prototypeId);

 private:
  void SyncPrimvars();

  PXR_NS::VtVec3fArray _translate;
  PXR_NS::VtVec4fArray _rotate;
  PXR_NS::VtVec3fArray _scale;
  PXR_NS::VtMatrix4dArray _instanceTransform;
};

HDCYCLES_NAMESPACE_CLOSE_SCOPE
