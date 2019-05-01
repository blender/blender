
#define M_1_SQRTPI 0.5641895835477563 /* 1/sqrt(pi) */

/**
 * We want to know how much a pixel is covered by a line.
 * We replace the square pixel with acircle of the same area and try to find the intersection area.
 * The area we search is the circular segment. https://en.wikipedia.org/wiki/Circular_segment
 * The formula for the area uses inverse trig function and is quite complexe. Instead,
 * we approximate it by using the smoothstep function and a 1.05 factor to the disc radius.
 */
#define DISC_RADIUS (M_1_SQRTPI * 1.05)
#define GRID_LINE_SMOOTH_START (0.5 - DISC_RADIUS)
#define GRID_LINE_SMOOTH_END (0.5 + DISC_RADIUS)

flat in vec4 finalColorOuter_f;
in vec4 finalColor_f;
noperspective in float edgeCoord_f;

out vec4 FragColor;

void main()
{
  float dist = abs(edgeCoord_f) - max(sizeEdge - 0.5, 0.0);
  float dist_outer = dist - max(sizeEdge, 1.0);
#ifdef USE_SMOOTH_WIRE
  float mix_w = smoothstep(GRID_LINE_SMOOTH_START, GRID_LINE_SMOOTH_END, dist);
  float mix_w_outer = smoothstep(GRID_LINE_SMOOTH_START, GRID_LINE_SMOOTH_END, dist_outer);
#else
  float mix_w = step(0.5, dist);
  float mix_w_outer = step(0.5, dist_outer);
#endif
  /* Line color & alpha. */
  FragColor = mix(finalColorOuter_f, finalColor_f, 1.0 - mix_w * finalColorOuter_f.a);
  /* Line edges shape. */
  FragColor.a *= 1.0 - (finalColorOuter_f.a > 0.0 ? mix_w_outer : mix_w);
}
