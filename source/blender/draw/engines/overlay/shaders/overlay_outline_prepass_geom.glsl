/* SPDX-FileCopyrightText: 2018-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(common_view_clipping_lib.glsl)
#pragma BLENDER_REQUIRE(common_view_lib.glsl)

void vert_from_gl_in(int v)
{
  gl_Position = gl_in[v].gl_Position;
  interp_out.ob_id = interp_in[v].ob_id;
  view_clipping_distances_set(gl_in[v]);
}

void main()
{
  bool is_persp = (drw_view.winmat[3][3] == 0.0);

  vec3 view_vec = (is_persp) ? normalize(vert[1].pos) : vec3(0.0, 0.0, -1.0);

  vec3 v10 = vert[0].pos - vert[1].pos;
  vec3 v12 = vert[2].pos - vert[1].pos;
  vec3 v13 = vert[3].pos - vert[1].pos;

  vec3 n0 = cross(v12, v10);
  vec3 n3 = cross(v13, v12);

  float fac0 = dot(view_vec, n0);
  float fac3 = dot(view_vec, n3);

  /* If both adjacent verts are facing the camera the same way,
   * then it isn't an outline edge. */
  if (sign(fac0) == sign(fac3)) {
    return;
  }

  /* Don't outline if concave edge. */
  /* That would hide a lot of non useful edge but it flickers badly.
   * TODO: revisit later... */
  // if (dot(n0, v13) > 0.01)
  //  return;

  vert_from_gl_in(1);
  EmitVertex();

  vert_from_gl_in(2);
  EmitVertex();

  EndPrimitive();
}
