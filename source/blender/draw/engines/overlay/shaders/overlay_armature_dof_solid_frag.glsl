/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/overlay_armature_infos.hh"

FRAGMENT_SHADER_CREATE_INFO(overlay_armature_dof)

void main()
{
  frag_color = float4(final_color.rgb, final_color.a * alpha);
  line_output = float4(0.0f);
}
