/* SPDX-FileCopyrightText: 2022 NVIDIA Corporation
 * SPDX-FileCopyrightText: 2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "hydra/config.h"

#include <pxr/imaging/hd/rprim.h>

HDCYCLES_NAMESPACE_OPEN_SCOPE

template<typename Base, typename CyclesBase> class HdCyclesGeometry : public Base {
 public:
  HdCyclesGeometry(const PXR_NS::SdfPath &rprimId
#if PXR_VERSION < 2102
                   ,
                   const PXR_NS::SdfPath &instancerId
#endif
  );

  void Sync(PXR_NS::HdSceneDelegate *sceneDelegate,
            PXR_NS::HdRenderParam *renderParam,
            PXR_NS::HdDirtyBits *dirtyBits,
            const PXR_NS::TfToken &reprToken) override;

  PXR_NS::HdDirtyBits GetInitialDirtyBitsMask() const override;

  virtual void Finalize(PXR_NS::HdRenderParam *renderParam) override;

 protected:
  void _InitRepr(const PXR_NS::TfToken &reprToken, PXR_NS::HdDirtyBits *dirtyBits) override;

  PXR_NS::HdDirtyBits _PropagateDirtyBits(PXR_NS::HdDirtyBits bits) const override;

  virtual void Populate(PXR_NS::HdSceneDelegate *sceneDelegate,
                        PXR_NS::HdDirtyBits dirtyBits,
                        bool &rebuild) = 0;

  PXR_NS::HdInterpolation GetPrimvarInterpolation(PXR_NS::HdSceneDelegate *sceneDelegate,
                                                  const PXR_NS::TfToken &name) const;

  CyclesBase *_geom = nullptr;
  std::vector<CCL_NS::Object *> _instances;

 private:
  void Initialize(PXR_NS::HdRenderParam *renderParam);

  void InitializeInstance(int index);

  PXR_NS::GfMatrix4d _geomTransform;
};

HDCYCLES_NAMESPACE_CLOSE_SCOPE
