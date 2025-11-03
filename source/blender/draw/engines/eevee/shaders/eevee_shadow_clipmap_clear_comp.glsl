/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/eevee_shadow_pipeline_infos.hh"

COMPUTE_SHADER_CREATE_INFO(eevee_shadow_clipmap_clear)

#include "gpu_shader_utildefines_lib.glsl"

void main()
{
  int index = int(gl_GlobalInvocationID.x);
  if (index < tilemaps_clip_buf_len) {
    tilemaps_clip_buf[index].clip_far = floatBitsToOrderedInt(-FLT_MAX);
    tilemaps_clip_buf[index].clip_near = floatBitsToOrderedInt(FLT_MAX);
  }
}
