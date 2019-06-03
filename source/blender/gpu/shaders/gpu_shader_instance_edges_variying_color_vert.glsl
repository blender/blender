
// Draw "fancy" wireframe, displaying front-facing, back-facing and
// silhouette lines differently.
// Mike Erwin, April 2015

// After working with this shader a while, convinced we should make
// separate shaders for perpective & ortho. (Oct 2016)

// Due to perspective, the line segment's endpoints might disagree on
// whether the adjacent faces are front facing. We use a geometry
// shader to resolve this properly.

uniform mat4 ViewMatrix;
uniform mat4 ProjectionMatrix;

in vec3 pos;
in vec3 N1, N2;  // normals of faces this edge joins (object coords)

/* Instance attrs */
in vec3 color;
in mat4 InstanceModelMatrix;

out vec4 MV_pos;
out float edgeClass;
out vec3 fCol;

// TODO: in float angle; // [-pi .. +pi], + peak, 0 flat, - valley

bool front(mat3 normal_matrix, vec3 N, vec3 eye)
{
  return dot(normal_matrix * N, eye) > 0.0;
}

void main()
{
  vec3 eye;

  mat4 model_view_matrix = ViewMatrix * InstanceModelMatrix;

  vec4 pos_4d = vec4(pos, 1.0);
  MV_pos = model_view_matrix * pos_4d;

  mat3 normal_matrix = transpose(inverse(mat3(model_view_matrix)));

  /* if persp */
  if (ProjectionMatrix[3][3] == 0.0) {
    eye = normalize(-MV_pos.xyz);
  }
  else {
    eye = vec3(0.0, 0.0, 1.0);
  }

  bool face_1_front = front(normal_matrix, N1, eye);
  bool face_2_front = front(normal_matrix, N2, eye);

  if (face_1_front && face_2_front) {
    edgeClass = 1.0;  // front-facing edge
  }
  else if (face_1_front || face_2_front) {
    edgeClass = 0.0;  // exactly one face is front-facing, silhouette edge
  }
  else {
    edgeClass = -1.0;  // back-facing edge
  }

  fCol = color;

#ifdef USE_WORLD_CLIP_PLANES
  world_clip_planes_calc_clip_distance((InstanceModelMatrix * vec4(pos, 1.0)).xyz);
#endif
}
