/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "GPU_shader_shared_utils.hh"

enum [[host_shared]] eImageDrawFlags : uint32_t {
  IMAGE_DRAW_FLAG_DEFAULT = 0,
  IMAGE_DRAW_FLAG_SHOW_ALPHA = (1 << 0),
  IMAGE_DRAW_FLAG_APPLY_ALPHA = (1 << 1),
  IMAGE_DRAW_FLAG_SHUFFLING = (1 << 2),
  IMAGE_DRAW_FLAG_DEPTH = (1 << 3),
};
#ifndef GPU_SHADER
ENUM_OPERATORS(eImageDrawFlags)
#endif
