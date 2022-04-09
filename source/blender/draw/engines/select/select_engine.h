/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2019 Blender Foundation. */

/** \file
 * \ingroup draw_engine
 */

#pragma once

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
