/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "DRW_render.hh"

namespace blender::draw::compositor_engine {

struct Engine : public DrawEngine::Pointer {
  DrawEngine *create_instance() final;
};

}  // namespace blender::draw::compositor_engine
