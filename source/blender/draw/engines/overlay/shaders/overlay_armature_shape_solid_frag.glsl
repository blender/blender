/* SPDX-FileCopyrightText: 2016-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/overlay_armature_infos.hh"

FRAGMENT_SHADER_CREATE_INFO(overlay_armature_shape_solid)

#include "select_lib.glsl"

void main()
{
  /* Manual back-face culling. Not ideal for performance
   * but needed for view clarity in X-ray mode and support
   * for inverted bone matrices. */
  if ((inverted == 1) == gl_FrontFacing) {
    gpu_discard_fragment();
    return;
  }
  frag_color = float4(final_color.rgb, alpha);
  line_output = float4(0.0f);
  select_id_output(select_id);
}
