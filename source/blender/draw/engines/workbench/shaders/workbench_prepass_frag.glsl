/* SPDX-FileCopyrightText: 2018-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(common_view_lib.glsl)
#pragma BLENDER_REQUIRE(workbench_common_lib.glsl)
#pragma BLENDER_REQUIRE(workbench_image_lib.glsl)

#ifdef WORKBENCH_NEXT

void main()
{
  out_object_id = uint(object_id);
  out_normal = workbench_normal_encode(gl_FrontFacing, normal_interp);

  out_material = vec4(color_interp, workbench_float_pair_encode(_roughness, metallic));

#  ifdef WORKBENCH_COLOR_TEXTURE
  out_material.rgb = workbench_image_color(uv_interp);
#  endif

#  ifdef WORKBENCH_LIGHTING_MATCAP
  /* For matcaps, save front facing in alpha channel. */
  out_material.a = float(gl_FrontFacing);
#  endif
}

#else

void main()
{
  out_normal = workbench_normal_encode(gl_FrontFacing, normal_interp);

  out_material = vec4(color_interp, workbench_float_pair_encode(_roughness, metallic));

  out_object_id = uint(object_id);

  if (useMatcap) {
    /* For matcaps, save front facing in alpha channel. */
    out_material.a = float(gl_FrontFacing);
  }

#  ifdef WORKBENCH_COLOR_TEXTURE
  out_material.rgb = workbench_image_color(uv_interp);
#  endif
}

#endif
