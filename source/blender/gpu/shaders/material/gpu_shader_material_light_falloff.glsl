/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

[[node]]
void node_light_falloff(
    float strength, float tsmooth, float &quadratic, float &linear, float &falloff_constant)
{
  quadratic = strength;
  linear = strength;
  falloff_constant = strength;
}
