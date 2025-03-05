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

#define SELECT_DEBUG_ENGINE "SELECT_DEBUG_ENGINE"

/* -------------------------------------------------------------------- */
/** \name Structs and static variables
 * \{ */

struct SELECTIDDEBUG_Data {
  void *engine_type;
};

namespace blender::draw::SelectDebug {

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

  StaticShader select_debug = {"select_debug_fullscreen"};
};

}  // namespace blender::draw::SelectDebug

using namespace blender::draw::SelectDebug;

/** \} */

/* -------------------------------------------------------------------- */
/** \name Engine Functions
 * \{ */

static void select_debug_draw_scene(void * /*vedata*/)
{
  GPUTexture *texture_u32 = DRW_engine_select_texture_get();
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

  DRW_manager_get()->submit(pass);
}

static void select_debug_engine_free()
{
  ShaderCache::release();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Engine Type
 * \{ */

DrawEngineType draw_engine_debug_select_type = {
    /*next*/ nullptr,
    /*prev*/ nullptr,
    /*idname*/ N_("Select ID Debug"),
    /*engine_init*/ nullptr,
    /*engine_free*/ &select_debug_engine_free,
    /*instance_free*/ nullptr,
    /*cache_init*/ nullptr,
    /*cache_populate*/ nullptr,
    /*cache_finish*/ nullptr,
    /*draw_scene*/ &select_debug_draw_scene,
    /*view_update*/ nullptr,
    /*id_update*/ nullptr,
    /*render_to_image*/ nullptr,
    /*store_metadata*/ nullptr,
};

/** \} */

#undef SELECT_DEBUG_ENGINE
