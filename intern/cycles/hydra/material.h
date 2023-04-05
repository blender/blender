/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2022 NVIDIA Corporation
 * Copyright 2022 Blender Foundation */

#pragma once

#include "hydra/config.h"

#include <pxr/imaging/hd/material.h>

HDCYCLES_NAMESPACE_OPEN_SCOPE

class HdCyclesMaterial final : public PXR_NS::HdMaterial {
 public:
  HdCyclesMaterial(const PXR_NS::SdfPath &sprimId);
  ~HdCyclesMaterial() override;

  PXR_NS::HdDirtyBits GetInitialDirtyBitsMask() const override;

  void Sync(PXR_NS::HdSceneDelegate *sceneDelegate,
            PXR_NS::HdRenderParam *renderParam,
            PXR_NS::HdDirtyBits *dirtyBits) override;

#if PXR_VERSION < 2011
  void Reload() override {}
#endif

  void Finalize(PXR_NS::HdRenderParam *renderParam) override;

  CCL_NS::Shader *GetCyclesShader() const
  {
    return _shader;
  }

 private:
  struct NodeDesc {
    CCL_NS::ShaderNode *node;
    const class UsdToCyclesMapping *mapping;
  };

  void Initialize(PXR_NS::HdRenderParam *renderParam);

  void UpdateParameters(NodeDesc &nodeDesc,
                        const std::map<PXR_NS::TfToken, PXR_NS::VtValue> &parameters,
                        const PXR_NS::SdfPath &nodePath);

  void UpdateParameters(const PXR_NS::HdMaterialNetwork &network);
  void UpdateParameters(const PXR_NS::HdMaterialNetwork2 &network);

  void UpdateConnections(NodeDesc &nodeDesc,
                         const PXR_NS::HdMaterialNode2 &matNode,
                         const PXR_NS::SdfPath &nodePath,
                         CCL_NS::ShaderGraph *shaderGraph);

  void PopulateShaderGraph(const PXR_NS::HdMaterialNetwork2 &network);

  CCL_NS::Shader *_shader = nullptr;
  std::unordered_map<PXR_NS::SdfPath, NodeDesc, PXR_NS::SdfPath::Hash> _nodes;
};

HDCYCLES_NAMESPACE_CLOSE_SCOPE
