
uniform mat4 ModelViewProjectionMatrix;
#ifdef USE_WORLD_CLIP_PLANES
uniform mat4 ModelMatrix;
#endif
uniform float size;
uniform float outlineWidth;

in vec3 pos;
out vec4 radii;

void main() {
	vec4 pos_4d = vec4(pos, 1.0);
	gl_Position = ModelViewProjectionMatrix * pos_4d;
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

#ifdef USE_WORLD_CLIP_PLANES
	world_clip_planes_calc_clip_distance((ModelMatrix * pos_4d).xyz);
#endif
}
