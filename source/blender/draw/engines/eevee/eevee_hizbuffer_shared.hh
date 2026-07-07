/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Shared code between host and client code-bases.
 */

#pragma once

#include "GPU_shader_shared_utils.hh"

#ifndef GPU_SHADER
namespace blender::eevee {
#endif

struct [[host_shared]] HiZData {
  /** Scale factor to remove HiZBuffer padding. */
  float2 uv_scale;

  float2 _pad0;
};

#ifndef GPU_SHADER
}  // namespace blender::eevee
#endif
