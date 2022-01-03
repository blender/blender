
/* To be compile with common_subdiv_lib.glsl */

/* Generate triangles from subdivision quads indices. */

layout(std430, binding = 1) writeonly buffer outputTriangles
{
  uint output_tris[];
};

#ifndef SINGLE_MATERIAL
layout(std430, binding = 2) readonly buffer inputPolygonMatOffset
{
  int polygon_mat_offset[];
};
#endif

void main()
{
  uint quad_index = get_global_invocation_index();
  if (quad_index >= total_dispatch_size) {
    return;
  }

  uint loop_index = quad_index * 4;

#ifdef SINGLE_MATERIAL
  uint triangle_loop_index = quad_index * 6;
#else
  uint coarse_quad_index = coarse_polygon_index_from_subdiv_quad_index(quad_index,
                                                                       coarse_poly_count);
  int mat_offset = polygon_mat_offset[coarse_quad_index];

  int triangle_loop_index = (int(quad_index) + mat_offset) * 6;
#endif

  output_tris[triangle_loop_index + 0] = loop_index + 0;
  output_tris[triangle_loop_index + 1] = loop_index + 1;
  output_tris[triangle_loop_index + 2] = loop_index + 2;
  output_tris[triangle_loop_index + 3] = loop_index + 0;
  output_tris[triangle_loop_index + 4] = loop_index + 2;
  output_tris[triangle_loop_index + 5] = loop_index + 3;
}
