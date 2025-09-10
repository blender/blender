/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "overlay_mode_transfer.hh"

#include "BLI_math_color.h"

#include "ED_object.hh"

namespace blender::draw::overlay {

void ModeTransfer::begin_sync(Resources &res, const State &state)
{
  object_factors_ = ed::object::mode_transfer_overlay_current_state();

  enabled_ = state.is_space_v3d() && !res.is_selection() && !object_factors_.is_empty();

  if (!enabled_) {
    /* Not used. But release the data. */
    ps_.init();
    return;
  }

  UI_GetThemeColor3fv(TH_VERTEX_SELECT, flash_color_);
  srgb_to_linearrgb_v4(flash_color_, flash_color_);

  ps_.init();
  ps_.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_LESS_EQUAL | DRW_STATE_WRITE_DEPTH,
                state.clipping_plane_count);
  ps_.shader_set(res.shaders->uniform_color.get());
  ps_.bind_ubo(OVERLAY_GLOBALS_SLOT, &res.globals_buf);
  ps_.bind_ubo(DRW_CLIPPING_UBO_SLOT, &res.clip_planes_buf);
}

}  // namespace blender::draw::overlay
