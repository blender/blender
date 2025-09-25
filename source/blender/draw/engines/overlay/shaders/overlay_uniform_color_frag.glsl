/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/overlay_paint_infos.hh"

FRAGMENT_SHADER_CREATE_INFO(overlay_paint_face)

void main()
{
  frag_color = ucolor;
#ifdef LINE_OUTPUT
  line_output = float4(0.0f);
#endif
}
