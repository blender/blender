/* SPDX-FileCopyrightText: 2019 Blender Foundation.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw_engine
 *
 * Engine for debugging the selection map drawing.
 */

#include "DNA_ID.h"
#include "DNA_vec_types.h"

#include "DRW_engine.h"
#include "DRW_select_buffer.h"

#include "draw_cache.h"
#include "draw_manager.h"

#include "select_engine.h"

#define SELECT_DEBUG_ENGINE "SELECT_DEBUG_ENGINE"

/* -------------------------------------------------------------------- */
/** \name Structs and static variables
 * \{ */

typedef struct SELECTIDDEBUG_PassList {
  struct DRWPass *debug_pass;
} SELECTIDDEBUG_PassList;

typedef struct SELECTIDDEBUG_Data {
  void *engine_type;
  DRWViewportEmptyList *fbl;
  DRWViewportEmptyList *txl;
  SELECTIDDEBUG_PassList *psl;
  DRWViewportEmptyList *stl;
} SELECTIDDEBUG_Data;

static struct {
  struct GPUShader *select_debug_sh;
} e_data = {{NULL}}; /* Engine data */

/** \} */

/* -------------------------------------------------------------------- */
/** \name Engine Functions
 * \{ */

static void select_debug_engine_init(void *vedata)
{
  SELECTIDDEBUG_PassList *psl = ((SELECTIDDEBUG_Data *)vedata)->psl;

  if (!e_data.select_debug_sh) {
    e_data.select_debug_sh = DRW_shader_create_fullscreen(
        "uniform usampler2D image;"
        "in vec4 uvcoordsvar;"
        "out vec4 fragColor;"
        "void main() {"
        "  uint px = texture(image, uvcoordsvar.xy).r;"
        "  fragColor = vec4(1.0, 1.0, 1.0, 0.0);"
        "  if (px != 0u) {"
        "    fragColor.a = 1.0;"
        "    px &= 0x3Fu;"
        "    fragColor.r = ((px >> 0) & 0x3u) / float(0x3u);"
        "    fragColor.g = ((px >> 2) & 0x3u) / float(0x3u);"
        "    fragColor.b = ((px >> 4) & 0x3u) / float(0x3u);"
        "  }"
        "}\n",
        NULL);
  }

  psl->debug_pass = DRW_pass_create("Debug Pass", DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND_ALPHA);
  GPUTexture *texture_u32 = DRW_engine_select_texture_get();
  if (texture_u32) {
    DRWShadingGroup *shgrp = DRW_shgroup_create(e_data.select_debug_sh, psl->debug_pass);
    DRW_shgroup_uniform_texture(shgrp, "image", texture_u32);
    DRW_shgroup_call_procedural_triangles(shgrp, NULL, 1);
  }
}

static void select_debug_draw_scene(void *vedata)
{
  SELECTIDDEBUG_PassList *psl = ((SELECTIDDEBUG_Data *)vedata)->psl;
  DRW_draw_pass(psl->debug_pass);
}

static void select_debug_engine_free(void)
{
  DRW_SHADER_FREE_SAFE(e_data.select_debug_sh);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Engine Type
 * \{ */

static const DrawEngineDataSize select_debug_data_size = DRW_VIEWPORT_DATA_SIZE(
    SELECTIDDEBUG_Data);

DrawEngineType draw_engine_debug_select_type = {
    NULL,
    NULL,
    N_("Select ID Debug"),
    &select_debug_data_size,
    &select_debug_engine_init,
    &select_debug_engine_free,
    /*instance_free*/ NULL,
    NULL,
    NULL,
    NULL,
    &select_debug_draw_scene,
    NULL,
    NULL,
    NULL,
    NULL,
};

/** \} */

#undef SELECT_DEBUG_ENGINE
