/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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
  out_thickness = thickness;
}
