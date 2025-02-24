/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/overlay_paint_info.hh"

FRAGMENT_SHADER_CREATE_INFO(overlay_paint_face)

void main()
{
  fragColor = ucolor;
#ifdef LINE_OUTPUT
  lineOutput = vec4(0.0);
#endif
}
