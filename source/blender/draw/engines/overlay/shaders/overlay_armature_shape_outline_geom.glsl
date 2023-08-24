/* SPDX-FileCopyrightText: 2018-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(common_view_clipping_lib.glsl)

void main(void)
{
  finalColor = vec4(geom_in[0].vColSize.rgb, 1.0);

  bool is_persp = (drw_view.winmat[3][3] == 0.0);

  vec3 view_vec = (is_persp) ? normalize(geom_in[1].vPos) : vec3(0.0, 0.0, -1.0);
  vec3 v10 = geom_in[0].vPos - geom_in[1].vPos;
  vec3 v12 = geom_in[2].vPos - geom_in[1].vPos;
  vec3 v13 = geom_in[3].vPos - geom_in[1].vPos;

  vec3 n0 = cross(v12, v10);
  vec3 n3 = cross(v13, v12);

  float fac0 = dot(view_vec, n0);
  float fac3 = dot(view_vec, n3);

  /* If one of the face is perpendicular to the view,
   * consider it and outline edge. */
  if (abs(fac0) > 1e-5 && abs(fac3) > 1e-5) {
    /* If both adjacent verts are facing the camera the same way,
     * then it isn't an outline edge. */
    if (sign(fac0) == sign(fac3)) {
      return;
    }
  }

  n0 = (geom_flat_in[0].inverted == 1) ? -n0 : n0;
  /* Don't outline if concave edge. */
  if (dot(n0, v13) > 0.0001) {
    return;
  }

  vec2 perp = normalize(geom_in[2].ssPos - geom_in[1].ssPos);
  vec2 edge_dir = vec2(-perp.y, perp.x);

  vec2 hidden_point;
  /* Take the farthest point to compute edge direction
   * (avoid problems with point behind near plane).
   * If the chosen point is parallel to the edge in screen space,
   * choose the other point anyway.
   * This fixes some issue with cubes in orthographic views. */
  if (geom_in[0].vPos.z < geom_in[3].vPos.z) {
    hidden_point = (abs(fac0) > 1e-5) ? geom_in[0].ssPos : geom_in[3].ssPos;
  }
  else {
    hidden_point = (abs(fac3) > 1e-5) ? geom_in[3].ssPos : geom_in[0].ssPos;
  }
  vec2 hidden_dir = normalize(hidden_point - geom_in[1].ssPos);

  float fac = dot(-hidden_dir, edge_dir);
  edge_dir *= (fac < 0.0) ? -1.0 : 1.0;

  gl_Position = geom_in[1].pPos;
  /* Offset away from the center to avoid overlap with solid shape. */
  gl_Position.xy += (edge_dir - perp) * sizeViewportInv * gl_Position.w;
  /* Improve AA bleeding inside bone silhouette. */
  gl_Position.z -= (is_persp) ? 1e-4 : 1e-6;
  edgeStart = edgePos = ((gl_Position.xy / gl_Position.w) * 0.5 + 0.5) * sizeViewport;
  view_clipping_distances_set(gl_in[1]);
  EmitVertex();

  gl_Position = geom_in[2].pPos;
  /* Offset away from the center to avoid overlap with solid shape. */
  gl_Position.xy += (edge_dir + perp) * sizeViewportInv * gl_Position.w;
  /* Improve AA bleeding inside bone silhouette. */
  gl_Position.z -= (is_persp) ? 1e-4 : 1e-6;
  edgeStart = edgePos = ((gl_Position.xy / gl_Position.w) * 0.5 + 0.5) * sizeViewport.xy;
  view_clipping_distances_set(gl_in[2]);
  EmitVertex();

  EndPrimitive();
}
