/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(gpu_shader_material_transform_utils.glsl)

void node_output_material_surface(Closure surface, out Closure out_surface)
{
  out_surface = surface;
}

void node_output_material_volume(Closure volume, out Closure out_volume)
{
  out_volume = volume;
}

void node_output_material_displacement(vec3 displacement, out vec3 out_displacement)
{
  out_displacement = displacement;
}

void node_output_material_thickness(float thickness, out float out_thickness)
{
  vec3 thickness_vec;
  direction_transform_object_to_world(vec3(max(thickness, 0.0)), thickness_vec);
  thickness_vec = abs(thickness_vec);
  /* Contrary to displacement we need to output a scalar quantity.
   * We arbitrarily choose to output the axis with the minimum extent since it is the axis along
   * which the object is usually viewed at. */
  out_thickness = min(min(thickness_vec.x, thickness_vec.y), thickness_vec.z);
}
