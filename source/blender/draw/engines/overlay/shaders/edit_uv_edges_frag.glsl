#pragma BLENDER_REQUIRE(common_globals_lib.glsl)
#pragma BLENDER_REQUIRE(common_overlay_lib.glsl)

uniform int lineStyle;
uniform bool doSmoothWire;
uniform float alpha;
uniform float dashLength;

in float selectionFac_f;
noperspective in float edgeCoord_f;
noperspective in vec2 stipplePos_f;
flat in vec2 stippleStart_f;

layout(location = 0) out vec4 fragColor;

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

void main()
{
  vec4 inner_color = vec4(vec3(0.0), 1.0);
  vec4 outer_color = vec4(0.0);

  vec2 dd = fwidth(stipplePos_f);
  float line_distance = distance(stipplePos_f, stippleStart_f) / max(dd.x, dd.y);

  if (lineStyle == OVERLAY_UV_LINE_STYLE_OUTLINE) {
    inner_color = mix(colorWireEdit, colorEdgeSelect, selectionFac_f);
    outer_color = vec4(vec3(0.0), 1.0);
  }
  else if (lineStyle == OVERLAY_UV_LINE_STYLE_DASH) {
    if (fract(line_distance / dashLength) < 0.5) {
      inner_color = mix(vec4(1.0), colorEdgeSelect, selectionFac_f);
    }
  }
  else if (lineStyle == OVERLAY_UV_LINE_STYLE_BLACK) {
    vec4 base_color = vec4(vec3(0.0), 1.0);
    inner_color = mix(base_color, colorEdgeSelect, selectionFac_f);
  }
  else if (lineStyle == OVERLAY_UV_LINE_STYLE_WHITE) {
    vec4 base_color = vec4(1.0);
    inner_color = mix(base_color, colorEdgeSelect, selectionFac_f);
  }
  else if (lineStyle == OVERLAY_UV_LINE_STYLE_SHADOW) {
    inner_color = colorUVShadow;
  }

  float dist = abs(edgeCoord_f) - max(sizeEdge - 0.5, 0.0);
  float dist_outer = dist - max(sizeEdge, 1.0);
  float mix_w;
  float mix_w_outer;

  if (doSmoothWire) {
    mix_w = smoothstep(GRID_LINE_SMOOTH_START, GRID_LINE_SMOOTH_END, dist);
    mix_w_outer = smoothstep(GRID_LINE_SMOOTH_START, GRID_LINE_SMOOTH_END, dist_outer);
  }
  else {
    mix_w = step(0.5, dist);
    mix_w_outer = step(0.5, dist_outer);
  }

  vec4 final_color = mix(outer_color, inner_color, 1.0 - mix_w * outer_color.a);
  final_color.a *= 1.0 - (outer_color.a > 0.0 ? mix_w_outer : mix_w);
  final_color.a *= alpha;

  fragColor = final_color;
}
