/* SPDX-FileCopyrightText: 2016 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 */

#pragma once

#include "DRW_render.hh"
#include "RE_engine.h"

namespace blender {

extern RenderEngineType DRW_engine_viewport_eevee_type;

namespace eevee {

struct Engine : public DrawEngine::Pointer {
  DrawEngine *create_instance() final;

  static void free_static();
};

}  // namespace eevee
}  // namespace blender
