
uniform mat4 ModelViewProjectionMatrix;

in vec3 pos;

#ifndef UNIFORM_ID
uniform uint offset;
in uint color;

flat out vec4 id;
#endif

void main()
{
#ifndef UNIFORM_ID
	id = vec4(
		(((color + offset)      ) & uint(0xFF)) * (1.0f / 255.0f),
		(((color + offset) >>  8) & uint(0xFF)) * (1.0f / 255.0f),
		(((color + offset) >> 16) & uint(0xFF)) * (1.0f / 255.0f),
		(((color + offset) >> 24)             ) * (1.0f / 255.0f));
#endif

	vec4 pos_4d = vec4(pos, 1.0);
	gl_Position = ModelViewProjectionMatrix * pos_4d;

#ifdef USE_WORLD_CLIP_PLANES
	/* Warning: ModelMatrix is typically used but select drawing is different. */
	world_clip_planes_calc_clip_distance(pos);
#endif
}
