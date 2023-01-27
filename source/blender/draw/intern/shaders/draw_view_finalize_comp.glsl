
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

void frustum_boundbox_calc(mat4 winmat, mat4 viewinv, out vec4 corners[8])
{
  float left, right, bottom, top, near, far;
  bool is_persp = winmat[3][3] == 0.0;

  projmat_dimensions(winmat, left, right, bottom, top, near, far);

  corners[0][2] = corners[3][2] = corners[7][2] = corners[4][2] = -near;
  corners[0][0] = corners[3][0] = left;
  corners[4][0] = corners[7][0] = right;
  corners[0][1] = corners[4][1] = bottom;
  corners[7][1] = corners[3][1] = top;

  /* Get the coordinates of the far plane. */
  if (is_persp) {
    float sca_far = far / near;
    left *= sca_far;
    right *= sca_far;
    bottom *= sca_far;
    top *= sca_far;
  }

  corners[1][2] = corners[2][2] = corners[6][2] = corners[5][2] = -far;
  corners[1][0] = corners[2][0] = left;
  corners[6][0] = corners[5][0] = right;
  corners[1][1] = corners[5][1] = bottom;
  corners[2][1] = corners[6][1] = top;

  /* Transform into world space. */
  for (int i = 0; i < 8; i++) {
    corners[i].xyz = transform_point(viewinv, corners[i].xyz);
  }
}

void planes_from_projmat(mat4 mat,
                         out vec4 left,
                         out vec4 right,
                         out vec4 bottom,
                         out vec4 top,
                         out vec4 near,
                         out vec4 far)
{
  /* References:
   *
   * https://fgiesen.wordpress.com/2012/08/31/frustum-planes-from-the-projection-matrix/
   * http://www8.cs.umu.se/kurser/5DV051/HT12/lab/plane_extraction.pdf
   */
  mat = transpose(mat);
  left = mat[3] + mat[0];
  right = mat[3] - mat[0];
  bottom = mat[3] + mat[1];
  top = mat[3] - mat[1];
  near = mat[3] + mat[2];
  far = mat[3] - mat[2];
}

void frustum_culling_planes_calc(mat4 winmat, mat4 viewmat, out vec4 planes[6])
{
  mat4 persmat = winmat * viewmat;
  planes_from_projmat(persmat, planes[0], planes[5], planes[1], planes[3], planes[4], planes[2]);

  /* Normalize. */
  for (int p = 0; p < 6; p++) {
    planes[p] /= length(planes[p].xyz);
  }
}

vec4 frustum_culling_sphere_calc(vec4 corners[8])
{
  /* Extract Bounding Sphere */
  /* TODO(fclem): This is significantly less precise than CPU, but it isn't used in most cases. */

  vec4 bsphere;
  bsphere.xyz = (corners[0].xyz + corners[6].xyz) * 0.5;
  bsphere.w = 0.0;
  for (int i = 0; i < 8; i++) {
    bsphere.w = max(bsphere.w, distance(bsphere.xyz, corners[i].xyz));
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

  frustum_boundbox_calc(drw_view.winmat, drw_view.viewinv, view_culling_buf[drw_view_id].corners);

  frustum_culling_planes_calc(
      drw_view.winmat, drw_view.viewmat, view_culling_buf[drw_view_id].planes);

  view_culling_buf[drw_view_id].bound_sphere = frustum_culling_sphere_calc(
      view_culling_buf[drw_view_id].corners);
}
