/* SPDX-FileCopyrightText: 2018-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/overlay_armature_infos.hh"

VERTEX_SHADER_CREATE_INFO(overlay_armature_stick)

#include "draw_model_lib.glsl"
#include "draw_view_clipping_lib.glsl"
#include "draw_view_lib.glsl"
#include "gpu_shader_utildefines_lib.glsl"
#include "select_lib.glsl"

/* project to screen space */
float2 proj(float4 hs_P)
{
  return (0.5f * (hs_P.xy / hs_P.w) + 0.5f) * uniform_buf.size_viewport;
}

void main()
{
  select_id_set(in_select_buf[gl_InstanceID]);

  StickBoneFlag bone_flag = StickBoneFlag(vclass);
  final_inner_color = flag_test(bone_flag, COL_HEAD) ? data_buf[gl_InstanceID].head_color :
                                                       data_buf[gl_InstanceID].tail_color;
  final_inner_color = flag_test(bone_flag, COL_BONE) ? data_buf[gl_InstanceID].bone_color :
                                                       final_inner_color;
  final_wire_color = (data_buf[gl_InstanceID].wire_color.a > 0.0f) ?
                         data_buf[gl_InstanceID].wire_color :
                         final_inner_color;
  color_fac = flag_test(bone_flag, COL_WIRE) ? 0.0f :
                                               (flag_test(bone_flag, COL_BONE) ? 1.0f : 2.0f);

  float4 boneStart_4d = float4(data_buf[gl_InstanceID].bone_start.xyz, 1.0f);
  float4 boneEnd_4d = float4(data_buf[gl_InstanceID].bone_end.xyz, 1.0f);
  float4 v0 = drw_view().viewmat * boneStart_4d;
  float4 v1 = drw_view().viewmat * boneEnd_4d;

  /* Clip the bone to the camera origin plane (not the clip plane)
   * to avoid glitches if one end is behind the camera origin (in perspective mode). */
  float clip_dist = (drw_view().winmat[3][3] == 0.0f) ?
                        -1e-7f :
                        1e20f; /* hard-coded, -1e-8f is giving glitches. */
  float3 bvec = v1.xyz - v0.xyz;
  float3 clip_pt = v0.xyz + bvec * ((v0.z - clip_dist) / -bvec.z);
  if (v0.z > clip_dist) {
    v0.xyz = clip_pt;
  }
  else if (v1.z > clip_dist) {
    v1.xyz = clip_pt;
  }

  float4 p0 = drw_view().winmat * v0;
  float4 p1 = drw_view().winmat * v1;

  bool is_head = flag_test(bone_flag, POS_HEAD);
  bool is_bone = flag_test(bone_flag, POS_BONE);

  float h = (is_head) ? p0.w : p1.w;

  float2 x_screen_vec = normalize(proj(p1) - proj(p0) + 1e-8f);
  float2 y_screen_vec = float2(x_screen_vec.y, -x_screen_vec.x);

  /* 2D screen aligned pos at the point */
  float2 vpos = pos.x * x_screen_vec + pos.y * y_screen_vec;
  vpos *= (drw_view().winmat[3][3] == 0.0f) ? h : 1.0f;
  vpos *= (data_buf[gl_InstanceID].wire_color.a > 0.0f) ? 1.0f : 0.5f;

  if (final_inner_color.a > 0.0f) {
    float stick_size = theme.sizes.pixel * 5.0f;
    gl_Position = (is_head) ? p0 : p1;
    gl_Position.xy += stick_size * (vpos * uniform_buf.size_viewport_inv);
    gl_Position.z += (is_bone) ? 0.0f : 1e-6f; /* Avoid Z fighting of head/tails. */
    view_clipping_distances((is_head ? boneStart_4d : boneEnd_4d).xyz);
  }
  else {
    gl_Position = float4(0.0f);
  }
}
