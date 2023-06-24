/* SPDX-FileCopyrightText: 2022 NVIDIA Corporation
 * SPDX-FileCopyrightText: 2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "hydra/config.h"

#include <pxr/base/gf/camera.h>
#include <pxr/imaging/hd/camera.h>
#include <pxr/imaging/hd/timeSampleArray.h>

HDCYCLES_NAMESPACE_OPEN_SCOPE

class HdCyclesCamera final : public PXR_NS::HdCamera {
 public:
  HdCyclesCamera(const PXR_NS::SdfPath &sprimId);
  ~HdCyclesCamera() override;

  void ApplyCameraSettings(PXR_NS::HdRenderParam *renderParam, CCL_NS::Camera *targetCamera) const;

  static void ApplyCameraSettings(PXR_NS::HdRenderParam *renderParam,
                                  const PXR_NS::GfCamera &cameraData,
                                  PXR_NS::CameraUtilConformWindowPolicy windowPolicy,
                                  CCL_NS::Camera *targetCamera);
  static void ApplyCameraSettings(PXR_NS::HdRenderParam *renderParam,
                                  const PXR_NS::GfMatrix4d &worldToViewMatrix,
                                  const PXR_NS::GfMatrix4d &projectionMatrix,
                                  const std::vector<PXR_NS::GfVec4d> &clipPlanes,
                                  CCL_NS::Camera *targetCamera);

  PXR_NS::HdDirtyBits GetInitialDirtyBitsMask() const override;

  void Sync(PXR_NS::HdSceneDelegate *sceneDelegate,
            PXR_NS::HdRenderParam *renderParam,
            PXR_NS::HdDirtyBits *dirtyBits) override;

  void Finalize(PXR_NS::HdRenderParam *renderParam) override;

 private:
  PXR_NS::GfCamera _data;
  PXR_NS::HdTimeSampleArray<PXR_NS::GfMatrix4d, 2> _transformSamples;
};

HDCYCLES_NAMESPACE_CLOSE_SCOPE
