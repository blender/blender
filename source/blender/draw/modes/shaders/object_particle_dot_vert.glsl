
uniform mat4 ModelViewMatrix;
uniform mat4 ProjectionMatrix;
uniform float pixel_size;
uniform float size;

in vec3 pos;
in float val;

out vec4 radii;
flat out float finalVal;

void main() {
	gl_Position = ModelViewMatrix * vec4(pos, 1.0);

	float psize = (ProjectionMatrix[3][3] == 0.0) ? (size / (-gl_Position.z * pixel_size)) : (size / pixel_size);

	gl_PointSize = psize;

	// calculate concentric radii in pixels
	float radius = 0.5 * psize;

	// start at the outside and progress toward the center
	radii[0] = radius;
	radii[1] = radius - 1.0;
	radii[2] = radius - 1.0;
	radii[3] = radius - 2.0;

	// convert to PointCoord units
	radii /= psize;

	gl_Position = ProjectionMatrix * gl_Position;

	finalVal = val;
}
