/* SPDX-FileCopyrightText: 2018-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/workbench_prepass_infos.hh"

FRAGMENT_SHADER_CREATE_INFO(workbench_prepass)
FRAGMENT_SHADER_CREATE_INFO(workbench_opaque)
FRAGMENT_SHADER_CREATE_INFO(workbench_lighting_matcap)

#include "draw_view_lib.glsl"
#include "workbench_common_lib.glsl"
#include "workbench_image_lib.glsl"

void main()
{
  out_object_id = uint(object_id);
  out_normal = workbench_normal_encode(gl_FrontFacing, normal_interp);

  out_material = float4(color_interp, workbench_float_pair_encode(_roughness, metallic));

#ifdef WORKBENCH_COLOR_TEXTURE
  out_material.rgb = workbench_image_color(uv_interp);
#endif

#ifdef WORKBENCH_LIGHTING_MATCAP
  /* For matcaps, save front facing in alpha channel. */
  out_material.a = float(gl_FrontFacing);
#endif
}
