/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/overlay_sculpt_curves_infos.hh"

FRAGMENT_SHADER_CREATE_INFO(overlay_sculpt_curves_selection)

void main()
{
  out_color = float4(float3(0.0f), 1.0f - mask_weight);
}
