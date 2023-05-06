
/**
 * Compute culling data for each views of a given view buffer.
 */

#pragma BLENDER_REQUIRE(common_math_lib.glsl)

void projmat_dimensions(mat4 winmat,
                        out float r_left,
                        out float r_right,
                        out float r_bottom,
                        out float r_top,
                        out float r_near,
                        out float r_far)
{
  const bool is_persp = winmat[3][3] == 0.0;
  if (is_persp) {
    float near = winmat[3][2] / (winmat[2][2] - 1.0);
    r_left = near * ((winmat[2][0] - 1.0) / winmat[0][0]);
    r_right = near * ((winmat[2][0] + 1.0) / winmat[0][0]);
    r_bottom = near * ((winmat[2][1] - 1.0) / winmat[1][1]);
    r_top = near * ((winmat[2][1] + 1.0) / winmat[1][1]);
    r_near = near;
    r_far = winmat[3][2] / (winmat[2][2] + 1.0);
  }
  else {
    r_left = (-winmat[3][0] - 1.0) / winmat[0][0];
    r_right = (-winmat[3][0] + 1.0) / winmat[0][0];
    r_bottom = (-winmat[3][1] - 1.0) / winmat[1][1];
    r_top = (-winmat[3][1] + 1.0) / winmat[1][1];
    r_near = (winmat[3][2] + 1.0) / winmat[2][2];
    r_far = (winmat[3][2] - 1.0) / winmat[2][2];
  }
}

void frustum_boundbox_calc(mat4 winmat, mat4 viewinv, out FrustumCorners frustum_corners)
{
  float left, right, bottom, top, near, far;
  bool is_persp = winmat[3][3] == 0.0;

  projmat_dimensions(winmat, left, right, bottom, top, near, far);

  frustum_corners.corners[0][2] = frustum_corners.corners[3][2] = frustum_corners.corners[7][2] =
      frustum_corners.corners[4][2] = -near;
  frustum_corners.corners[0][0] = frustum_corners.corners[3][0] = left;
  frustum_corners.corners[4][0] = frustum_corners.corners[7][0] = right;
  frustum_corners.corners[0][1] = frustum_corners.corners[4][1] = bottom;
  frustum_corners.corners[7][1] = frustum_corners.corners[3][1] = top;

  /* Get the coordinates of the far plane. */
  if (is_persp) {
    float sca_far = far / near;
    left *= sca_far;
    right *= sca_far;
    bottom *= sca_far;
    top *= sca_far;
  }

  frustum_corners.corners[1][2] = frustum_corners.corners[2][2] = frustum_corners.corners[6][2] =
      frustum_corners.corners[5][2] = -far;
  frustum_corners.corners[1][0] = frustum_corners.corners[2][0] = left;
  frustum_corners.corners[6][0] = frustum_corners.corners[5][0] = right;
  frustum_corners.corners[1][1] = frustum_corners.corners[5][1] = bottom;
  frustum_corners.corners[2][1] = frustum_corners.corners[6][1] = top;

  /* Transform into world space. */
  for (int i = 0; i < 8; i++) {
    frustum_corners.corners[i].xyz = transform_point(viewinv, frustum_corners.corners[i].xyz);
  }
}

void planes_from_projmat(mat4 mat, out FrustumPlanes frustum_planes)
{
  /* References:
   *
   * https://fgiesen.wordpress.com/2012/08/31/frustum-planes-from-the-projection-matrix/
   * http://www8.cs.umu.se/kurser/5DV051/HT12/lab/plane_extraction.pdf
   */
  mat = transpose(mat);
  frustum_planes.planes[0] = mat[3] + mat[0];
  frustum_planes.planes[1] = mat[3] - mat[0];
  frustum_planes.planes[2] = mat[3] + mat[1];
  frustum_planes.planes[3] = mat[3] - mat[1];
  frustum_planes.planes[4] = mat[3] + mat[2];
  frustum_planes.planes[5] = mat[3] - mat[2];
}

void frustum_culling_planes_calc(mat4 winmat, mat4 viewmat, out FrustumPlanes frustum_planes)
{
  mat4 persmat = winmat * viewmat;
  planes_from_projmat(persmat, frustum_planes);

  /* Normalize. */
  for (int p = 0; p < 6; p++) {
    frustum_planes.planes[p] /= length(frustum_planes.planes[p].xyz);
  }
}

vec4 frustum_culling_sphere_calc(FrustumCorners frustum_corners)
{
  /* Extract Bounding Sphere */
  /* TODO(fclem): This is significantly less precise than CPU, but it isn't used in most cases. */

  vec4 bsphere;
  bsphere.xyz = (frustum_corners.corners[0].xyz + frustum_corners.corners[6].xyz) * 0.5;
  bsphere.w = 0.0;
  for (int i = 0; i < 8; i++) {
    bsphere.w = max(bsphere.w, distance(bsphere.xyz, frustum_corners.corners[i].xyz));
  }
  return bsphere;
}

void main()
{
  drw_view_id = int(gl_LocalInvocationID.x);

  /* Invalid views are disabled. */
  if (all(equal(drw_view.viewinv[2].xyz, vec3(0.0)))) {
    /* Views with negative radius are treated as disabled. */
    view_culling_buf[drw_view_id].bound_sphere = vec4(-1.0);
    return;
  }

  /* Read frustom_corners from device memory, update, and write back. */
  FrustumCorners frustum_corners = view_culling_buf[drw_view_id].frustum_corners;
  frustum_boundbox_calc(drw_view.winmat, drw_view.viewinv, frustum_corners);
  view_culling_buf[drw_view_id].frustum_corners = frustum_corners;

  /* Read frustum_planes from device memory, update, and write back. */
  FrustumPlanes frustum_planes = view_culling_buf[drw_view_id].frustum_planes;
  frustum_culling_planes_calc(drw_view.winmat, drw_view.viewmat, frustum_planes);

  view_culling_buf[drw_view_id].frustum_planes = frustum_planes;
  view_culling_buf[drw_view_id].bound_sphere = frustum_culling_sphere_calc(frustum_corners);
}
