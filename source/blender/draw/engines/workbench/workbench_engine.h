/* SPDX-FileCopyrightText: 2016 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw_engine
 */

#pragma once

#include "DRW_render.hh"

struct RenderEngineType;

extern RenderEngineType DRW_engine_viewport_workbench_type;

namespace blender::workbench {

struct Engine : public DrawEngine::Pointer {
  DrawEngine *create_instance() final;

  static void free_static();
};

}  // namespace blender::workbench
