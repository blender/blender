/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/workbench_shadow_infos.hh"

FRAGMENT_SHADER_CREATE_INFO(workbench_shadow_common)

void main()
{
  /* No color output, only depth (line below is implicit). */
  // gl_FragDepth = gl_FragCoord.z;
}
