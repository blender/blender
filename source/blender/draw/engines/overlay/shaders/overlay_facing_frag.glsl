/* SPDX-FileCopyrightText: 2018-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/overlay_facing_infos.hh"

FRAGMENT_SHADER_CREATE_INFO(overlay_facing_base)

void main()
{
  frag_color = gl_FrontFacing ? theme.colors.face_front : theme.colors.face_back;
  /* Pre-multiply the output as we do not do any blending in the frame-buffer. */
  frag_color.rgb *= frag_color.a;
}
