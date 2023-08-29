/* SPDX-FileCopyrightText: 2018-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(common_view_clipping_lib.glsl)
#pragma BLENDER_REQUIRE(common_view_lib.glsl)

/* project to screen space */
vec2 proj(vec4 pos)
{
  return (0.5 * (pos.xy / pos.w) + 0.5) * sizeViewport.xy;
}

void main()
{
  vec4 bone_color, state_color;
  mat4 model_mat = extract_matrix_packed_data(inst_obmat, state_color, bone_color);

  mat4 model_view_matrix = drw_view.viewmat * model_mat;
  mat4 sphereMatrix = inverse(model_view_matrix);

  bool is_persp = (drw_view.winmat[3][3] == 0.0);

  /* This is the local space camera ray (not normalize).
   * In perspective mode it's also the view-space position
   * of the sphere center. */
  vec3 cam_ray = (is_persp) ? model_view_matrix[3].xyz : vec3(0.0, 0.0, -1.0);
  cam_ray = mat3(sphereMatrix) * cam_ray;

  /* Sphere center distance from the camera (persp) in local space. */
  float cam_dist = length(cam_ray);

  /* Compute view aligned orthonormal space. */
  vec3 z_axis = cam_ray / cam_dist;
  vec3 x_axis = normalize(cross(sphereMatrix[1].xyz, z_axis));
  vec3 y_axis = cross(z_axis, x_axis);
  float z_ofs = 0.0;

  if (is_persp) {
    /* For perspective, the projected sphere radius
     * can be bigger than the center disc. Compute the
     * max angular size and compensate by sliding the disc
     * towards the camera and scale it accordingly. */
    const float half_pi = 3.1415926 * 0.5;
    const float rad = 0.05;
    /* Let be (in local space):
     * V the view vector origin.
     * O the sphere origin.
     * T the point on the target circle.
     * We compute the angle between (OV) and (OT). */
    float a = half_pi - asin(rad / cam_dist);
    float cos_b = cos(a);
    float sin_b = sqrt(clamp(1.0 - cos_b * cos_b, 0.0, 1.0));

    x_axis *= sin_b;
    y_axis *= sin_b;
    z_ofs = -rad * cos_b;
  }

  /* Camera oriented position (but still in local space) */
  vec3 cam_pos0 = x_axis * pos.x + y_axis * pos.y + z_axis * z_ofs;

  vec4 V = model_view_matrix * vec4(cam_pos0, 1.0);
  gl_Position = drw_view.winmat * V;
  vec4 center = drw_view.winmat * vec4(model_view_matrix[3].xyz, 1.0);

  /* Offset away from the center to avoid overlap with solid shape. */
  vec2 ofs_dir = normalize(proj(gl_Position) - proj(center));
  gl_Position.xy += ofs_dir * sizeViewportInv * gl_Position.w;

  edgeStart = edgePos = proj(gl_Position);

  finalColor = vec4(bone_color.rgb, 1.0);

  vec4 world_pos = model_mat * vec4(cam_pos0, 1.0);
  view_clipping_distances(world_pos.xyz);
}
