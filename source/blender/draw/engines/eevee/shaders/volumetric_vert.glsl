
#pragma BLENDER_REQUIRE(common_view_lib.glsl)

out vec4 vPos;

RESOURCE_ID_VARYING

void main()
{
  /* Generate Triangle : less memory fetches from a VBO */
  int v_id = gl_VertexID % 3; /* Vertex Id */
  int t_id = gl_VertexID / 3; /* Triangle Id */

  /* Crappy diagram
   * ex 1
   *    | \
   *    |   \
   *  1 |     \
   *    |       \
   *    |         \
   *  0 |           \
   *    |             \
   *    |               \
   * -1 0 --------------- 2
   *   -1     0     1     ex
   */
  vPos.x = float(v_id / 2) * 4.0 - 1.0; /* int divisor round down */
  vPos.y = float(v_id % 2) * 4.0 - 1.0;
  vPos.z = float(t_id);
  vPos.w = 1.0;

  PASS_RESOURCE_ID

#ifdef USE_ATTR
  pass_attr(vec3(0.0), mat3(1), mat4(1));
#endif
}
