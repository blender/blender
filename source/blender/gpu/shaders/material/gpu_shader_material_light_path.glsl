/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

void node_light_path(float &is_camera_ray,
                     float &is_shadow_ray,
                     float &is_diffuse_ray,
                     float &is_glossy_ray,
                     float &is_singular_ray,
                     float &is_reflection_ray,
                     float &is_transmission_ray,
                     float &is_volume_scatter_ray,
                     float &ray_length,
                     float &ray_depth,
                     float &diffuse_depth,
                     float &glossy_depth,
                     float &transparent_depth,
                     float &transmission_depth,
                     float &path_depth)
{
  /* Supported. */
  is_camera_ray = float(g_data.ray_type == RAY_TYPE_CAMERA);
  is_shadow_ray = float(g_data.ray_type == RAY_TYPE_SHADOW);
  is_diffuse_ray = float(g_data.ray_type == RAY_TYPE_DIFFUSE);
  is_glossy_ray = float(g_data.ray_type == RAY_TYPE_GLOSSY);
  /* Kind of supported. */
  is_singular_ray = is_glossy_ray;
  is_reflection_ray = is_glossy_ray;
  is_transmission_ray = is_glossy_ray;
  ray_depth = g_data.ray_depth;
  diffuse_depth = (is_diffuse_ray == 1.0f) ? g_data.ray_depth : 0.0f;
  glossy_depth = (is_glossy_ray == 1.0f) ? g_data.ray_depth : 0.0f;
  transmission_depth = (is_transmission_ray == 1.0f) ? glossy_depth : 0.0f;
  ray_length = g_data.ray_length;
  /* Not supported. */
  transparent_depth = 0.0f;
  is_volume_scatter_ray = 0.0f;
  path_depth = 0.0f;
}
