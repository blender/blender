/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <cstdint>
#include <memory>
#include <string>

#include "BLI_hash.hh"

#include "GPU_shader.h"
#include "GPU_texture.h"

#include "COM_cached_shader.hh"
#include "COM_result.hh"

#include "gpu_shader_create_info.hh"

namespace blender::realtime_compositor {

/* --------------------------------------------------------------------
 * Cached Shader Key.
 */

CachedShaderKey::CachedShaderKey(const char *info_name, ResultPrecision precision)
    : info_name(info_name), precision(precision)
{
}

uint64_t CachedShaderKey::hash() const
{
  return get_default_hash_2(info_name, precision);
}

bool operator==(const CachedShaderKey &a, const CachedShaderKey &b)
{
  return a.info_name == b.info_name && a.precision == b.precision;
}

/* --------------------------------------------------------------------
 * Cached Shader.
 */

/* Given a texture format, return a corresponding texture format with the target precision. */
static eGPUTextureFormat change_texture_format_precision(eGPUTextureFormat format,
                                                         ResultPrecision target_precision)
{
  switch (target_precision) {
    case ResultPrecision::Half:
      switch (format) {
        /* Already half precision, return the input format. */
        case GPU_R16F:
        case GPU_RG16F:
        case GPU_RGB16F:
        case GPU_RGBA16F:
        case GPU_RG16I:
          return format;

        case GPU_R32F:
          return GPU_R16F;
        case GPU_RG32F:
          return GPU_RG16F;
        case GPU_RGB32F:
          return GPU_RGB16F;
        case GPU_RGBA32F:
          return GPU_RGBA16F;
        case GPU_RG32I:
          return GPU_RG16I;
        default:
          break;
      }
      break;
    case ResultPrecision::Full:
      switch (format) {
        /* Already full precision, return the input format. */
        case GPU_R32F:
        case GPU_RG32F:
        case GPU_RGB32F:
        case GPU_RGBA32F:
        case GPU_RG32I:
          return format;

        case GPU_R16F:
          return GPU_R32F;
        case GPU_RG16F:
          return GPU_RG32F;
        case GPU_RGB16F:
          return GPU_RGB32F;
        case GPU_RGBA16F:
          return GPU_RGBA32F;
        case GPU_RG16I:
          return GPU_RG32I;
        default:
          break;
      }
      break;
  }

  BLI_assert_unreachable();
  return format;
}

CachedShader::CachedShader(const char *info_name, ResultPrecision precision)
{
  using namespace gpu::shader;
  ShaderCreateInfo info = *reinterpret_cast<const ShaderCreateInfo *>(
      GPU_shader_create_info_get(info_name));

  /* Finalize first in case the create info had additional info. */
  info.finalize();

  /* Change the format of image resource to the target precision. */
  for (ShaderCreateInfo::Resource &resource : info.pass_resources_) {
    if (resource.bind_type != ShaderCreateInfo::Resource::BindType::IMAGE) {
      continue;
    }
    resource.image.format = change_texture_format_precision(resource.image.format, precision);
  }

  shader_ = GPU_shader_create_from_info(reinterpret_cast<const GPUShaderCreateInfo *>(&info));
}

CachedShader::~CachedShader()
{
  GPU_shader_free(shader_);
}

GPUShader *CachedShader::shader() const
{
  return shader_;
}

/* --------------------------------------------------------------------
 * Cached Shader Container.
 */

void CachedShaderContainer::reset()
{
  /* First, delete all resources that are no longer needed. */
  map_.remove_if([](auto item) { return !item.value->needed; });

  /* Second, reset the needed status of the remaining resources to false to ready them to track
   * their needed status for the next evaluation. */
  for (auto &value : map_.values()) {
    value->needed = false;
  }
}

GPUShader *CachedShaderContainer::get(const char *info_name, ResultPrecision precision)
{
  const CachedShaderKey key(info_name, precision);

  auto &cached_shader = *map_.lookup_or_add_cb(
      key, [&]() { return std::make_unique<CachedShader>(info_name, precision); });

  cached_shader.needed = true;
  return cached_shader.shader();
}

}  // namespace blender::realtime_compositor
