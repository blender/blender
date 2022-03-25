
/*
 * Fragment Shader for dashed lines, with uniform multi-color(s),
 * or any single-color, and any thickness.
 *
 * Dashed is performed in screen space.
 */

#ifndef USE_GPU_SHADER_CREATE_INFO

uniform float dash_width;

/* Simple mode, discarding non-dash parts (so no need for blending at all). */
uniform float dash_factor; /* if > 1.0, solid line. */

/* More advanced mode, allowing for complex, multi-colored patterns.
 * Enabled when colors_len > 0. */
/* NOTE: max number of steps/colors in pattern is 32! */
uniform int colors_len; /* Enabled if > 0, 1 for solid line. */
uniform vec4 colors[32];

flat in vec4 color_vert;

noperspective in vec2 stipple_pos;
flat in vec2 stipple_start;

out vec4 fragColor;
#endif

void main()
{
  float distance_along_line = distance(stipple_pos, stipple_start);
  /* Multi-color option. */
  if (colors_len > 0) {
    /* Solid line case, simple. */
    if (colors_len == 1) {
      fragColor = colors[0];
    }
    /* Actually dashed line... */
    else {
      float normalized_distance = fract(distance_along_line / dash_width);
      fragColor = colors[int(normalized_distance * colors_len)];
    }
  }
  /* Single color option. */
  else {
    /* Solid line case, simple. */
    if (dash_factor >= 1.0f) {
      fragColor = color_vert;
    }
    /* Actually dashed line... */
    else {
      float normalized_distance = fract(distance_along_line / dash_width);
      if (normalized_distance <= dash_factor) {
        fragColor = color_vert;
      }
      else {
        discard;
      }
    }
  }
}
