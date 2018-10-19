
uniform mat4 ModelViewProjectionMatrix;
uniform vec2 ViewportSize = vec2(-1, -1);

const float line_falloff = 1.0;
const float circle_scale = sqrt(2.0 / 3.1416);
const float square_scale = sqrt(0.5);

const float diagonal_scale = sqrt(0.5);

in vec2 pos;
in float size;
in vec4 color;
in vec4 outlineColor;
in int flags;

flat out vec4 finalColor;
flat out vec4 finalOutlineColor;

flat out int finalFlags;

flat out vec4 radii;
flat out vec4 thresholds;

bool test(int bit) {
	return (flags & bit) != 0;
}

vec2 line_thresholds(float width) {
	return vec2(max(0, width - line_falloff), width);
}

void main() {
	gl_Position = ModelViewProjectionMatrix * vec4(pos, 0.0, 1.0);

	/* Align to pixel grid if the viewport size is known. */
	if (ViewportSize.x > 0) {
		vec2 scale = ViewportSize * 0.5;
		vec2 px_pos = (gl_Position.xy + 1) * scale;
		vec2 adj_pos = round(px_pos - 0.5) + 0.5;
		gl_Position.xy = adj_pos / scale - 1;
	}

	/* Pass through parameters. */
	finalColor = color;
	finalOutlineColor = outlineColor;
	finalFlags = flags;

	if (!test(0xF)) {
		finalFlags |= 1;
	}

	/* Size-dependent line thickness. */
	float half_width = (0.06 + (size - 10) * 0.04);
	float line_width = half_width + line_falloff;

	/* Outline thresholds. */
	thresholds.xy = line_thresholds(line_width);

	/* Inner dot thresholds. */
	thresholds.zw = line_thresholds(line_width * 1.6);

	/* Extend the primitive size by half line width on either side; odd for symmetry. */
	float ext_radius = round(0.5 * size) + thresholds.x;

	gl_PointSize = ceil(ext_radius + thresholds.y) * 2 + 1;

	/* Diamond radius. */
	radii[0] = ext_radius * diagonal_scale;

	/* Circle radius. */
	radii[1] = ext_radius * circle_scale;

	/* Square radius. */
	radii[2] = round(ext_radius * square_scale);

	/* Min/max cutout offset. */
	radii[3] = -line_falloff;

	/* Convert to PointCoord units. */
	radii /= gl_PointSize;
	thresholds /= gl_PointSize;
}
