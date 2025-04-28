/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

void node_object_info(float mat_index,
                      out float3 location,
                      out float4 color,
                      out float alpha,
                      out float object_index,
                      out float material_index,
                      out float random)
{
  location = drw_modelmat()[3].xyz;
  ObjectInfos info = drw_object_infos();
  color = info.ob_color;
  alpha = info.ob_color.a;
  object_index = info.index;
  /* TODO(fclem): Put that inside the Material UBO. */
  material_index = mat_index;
  random = info.random;
}
