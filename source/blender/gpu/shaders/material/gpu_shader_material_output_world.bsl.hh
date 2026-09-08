/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_material_interface.bsl.hh"

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
