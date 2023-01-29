
#pragma USE_SSBO_VERTEX_FETCH(LineList, 2)
#pragma BLENDER_REQUIRE(common_view_clipping_lib.glsl)
#pragma BLENDER_REQUIRE(common_view_lib.glsl)

#define DISCARD_VERTEX \
  gl_Position = finalColor = vec4(0.0); \
  edgeStart = edgePos = vec2(0.0); \
  return;

/* Project to screen space. */
vec2 proj(vec4 pos)
{
  return (0.5 * (pos.xy / pos.w) + 0.5) * sizeViewport.xy;
}

void do_vertex_shader(mat4 in_inst_obmat,
                      vec3 in_pos,
                      vec3 in_snor,
                      out vec4 out_pPos,
                      out vec3 out_vPos,
                      out vec2 out_ssPos,
                      out vec2 out_ssNor,
                      out vec4 out_vColSize,
                      out int out_inverted,
                      out vec4 out_wpos)
{
  vec4 bone_color, state_color;
  mat4 model_mat = extract_matrix_packed_data(in_inst_obmat, state_color, bone_color);

  vec4 worldPosition = model_mat * vec4(in_pos, 1.0);
  vec4 viewpos = ViewMatrix * worldPosition;
  out_wpos = worldPosition;
  out_vPos = viewpos.xyz;
  out_pPos = ProjectionMatrix * viewpos;

  out_inverted = int(dot(cross(model_mat[0].xyz, model_mat[1].xyz), model_mat[2].xyz) < 0.0);

  /* This is slow and run per vertex, but it's still faster than
   * doing it per instance on CPU and sending it on via instance attribute. */
  mat3 normal_mat = transpose(inverse(mat3(model_mat)));
  /* TODO FIX: there is still a problem with this vector
   * when the bone is scaled or in persp mode. But it's
   * barely visible at the outline corners. */
  out_ssNor = normalize(normal_world_to_view(normal_mat * in_snor).xy);

  out_ssPos = proj(out_pPos);

  out_vColSize = bone_color;
}

void main()
{
  /* Outputs a singular vertex as part of a LineList primitive, however, requires access to
   * neighboring 4 vertices. */
  /* Fetch verts from input type lines adjacency. */
  int line_prim_id = (gl_VertexID / 2);
  int line_vertex_id = gl_VertexID % 2;
  int base_vertex_id = line_prim_id * 2;

  /* IF Input Primitive Type == Lines_Adjacency, then indices are accessed as per GL specification:
   * i.e. 4 indices per unique prim (Provoking vert 4i-2)
   *
   * IF Input Primitive Type == LineStrip_Adjacency, then indices are accessed using:
   *  - 2 indices per unique prim, plus 1 index at each end, such that the strided
   *  - 4-index block can be walked. */
  vec3 in_pos[4];
  in_pos[0] = vertex_fetch_attribute_raw(vertex_id_from_index_id(4 * line_prim_id), pos, vec3);
  in_pos[1] = vertex_fetch_attribute_raw(vertex_id_from_index_id(4 * line_prim_id + 1), pos, vec3);
  in_pos[2] = vertex_fetch_attribute_raw(vertex_id_from_index_id(4 * line_prim_id + 2), pos, vec3);
  in_pos[3] = vertex_fetch_attribute_raw(vertex_id_from_index_id(4 * line_prim_id + 3), pos, vec3);

  vec3 in_snor[4];
  in_snor[0] = vertex_fetch_attribute_raw(vertex_id_from_index_id(4 * line_prim_id), snor, vec3);
  in_snor[1] = vertex_fetch_attribute_raw(
      vertex_id_from_index_id(4 * line_prim_id + 1), snor, vec3);
  in_snor[2] = vertex_fetch_attribute_raw(
      vertex_id_from_index_id(4 * line_prim_id + 2), snor, vec3);
  in_snor[3] = vertex_fetch_attribute_raw(
      vertex_id_from_index_id(4 * line_prim_id + 3), snor, vec3);

  mat4 in_inst_obmat = vertex_fetch_attribute(gl_VertexID, inst_obmat, mat4);

  /* Run original GL vertex shader implementation per vertex in adjacency list. */
  vec4 pPos[4];
  vec3 vPos[4];
  vec2 ssPos[4];
  vec2 ssNor[4];
  vec4 vColSize[4];
  int inverted[4];
  vec4 wPos[4];

  for (int v = 0; v < 4; v++) {
    do_vertex_shader(in_inst_obmat,
                     in_pos[v],
                     in_snor[v],
                     pPos[v],
                     vPos[v],
                     ssPos[v],
                     ssNor[v],
                     vColSize[v],
                     inverted[v],
                     wPos[v]);
  }

  /* Geometry Shader equivalent to calculate vertex output position. */
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
      DISCARD_VERTEX
    }
  }

  n0 = (inverted[0] == 1) ? -n0 : n0;
  /* Don't outline if concave edge. */
  if (dot(n0, v13) > 0.0001) {
    DISCARD_VERTEX
  }

  vec2 perp = normalize(ssPos[2] - ssPos[1]);
  vec2 edge_dir = vec2(-perp.y, perp.x);

  vec2 hidden_point;
  /* Take the farthest point to compute edge direction
   * (avoid problems with point behind near plane).
   * If the chosen point is parallel to the edge in screen space,
   * choose the other point anyway.
   * This fixes some issue with cubes in orthographic views. */
  if (vPos[0].z < vPos[3].z) {
    hidden_point = (abs(fac0) > 1e-5) ? ssPos[0] : ssPos[3];
  }
  else {
    hidden_point = (abs(fac3) > 1e-5) ? ssPos[3] : ssPos[0];
  }
  vec2 hidden_dir = normalize(hidden_point - ssPos[1]);

  float fac = dot(-hidden_dir, edge_dir);
  edge_dir *= (fac < 0.0) ? -1.0 : 1.0;

  /* Output corresponding value based on which vertex this corresponds to in the
   * original input primitive. */
  if (line_vertex_id == 0) {
    gl_Position = pPos[1];
    /* Offset away from the center to avoid overlap with solid shape. */
    gl_Position.xy += (edge_dir - perp) * sizeViewportInv * gl_Position.w;
    /* Improve AA bleeding inside bone silhouette. */
    gl_Position.z -= (is_persp) ? 1e-4 : 1e-6;
    edgeStart = edgePos = ((gl_Position.xy / gl_Position.w) * 0.5 + 0.5) * sizeViewport.xy;
    view_clipping_distances(wPos[1].xyz);
  }
  else {
    gl_Position = pPos[2];
    /* Offset away from the center to avoid overlap with solid shape. */
    gl_Position.xy += (edge_dir + perp) * sizeViewportInv * gl_Position.w;
    /* Improve AA bleeding inside bone silhouette. */
    gl_Position.z -= (is_persp) ? 1e-4 : 1e-6;
    edgeStart = edgePos = ((gl_Position.xy / gl_Position.w) * 0.5 + 0.5) * sizeViewport.xy;
    view_clipping_distances(wPos[2].xyz);
  }
}
