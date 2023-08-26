/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

void node_light_falloff(float strength,
                        float tsmooth,
                        out float quadratic,
                        out float linear,
                        out float falloff_constant)
{
  quadratic = strength;
  linear = strength;
  falloff_constant = strength;
}
