/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Virtual shadow-mapping: Usage tagging
 *
 * Shadow pages are only allocated if they are visible.
 * This pass iterates the surfels buffer and tag all tiles that are needed for light shadowing as
 * needed.
 */

#include "infos/eevee_shadow_pipeline_infos.hh"

COMPUTE_SHADER_CREATE_INFO(eevee_shadow_tag_usage_surfels)

#include "eevee_shadow_tag_usage_lib.glsl"

void main()
{
  uint index = gl_GlobalInvocationID.x;
  if (index >= capture_info_buf.surfel_len) {
    return;
  }

  Surfel surfel = surfel_buf[index];
  shadow_tag_usage_surfel(surfel, directional_level);
}
