
/* To be compile with common_subdiv_lib.glsl */

#ifdef CUSTOM_NORMALS
struct CustomNormal {
  float x;
  float y;
  float z;
};

layout(std430, binding = 0) readonly buffer inputNormals
{
  CustomNormal custom_normals[];
};
#else
layout(std430, binding = 0) readonly buffer inputNormals
{
  vec3 vertex_normals[];
};

layout(std430, binding = 1) readonly buffer inputSubdivVertLoopMap
{
  uint vert_loop_map[];
};
#endif

layout(std430, binding = 2) buffer outputPosNor
{
  PosNorLoop pos_nor[];
};

void main()
{
  /* We execute for each quad. */
  uint quad_index = get_global_invocation_index();
  if (quad_index >= total_dispatch_size) {
    return;
  }

  uint start_loop_index = quad_index * 4;

#ifdef CUSTOM_NORMALS
  for (int i = 0; i < 4; i++) {
    CustomNormal custom_normal = custom_normals[start_loop_index + i];
    vec3 nor = vec3(custom_normal.x, custom_normal.y, custom_normal.z);
    set_vertex_nor(pos_nor[start_loop_index + i], normalize(nor));
  }
#else
  for (int i = 0; i < 4; i++) {
    uint subdiv_vert_index = vert_loop_map[start_loop_index + i];
    vec3 nor = vertex_normals[subdiv_vert_index];
    set_vertex_nor(pos_nor[start_loop_index + i], nor);
  }
#endif
}
