/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw_engine
 *
 * Engine for drawing a selection map where the pixels indicate the selection indices.
 */

#include "overlay_instance.hh"

#include "overlay_engine.h"

namespace blender::draw::overlay {

DrawEngine *Engine::create_instance()
{
  return new Instance();
}

void Engine::free_static()
{
  ShaderModule::module_free();
}

}  // namespace blender::draw::overlay
