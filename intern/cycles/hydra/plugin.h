/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2022 NVIDIA Corporation
 * Copyright 2022 Blender Foundation */

#pragma once

#include "hydra/config.h"
#include <pxr/imaging/hd/rendererPlugin.h>

PXR_NAMESPACE_OPEN_SCOPE

class HdCyclesPlugin final : public PXR_NS::HdRendererPlugin {
 public:
  HdCyclesPlugin();
  ~HdCyclesPlugin() override;

#if PXR_VERSION < 2302
  bool IsSupported() const override;
#else
  bool IsSupported(bool gpuEnabled) const override;
#endif

  PXR_NS::HdRenderDelegate *CreateRenderDelegate() override;
  PXR_NS::HdRenderDelegate *CreateRenderDelegate(const PXR_NS::HdRenderSettingsMap &) override;

  void DeleteRenderDelegate(PXR_NS::HdRenderDelegate *) override;
};

PXR_NAMESPACE_CLOSE_SCOPE
