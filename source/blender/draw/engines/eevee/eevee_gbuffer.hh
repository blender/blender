/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup eevee
 *
 * Gbuffer layout used for deferred shading pipeline.
 */

#pragma once

#include "DRW_gpu_wrapper.hh"
#include "DRW_render.hh"
#include "GPU_capabilities.hh"
#include "GPU_platform.hh"

#include "eevee_defines.hh"

namespace blender::eevee {

using namespace draw;

class Instance;

/**
 * Full-screen textures containing geometric and surface data.
 * Used by deferred shading passes. Only one g-buffer is allocated per view
 * and is reused for each deferred layer. This is why there can only be temporary
 * texture inside it.
 *
 * Everything is stored inside two array texture, one for each format. This is to fit the
 * limitation of the number of images we can bind on a single shader.
 *
 * The content of the g-buffer is polymorphic. A 8bit header specify the layout of the data.
 * The first layer is always written to while others are written only if needed using imageStore
 * operations reducing the bandwidth needed.
 * Except for some special configurations, the g-buffer holds up to 3 closures.
 *
 * For each output closure, we also output the color to apply after the lighting computation.
 * The color is stored with a 2 exponent that allows input color with component higher than 1.
 * Color degradation is expected to happen in this case.
 *
 * Here are special configurations:
 *
 * - Opaque Dielectric:
 *   - 1 Diffuse lobe and 1 Reflection lobe without anisotropy.
 *   - Share a single normal.
 *   - Reflection is not colored.
 *   - Layout:
 *     - Color 1 : Diffuse color
 *     - Closure 1 R : Normal packed X
 *     - Closure 1 G : Normal packed Y
 *     - Closure 1 B : Roughness (isotropic)
 *     - Closure 1 A : Reflection intensity
 *
 * - Simple Car-paint: (TODO)
 *   - 2 Reflection lobe without anisotropy.
 *   - Share a single normal.
 *   - Coat layer is not colored.
 *   - Layout:
 *     - Color 1 : Bottom layer color
 *     - Closure 1 R : Normal packed X
 *     - Closure 1 G : Normal packed Y
 *     - Closure 1 B : Roughness (isotropic)
 *     - Closure 1 A : Coat layer intensity
 *
 * - Simple Glass: (TODO)
 *   - 1 Refraction lobe and 1 Reflection lobe without anisotropy.
 *   - Share a single normal.
 *   - Reflection intensity is derived from IOR.
 *   - Layout:
 *     - Color 1 : Refraction color
 *     - Closure 1 R : Normal packed X
 *     - Closure 1 G : Normal packed Y
 *     - Closure 1 B : Roughness (isotropic)
 *     - Closure 1 A : IOR
 *
 * Here are Closure configurations:
 *
 * - Reflection (Isotropic):
 *   - Layout:
 *     - Color : Reflection color
 *     - Closure 1 R : Normal packed X
 *     - Closure 1 G : Normal packed Y
 *     - Closure 1 B : Roughness
 *     - Closure 1 A : Unused
 *
 * - Reflection (Anisotropic): (TODO)
 *   - Layout:
 *     - Color : Reflection color
 *     - Closure 1 R : Normal packed X
 *     - Closure 1 G : Normal packed Y
 *     - Closure 1 B : Tangent packed X
 *     - Closure 1 A : Tangent packed Y
 *     - Closure 2 R : Roughness X
 *     - Closure 2 G : Roughness Y
 *     - Closure 2 B : Unused
 *     - Closure 2 A : Unused
 *
 * - Refraction (Isotropic):
 *   - Layout:
 *     - Color : Refraction color
 *     - Closure 1 R : Normal packed X
 *     - Closure 1 G : Normal packed Y
 *     - Closure 1 B : Roughness
 *     - Closure 1 A : IOR
 *
 * - Diffuse:
 *   - Layout:
 *     - Color : Diffuse color
 *     - Closure 1 R : Normal packed X
 *     - Closure 1 G : Normal packed Y
 *     - Closure 1 B : Unused
 *     - Closure 1 A : Unused (Could be used for diffuse roughness)
 *
 * - Sub-Surface Scattering:
 *   - Layout:
 *     - Color : Diffuse color
 *     - Closure 1 R : Normal packed X
 *     - Closure 1 G : Normal packed Y
 *     - Closure 1 B : Thickness
 *     - Closure 1 A : Unused (Could be used for diffuse roughness)
 *     - Closure 2 R : Scattering radius R
 *     - Closure 2 G : Scattering radius G
 *     - Closure 2 B : Scattering radius B
 *     - Closure 2 A : Object ID
 *
 */
struct GBuffer {
 public:
  /* TODO(fclem): Use texture from pool once they support texture array and layer views. */
  Texture header_tx = {"GBufferHeader"};
  Texture closure_tx = {"GBufferClosure"};
  Texture normal_tx = {"GBufferNormal"};

  /* Expected number of layer written through the framebuffer. */
  const uint header_fb_layer_count = GBUF_HEADER_FB_LAYER_COUNT;
  const uint closure_fb_layer_count = GBUF_CLOSURE_FB_LAYER_COUNT;
  const uint normal_fb_layer_count = GBUF_NORMAL_FB_LAYER_COUNT;

 private:
  /* References to optional GBuffer layers that are not always required or written to.
   * These will point to either the dummy textures bellow or to a layer range view of the above
   * textures. In the later case, these layers are written with imageStore instead of being part
   * of the #Framebuffer. */
  gpu::Texture *closure_opt_layers_ = nullptr;
  gpu::Texture *normal_opt_layers_ = nullptr;
  gpu::Texture *header_opt_layers_ = nullptr;

  /* Textures used to fulfill the GBuffer optional layers binding when textures do not have enough
   * layers for the optional layers image views. The shader are then expected to never write to
   * them. */
  Texture dummy_header_tx_ = {"GBufferDummyHeader"};
  Texture dummy_closure_tx_ = {"GBufferDummyClosure"};
  Texture dummy_normal_tx_ = {"GBufferDummyNormal"};

 public:
  void acquire(int2 extent, int header_count, int data_count, int normal_count)
  {
    /* Always allocate enough layers so that the frame-buffer attachments are always valid. */
    header_count = max_ii(header_fb_layer_count, header_count);
    data_count = max_ii(closure_fb_layer_count, data_count);
    normal_count = max_ii(normal_fb_layer_count, normal_count);

    eGPUTextureUsage dummy_use = GPU_TEXTURE_USAGE_SHADER_READ | GPU_TEXTURE_USAGE_SHADER_WRITE;
    dummy_header_tx_.ensure_2d_array(gpu::TextureFormat::UINT_32, int2(1), 1, dummy_use);
    dummy_closure_tx_.ensure_2d_array(gpu::TextureFormat::UNORM_10_10_10_2, int2(1), 1, dummy_use);
    dummy_normal_tx_.ensure_2d_array(gpu::TextureFormat::UNORM_16_16, int2(1), 1, dummy_use);

    eGPUTextureUsage usage = GPU_TEXTURE_USAGE_SHADER_READ | GPU_TEXTURE_USAGE_SHADER_WRITE |
                             GPU_TEXTURE_USAGE_ATTACHMENT;
    header_tx.ensure_2d_array(gpu::TextureFormat::UINT_32, extent, header_count, usage);
    closure_tx.ensure_2d_array(gpu::TextureFormat::UNORM_10_10_10_2, extent, data_count, usage);
    normal_tx.ensure_2d_array(gpu::TextureFormat::UNORM_16_16, extent, normal_count, usage);
    /* Ensure layer view for frame-buffer attachment. */
    header_tx.ensure_layer_views();
    closure_tx.ensure_layer_views();
    normal_tx.ensure_layer_views();
    /* Ensure layer view for image store. */
    auto range = [](int layer_count, int fb_layer_count, Texture &tx, Texture &dummy) {
      return (layer_count > fb_layer_count) ?
                 tx.layer_range_view(fb_layer_count, layer_count - fb_layer_count) :
                 dummy;
    };
    header_opt_layers_ = range(header_count, header_fb_layer_count, header_tx, dummy_header_tx_);
    closure_opt_layers_ = range(data_count, closure_fb_layer_count, closure_tx, dummy_closure_tx_);
    normal_opt_layers_ = range(normal_count, normal_fb_layer_count, normal_tx, dummy_normal_tx_);
  }

  /* Bind the GBuffer frame-buffer correctly using the correct workarounds. */
  void bind(Framebuffer &gbuffer_fb)
  {
    /* Workaround a Metal bug that is only showing up on ATI/Intel GPUs. */
    if (GPU_type_matches(
            GPU_DEVICE_ATI | GPU_DEVICE_INTEL | GPU_DEVICE_INTEL_UHD, GPU_OS_MAC, GPU_DRIVER_ANY))
    {
      header_tx.clear(uint4(0));
      GPU_framebuffer_bind(gbuffer_fb);
      return;
    }

    if (!GPU_stencil_export_support()) {
      /* Clearing custom load-store frame-buffers is invalid,
       * clear the stencil as a regular frame-buffer first. */
      GPU_framebuffer_bind(gbuffer_fb);
      GPU_framebuffer_clear_stencil(gbuffer_fb, 0x0u);
    }
    GPU_framebuffer_bind_ex(
        gbuffer_fb,
        {
            {GPU_LOADACTION_LOAD, GPU_STOREACTION_STORE},       /* Depth. */
            {GPU_LOADACTION_LOAD, GPU_STOREACTION_STORE},       /* Combined. */
            {GPU_LOADACTION_CLEAR, GPU_STOREACTION_STORE, {0}}, /* GBuf Header. */
            {GPU_LOADACTION_DONT_CARE, GPU_STOREACTION_STORE},  /* GBuf Normal. */
            {GPU_LOADACTION_DONT_CARE, GPU_STOREACTION_STORE},  /* GBuf Closure. */
            {GPU_LOADACTION_DONT_CARE, GPU_STOREACTION_STORE},  /* GBuf Closure 2. */
        });
  }

  void release()
  {
    /* TODO(fclem): Use texture from pool once they support texture array. */
    // header_tx.release();
    // closure_tx.release();
    // normal_tx.release();

    header_opt_layers_ = nullptr;
    closure_opt_layers_ = nullptr;
    normal_opt_layers_ = nullptr;
  }

  template<typename PassType> void bind_resources(PassType &pass)
  {
    pass.bind_texture("gbuf_header_tx", &header_tx);
    pass.bind_texture("gbuf_closure_tx", &closure_tx);
    pass.bind_texture("gbuf_normal_tx", &normal_tx);
  }

  template<typename PassType> void bind_optional_layers(PassType &pass)
  {
    pass.bind_image(GBUF_NORMAL_SLOT, &normal_opt_layers_);
    pass.bind_image(GBUF_CLOSURE_SLOT, &closure_opt_layers_);
    pass.bind_image(GBUF_HEADER_SLOT, &header_opt_layers_);
  }
};

}  // namespace blender::eevee
