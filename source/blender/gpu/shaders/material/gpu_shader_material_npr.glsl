/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

void npr_input(out vec4 combined_color,
               out vec4 diffuse_color,
               out vec4 diffuse_direct,
               out vec4 diffuse_indirect,
               out vec4 specular_color,
               out vec4 specular_direct,
               out vec4 specular_indirect)
{
#if defined(NPR_SHADER) && defined(GPU_FRAGMENT_SHADER)
  combined_color = g_combined_color;
  diffuse_color = g_diffuse_color;
  diffuse_direct = g_diffuse_direct;
  diffuse_indirect = g_diffuse_indirect;
  specular_color = g_specular_color;
  specular_direct = g_specular_direct;
  specular_indirect = g_specular_indirect;
#else
  combined_color = vec4(0.0);
  diffuse_color = vec4(0.0);
  diffuse_direct = vec4(0.0);
  diffuse_indirect = vec4(0.0);
  specular_color = vec4(0.0);
  specular_direct = vec4(0.0);
  specular_indirect = vec4(0.0);
#endif
}

void npr_output(vec4 color, out vec4 out_color)
{
  out_color = color;
}
