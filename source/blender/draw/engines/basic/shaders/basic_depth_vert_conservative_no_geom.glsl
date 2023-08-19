
#pragma USE_SSBO_VERTEX_FETCH(TriangleList, 3)

#pragma BLENDER_REQUIRE(common_view_clipping_lib.glsl)
#pragma BLENDER_REQUIRE(common_view_lib.glsl)

void main()
{
  /* Calculate triangle vertex info. */
  int output_triangle_id = gl_VertexID / 3;
  int output_triangle_vertex_id = gl_VertexID % 3;
  int base_vertex_id = 0;

  if (vertex_fetch_get_input_prim_type() == GPU_PRIM_TRIS) {
    base_vertex_id = output_triangle_id * 3;
  }
  else if (vertex_fetch_get_input_prim_type() == GPU_PRIM_TRI_STRIP) {
    base_vertex_id = output_triangle_id;
  }
  /* NOTE: Triangle fan unsupported in Metal. Will be converted upfront. */

  /* Perform vertex shader calculations per input vertex. */

  /* input pos vertex attribute. */
  vec3 in_pos[3];
  /* Calculated per-vertex world pos. */
  vec3 world_pos[3];
  /* Output gl_Position per vertex. */
  vec3 ndc_pos[3];
  /* Geometry shader normalized position. */
  vec3 pos[3];

  for (int i = 0; i < 3; i++) {
    in_pos[0] = vertex_fetch_attribute(base_vertex_id + i, pos, vec3);
    world_pos[0] = point_object_to_world(in_pos[i]);
    ndc_pos[i] = point_world_to_ndc(world_pos[i]);
    pos[i] = ndc_pos[i].xyz / ndc_pos[i].w;
  }

  /* Geometry Shader equivalent calculation
   * In this no_geom mode using SSBO vertex fetch, rather than emitting 3 vertices, the vertex
   * shader is innovated 3 times, and output is determined based on vertex ID within a triangle
   * 0..2. */
  vec3 plane = normalize(cross(pos[1] - pos[0], pos[2] - pos[0]));
  /* Compute NDC bound box. */
  vec4 bbox = vec4(min(min(pos[0].xy, pos[1].xy), pos[2].xy),
                   max(max(pos[0].xy, pos[1].xy), pos[2].xy));
  /* Convert to pixel space. */
  bbox = (bbox * 0.5 + 0.5) * sizeViewport.xyxy;
  /* Detect failure cases where triangles would produce no fragments. */
  bvec2 is_subpixel = lessThan(bbox.zw - bbox.xy, vec2(1.0));
  /* View aligned triangle. */
  const float threshold = 0.00001;
  bool is_coplanar = abs(plane.z) < threshold;

  /* Determine output position per-vertex in each triangle */
  gl_Position = ndc_pos[output_triangle_vertex_id];
  if (all(is_subpixel)) {
    vec2 ofs = (i == 0) ? vec2(-1.0) : ((i == 1) ? vec2(2.0, -1.0) : vec2(-1.0, 2.0));
    /* HACK: Fix cases where the triangle is too small make it cover at least one pixel. */
    gl_Position.xy += sizeViewportInv * gl_Position.w * ofs;
  }
  /* Test if the triangle is almost parallel with the view to avoid precision issues. */
  else if (any(is_subpixel) || is_coplanar) {
    /* HACK: Fix cases where the triangle is Parallel to the view by deforming it slightly. */
    vec2 ofs = (i == 0) ? vec2(-1.0) : ((i == 1) ? vec2(1.0, -1.0) : vec2(1.0));
    gl_Position.xy += sizeViewportInv * gl_Position.w * ofs;
  }
  else {
    /* Triangle expansion should happen here, but we decide to not implement it for
     * depth precision & performance reasons. */
  }

  /* Assign vertex shader clipping distances. */
  view_clipping_distances(world_pos[output_triangle_vertex_id]);
}
