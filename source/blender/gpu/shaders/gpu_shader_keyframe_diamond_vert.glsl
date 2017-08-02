
uniform mat4 ModelViewProjectionMatrix;

const float pixel_fudge = sqrt(2.0);
const float outline_width = 1.15 * pixel_fudge;

in vec2 pos;
in float size;
in vec4 color;
in vec4 outlineColor;
out vec4 finalColor;
out vec4 finalOutlineColor;
out vec4 radii;

void main() {
	gl_Position = ModelViewProjectionMatrix * vec4(pos, 0.0, 1.0);

	// pass through unchanged
	gl_PointSize = size + pixel_fudge; // 0.5 pixel_fudge on either side
	finalColor = color;
	finalOutlineColor = outlineColor;

	// calculate concentric radii in pixels
	float radius = 0.5 * gl_PointSize;

	// start at the outside and progress toward the center
	radii[0] = radius;
	radii[1] = radius - pixel_fudge;
	radii[2] = radius - outline_width;
	radii[3] = radius - outline_width - pixel_fudge;

	// convert to PointCoord units
	radii /= size;
}
