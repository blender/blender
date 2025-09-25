/* SPDX-FileCopyrightText: 2018-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/workbench_volume_infos.hh"

VERTEX_SHADER_CREATE_INFO(workbench_volume)
VERTEX_SHADER_CREATE_INFO(workbench_volume_slice)
VERTEX_SHADER_CREATE_INFO(workbench_volume_coba)
VERTEX_SHADER_CREATE_INFO(workbench_volume_cubic)
VERTEX_SHADER_CREATE_INFO(workbench_volume_smoke)

#include "draw_model_lib.glsl"
#include "draw_object_infos_lib.glsl"
#include "draw_view_lib.glsl"

void main()
{
  drw_ResourceID_iface.resource_index = drw_resource_id_raw();

#ifdef VOLUME_SLICE
  if (slice_axis == 0) {
    local_position = float3(slice_position * 2.0f - 1.0f, pos.xy);
  }
  else if (slice_axis == 1) {
    local_position = float3(pos.x, slice_position * 2.0f - 1.0f, pos.y);
  }
  else {
    local_position = float3(pos.xy, slice_position * 2.0f - 1.0f);
  }
  float3 final_pos = local_position;
#else
  float3 final_pos = pos;
#endif

#ifdef VOLUME_SMOKE
  ObjectInfos info = drw_object_infos();
  final_pos = ((final_pos * 0.5f + 0.5f) - info.orco_add) / info.orco_mul;
#else
  final_pos = (volume_texture_to_object * float4(final_pos * 0.5f + 0.5f, 1.0f)).xyz;
#endif
  gl_Position = drw_point_world_to_homogenous(drw_point_object_to_world(final_pos));
}
