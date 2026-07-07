/* SPDX-FileCopyrightText: 2018-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/overlay_armature_infos.hh"

VERTEX_SHADER_CREATE_INFO(overlay_armature_sphere_outline)

#include "draw_view_clipping_lib.glsl"
#include "draw_view_lib.glsl"
#include "overlay_common_lib.glsl"
#include "select_lib.glsl"

/* project to screen space */
float2 proj(float4 pos)
{
  return (0.5f * (pos.xy / pos.w) + 0.5f) * uniform_buf.size_viewport;
}

void main()
{
  select_id_set(in_select_buf[gl_InstanceID]);
  float4 bone_color, state_color;
  float4x4 inst_obmat = data_buf[gl_InstanceID];
  float4x4 model_mat = extract_matrix_packed_data(inst_obmat, state_color, bone_color);

  float4x4 model_view_matrix = drw_view().viewmat * model_mat;
  float4x4 sphere_matrix = inverse(model_view_matrix);

  bool is_persp = (drw_view().winmat[3][3] == 0.0f);

  /* This is the local space camera ray (not normalize).
   * In perspective mode it's also the view-space position
   * of the sphere center. */
  float3 cam_ray = (is_persp) ? model_view_matrix[3].xyz : float3(0.0f, 0.0f, -1.0f);
  cam_ray = to_float3x3(sphere_matrix) * cam_ray;

  /* Sphere center distance from the camera (persp) in local space. */
  float cam_dist = length(cam_ray);

  /* Compute view aligned orthonormal space. */
  float3 z_axis = cam_ray / cam_dist;
  float3 x_axis = normalize(cross(sphere_matrix[1].xyz, z_axis));
  float3 y_axis = cross(z_axis, x_axis);
  float z_ofs = 0.0f;

  if (is_persp) {
    /* For perspective, the projected sphere radius
     * can be bigger than the center disc. Compute the
     * max angular size and compensate by sliding the disc
     * towards the camera and scale it accordingly. */
    constexpr float half_pi = 3.1415926f * 0.5f;
    constexpr float rad = 0.05f;
    /* Let be (in local space):
     * V the view vector origin.
     * O the sphere origin.
     * T the point on the target circle.
     * We compute the angle between (OV) and (OT). */
    float a = half_pi - asin(rad / cam_dist);
    float cos_b = cos(a);
    float sin_b = sqrt(clamp(1.0f - cos_b * cos_b, 0.0f, 1.0f));

    x_axis *= sin_b;
    y_axis *= sin_b;
    z_ofs = -rad * cos_b;
  }

  /* Camera oriented position (but still in local space) */
  float3 cam_pos0 = x_axis * pos.x + y_axis * pos.y + z_axis * z_ofs;

  float4 V = model_view_matrix * float4(cam_pos0, 1.0f);
  gl_Position = drw_view().winmat * V;
  float4 center = drw_view().winmat * float4(model_view_matrix[3].xyz, 1.0f);

  /* Offset away from the center to avoid overlap with solid shape. */
  float2 ofs_dir = normalize(proj(gl_Position) - proj(center));
  gl_Position.xy += ofs_dir * uniform_buf.size_viewport_inv * gl_Position.w;

  edge_start = edge_pos = proj(gl_Position);

  final_color = float4(bone_color.rgb, 1.0f);

  float4 world_pos = model_mat * float4(cam_pos0, 1.0f);
  view_clipping_distances(world_pos.xyz);
}
