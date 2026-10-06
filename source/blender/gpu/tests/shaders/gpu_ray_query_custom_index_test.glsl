/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/gpu_shader_test_infos.hh"

COMPUTE_SHADER_CREATE_INFO(gpu_ray_query_custom_index_test)

void main()
{
  uint ray_index = gl_GlobalInvocationID.x;

  rayQuery query;
  rayQueryInitialize(query,
                     scene_as,
                     gpu_RayFlagsTerminateOnFirstHit,
                     0xFFu,
                     ray_pos_in[ray_index].xyz,
                     0.01,
                     ray_dir_in[ray_index].xyz,
                     5.0);
  rayQueryProceed(query);

  custom_index_out[ray_index] = rayQueryGetIntersectionInstanceCustomIndex(query, true);
}
