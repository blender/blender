/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/overlay_armature_info.hh"

FRAGMENT_SHADER_CREATE_INFO(overlay_armature_dof)

void main()
{
  fragColor = float4(finalColor.rgb, finalColor.a * alpha);
  lineOutput = float4(0.0f);
}
