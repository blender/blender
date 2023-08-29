/* SPDX-FileCopyrightText: 2022 NVIDIA Corporation
 * SPDX-FileCopyrightText: 2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "hydra/config.h"

#include <pxr/imaging/hd/light.h>

HDCYCLES_NAMESPACE_OPEN_SCOPE

class HdCyclesLight final : public PXR_NS::HdLight {
 public:
  HdCyclesLight(const PXR_NS::SdfPath &sprimId, const PXR_NS::TfToken &lightType);
  ~HdCyclesLight() override;

  PXR_NS::HdDirtyBits GetInitialDirtyBitsMask() const override;

  void Sync(PXR_NS::HdSceneDelegate *sceneDelegate,
            PXR_NS::HdRenderParam *renderParam,
            PXR_NS::HdDirtyBits *dirtyBits) override;

  void Finalize(PXR_NS::HdRenderParam *renderParam) override;

 private:
  void Initialize(PXR_NS::HdRenderParam *renderParam);

  void PopulateShaderGraph(PXR_NS::HdSceneDelegate *sceneDelegate);

  CCL_NS::Light *_light = nullptr;
  PXR_NS::TfToken _lightType;
};

HDCYCLES_NAMESPACE_CLOSE_SCOPE
