/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "eevee_shader_shared.hh"

namespace blender::eevee {

class Instance;

class IrradianceCache {
 private:
  Instance &inst_;

  DebugSurfelBuf debug_surfels;
  PassSimple debug_surfels_ps_ = {"IrradianceCache.Debug"};
  GPUShader *debug_surfels_sh_ = nullptr;

  /* TODO: Remove this. */
  void generate_random_surfels();

 public:
  IrradianceCache(Instance &inst) : inst_(inst){};
  ~IrradianceCache(){};

  void init();
  void sync();

  void debug_pass_sync();
  void debug_draw(View &view, GPUFrameBuffer *view_fb);
};

}  // namespace blender::eevee
