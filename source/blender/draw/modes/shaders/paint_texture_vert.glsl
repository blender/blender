
uniform mat4 ModelViewProjectionMatrix;
uniform mat4 ModelMatrix;

in vec2 u; /* active uv map */
in vec3 pos;

#ifdef TEXTURE_PAINT_MASK
in vec2 mu; /* masking uv map */
#endif

out vec2 uv_interp;

#ifdef TEXTURE_PAINT_MASK
out vec2 masking_uv_interp;
#endif

void main()
{
	gl_Position = ModelViewProjectionMatrix * vec4(pos, 1.0);

	uv_interp = u;

#ifdef TEXTURE_PAINT_MASK
	masking_uv_interp = mu;
#endif

#ifdef USE_WORLD_CLIP_PLANES
	world_clip_planes_calc_clip_distance((ModelMatrix * vec4(pos, 1.0)).xyz);
#endif
}
