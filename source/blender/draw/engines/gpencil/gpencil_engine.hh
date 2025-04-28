/* SPDX-FileCopyrightText: 2017 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#pragma once

#include "DRW_render.hh"

namespace blender::draw::gpencil {

struct Engine : public DrawEngine::Pointer {
  DrawEngine *create_instance() final;

  static void render_to_image(RenderEngine *engine, RenderLayer *render_layer, const rcti rect);
  static void free_static();
};

}  // namespace blender::draw::gpencil
