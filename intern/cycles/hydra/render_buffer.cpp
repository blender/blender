/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2022 NVIDIA Corporation
 * Copyright 2022 Blender Foundation */

#include "hydra/render_buffer.h"
#include "hydra/session.h"
#include "util/half.h"

#include <pxr/base/gf/vec3i.h>
#include <pxr/base/gf/vec4f.h>

HDCYCLES_NAMESPACE_OPEN_SCOPE

HdCyclesRenderBuffer::HdCyclesRenderBuffer(const SdfPath &bprimId) : HdRenderBuffer(bprimId) {}

HdCyclesRenderBuffer::~HdCyclesRenderBuffer() {}

void HdCyclesRenderBuffer::Finalize(HdRenderParam *renderParam)
{
  // Remove this render buffer from AOV bindings
  // This ensures that 'OutputDriver' does not attempt to write to it anymore
  static_cast<HdCyclesSession *>(renderParam)->RemoveAovBinding(this);

  HdRenderBuffer::Finalize(renderParam);
}

bool HdCyclesRenderBuffer::Allocate(const GfVec3i &dimensions, HdFormat format, bool multiSampled)
{
  if (dimensions[2] != 1) {
    TF_RUNTIME_ERROR("HdCyclesRenderBuffer::Allocate called with dimensions that are not 2D.");
    return false;
  }

  const size_t oldSize = _dataSize;
  const size_t newSize = dimensions[0] * dimensions[1] * HdDataSizeOfFormat(format);
  if (oldSize == newSize) {
    return true;
  }

  if (IsMapped()) {
    TF_RUNTIME_ERROR("HdCyclesRenderBuffer::Allocate called while buffer is mapped.");
    return false;
  }

  _width = dimensions[0];
  _height = dimensions[1];
  _format = format;
  _dataSize = newSize;
  _resourceUsed = false;

  return true;
}

void HdCyclesRenderBuffer::_Deallocate()
{
  _width = 0u;
  _height = 0u;
  _format = HdFormatInvalid;

  _data.clear();
  _data.shrink_to_fit();
  _dataSize = 0;

  _resource = VtValue();
}

void *HdCyclesRenderBuffer::Map()
{
  // Mapping is not implemented when a resource is set
  if (!_resource.IsEmpty()) {
    return nullptr;
  }

  if (_data.size() != _dataSize) {
    _data.resize(_dataSize);
  }

  ++_mapped;

  return _data.data();
}

void HdCyclesRenderBuffer::Unmap()
{
  --_mapped;
}

bool HdCyclesRenderBuffer::IsMapped() const
{
  return _mapped != 0;
}

void HdCyclesRenderBuffer::Resolve() {}

bool HdCyclesRenderBuffer::IsConverged() const
{
  return _converged;
}

void HdCyclesRenderBuffer::SetConverged(bool converged)
{
  _converged = converged;
}

bool HdCyclesRenderBuffer::IsResourceUsed() const
{
  return _resourceUsed;
}

VtValue HdCyclesRenderBuffer::GetResource(bool multiSampled) const
{
  TF_UNUSED(multiSampled);

  _resourceUsed = true;

  return _resource;
}

void HdCyclesRenderBuffer::SetResource(const VtValue &resource)
{
  _resource = resource;
}

namespace {

struct SimpleConversion {
  static float convert(float value)
  {
    return value;
  }
};
struct IdConversion {
  static int32_t convert(float value)
  {
    return static_cast<int32_t>(value) - 1;
  }
};
struct UInt8Conversion {
  static uint8_t convert(float value)
  {
    return static_cast<uint8_t>(value * 255.f);
  }
};
struct SInt8Conversion {
  static int8_t convert(float value)
  {
    return static_cast<int8_t>(value * 127.f);
  }
};
struct HalfConversion {
  static half convert(float value)
  {
    return float_to_half_image(value);
  }
};

template<typename SrcT, typename DstT, typename Convertor = SimpleConversion>
void writePixels(const SrcT *srcPtr,
                 const GfVec2i &srcSize,
                 int srcChannelCount,
                 DstT *dstPtr,
                 const GfVec2i &dstSize,
                 int dstChannelCount,
                 const Convertor &convertor = {})
{
  const auto writeSize = GfVec2i(GfMin(srcSize[0], dstSize[0]), GfMin(srcSize[1], dstSize[1]));
  const auto writeChannelCount = GfMin(srcChannelCount, dstChannelCount);

  for (int y = 0; y < writeSize[1]; ++y) {
    for (int x = 0; x < writeSize[0]; ++x) {
      for (int c = 0; c < writeChannelCount; ++c) {
        dstPtr[x * dstChannelCount + c] = convertor.convert(srcPtr[x * srcChannelCount + c]);
      }
    }
    srcPtr += srcSize[0] * srcChannelCount;
    dstPtr += dstSize[0] * dstChannelCount;
  }
}

}  // namespace

void HdCyclesRenderBuffer::WritePixels(const float *srcPixels,
                                       const PXR_NS::GfVec2i &srcOffset,
                                       const GfVec2i &srcDims,
                                       int srcChannels,
                                       bool isId)
{
  uint8_t *dstPixels = _data.data();

  const size_t formatSize = HdDataSizeOfFormat(_format);
  dstPixels += srcOffset[1] * (formatSize * _width) + srcOffset[0] * formatSize;

  switch (_format) {
    case HdFormatUNorm8:
    case HdFormatUNorm8Vec2:
    case HdFormatUNorm8Vec3:
    case HdFormatUNorm8Vec4:
      writePixels(srcPixels,
                  srcDims,
                  srcChannels,
                  dstPixels,
                  GfVec2i(_width, _height),
                  1 + (_format - HdFormatUNorm8),
                  UInt8Conversion());
      break;

    case HdFormatSNorm8:
    case HdFormatSNorm8Vec2:
    case HdFormatSNorm8Vec3:
    case HdFormatSNorm8Vec4:
      writePixels(srcPixels,
                  srcDims,
                  srcChannels,
                  dstPixels,
                  GfVec2i(_width, _height),
                  1 + (_format - HdFormatSNorm8),
                  SInt8Conversion());
      break;

    case HdFormatFloat16:
    case HdFormatFloat16Vec2:
    case HdFormatFloat16Vec3:
    case HdFormatFloat16Vec4:
      writePixels(srcPixels,
                  srcDims,
                  srcChannels,
                  reinterpret_cast<half *>(dstPixels),
                  GfVec2i(_width, _height),
                  1 + (_format - HdFormatFloat16),
                  HalfConversion());
      break;

    case HdFormatFloat32:
    case HdFormatFloat32Vec2:
    case HdFormatFloat32Vec3:
    case HdFormatFloat32Vec4:
      writePixels(srcPixels,
                  srcDims,
                  srcChannels,
                  reinterpret_cast<float *>(dstPixels),
                  GfVec2i(_width, _height),
                  1 + (_format - HdFormatFloat32));
      break;

    case HdFormatInt32:
      // Special case for ID AOVs (see 'HdCyclesMesh::Sync')
      if (isId) {
        writePixels(srcPixels,
                    srcDims,
                    srcChannels,
                    reinterpret_cast<int *>(dstPixels),
                    GfVec2i(_width, _height),
                    1,
                    IdConversion());
      }
      else {
        writePixels(srcPixels,
                    srcDims,
                    srcChannels,
                    reinterpret_cast<int *>(dstPixels),
                    GfVec2i(_width, _height),
                    1);
      }
      break;
    case HdFormatInt32Vec2:
    case HdFormatInt32Vec3:
    case HdFormatInt32Vec4:
      writePixels(srcPixels,
                  srcDims,
                  srcChannels,
                  reinterpret_cast<int *>(dstPixels),
                  GfVec2i(_width, _height),
                  1 + (_format - HdFormatInt32));
      break;

    default:
      TF_RUNTIME_ERROR("HdCyclesRenderBuffer::WritePixels called with unsupported format.");
      break;
  }
}

HDCYCLES_NAMESPACE_CLOSE_SCOPE
