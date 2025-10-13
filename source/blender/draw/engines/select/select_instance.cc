/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup select
 */

#include "DRW_render.hh"

#include "select_engine.hh"

#include "../overlay/overlay_instance.hh"
#include "select_instance.hh"

namespace blender::draw::select {

class Instance : public overlay::Instance {
 public:
  Instance() : overlay::Instance(SelectionType::ENABLED) {};
};

DrawEngine *Engine::create_instance()
{
  return new Instance();
}

}  // namespace blender::draw::select
