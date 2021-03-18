/**
 * 2D Quadratic Bezier thick line drawing
 */

#define MID_VERTEX 65

/* u is position along the curve, defining the tangent space.
 * v is "signed" distance (compressed to [0..1] range) from the pos in expand direction */
in vec2 uv;
in vec2 pos; /* verts position in the curve tangent space */
in vec2 expand;

#ifdef USE_INSTANCE
/* Instance attrs. */
in vec2 P0;
in vec2 P1;
in vec2 P2;
in vec2 P3;
in ivec4 colid_doarrow;
in ivec2 domuted;

uniform vec4 colors[6];

#  define colStart colors[colid_doarrow[0]]
#  define colEnd colors[colid_doarrow[1]]
#  define colShadow colors[colid_doarrow[2]]
#  define doArrow (colid_doarrow[3] != 0)
#  define doMuted (domuted[0] != 0)

#else
/* Single curve drawcall, use uniform. */
uniform vec2 bezierPts[4];

#  define P0 bezierPts[0]
#  define P1 bezierPts[1]
#  define P2 bezierPts[2]
#  define P3 bezierPts[3]

uniform vec4 colors[3];
uniform bool doArrow;
uniform bool doMuted;

#  define colShadow colors[0]
#  define colStart colors[1]
#  define colEnd colors[2]

#endif

uniform float expandSize;
uniform float arrowSize;
uniform mat4 ModelViewProjectionMatrix;

out float colorGradient;
out vec4 finalColor;

void main(void)
{
  float t = uv.x;
  float t2 = t * t;
  float t2_3 = 3.0 * t2;
  float one_minus_t = 1.0 - t;
  float one_minus_t2 = one_minus_t * one_minus_t;
  float one_minus_t2_3 = 3.0 * one_minus_t2;

  vec2 point = (P0 * one_minus_t2 * one_minus_t + P1 * one_minus_t2_3 * t +
                P2 * t2_3 * one_minus_t + P3 * t2 * t);

  vec2 tangent = ((P1 - P0) * one_minus_t2_3 + (P2 - P1) * 6.0 * (t - t2) + (P3 - P2) * t2_3);

  /* tangent space at t */
  tangent = normalize(tangent);
  vec2 normal = tangent.yx * vec2(-1.0, 1.0);

  /* Position vertex on the curve tangent space */
  point += (pos.x * tangent + pos.y * normal) * arrowSize;

  gl_Position = ModelViewProjectionMatrix * vec4(point, 0.0, 1.0);

  vec2 exp_axis = expand.x * tangent + expand.y * normal;

  /* rotate & scale the expand axis */
  exp_axis = ModelViewProjectionMatrix[0].xy * exp_axis.xx +
             ModelViewProjectionMatrix[1].xy * exp_axis.yy;

  float expand_dist = (uv.y * 2.0 - 1.0);
  colorGradient = expand_dist;

  if (gl_VertexID < MID_VERTEX) {
    /* Shadow pass */
    finalColor = colShadow;
  }
  else {
    /* Second pass */
    finalColor = mix(colStart, colEnd, uv.x);
    expand_dist *= 0.5;
    if (doMuted) {
      finalColor[3] = 0.65;
    }
  }

  /* Expand into a line */
  gl_Position.xy += exp_axis * expandSize * expand_dist;

  /* If the link is not muted or is not a reroute arrow the points are squashed to the center of
   * the line. Magic numbers are defined in drawnode.c */
  if ((expand.x == 1.0 && !doMuted) ||
      (expand.y != 1.0 && (pos.x < 0.70 || pos.x > 0.71) && !doArrow)) {
    gl_Position.xy *= 0.0;
  }
}
