/* SPDX-FileCopyrightText: 2022 NVIDIA Corporation
 * SPDX-FileCopyrightText: 2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "hydra/config.h"
#include <pxr/imaging/hd/rendererPlugin.h>

PXR_NAMESPACE_OPEN_SCOPE

class HdCyclesPlugin final : public PXR_NS::HdRendererPlugin {
 public:
  HdCyclesPlugin();
  ~HdCyclesPlugin() override;

#if PXR_VERSION >= 2511
  bool IsSupported(HdRendererCreateArgs const &rendererCreateArgs,
                   std::string *reasonWhyNot = nullptr) const override;
#endif
  bool IsSupported(bool gpuEnabled) const override;

  PXR_NS::HdRenderDelegate *CreateRenderDelegate() override;
  PXR_NS::HdRenderDelegate *CreateRenderDelegate(
      const PXR_NS::HdRenderSettingsMap & /*settingsMap*/) override;

  void DeleteRenderDelegate(PXR_NS::HdRenderDelegate *) override;
};

PXR_NAMESPACE_CLOSE_SCOPE
