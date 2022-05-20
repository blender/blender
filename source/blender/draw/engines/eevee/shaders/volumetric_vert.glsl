
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
}

/* Stubs */
vec2 btdf_lut(float a, float b, float c)
{
  return vec2(0.0);
}

vec2 brdf_lut(float a, float b)
{
  return vec2(0.0);
}

vec3 F_brdf_multi_scatter(vec3 a, vec3 b, vec2 c)
{
  return vec3(0.0);
}

vec3 F_brdf_single_scatter(vec3 a, vec3 b, vec2 c)
{
  return vec3(0.0);
}

float F_eta(float a, float b)
{
  return 0.0;
}

vec3 coordinate_camera(vec3 P)
{
  return vec3(0.0);
}

vec3 coordinate_screen(vec3 P)
{
  return vec3(0.0);
}

vec3 coordinate_reflect(vec3 P, vec3 N)
{
  return vec3(0.0);
}

vec3 coordinate_incoming(vec3 P)
{
  return vec3(0.0);
}
