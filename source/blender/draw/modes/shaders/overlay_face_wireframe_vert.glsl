
uniform mat4 ModelViewProjectionMatrix;
uniform mat4 ModelMatrix;
uniform mat3 NormalMatrix;

uniform float wireStepParam;

float get_edge_sharpness(float wd)
{
	return (wd == 1.0) ? 1.0 : ((wd == 0.0) ? -1.0 : (wd + wireStepParam));
}

/* Geometry shader version */
#if defined(SELECT_EDGES) || defined(USE_SCULPT)

in vec3 pos;
in vec3 nor;
in float wd; /* wiredata */

out float facing_g;
out float edgeSharpness_g;

void main()
{
#  ifndef USE_SCULPT
	edgeSharpness_g = get_edge_sharpness(wd);
#  else
	/* TODO approximation using normals. */
	edgeSharpness_g = 1.0;
#  endif

	gl_Position = ModelViewProjectionMatrix * vec4(pos, 1.0);

	facing_g = normalize(NormalMatrix * nor).z;

#ifdef USE_WORLD_CLIP_PLANES
	world_clip_planes_calc_clip_distance((ModelMatrix * vec4(pos, 1.0)).xyz);
#endif
}

#else /* SELECT_EDGES */

in vec3 pos;
in vec3 nor;
in float wd;

out float facing;
flat out float edgeSharpness;

void main()
{
	gl_Position = ModelViewProjectionMatrix * vec4(pos, 1.0);

	edgeSharpness = get_edge_sharpness(wd);

	facing = normalize(NormalMatrix * nor).z;

#ifdef USE_WORLD_CLIP_PLANES
	world_clip_planes_calc_clip_distance((ModelMatrix * vec4(pos, 1.0)).xyz);
#endif
}

#endif /* SELECT_EDGES */
