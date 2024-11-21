/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Display debug edge list.
 */

#include "draw_debug_info.hh"

FRAGMENT_SHADER_CREATE_INFO(draw_debug_draw_display)

void main()
{
  out_color = interp.color;
}
