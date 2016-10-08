
uniform float size;
uniform float outlineWidth;

#if __VERSION__ == 120
  attribute vec2 pos;
  varying vec4 radii;
#else
  in vec2 pos;
  out vec4 radii;
#endif

void main() {
	gl_Position = gl_ModelViewProjectionMatrix * vec4(pos, 0.0, 1.0);
	gl_PointSize = size;

	// calculate concentric radii in pixels
	float radius = 0.5 * size;

	// start at the outside and progress toward the center
	radii[0] = radius;
	radii[1] = radius - 1.0;
	radii[2] = radius - outlineWidth;
	radii[3] = radius - outlineWidth - 1.0;

	// convert to PointCoord units
	radii /= size;
}
