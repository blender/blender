/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_CompositorContext.h"

namespace blender::compositor {

CompositorContext::CompositorContext()
{
  scene_ = nullptr;
  rd_ = nullptr;
  bnodetree_ = nullptr;
}

int CompositorContext::get_framenumber() const
{
  BLI_assert(rd_);
  return rd_->cfra;
}

Size2f CompositorContext::get_render_size() const
{
  return {get_render_data()->xsch * get_render_percentage_as_factor(),
          get_render_data()->ysch * get_render_percentage_as_factor()};
}

}  // namespace blender::compositor
