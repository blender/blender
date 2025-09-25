/* SPDX-FileCopyrightText: 2018-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/overlay_armature_infos.hh"

VERTEX_SHADER_CREATE_INFO(overlay_armature_envelope_outline)

#include "draw_view_clipping_lib.glsl"
#include "draw_view_lib.glsl"
#include "select_lib.glsl"

/* project to screen space */
float2 proj(float4 pos)
{
  return (0.5f * (pos.xy / pos.w) + 0.5f) * uniform_buf.size_viewport;
}

float2 compute_dir(float2 v0, float2 v1, float2 v2)
{
  float2 dir = normalize(v2 - v0);
  dir = float2(dir.y, -dir.x);
  return dir;
}

float3x3 compute_mat(float4 sphere, float3 bone_vec, out float z_ofs)
{
  bool is_persp = (drw_view().winmat[3][3] == 0.0f);
  float3 cam_ray = (is_persp) ? sphere.xyz - drw_view().viewinv[3].xyz :
                                -drw_view().viewinv[2].xyz;

  /* Sphere center distance from the camera (persp) in world space. */
  float cam_dist = length(cam_ray);

  /* Compute view aligned orthonormal space. */
  float3 z_axis = cam_ray / cam_dist;
  float3 x_axis = normalize(cross(bone_vec, z_axis));
  float3 y_axis = cross(z_axis, x_axis);
  z_ofs = 0.0f;

  if (is_persp) {
    /* For perspective, the projected sphere radius
     * can be bigger than the center disc. Compute the
     * max angular size and compensate by sliding the disc
     * towards the camera and scale it accordingly. */
    constexpr float half_pi = 3.1415926f * 0.5f;
    float rad = sphere.w;
    /* Let be :
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

  return float3x3(x_axis, y_axis, z_axis);
}

struct Bone {
  float3 vec;
  float sinb;
};

bool bone_blend_starts(float3 p, Bone b)
{
  /* we just want to know when the head sphere starts interpolating. */
  return dot(p, b.vec) > -b.sinb;
}

float3 get_outline_point(float2 pos,
                         float4 sph_near,
                         float4 sph_far,
                         float3x3 mat_near,
                         float3x3 mat_far,
                         float z_ofs_near,
                         float z_ofs_far,
                         Bone b)
{
  /* Compute outline position on the nearest sphere and check
   * if it penetrates the capsule body. If it does, put this
   * vertex on the farthest sphere. */
  float3 wpos = mat_near * float3(pos * sph_near.w, z_ofs_near);
  if (bone_blend_starts(wpos, b)) {
    wpos = sph_far.xyz + mat_far * float3(pos * sph_far.w, z_ofs_far);
  }
  else {
    wpos += sph_near.xyz;
  }
  return wpos;
}

void main()
{
  select_id_set(in_select_buf[gl_InstanceID]);

  float dst_head = distance(data_buf[gl_InstanceID].head_sphere.xyz, drw_view().viewinv[3].xyz);
  float dst_tail = distance(data_buf[gl_InstanceID].tail_sphere.xyz, drw_view().viewinv[3].xyz);
  // float dst_head = -dot(data_buf[gl_InstanceID].head_sphere.xyz, drw_view().viewmat[2].xyz);
  // float dst_tail = -dot(data_buf[gl_InstanceID].tail_sphere.xyz, drw_view().viewmat[2].xyz);

  float4 sph_near, sph_far;
  if ((dst_head > dst_tail) && (drw_view().winmat[3][3] == 0.0f)) {
    sph_near = data_buf[gl_InstanceID].tail_sphere;
    sph_far = data_buf[gl_InstanceID].head_sphere;
  }
  else {
    sph_near = data_buf[gl_InstanceID].head_sphere;
    sph_far = data_buf[gl_InstanceID].tail_sphere;
  }

  float3 bone_vec = (sph_far.xyz - sph_near.xyz) + 1e-8f;

  Bone b;
  float bone_lenrcp = 1.0f / max(1e-8f, sqrt(dot(bone_vec, bone_vec)));
  b.sinb = (sph_far.w - sph_near.w) * bone_lenrcp * sph_near.w;
  b.vec = bone_vec * bone_lenrcp;

  float z_ofs_near, z_ofs_far;
  float3x3 mat_near = compute_mat(sph_near, bone_vec, z_ofs_near);
  float3x3 mat_far = compute_mat(sph_far, bone_vec, z_ofs_far);

  float3 wpos0 = get_outline_point(
      pos0, sph_near, sph_far, mat_near, mat_far, z_ofs_near, z_ofs_far, b);
  float3 wpos1 = get_outline_point(
      pos1, sph_near, sph_far, mat_near, mat_far, z_ofs_near, z_ofs_far, b);
  float3 wpos2 = get_outline_point(
      pos2, sph_near, sph_far, mat_near, mat_far, z_ofs_near, z_ofs_far, b);

  view_clipping_distances(wpos1);

  float4 p0 = drw_point_world_to_homogenous(wpos0);
  float4 p1 = drw_point_world_to_homogenous(wpos1);
  float4 p2 = drw_point_world_to_homogenous(wpos2);

  gl_Position = p1;

  /* compute position from 3 vertex because the change in direction
   * can happen very quickly and lead to very thin edges. */
  float2 ss0 = proj(p0);
  float2 ss1 = proj(p1);
  float2 ss2 = proj(p2);
  float2 ofs_dir = compute_dir(ss0, ss1, ss2);

  /* Offset away from the center to avoid overlap with solid shape. */
  gl_Position.xy += ofs_dir * uniform_buf.size_viewport_inv * gl_Position.w;

  edge_start = edge_pos = proj(gl_Position);

  final_color = float4(data_buf[gl_InstanceID].bone_color_and_wire_width.rgb, 1.0f);
}
