
// Draw dashed lines, perforated in screen space.

noperspective in float distance_along_line;
out vec4 fragColor;

uniform float dash_width;

/* Simple mode, discarding non-dash parts (so no need for blending at all). */
uniform float dash_factor;  /* if > 1.0, solid line. */
uniform vec4 color;

/* More advanced mode, allowing for complex, multi-colored patterns. Enabled when num_colors > 0. */
/* Note: max number of steps/colors in pattern is 32! */
uniform int num_colors;  /* Enabled if > 0, 1 for solid line. */
uniform vec4 colors[32];

void main()
{
	/* Solid line cases, simple. */
	if (num_colors == 1) {
		fragColor = colors[0];
	}
	else if (dash_factor >= 1.0f) {
		fragColor = color;
	}
	else {
		/* Actually dashed line... */
		float normalized_distance = fract(distance_along_line / dash_width);
		if (num_colors > 0) {
			fragColor = colors[int(normalized_distance * num_colors)];
		}
		else {
			if (normalized_distance <= dash_factor) {
				fragColor = color;
			}
			else {
				discard;
			}
		}
	}
}
