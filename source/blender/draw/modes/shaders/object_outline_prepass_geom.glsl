
layout(lines_adjacency) in;
layout(line_strip, max_vertices = 2) out;

in vec4 pPos[];
in vec3 vPos[];

void vert_from_gl_in(int v)
{
  gl_Position = pPos[v];
#ifdef USE_WORLD_CLIP_PLANES
  world_clip_planes_set_clip_distance(gl_in[v].gl_ClipDistance);
#endif
}

void main()
{
  bool is_persp = (ProjectionMatrix[3][3] == 0.0);

  vec3 view_vec = (is_persp) ? normalize(vPos[1]) : vec3(0.0, 0.0, -1.0);

  vec3 v10 = vPos[0] - vPos[1];
  vec3 v12 = vPos[2] - vPos[1];
  vec3 v13 = vPos[3] - vPos[1];

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
   * TODO revisit later... */
  // if (dot(n0, v13) > 0.01)
  //  return;

  vert_from_gl_in(1);
  EmitVertex();

  vert_from_gl_in(2);
  EmitVertex();

  EndPrimitive();
}
