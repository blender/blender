/* SPDX-FileCopyrightText: 2020-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

void workbench_material_data_get(int handle,
                                 vec3 vertex_color,
                                 out vec3 color,
                                 out float alpha,
                                 out float roughness,
                                 out float metallic)
{
#ifndef WORKBENCH_NEXT
  handle = (materialIndex != -1) ? materialIndex : handle;
  vec4 data = materials_data[uint(handle) & 0xFFFu];
  color = data.rgb;
  if (materialIndex == 0) {
    color = vertex_color;
  }
#else

#  ifdef WORKBENCH_COLOR_MATERIAL
  vec4 data = materials_data[handle];
#  else
  vec4 data = vec4(0.0);
#  endif
  color = (data.r == -1) ? vertex_color : data.rgb;
#endif

  uint encoded_data = floatBitsToUint(data.w);
  alpha = float((encoded_data >> 16u) & 0xFFu) * (1.0 / 255.0);
  roughness = float((encoded_data >> 8u) & 0xFFu) * (1.0 / 255.0);
  metallic = float(encoded_data & 0xFFu) * (1.0 / 255.0);
}
