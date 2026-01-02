/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

[[node]]
void node_output_world_surface(Closure surface, Closure &out_surface)
{
  out_surface = surface;
}

[[node]]
void node_output_world_volume(Closure volume, Closure &out_volume)
{
  out_volume = volume;
}
