/* SPDX-FileCopyrightText: 2022 NVIDIA Corporation
 * SPDX-FileCopyrightText: 2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "hydra/config.h"

#include <pxr/imaging/hd/renderBuffer.h>

HDCYCLES_NAMESPACE_OPEN_SCOPE

class HdCyclesRenderBuffer final : public PXR_NS::HdRenderBuffer {
 public:
  HdCyclesRenderBuffer(const PXR_NS::SdfPath &bprimId);
  ~HdCyclesRenderBuffer() override;

  void Finalize(PXR_NS::HdRenderParam *renderParam) override;

  bool Allocate(const PXR_NS::GfVec3i &dimensions,
                PXR_NS::HdFormat format,
                bool multiSampled) override;

  unsigned int GetWidth() const override
  {
    return _width;
  }

  unsigned int GetHeight() const override
  {
    return _height;
  }

  unsigned int GetDepth() const override
  {
    return 1u;
  }

  PXR_NS::HdFormat GetFormat() const override
  {
    return _format;
  }

  bool IsMultiSampled() const override
  {
    return false;
  }

  void *Map() override;

  void Unmap() override;

  bool IsMapped() const override;

  void Resolve() override;

  bool IsConverged() const override;

  void SetConverged(bool converged);

  bool IsResourceUsed() const;

  PXR_NS::VtValue GetResource(bool multiSampled = false) const override;

  void SetResource(const PXR_NS::VtValue &resource);

  void WritePixels(const float *pixels,
                   const PXR_NS::GfVec2i &offset,
                   const PXR_NS::GfVec2i &dims,
                   int channels,
                   bool isId = false);

 private:
  void _Deallocate() override;

  unsigned int _width = 0u;
  unsigned int _height = 0u;
  PXR_NS::HdFormat _format = PXR_NS::HdFormatInvalid;
  size_t _dataSize = 0;

  std::vector<uint8_t> _data;
  PXR_NS::VtValue _resource;
  mutable std::atomic_bool _resourceUsed = false;

  std::atomic_int _mapped = 0;
  std::atomic_bool _converged = false;
};

HDCYCLES_NAMESPACE_CLOSE_SCOPE
