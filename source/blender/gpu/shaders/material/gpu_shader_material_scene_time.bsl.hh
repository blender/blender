/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_material_interface.bsl.hh"

[[node]]
void node_scene_time([[resource_table]] KernelGlobals &kg, float &seconds, float &frame)
{
  scene_time_uniforms(kg, seconds, frame);
}
