/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_CompositorContext.h"

namespace blender::compositor {

CompositorContext::CompositorContext()
{
  scene_ = nullptr;
  rd_ = nullptr;
  quality_ = eCompositorQuality::High;
  hasActiveOpenCLDevices_ = false;
  fast_calculation_ = false;
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

eExecutionModel CompositorContext::get_execution_model() const
{
  if (U.experimental.use_full_frame_compositor) {
    BLI_assert(bnodetree_ != nullptr);
    switch (bnodetree_->execution_mode) {
      case 1:
        return eExecutionModel::FullFrame;
      case 0:
        return eExecutionModel::Tiled;
      default:
        BLI_assert_msg(0, "Invalid execution mode");
    }
  }
  return eExecutionModel::Tiled;
}

}  // namespace blender::compositor
