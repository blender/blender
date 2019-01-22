
uniform mat4 ModelViewProjectionMatrix;
uniform mat4 ModelMatrix;

in vec3 pos;
in vec4 nor; /* select flag on the 4th component */

void main()
{
	gl_Position = ModelViewProjectionMatrix * vec4(pos, 1.0);
	/* Don't draw faces that are selected. */
	if (nor.w > 0.0) {
		gl_Position = vec4(0.0, 0.0, 0.0, 1.0);
	}
	else {
#ifdef USE_WORLD_CLIP_PLANES
		world_clip_planes_calc_clip_distance((ModelMatrix * vec4(pos, 1.0)).xyz);
#endif
	}
}
