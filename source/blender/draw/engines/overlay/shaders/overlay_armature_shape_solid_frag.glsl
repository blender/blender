/* SPDX-FileCopyrightText: 2016-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "select_lib.glsl"

void main()
{
  /* Manual back-face culling. Not ideal for performance
   * but needed for view clarity in X-ray mode and support
   * for inverted bone matrices. */
  if ((inverted == 1) == gl_FrontFacing) {
    discard;
    return;
  }
  fragColor = vec4(finalColor.rgb, alpha);
  lineOutput = vec4(0.0);
  select_id_output(select_id);
}
