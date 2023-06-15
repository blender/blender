/* SPDX-FileCopyrightText: 2022 NVIDIA Corporation
 * SPDX-FileCopyrightText: 2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "hydra/config.h"
#include "hydra/geometry.h"

#include <pxr/imaging/hd/basisCurves.h>

HDCYCLES_NAMESPACE_OPEN_SCOPE

class HdCyclesCurves final : public HdCyclesGeometry<PXR_NS::HdBasisCurves, CCL_NS::Hair> {
 public:
  HdCyclesCurves(
      const PXR_NS::SdfPath &rprimId
#if PXR_VERSION < 2102
      ,
      const PXR_NS::SdfPath &instancerId = {}
#endif
  );
  ~HdCyclesCurves() override;

  PXR_NS::HdDirtyBits GetInitialDirtyBitsMask() const override;

 private:
  PXR_NS::HdDirtyBits _PropagateDirtyBits(PXR_NS::HdDirtyBits bits) const override;

  void Populate(PXR_NS::HdSceneDelegate *sceneDelegate,
                PXR_NS::HdDirtyBits dirtyBits,
                bool &rebuild) override;

  void PopulatePoints(PXR_NS::HdSceneDelegate *sceneDelegate);
  void PopulateWidths(PXR_NS::HdSceneDelegate *sceneDelegate);

  void PopulatePrimvars(PXR_NS::HdSceneDelegate *sceneDelegate);

  void PopulateTopology(PXR_NS::HdSceneDelegate *sceneDelegate);
};

HDCYCLES_NAMESPACE_CLOSE_SCOPE
