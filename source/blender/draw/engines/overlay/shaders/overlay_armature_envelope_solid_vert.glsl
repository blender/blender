/* SPDX-FileCopyrightText: 2018-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/overlay_armature_infos.hh"

VERTEX_SHADER_CREATE_INFO(overlay_armature_envelope_solid)

#include "draw_view_clipping_lib.glsl"
#include "draw_view_lib.glsl"
#include "select_lib.glsl"

void main()
{
  select_id_set(in_select_buf[gl_InstanceID]);

  float3 bone_vec = data_buf[gl_InstanceID].tail_sphere.xyz -
                    data_buf[gl_InstanceID].head_sphere.xyz;
  float bone_len = max(1e-8f, sqrt(dot(bone_vec, bone_vec)));
  float bone_lenrcp = 1.0f / bone_len;
#ifdef SMOOTH_ENVELOPE
  float sinb = (data_buf[gl_InstanceID].tail_sphere.w - data_buf[gl_InstanceID].head_sphere.w) *
               bone_lenrcp;
#else
  constexpr float sinb = 0.0f;
#endif

  float3 y_axis = bone_vec * bone_lenrcp;
  float3 z_axis = normalize(cross(data_buf[gl_InstanceID].x_axis.xyz, -y_axis));
  float3 x_axis = cross(
      y_axis, z_axis); /* cannot trust data_buf[gl_InstanceID].x_axis.xyz to be orthogonal. */

  float3 sp, nor;
  nor = sp = pos.xyz;

  /* In bone space */
  bool is_head = (pos.z < -sinb);
  sp *= (is_head) ? data_buf[gl_InstanceID].head_sphere.w : data_buf[gl_InstanceID].tail_sphere.w;
  sp.z += (is_head) ? 0.0f : bone_len;

  /* Convert to world space */
  float3x3 bone_mat = float3x3(x_axis, y_axis, z_axis);
  sp = bone_mat * sp.xzy + data_buf[gl_InstanceID].head_sphere.xyz;
  nor = bone_mat * nor.xzy;

  view_normal = to_float3x3(drw_view().viewmat) * nor;

  final_state_color = data_buf[gl_InstanceID].state_color.xyz;
  final_bone_color = data_buf[gl_InstanceID].bone_color_and_wire_width.xyz;

  view_clipping_distances(sp);

  float4 pos_4d = float4(sp, 1.0f);
  gl_Position = drw_view().winmat * (drw_view().viewmat * pos_4d);
}
