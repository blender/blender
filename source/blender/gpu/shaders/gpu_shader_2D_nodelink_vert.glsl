/**
 * 2D Cubic Bezier thick line drawing
 */

/**
 * `uv.x` is position along the curve, defining the tangent space.
 * `uv.y` is "signed" distance (compressed to [0..1] range) from the pos in expand direction
 * `pos` is the verts position in the curve tangent space
 */

#define MID_VERTEX 65

void main(void)
{
  const float start_gradient_threshold = 0.35;
  const float end_gradient_threshold = 0.65;

#ifdef USE_INSTANCE
#  define colStart (colid_doarrow[0] < 3u ? start_color : node_link_data.colors[colid_doarrow[0]])
#  define colEnd (colid_doarrow[1] < 3u ? end_color : node_link_data.colors[colid_doarrow[1]])
#  define colShadow node_link_data.colors[colid_doarrow[2]]
#  define doArrow (colid_doarrow[3] != 0u)
#  define doMuted (domuted[0] != 0u)
#else
  vec2 P0 = node_link_data.bezierPts[0].xy;
  vec2 P1 = node_link_data.bezierPts[1].xy;
  vec2 P2 = node_link_data.bezierPts[2].xy;
  vec2 P3 = node_link_data.bezierPts[3].xy;
  bool doArrow = node_link_data.doArrow;
  bool doMuted = node_link_data.doMuted;
  float dim_factor = node_link_data.dim_factor;
  float thickness = node_link_data.thickness;
  vec3 dash_params = node_link_data.dash_params.xyz;

  vec4 colShadow = node_link_data.colors[0];
  vec4 colStart = node_link_data.colors[1];
  vec4 colEnd = node_link_data.colors[2];
#endif

  float line_thickness = thickness;

  if (gl_VertexID < MID_VERTEX) {
    /* Outline pass. */
    finalColor = colShadow;
  }
  else {
    /* Second pass. */
    if (uv.x < start_gradient_threshold) {
      finalColor = colStart;
    }
    else if (uv.x > end_gradient_threshold) {
      finalColor = colEnd;
    }
    else {
      float mixFactor = (uv.x - start_gradient_threshold) /
                        (end_gradient_threshold - start_gradient_threshold);
      finalColor = mix(colStart, colEnd, mixFactor);
    }
    line_thickness *= 0.65f;
    if (doMuted) {
      finalColor[3] = 0.65;
    }
  }

  aspect = node_link_data.aspect;
  /* Parameters for the dashed line. */
  isMainLine = expand.y != 1.0 ? 0 : 1;
  dashLength = dash_params.x;
  dashFactor = dash_params.y;
  dashAlpha = dash_params.z;
  /* Approximate line length, no need for real bezier length calculation. */
  lineLength = distance(P0, P3);
  /* TODO: Incorrect U, this leads to non-uniform dash distribution. */
  lineU = uv.x;

  float t = uv.x;
  float t2 = t * t;
  float t2_3 = 3.0 * t2;
  float one_minus_t = 1.0 - t;
  float one_minus_t2 = one_minus_t * one_minus_t;
  float one_minus_t2_3 = 3.0 * one_minus_t2;

  vec2 point = (P0 * one_minus_t2 * one_minus_t + P1 * one_minus_t2_3 * t +
                P2 * t2_3 * one_minus_t + P3 * t2 * t);

  vec2 tangent = ((P1 - P0) * one_minus_t2_3 + (P2 - P1) * 6.0 * (t - t2) + (P3 - P2) * t2_3);

  /* Tangent space at t. If the inner and outer control points overlap, the tangent is invalid.
   * Use the vector between the sockets instead. */
  tangent = is_zero(tangent) ? normalize(P3 - P0) : normalize(tangent);
  vec2 normal = tangent.yx * vec2(-1.0, 1.0);

  /* Position vertex on the curve tangent space */
  point += (pos.x * tangent + pos.y * normal) * node_link_data.arrowSize;

  gl_Position = ModelViewProjectionMatrix * vec4(point, 0.0, 1.0);

  vec2 exp_axis = expand.x * tangent + expand.y * normal;

  /* rotate & scale the expand axis */
  exp_axis = ModelViewProjectionMatrix[0].xy * exp_axis.xx +
             ModelViewProjectionMatrix[1].xy * exp_axis.yy;

  float expand_dist = line_thickness * (uv.y * 2.0 - 1.0);
  colorGradient = expand_dist;
  lineThickness = line_thickness;

  finalColor[3] *= dim_factor;

  /* Expand into a line */
  gl_Position.xy += exp_axis * node_link_data.aspect * expand_dist;

  /* If the link is not muted or is not a reroute arrow the points are squashed to the center of
   * the line. Magic numbers are defined in drawnode.c */
  if ((expand.x == 1.0 && !doMuted) ||
      (expand.y != 1.0 && (pos.x < 0.70 || pos.x > 0.71) && !doArrow))
  {
    gl_Position.xy *= 0.0;
  }
}
