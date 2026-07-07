/* SPDX-FileCopyrightText: 2018-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/overlay_armature_infos.hh"

VERTEX_SHADER_CREATE_INFO(overlay_armature_sphere_solid)

#include "draw_view_clipping_lib.glsl"
#include "draw_view_lib.glsl"
#include "overlay_common_lib.glsl"
#include "select_lib.glsl"

/* Sphere radius */
#define rad 0.05f

void main()
{
  select_id_set(in_select_buf[gl_InstanceID]);

  float4 bone_color, state_color;
  float4x4 inst_obmat = data_buf[gl_InstanceID];
  float4x4 model_mat = extract_matrix_packed_data(inst_obmat, state_color, bone_color);

  float4x4 model_view_matrix = drw_view().viewmat * model_mat;
  const float4x4 sphere_matrix = inverse(model_view_matrix);
  sphere_matrix0 = sphere_matrix[0];
  sphere_matrix1 = sphere_matrix[1];
  sphere_matrix2 = sphere_matrix[2];
  sphere_matrix3 = sphere_matrix[3];

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

  float z_ofs = -rad - 1e-8f; /* offset to the front of the sphere */
  if (is_persp) {
    /* For perspective, the projected sphere radius
     * can be bigger than the center disc. Compute the
     * max angular size and compensate by sliding the disc
     * towards the camera and scale it accordingly. */
    constexpr float half_pi = 3.1415926f * 0.5f;
    /* Let be (in local space):
     * V the view vector origin.
     * O the sphere origin.
     * T the point on the target circle.
     * We compute the angle between (OV) and (OT). */
    float a = half_pi - asin(rad / cam_dist);
    float cos_b = cos(a);
    float sin_b = sqrt(clamp(1.0f - cos_b * cos_b, 0.0f, 1.0f));
#if 1
    /* Instead of choosing the biggest circle in screen-space,
     * we choose the nearest with the same angular size. This
     * permit us to leverage GL_ARB_conservative_depth in the
     * fragment shader. */
    float minor = cam_dist - rad;
    float major = cam_dist - cos_b * rad;
    float fac = minor / major;
    sin_b *= fac;
#else
    z_ofs = -rad * cos_b;
#endif
    x_axis *= sin_b;
    y_axis *= sin_b;
  }

  /* Camera oriented position (but still in local space) */
  float3 cam_pos = x_axis * pos.x + y_axis * pos.y + z_axis * z_ofs;

  float4 pos_4d = float4(cam_pos, 1.0f);
  float4 V = model_view_matrix * pos_4d;
  gl_Position = drw_view().winmat * V;
  view_position = V.xyz;

  final_state_color = state_color.xyz;
  final_bone_color = bone_color.xyz;

  float4 world_pos = model_mat * pos_4d;
  view_clipping_distances(world_pos.xyz);
}
