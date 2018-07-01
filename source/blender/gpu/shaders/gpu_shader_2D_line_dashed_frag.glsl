
/*
 * Fragment Shader for dashed lines, with uniform multi-color(s), or any single-color, and any thickness.
 *
 * Dashed is performed in screen space.
 */

uniform float dash_width;

/* Simple mode, discarding non-dash parts (so no need for blending at all). */
uniform float dash_factor;  /* if > 1.0, solid line. */

/* More advanced mode, allowing for complex, multi-colored patterns. Enabled when colors_len > 0. */
/* Note: max number of steps/colors in pattern is 32! */
uniform int colors_len;  /* Enabled if > 0, 1 for solid line. */
uniform vec4 colors[32];

noperspective in float distance_along_line;
noperspective in vec4 color_geom;

out vec4 fragColor;

void main()
{
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
			fragColor = color_geom;
		}
		/* Actually dashed line... */
		else {
			float normalized_distance = fract(distance_along_line / dash_width);
			if (normalized_distance <= dash_factor) {
				fragColor = color_geom;
			}
			else {
				discard;
			}
		}
	}
}
