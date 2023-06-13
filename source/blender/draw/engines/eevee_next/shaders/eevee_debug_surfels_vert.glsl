#pragma BLENDER_REQUIRE(common_view_lib.glsl)
#pragma BLENDER_REQUIRE(common_math_geom_lib.glsl)

void main()
{
  surfel_index = gl_InstanceID;
  DebugSurfel surfel = surfels_buf[surfel_index];

  vec3 lP;

  switch (gl_VertexID) {
    case 0:
      lP = vec3(-1, 1, 0);
      break;
    case 1:
      lP = vec3(-1, -1, 0);
      break;
    case 2:
      lP = vec3(1, 1, 0);
      break;
    case 3:
      lP = vec3(1, -1, 0);
      break;
  }

  vec3 N = surfel.normal;
  vec3 T, B;
  make_orthonormal_basis(N, T, B);

  mat4 model_matrix = mat4(vec4(T * surfel_radius, 0),
                           vec4(B * surfel_radius, 0),
                           vec4(N * surfel_radius, 0),
                           vec4(surfel.position, 1));

  P = (model_matrix * vec4(lP, 1)).xyz;

  gl_Position = point_world_to_ndc(P);
}
