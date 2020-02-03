
layout(lines_adjacency) in;
layout(line_strip, max_vertices = 2) out;

in vec4 pPos[];
in vec3 vPos[];
in vec2 ssPos[];
in vec2 ssNor[];
in vec4 vColSize[];
in int inverted[];

flat out vec4 finalColor;
flat out vec2 edgeStart;
noperspective out vec2 edgePos;

void main(void)
{
  finalColor = vec4(vColSize[0].rgb, 1.0);

  bool is_persp = (ProjectionMatrix[3][3] == 0.0);

  vec3 view_vec = (is_persp) ? normalize(vPos[1]) : vec3(0.0, 0.0, -1.0);
  vec3 v10 = vPos[0] - vPos[1];
  vec3 v12 = vPos[2] - vPos[1];
  vec3 v13 = vPos[3] - vPos[1];

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

  n0 = (inverted[0] == 1) ? -n0 : n0;
  /* Don't outline if concave edge. */
  if (dot(n0, v13) > 0.0001) {
    return;
  }

  vec2 perp = normalize(ssPos[2] - ssPos[1]);
  vec2 edge_dir = vec2(-perp.y, perp.x);

  vec2 hidden_point;
  /* Take the farthest point to compute edge direction
   * (avoid problems with point behind near plane).
   * If the chosen point is parallel to the edge in screen space,
   * choose the other point anyway.
   * This fixes some issue with cubes in orthographic views.*/
  if (vPos[0].z < vPos[3].z) {
    hidden_point = (abs(fac0) > 1e-5) ? ssPos[0] : ssPos[3];
  }
  else {
    hidden_point = (abs(fac3) > 1e-5) ? ssPos[3] : ssPos[0];
  }
  vec2 hidden_dir = normalize(hidden_point - ssPos[1]);

  float fac = dot(-hidden_dir, edge_dir);
  edge_dir *= (fac < 0.0) ? -1.0 : 1.0;

  gl_Position = pPos[1];
  /* Offset away from the center to avoid overlap with solid shape. */
  gl_Position.xy += (edge_dir - perp) * sizeViewportInv.xy * gl_Position.w;
  /* Improve AA bleeding inside bone silhouette. */
  gl_Position.z -= (is_persp) ? 1e-4 : 1e-6;
  edgeStart = edgePos = ((gl_Position.xy / gl_Position.w) * 0.5 + 0.5) * sizeViewport.xy;
#ifdef USE_WORLD_CLIP_PLANES
  world_clip_planes_set_clip_distance(gl_in[1].gl_ClipDistance);
#endif
  EmitVertex();

  gl_Position = pPos[2];
  /* Offset away from the center to avoid overlap with solid shape. */
  gl_Position.xy += (edge_dir + perp) * sizeViewportInv.xy * gl_Position.w;
  /* Improve AA bleeding inside bone silhouette. */
  gl_Position.z -= (is_persp) ? 1e-4 : 1e-6;
  edgeStart = edgePos = ((gl_Position.xy / gl_Position.w) * 0.5 + 0.5) * sizeViewport.xy;
#ifdef USE_WORLD_CLIP_PLANES
  world_clip_planes_set_clip_distance(gl_in[2].gl_ClipDistance);
#endif
  EmitVertex();

  EndPrimitive();
}
