/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */
#include "DRW_render.hh"

#include "BLI_string.h"

#include "gpencil_engine.h"

namespace blender::draw::gpencil {

using StaticShader = gpu::StaticShader;

class ShaderCache {
 private:
  static gpu::StaticShaderCache<ShaderCache> &get_static_cache()
  {
    static gpu::StaticShaderCache<ShaderCache> static_cache;
    return static_cache;
  }

 public:
  static ShaderCache &get()
  {
    return get_static_cache().get();
  }
  static void release()
  {
    get_static_cache().release();
  }

  /* SMAA antialiasing */
  StaticShader antialiasing[3] = {{"gpencil_antialiasing_stage_0"},
                                  {"gpencil_antialiasing_stage_1"},
                                  {"gpencil_antialiasing_stage_2"}};
  /* GPencil Object rendering */
  StaticShader geometry = {"gpencil_geometry"};
  /* All layer blend types in one shader! */
  StaticShader layer_blend = {"gpencil_layer_blend"};
  /* Merge the final object depth to the depth buffer. */
  StaticShader depth_merge = {"gpencil_depth_merge"};
  /* Invert the content of the mask buffer. */
  StaticShader mask_invert = {"gpencil_mask_invert"};
  /* Effects. */
  StaticShader fx_composite = {"gpencil_fx_composite"};
  StaticShader fx_colorize = {"gpencil_fx_colorize"};
  StaticShader fx_blur = {"gpencil_fx_blur"};
  StaticShader fx_glow = {"gpencil_fx_glow"};
  StaticShader fx_pixelize = {"gpencil_fx_pixelize"};
  StaticShader fx_rim = {"gpencil_fx_rim"};
  StaticShader fx_shadow = {"gpencil_fx_shadow"};
  StaticShader fx_transform = {"gpencil_fx_transform"};
};

}  // namespace blender::draw::gpencil

using namespace blender::draw::gpencil;

void GPENCIL_shader_free()
{
  ShaderCache::get().release();
}

GPUShader *GPENCIL_shader_antialiasing(int stage)
{
  BLI_assert(stage < 3);
  return ShaderCache::get().antialiasing[stage].get();
}

GPUShader *GPENCIL_shader_geometry_get()
{
  return ShaderCache::get().geometry.get();
}

GPUShader *GPENCIL_shader_layer_blend_get()
{
  return ShaderCache::get().layer_blend.get();
}

GPUShader *GPENCIL_shader_mask_invert_get()
{
  return ShaderCache::get().mask_invert.get();
}

GPUShader *GPENCIL_shader_depth_merge_get()
{
  return ShaderCache::get().depth_merge.get();
}

/* ------- FX Shaders --------- */

GPUShader *GPENCIL_shader_fx_blur_get()
{
  return ShaderCache::get().fx_blur.get();
}

GPUShader *GPENCIL_shader_fx_colorize_get()
{
  return ShaderCache::get().fx_colorize.get();
}

GPUShader *GPENCIL_shader_fx_composite_get()
{
  return ShaderCache::get().fx_composite.get();
}

GPUShader *GPENCIL_shader_fx_glow_get()
{
  return ShaderCache::get().fx_glow.get();
}

GPUShader *GPENCIL_shader_fx_pixelize_get()
{
  return ShaderCache::get().fx_pixelize.get();
}

GPUShader *GPENCIL_shader_fx_rim_get()
{
  return ShaderCache::get().fx_rim.get();
}

GPUShader *GPENCIL_shader_fx_shadow_get()
{
  return ShaderCache::get().fx_shadow.get();
}

GPUShader *GPENCIL_shader_fx_transform_get()
{
  return ShaderCache::get().fx_transform.get();
}
