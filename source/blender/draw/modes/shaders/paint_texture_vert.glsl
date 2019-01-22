
uniform mat4 ModelViewProjectionMatrix;
uniform mat4 ModelMatrix;

in vec2 u; /* active uv map */
in vec3 pos;

out vec2 uv_interp;

void main()
{
	gl_Position = ModelViewProjectionMatrix * vec4(pos, 1.0);

	uv_interp = u;

#ifdef USE_WORLD_CLIP_PLANES
		world_clip_planes_calc_clip_distance((ModelMatrix * vec4(pos, 1.0)).xyz);
#endif
}
