/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#pragma once

#include "draw_command_shared.hh"

#ifndef GPU_SHADER
#  include "BLI_math_vector.hh"

namespace blender {

using namespace blender::math;

#endif

/* -------------------------------------------------------------------- */
/** \name Debug draw shapes
 * \{ */

struct [[host_shared]] DRWDebugVertPair {
  /* This is a weird layout, but needed to be able to use DRWDebugVertPair as
   * a DrawCommand and avoid alignment issues. See drw_debug_lines_buf[] definition. */
  uint pos1_x;
  uint pos1_y;
  uint pos1_z;
  /* Named vert_color to avoid global namespace collision with uniform color. */
  uint vert_color;

  uint pos2_x;
  uint pos2_y;
  uint pos2_z;
  /* Number of time this line is supposed to be displayed. Decremented by one on display. */
  uint lifetime;
};

inline DRWDebugVertPair debug_line_make(uint in_pos1_x,
                                        uint in_pos1_y,
                                        uint in_pos1_z,
                                        uint in_pos2_x,
                                        uint in_pos2_y,
                                        uint in_pos2_z,
                                        uint in_vert_color,
                                        uint in_lifetime)
{
  DRWDebugVertPair debug_vert;
  debug_vert.pos1_x = in_pos1_x;
  debug_vert.pos1_y = in_pos1_y;
  debug_vert.pos1_z = in_pos1_z;
  debug_vert.pos2_x = in_pos2_x;
  debug_vert.pos2_y = in_pos2_y;
  debug_vert.pos2_z = in_pos2_z;
  debug_vert.vert_color = in_vert_color;
  debug_vert.lifetime = in_lifetime;
  return debug_vert;
}

inline uint debug_color_pack(float4 v_color)
{
  v_color = clamp(v_color, 0.0f, 1.0f);
  uint result = 0;
  result |= uint(v_color.x * 255.0) << 0u;
  result |= uint(v_color.y * 255.0) << 8u;
  result |= uint(v_color.z * 255.0) << 16u;
  result |= uint(v_color.w * 255.0) << 24u;
  return result;
}

/* Take the header (DrawCommand) into account. */
#define DRW_DEBUG_DRAW_VERT_MAX (2 * 1024) - 1

/* The debug draw buffer is laid-out as the following struct.
 * But we use plain array in shader code instead because of driver issues. */
struct [[host_shared]] DRWDebugDrawBuffer {
  struct DrawCommand command;
  struct DRWDebugVertPair verts[DRW_DEBUG_DRAW_VERT_MAX];
};

/* Equivalent to `DRWDebugDrawBuffer.command.v_count`. */
#define drw_debug_draw_v_count(buf) buf[0].pos1_x
/**
 * Offset to the first data. Equal to: `sizeof(DrawCommand) / sizeof(DRWDebugVertPair)`.
 * This is needed because we bind the whole buffer as a `DRWDebugVertPair` array.
 */
#define drw_debug_draw_offset 1

/** \} */

#ifndef GPU_SHADER
}  // namespace blender
#endif
