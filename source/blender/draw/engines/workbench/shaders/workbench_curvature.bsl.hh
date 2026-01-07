/* SPDX-FileCopyrightText: 2018-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "workbench_common.bsl.hh"

namespace workbench {

float curvature_soft_clamp(float curvature, float control)
{
  if (curvature < 0.5f / control) {
    return curvature * (1.0f - curvature * control);
  }
  return 0.25f / control;
}

void curvature_compute([[resource_table]] const workbench::World &world,
                       usampler2D object_id_tx,
                       sampler2D normal_tx,
                       float2 uv,
                       float &curvature)
{
  curvature = 0.0f;

  const WorldData &world_data = world.world_data;

  float3 offset = float3(world_data.viewport_size_inv, 0.0f) * world_data.ui_scale;
  uint object_up = texture(object_id_tx, uv + offset.zy).r;
  uint object_down = texture(object_id_tx, uv - offset.zy).r;
  uint object_right = texture(object_id_tx, uv + offset.xz).r;
  uint object_left = texture(object_id_tx, uv - offset.xz).r;

  /* Remove object outlines. */
  if ((object_up != object_down) || (object_right != object_left)) {
    return;
  }
  /* Avoid shading background pixels. */
  if ((object_up == object_right) && (object_right == 0u)) {
    return;
  }

  float normal_up = workbench::normal_decode(texture(normal_tx, uv + offset.zy)).g;
  float normal_down = workbench::normal_decode(texture(normal_tx, uv - offset.zy)).g;
  float normal_right = workbench::normal_decode(texture(normal_tx, uv + offset.xz)).r;
  float normal_left = workbench::normal_decode(texture(normal_tx, uv - offset.xz)).r;

  float normal_diff = (normal_up - normal_down) + (normal_right - normal_left);

  if (normal_diff < 0) {
    curvature = -2.0f * curvature_soft_clamp(-normal_diff, world_data.curvature_valley);
  }
  else {
    curvature = 2.0f * curvature_soft_clamp(normal_diff, world_data.curvature_ridge);
  }
}

}  // namespace workbench
