/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#pragma once

#include "GPU_shader.hh"

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
  /* Accumulation antialiasing */
  StaticShader accumulation = {"gpencil_antialiasing_accumulation"};
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
