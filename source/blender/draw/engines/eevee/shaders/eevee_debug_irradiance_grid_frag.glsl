/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/eevee_lightprobe_volume_infos.hh"

FRAGMENT_SHADER_CREATE_INFO(eevee_debug_irradiance_grid)

void main()
{
  out_color = interp_color;
}
