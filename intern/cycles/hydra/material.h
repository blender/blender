/* SPDX-FileCopyrightText: 2022 NVIDIA Corporation
 * SPDX-FileCopyrightText: 2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "hydra/config.h"

#include <pxr/imaging/hd/material.h>
#include <pxr/imaging/hd/materialNetworkSchema.h>
#include <pxr/imaging/hd/materialNodeParameterSchema.h>
#include <pxr/imaging/hd/materialNodeSchema.h>

HDCYCLES_NAMESPACE_OPEN_SCOPE

class HdCyclesMaterial final : public PXR_NS::HdMaterial {
 public:
  HdCyclesMaterial(const PXR_NS::SdfPath &sprimId);
  ~HdCyclesMaterial() override;

  PXR_NS::HdDirtyBits GetInitialDirtyBitsMask() const override;

  void Sync(PXR_NS::HdSceneDelegate *sceneDelegate,
            PXR_NS::HdRenderParam *renderParam,
            PXR_NS::HdDirtyBits *dirtyBits) override;

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
                        PXR_NS::HdMaterialNodeParameterContainerSchema params,
                        const PXR_NS::SdfPath &nodePath);

  void UpdateParameters(PXR_NS::HdMaterialNetworkSchema network);

  void UpdateConnections(NodeDesc &nodeDesc,
                         PXR_NS::HdMaterialNodeSchema nodeSchema,
                         const PXR_NS::SdfPath &nodePath,
                         CCL_NS::ShaderGraph *shaderGraph);

  void PopulateShaderGraph(PXR_NS::HdMaterialNetworkSchema network);

  CCL_NS::Shader *_shader = nullptr;
  std::unordered_map<PXR_NS::SdfPath, NodeDesc, PXR_NS::SdfPath::Hash> _nodes;
};

HDCYCLES_NAMESPACE_CLOSE_SCOPE
