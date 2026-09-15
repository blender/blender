/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_material_interface.bsl.hh"

[[node]]
void node_object_info(float mat_index,
                      float3 &location,
                      float4 &color,
                      float &alpha,
                      float &object_index,
                      float &material_index,
                      float &random)
{
  /* TODO(fclem): EEVEE implementation leaking. */
  location = object_matrices_get().model[3].xyz;
  ObjectInfos info = object_infos_get();
  color = info.ob_color;
  alpha = info.ob_color.a;
  object_index = info.index;
  /* TODO(fclem): Put that inside the Material UBO. */
  material_index = mat_index;
  random = info.random;
}
