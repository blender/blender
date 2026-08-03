/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_compat.hh"

#include "infos/gpu_shader_test_infos.hh"

#ifdef GPU_VERTEX_SHADER
VERTEX_SHADER_CREATE_INFO(gpu_ray_query_raster_test)

void main()
{
  /* Full-screen triangle. */
  int v = gl_VertexID % 3;
  float x = -1.0f + float((v & 1) << 2);
  float y = -1.0f + float((v & 2) << 1);
  gl_Position = float4(x, y, 0.0f, 1.0f);
}
#endif

#ifdef GPU_FRAGMENT_SHADER
FRAGMENT_SHADER_CREATE_INFO(gpu_ray_query_raster_test)

void main()
{
  int ray_index = int(gl_FragCoord.x);

  rayQueryEXT query;
  rayQueryInitializeEXT(query,
                        scene_as,
                        gl_RayFlagsTerminateOnFirstHitEXT,
                        0xFFu,
                        ray_pos_in[ray_index].xyz,
                        0.01f,
                        ray_dir_in[ray_index].xyz,
                        5.0f);
  rayQueryProceedEXT(query);

  bool is_hit = rayQueryGetIntersectionTypeEXT(query, true) !=
                gl_RayQueryCommittedIntersectionNoneEXT;

  hit_out = int(is_hit);
}
#endif
