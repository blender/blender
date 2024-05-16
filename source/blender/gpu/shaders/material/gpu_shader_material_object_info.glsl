/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

void node_object_info(float mat_index,
                      out vec3 location,
                      out vec4 color,
                      out float alpha,
                      out float object_index,
                      out float material_index,
                      out float random)
{
  location = ModelMatrix[3].xyz;
  color = ObjectColor;
  alpha = ObjectColor.a;
#ifdef OBINFO_NEW
  object_index = floatBitsToUint(ObjectInfo.x);
#else
  object_index = ObjectInfo.x;
#endif
  /* TODO(fclem): Put that inside the Material UBO. */
  material_index = mat_index;
  random = ObjectInfo.z;
}
