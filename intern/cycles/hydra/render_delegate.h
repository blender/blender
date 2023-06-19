/* SPDX-FileCopyrightText: 2022 NVIDIA Corporation
 * SPDX-FileCopyrightText: 2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "hydra/config.h"

#include <pxr/imaging/hd/renderDelegate.h>
#include <pxr/imaging/hgi/hgi.h>

HDCYCLES_NAMESPACE_OPEN_SCOPE

// clang-format off
#define HD_CYCLES_RENDER_SETTINGS_TOKENS \
    (stageMetersPerUnit) \
    ((device, "cycles:device")) \
    ((threads, "cycles:threads")) \
    ((timeLimit, "cycles:time_limit")) \
    ((samples, "cycles:samples")) \
    ((sampleOffset, "cycles:sample_offset"))
// clang-format on

TF_DECLARE_PUBLIC_TOKENS(HdCyclesRenderSettingsTokens, HD_CYCLES_RENDER_SETTINGS_TOKENS);

class HdCyclesDelegate final : public PXR_NS::HdRenderDelegate {
 public:
  HdCyclesDelegate(const PXR_NS::HdRenderSettingsMap &settingsMap,
                   CCL_NS::Session *session_ = nullptr,
                   const bool keep_nodes = false);
  ~HdCyclesDelegate() override;

  void SetDrivers(const PXR_NS::HdDriverVector &drivers) override;

  bool IsDisplaySupported() const;

  PXR_NS::Hgi *GetHgi() const
  {
    return _hgi;
  }

  const PXR_NS::TfTokenVector &GetSupportedRprimTypes() const override;
  const PXR_NS::TfTokenVector &GetSupportedSprimTypes() const override;
  const PXR_NS::TfTokenVector &GetSupportedBprimTypes() const override;

  PXR_NS::HdRenderParam *GetRenderParam() const override;

  PXR_NS::HdResourceRegistrySharedPtr GetResourceRegistry() const override;

  PXR_NS::HdRenderSettingDescriptorList GetRenderSettingDescriptors() const override;

  bool IsPauseSupported() const override;

  bool Pause() override;
  bool Resume() override;

  PXR_NS::HdRenderPassSharedPtr CreateRenderPass(
      PXR_NS::HdRenderIndex *index, const PXR_NS::HdRprimCollection &collection) override;

  PXR_NS::HdInstancer *CreateInstancer(PXR_NS::HdSceneDelegate *delegate,
                                       const PXR_NS::SdfPath &id
#if PXR_VERSION < 2102
                                       ,
                                       const PXR_NS::SdfPath &instancerId
#endif
                                       ) override;
  void DestroyInstancer(PXR_NS::HdInstancer *instancer) override;

  PXR_NS::HdRprim *CreateRprim(const PXR_NS::TfToken &typeId,
                               const PXR_NS::SdfPath &rprimId
#if PXR_VERSION < 2102
                               ,
                               const PXR_NS::SdfPath &instancerId
#endif
                               ) override;
  void DestroyRprim(PXR_NS::HdRprim *rPrim) override;

  PXR_NS::HdSprim *CreateSprim(const PXR_NS::TfToken &typeId,
                               const PXR_NS::SdfPath &sprimId) override;
  PXR_NS::HdSprim *CreateFallbackSprim(const PXR_NS::TfToken &typeId) override;
  void DestroySprim(PXR_NS::HdSprim *sPrim) override;

  PXR_NS::HdBprim *CreateBprim(const PXR_NS::TfToken &typeId,
                               const PXR_NS::SdfPath &bprimId) override;
  PXR_NS::HdBprim *CreateFallbackBprim(const PXR_NS::TfToken &typeId) override;
  void DestroyBprim(PXR_NS::HdBprim *bPrim) override;

  void CommitResources(PXR_NS::HdChangeTracker *tracker) override;

  PXR_NS::TfToken GetMaterialBindingPurpose() const override;

#if HD_API_VERSION < 41
  PXR_NS::TfToken GetMaterialNetworkSelector() const override;
#else
  PXR_NS::TfTokenVector GetMaterialRenderContexts() const override;
#endif

  PXR_NS::VtDictionary GetRenderStats() const override;

  PXR_NS::HdAovDescriptor GetDefaultAovDescriptor(const PXR_NS::TfToken &name) const override;

  void SetRenderSetting(const PXR_NS::TfToken &key, const PXR_NS::VtValue &value) override;

  PXR_NS::VtValue GetRenderSetting(const PXR_NS::TfToken &key) const override;

 private:
  PXR_NS::Hgi *_hgi = nullptr;
  std::unique_ptr<HdCyclesSession> _renderParam;
};

HDCYCLES_NAMESPACE_CLOSE_SCOPE
