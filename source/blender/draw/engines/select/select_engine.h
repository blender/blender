/* SPDX-FileCopyrightText: 2019 Blender Foundation.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw_engine
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* select_engine.c */

extern DrawEngineType draw_engine_select_type;
extern RenderEngineType DRW_engine_viewport_select_type;

#ifdef WITH_DRAW_DEBUG
/* select_debug_engine.c */

extern DrawEngineType draw_engine_debug_select_type;
#endif

struct SELECTID_Context *DRW_select_engine_context_get(void);

struct GPUFrameBuffer *DRW_engine_select_framebuffer_get(void);
struct GPUTexture *DRW_engine_select_texture_get(void);

/* select_instance.cc */

extern DrawEngineType draw_engine_select_next_type;

#ifdef __cplusplus
}
#endif
