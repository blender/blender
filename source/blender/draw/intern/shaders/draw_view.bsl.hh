/* SPDX-FileCopyrightText: 2018-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "draw_shader_shared.hh"

namespace draw {

struct View {
  [[uniform(DRW_VIEW_UBO_SLOT)]] const ViewMatrices view_buf[64];

  /* `view_id` should be the result of `draw::ID::view_id()` or manually indexed view. */
  ViewMatrices get(uint view_id) const
  {
    return view_buf[view_id];
  }
};

}  // namespace draw
