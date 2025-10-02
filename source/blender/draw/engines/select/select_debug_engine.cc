/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw_engine
 *
 * Engine for debugging the selection map drawing.
 */

#include "BLT_translation.hh"

#include "DNA_ID.h"

#include "DRW_engine.hh"
#include "DRW_render.hh"

#include "draw_manager.hh"
#include "draw_pass.hh"

#include "select_engine.hh"

/* -------------------------------------------------------------------- */
/** \name Structs and static variables
 * \{ */

namespace blender::draw::edit_select_debug {

class ShaderCache {
  using StaticShader = gpu::StaticShader;

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

  StaticShader select_debug = {"select_debug_fullscreen"};
};

class Instance : public DrawEngine {
  StringRefNull name_get() final
  {
    return "Select ID Debug";
  }

  void init() final {};
  void begin_sync() final {};
  void object_sync(blender::draw::ObjectRef & /*ob_ref*/,
                   blender::draw::Manager & /*manager*/) final {};
  void end_sync() final {};

  void draw(blender::draw::Manager &manager) final
  {
    gpu::Texture *texture_u32 = DRW_engine_select_texture_get();
    if (texture_u32 == nullptr) {
      return;
    }
    using namespace blender::draw;

    PassSimple pass = {"SelectEngineDebug"};
    pass.init();
    pass.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND_ALPHA);
    pass.shader_set(ShaderCache::get().select_debug.get());
    pass.bind_texture("image", texture_u32);
    pass.bind_texture("image", texture_u32);
    pass.draw_procedural(GPU_PRIM_TRIS, 1, 3);

    DRW_submission_start();
    manager.submit(pass);
    DRW_submission_end();
  }
};

DrawEngine *Engine::create_instance()
{
  return new Instance();
}

void Engine::free_static()
{
  ShaderCache::release();
}

}  // namespace blender::draw::edit_select_debug
