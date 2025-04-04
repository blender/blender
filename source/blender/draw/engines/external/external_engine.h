/* SPDX-FileCopyrightText: 2017 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw_engine
 */

#pragma once

#include "DRW_render.hh"

struct RenderEngineType;

extern RenderEngineType DRW_engine_viewport_external_type;

/* Check whether an external engine is to be used to draw content of an image editor.
 * If the drawing is possible, the render engine is "acquired" so that it is not freed by the
 * render engine for until drawing is finished.
 *
 * NOTE: Released by the draw engine when it is done drawing. */
bool DRW_engine_external_acquire_for_image_editor(const DRWContext *draw_ctx);

namespace blender::draw::external {

struct Engine : public DrawEngine::Pointer {
  DrawEngine *create_instance() final;
};

}  // namespace blender::draw::external
