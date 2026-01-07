/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef GPU_SHADER
#  pragma once

#  include "GPU_shader_shared_utils.hh"

namespace blender::draw::select {

#endif

/* Matches eV3DSelectMode */
enum [[host_shared]] SelectType : uint32_t {
  SELECT_ALL = 0u,
  SELECT_PICK_ALL = 1u,
  SELECT_PICK_NEAREST = 2u,
};

struct [[host_shared]] SelectInfoData {
  int2 cursor;
  enum SelectType mode;
  uint _pad0;
};

#ifndef GPU_SHADER
}  // namespace blender::draw::select
#endif
