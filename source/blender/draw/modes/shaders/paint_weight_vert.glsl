
uniform mat4 ModelViewProjectionMatrix;
uniform mat4 ModelMatrix;

in float weight;
in vec3 pos;

out vec2 weight_interp; /* (weight, alert) */

void main()
{
	gl_Position = ModelViewProjectionMatrix * vec4(pos, 1.0);

	/* Separate actual weight and alerts for independent interpolation */
	weight_interp = max(vec2(weight, -weight), 0.0);

#ifdef USE_WORLD_CLIP_PLANES
	world_clip_planes_calc_clip_distance((ModelMatrix * vec4(pos, 1.0)).xyz);
#endif
}
