/* SPDX-FileCopyrightText: 2016 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#include "draw_manager_c.hh"
#include "draw_texture_pool.hh"

#ifndef NDEBUG
/* Maybe `gpu_texture.cc` is a better place for this. */
static bool drw_texture_format_supports_framebuffer(eGPUTextureFormat format)
{
  /* Some formats do not work with frame-buffers. */
  switch (format) {
    /* Only add formats that are COMPATIBLE with FB.
     * Generally they are multiple of 16bit. */
    case GPU_R8:
    case GPU_R8UI:
    case GPU_R16F:
    case GPU_R16I:
    case GPU_R16UI:
    case GPU_R16:
    case GPU_R32F:
    case GPU_R32UI:
    case GPU_RG8:
    case GPU_RG16:
    case GPU_RG16F:
    case GPU_RG16I:
    case GPU_RG32F:
    case GPU_RGB10_A2:
    case GPU_R11F_G11F_B10F:
    case GPU_RGBA8:
    case GPU_RGBA16:
    case GPU_RGBA16F:
    case GPU_RGBA32F:
    case GPU_DEPTH_COMPONENT16:
    case GPU_DEPTH_COMPONENT24:
    case GPU_DEPTH24_STENCIL8:
    case GPU_DEPTH_COMPONENT32F:
      return true;
    default:
      return false;
  }
}
#endif

void drw_texture_set_parameters(GPUTexture *tex, DRWTextureFlag flags)
{
  if (tex == nullptr) {
    return;
  }

  if (flags & DRW_TEX_MIPMAP) {
    GPU_texture_mipmap_mode(tex, true, flags & DRW_TEX_FILTER);
    GPU_texture_update_mipmap_chain(tex);
  }
  else {
    GPU_texture_filter_mode(tex, flags & DRW_TEX_FILTER);
  }
  GPU_texture_anisotropic_filter(tex, false);
  GPU_texture_extend_mode(
      tex, flags & DRW_TEX_WRAP ? GPU_SAMPLER_EXTEND_MODE_REPEAT : GPU_SAMPLER_EXTEND_MODE_EXTEND);
  GPU_texture_compare_mode(tex, flags & DRW_TEX_COMPARE);
}

GPUTexture *DRW_texture_pool_query_2d_ex(
    int w, int h, eGPUTextureFormat format, eGPUTextureUsage usage, DrawEngineType *engine_type)
{
  BLI_assert(drw_texture_format_supports_framebuffer(format));
  GPUTexture *tex = DRW_texture_pool_query(
      DST.vmempool->texture_pool, w, h, format, usage, engine_type);

  return tex;
}

GPUTexture *DRW_texture_pool_query_2d(int w,
                                      int h,
                                      eGPUTextureFormat format,
                                      DrawEngineType *engine_type)
{
  return DRW_texture_pool_query_2d_ex(w, h, format, GPU_TEXTURE_USAGE_GENERAL, engine_type);
}

GPUTexture *DRW_texture_pool_query_fullscreen_ex(eGPUTextureFormat format,
                                                 eGPUTextureUsage usage,
                                                 DrawEngineType *engine_type)
{
  const float *size = DRW_viewport_size_get();
  return DRW_texture_pool_query_2d_ex(int(size[0]), int(size[1]), format, usage, engine_type);
}

GPUTexture *DRW_texture_pool_query_fullscreen(eGPUTextureFormat format,
                                              DrawEngineType *engine_type)
{
  return DRW_texture_pool_query_fullscreen_ex(format, GPU_TEXTURE_USAGE_GENERAL, engine_type);
}
