/* SPDX-FileCopyrightText: 2022 NVIDIA Corporation
 * SPDX-FileCopyrightText: 2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "hydra/config.h"
#include "hydra/geometry.h"

#include <pxr/imaging/hd/mesh.h>
#include <pxr/imaging/hd/meshUtil.h>

HDCYCLES_NAMESPACE_OPEN_SCOPE

class HdCyclesMesh final : public HdCyclesGeometry<PXR_NS::HdMesh, CCL_NS::Mesh> {
 public:
  HdCyclesMesh(
      const PXR_NS::SdfPath &rprimId
#if PXR_VERSION < 2102
      ,
      const PXR_NS::SdfPath &instancerId = {}
#endif
  );
  ~HdCyclesMesh() override;

  PXR_NS::HdDirtyBits GetInitialDirtyBitsMask() const override;

  void Finalize(PXR_NS::HdRenderParam *renderParam) override;

 private:
  PXR_NS::HdDirtyBits _PropagateDirtyBits(PXR_NS::HdDirtyBits bits) const override;

  void Populate(PXR_NS::HdSceneDelegate *sceneDelegate,
                PXR_NS::HdDirtyBits dirtyBits,
                bool &rebuild) override;

  void PopulatePoints(PXR_NS::HdSceneDelegate *sceneDelegate);
  void PopulateNormals(PXR_NS::HdSceneDelegate *sceneDelegate);

  void PopulatePrimvars(PXR_NS::HdSceneDelegate *sceneDelegate);

  void PopulateTopology(PXR_NS::HdSceneDelegate *sceneDelegate);

  PXR_NS::HdMeshUtil _util;
  PXR_NS::HdMeshTopology _topology;
  PXR_NS::VtIntArray _primitiveParams;
};

HDCYCLES_NAMESPACE_CLOSE_SCOPE
