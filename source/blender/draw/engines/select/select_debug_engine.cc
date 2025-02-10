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

#include "draw_manager.hh"
#include "draw_pass.hh"

#include "select_engine.hh"

#define SELECT_DEBUG_ENGINE "SELECT_DEBUG_ENGINE"

/* -------------------------------------------------------------------- */
/** \name Structs and static variables
 * \{ */

struct SELECTIDDEBUG_Data {
  void *engine_type;
  DRWViewportEmptyList *fbl;
  DRWViewportEmptyList *txl;
  DRWViewportEmptyList *psl;
  DRWViewportEmptyList *stl;
};

static struct {
  struct GPUShader *select_debug_sh;
} e_data = {nullptr}; /* Engine data */

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

  if (!e_data.select_debug_sh) {
    e_data.select_debug_sh = GPU_shader_create_from_info_name("select_debug_fullscreen");
  }

  using namespace blender::draw;

  PassSimple pass = {"SelectEngineDebug"};
  pass.init();
  pass.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND_ALPHA);
  pass.shader_set(e_data.select_debug_sh);
  pass.bind_texture("image", texture_u32);
  pass.bind_texture("image", texture_u32);
  pass.draw_procedural(GPU_PRIM_TRIS, 1, 3);

  DRW_manager_get()->submit(pass);
}

static void select_debug_engine_free()
{
  GPU_SHADER_FREE_SAFE(e_data.select_debug_sh);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Engine Type
 * \{ */

static const DrawEngineDataSize select_debug_data_size = DRW_VIEWPORT_DATA_SIZE(
    SELECTIDDEBUG_Data);

DrawEngineType draw_engine_debug_select_type = {
    /*next*/ nullptr,
    /*prev*/ nullptr,
    /*idname*/ N_("Select ID Debug"),
    /*vedata_size*/ &select_debug_data_size,
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
