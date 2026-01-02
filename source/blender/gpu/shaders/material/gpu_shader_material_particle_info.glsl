/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

[[node]]
void particle_info(float &index,
                   float &random,
                   float &age,
                   float &life_time,
                   float3 &location,
                   float &size,
                   float3 &velocity,
                   float3 &angular_velocity)
{
  /* Unsupported for now. */
  index = 0.0f;
  random = 0.0f;
  age = 0.0f;
  life_time = 0.0f;
  size = 0.0f;

  location = float3(0.0f);
  velocity = float3(0.0f);
  angular_velocity = float3(0.0f);
}
