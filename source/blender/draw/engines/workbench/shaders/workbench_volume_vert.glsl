/* SPDX-FileCopyrightText: 2018-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/workbench_volume_info.hh"

VERTEX_SHADER_CREATE_INFO(workbench_volume)
VERTEX_SHADER_CREATE_INFO(workbench_volume_slice)
VERTEX_SHADER_CREATE_INFO(workbench_volume_coba)
VERTEX_SHADER_CREATE_INFO(workbench_volume_cubic)
VERTEX_SHADER_CREATE_INFO(workbench_volume_smoke)

#include "draw_model_lib.glsl"
#include "draw_view_lib.glsl"

void main()
{
  drw_ResourceID_iface.resource_index = resource_id;

#ifdef VOLUME_SLICE
  if (sliceAxis == 0) {
    localPos = vec3(slicePosition * 2.0 - 1.0, pos.xy);
  }
  else if (sliceAxis == 1) {
    localPos = vec3(pos.x, slicePosition * 2.0 - 1.0, pos.y);
  }
  else {
    localPos = vec3(pos.xy, slicePosition * 2.0 - 1.0);
  }
  vec3 final_pos = localPos;
#else
  vec3 final_pos = pos;
#endif

#ifdef VOLUME_SMOKE
  final_pos = ((final_pos * 0.5 + 0.5) - OrcoTexCoFactors[0].xyz) / OrcoTexCoFactors[1].xyz;
#else
  final_pos = (volumeTextureToObject * vec4(final_pos * 0.5 + 0.5, 1.0)).xyz;
#endif
  gl_Position = drw_point_world_to_homogenous(drw_point_object_to_world(final_pos));
}
