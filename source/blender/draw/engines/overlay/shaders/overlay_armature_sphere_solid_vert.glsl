/* SPDX-FileCopyrightText: 2018-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(common_view_clipping_lib.glsl)
#pragma BLENDER_REQUIRE(common_view_lib.glsl)
#pragma BLENDER_REQUIRE(select_lib.glsl)

/* Sphere radius */
const float rad = 0.05;

void main()
{
  select_id_set(in_select_buf[gl_InstanceID]);

  vec4 bone_color, state_color;
  mat4 model_mat = extract_matrix_packed_data(inst_obmat, state_color, bone_color);

  mat4 model_view_matrix = drw_view.viewmat * model_mat;
  sphereMatrix = inverse(model_view_matrix);

  bool is_persp = (drw_view.winmat[3][3] == 0.0);

  /* This is the local space camera ray (not normalize).
   * In perspective mode it's also the viewspace position
   * of the sphere center. */
  vec3 cam_ray = (is_persp) ? model_view_matrix[3].xyz : vec3(0.0, 0.0, -1.0);
  cam_ray = mat3(sphereMatrix) * cam_ray;

  /* Sphere center distance from the camera (persp) in local space. */
  float cam_dist = length(cam_ray);

  /* Compute view aligned orthonormal space. */
  vec3 z_axis = cam_ray / cam_dist;
  vec3 x_axis = normalize(cross(sphereMatrix[1].xyz, z_axis));
  vec3 y_axis = cross(z_axis, x_axis);

  float z_ofs = -rad - 1e-8; /* offset to the front of the sphere */
  if (is_persp) {
    /* For perspective, the projected sphere radius
     * can be bigger than the center disc. Compute the
     * max angular size and compensate by sliding the disc
     * towards the camera and scale it accordingly. */
    const float half_pi = 3.1415926 * 0.5;
    /* Let be (in local space):
     * V the view vector origin.
     * O the sphere origin.
     * T the point on the target circle.
     * We compute the angle between (OV) and (OT). */
    float a = half_pi - asin(rad / cam_dist);
    float cos_b = cos(a);
    float sin_b = sqrt(clamp(1.0 - cos_b * cos_b, 0.0, 1.0));
#if 1
    /* Instead of choosing the biggest circle in screenspace,
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
  vec3 cam_pos = x_axis * pos.x + y_axis * pos.y + z_axis * z_ofs;

  vec4 pos_4d = vec4(cam_pos, 1.0);
  vec4 V = model_view_matrix * pos_4d;
  gl_Position = drw_view.winmat * V;
  viewPosition = V.xyz;

  finalStateColor = state_color.xyz;
  finalBoneColor = bone_color.xyz;

  vec4 world_pos = model_mat * pos_4d;
  view_clipping_distances(world_pos.xyz);
}
