/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw_engine
 */

#pragma once

#include "DRW_render.hh"

namespace blender {

/* `select_engine.cc` */

#ifdef WITH_DRAW_DEBUG
/* `select_debug_engine.cc` */

namespace draw::edit_select_debug {

struct Engine : public DrawEngine::Pointer {
  DrawEngine *create_instance() final;

  static void free_static();
};

}  // namespace draw::edit_select_debug

#endif

struct SELECTID_Context *DRW_select_engine_context_get();
gpu::FrameBuffer *DRW_engine_select_framebuffer_get();
gpu::Texture *DRW_engine_select_texture_get();

/* select_instance.cc */

namespace draw::select {

struct Engine : public DrawEngine::Pointer {
  DrawEngine *create_instance() final;
};

}  // namespace draw::select

namespace draw::edit_select {

struct Engine : public DrawEngine::Pointer {
  DrawEngine *create_instance() final;

  static void free_static();
};

}  // namespace draw::edit_select

}  // namespace blender
