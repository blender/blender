/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

void particle_info(out float index,
                   out float random,
                   out float age,
                   out float life_time,
                   out vec3 location,
                   out float size,
                   out vec3 velocity,
                   out vec3 angular_velocity)
{
  /* Unsupported for now. */
  index = 0.0;
  random = 0.0;
  age = 0.0;
  life_time = 0.0;
  size = 0.0;

  location = vec3(0.0);
  velocity = vec3(0.0);
  angular_velocity = vec3(0.0);
}
