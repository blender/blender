#pragma BLENDER_REQUIRE(common_view_lib.glsl)
#pragma USE_SSBO_VERTEX_FETCH(TriangleList, 6)

#ifdef DOUBLE_MANIFOLD
#  define vert_len 6 /* Triangle Strip with 6 verts = 4 triangles = 12 verts. */
#  define emit_triangle_count 2
#else
#  define vert_len 6 /* Triangle Strip with 6 verts = 4 triangles = 12 verts. */
#  define emit_triangle_count 2
#endif

struct VertexData {
  vec3 pos;           /* local position */
  vec4 frontPosition; /* final ndc position */
  vec4 backPosition;
};

/* Input geometry triangle list. */
VertexData vData[3] = {};

#define DISCARD_VERTEX \
  gl_Position = vec4(0.0); \
  return;

vec4 get_pos(int v, bool backface)
{
  return (backface) ? vData[v].backPosition : vData[v].frontPosition;
}

void emit_cap(const bool front, bool reversed, int triangle_vertex_id)
{
  /* Inverse. */
  ivec2 idx = (reversed) ? ivec2(2, 1) : ivec2(1, 2);

  /* Output position depending on vertex ID. */
  switch (triangle_vertex_id) {
    case 0: {
      gl_Position = (front) ? vData[0].frontPosition : vData[0].backPosition;
    } break;

    case 1: {
      gl_Position = (front) ? vData[idx.x].frontPosition : vData[idx.y].backPosition;
    } break;

    case 2: {
      gl_Position = (front) ? vData[idx.y].frontPosition : vData[idx.x].backPosition;
    } break;
  }

  /* Apply depth bias. Prevents Z-fighting artifacts when fast-math is enabled. */
  gl_Position.z += 0.00005;
}

void main()
{
  /* Output Data indexing. */
  int input_prim_index = int(gl_VertexID / 6);
  int output_vertex_id = gl_VertexID % 6;
  int output_triangle_id = output_vertex_id / 3;

  /* Source primitive data location derived from output primitive. */
  int input_base_vertex_id = input_prim_index * 3;

  /* In data is triangles - Should be guaranteed. */
  /* Read input position data. */
  vData[0].pos = vertex_fetch_attribute(input_base_vertex_id + 0, pos, vec3);
  vData[1].pos = vertex_fetch_attribute(input_base_vertex_id + 1, pos, vec3);
  vData[2].pos = vertex_fetch_attribute(input_base_vertex_id + 2, pos, vec3);

  /* Calculate front/back Positions. */
  vData[0].frontPosition = point_object_to_ndc(vData[0].pos);
  vData[0].backPosition = point_object_to_ndc(vData[0].pos + lightDirection * lightDistance);

  vData[1].frontPosition = point_object_to_ndc(vData[1].pos);
  vData[1].backPosition = point_object_to_ndc(vData[1].pos + lightDirection * lightDistance);

  vData[2].frontPosition = point_object_to_ndc(vData[2].pos);
  vData[2].backPosition = point_object_to_ndc(vData[2].pos + lightDirection * lightDistance);

  /* Geometry shader equivalent calc. */
  vec3 v10 = vData[0].pos - vData[1].pos;
  vec3 v12 = vData[2].pos - vData[1].pos;

  vec3 n = cross(v12, v10);
  float facing = dot(n, lightDirection);

  bool backface = facing > 0.0;

#ifdef DOUBLE_MANIFOLD
  /* In case of non manifold geom, we only increase/decrease
   * the stencil buffer by one but do every faces as they were facing the light. */
  bool invert = backface;
  const bool is_manifold = false;
#else
  const bool invert = false;
  const bool is_manifold = true;
#endif

  if (!is_manifold || !backface) {
    bool do_front = (output_triangle_id == 0) ? true : false;
    emit_cap(do_front, invert, output_vertex_id % 3);
  }
  else {
    DISCARD_VERTEX
  }
}
