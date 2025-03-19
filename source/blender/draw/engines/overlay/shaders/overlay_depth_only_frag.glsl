/* SPDX-FileCopyrightText: 2016-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "select_lib.glsl"

void main()
{
#ifdef SELECT_ENABLE
  if (globalsBlock.backface_culling && !gl_FrontFacing) {
    /* Return early since we are not using early depth testing. */
    return;
  }
#endif
  /* No color output, only depth (line below is implicit). */
  // gl_FragDepth = gl_FragCoord.z;

  /* This is optimized to NOP in the non select case. */
  select_id_output(select_id);
}
