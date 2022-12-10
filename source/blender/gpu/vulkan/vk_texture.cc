/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup gpu
 */

#include "vk_texture.hh"

namespace blender::gpu {

void VKTexture::generate_mipmap()
{
}

void VKTexture::copy_to(Texture * /*tex*/)
{
}

void VKTexture::clear(eGPUDataFormat /*format*/, const void * /*data*/)
{
}

void VKTexture::swizzle_set(const char /*swizzle_mask*/[4])
{
}

void VKTexture::stencil_texture_mode_set(bool /*use_stencil*/)
{
}

void VKTexture::mip_range_set(int /*min*/, int /*max*/)
{
}

void *VKTexture::read(int /*mip*/, eGPUDataFormat /*format*/)
{
  return nullptr;
}

void VKTexture::update_sub(int /*mip*/,
                           int /*offset*/[3],
                           int /*extent*/[3],
                           eGPUDataFormat /*format*/,
                           const void * /*data*/)
{
}

void VKTexture::update_sub(int /*offset*/[3],
                           int /*extent*/[3],
                           eGPUDataFormat /*format*/,
                           GPUPixelBuffer * /*pixbuf*/)
{
}

/* TODO(fclem): Legacy. Should be removed at some point. */
uint VKTexture::gl_bindcode_get() const
{
  return 0;
}

bool VKTexture::init_internal()
{
  return false;
}

bool VKTexture::init_internal(GPUVertBuf * /*vbo*/)
{
  return false;
}

bool VKTexture::init_internal(const GPUTexture * /*src*/, int /*mip_offset*/, int /*layer_offset*/)
{
  return false;
}

}  // namespace blender::gpu