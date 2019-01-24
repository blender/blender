
uniform mat4 ModelViewProjectionMatrix;
uniform mat4 ModelMatrix;
uniform mat3 NormalMatrix;

uniform vec2 wireStepParam;

vec3 get_edge_sharpness(vec3 wd)
{
	bvec3 do_edge = greaterThan(wd, vec3(0.0));
	bvec3 force_edge = equal(wd, vec3(1.0));
	wd = clamp(wireStepParam.x * wd + wireStepParam.y, 0.0, 1.0);
	return clamp(wd * vec3(do_edge) + vec3(force_edge), 0.0, 1.0);
}

float get_edge_sharpness(float wd)
{
	bool do_edge = (wd > 0.0);
	bool force_edge = (wd == 1.0);
	wd = (wireStepParam.x * wd + wireStepParam.y);
	return clamp(wd * float(do_edge) + float(force_edge), 0.0, 1.0);
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

/* Consecutive pos of the nth vertex
 * Only valid for first vertex in the triangle.
 * Assuming GL_FRIST_VERTEX_CONVENTION. */
in vec3 pos0;
in vec3 pos1;
in vec3 pos2;
in float wd0; /* wiredata */
in float wd1;
in float wd2;
in vec3 nor;

out float facing;
out vec3 barycentric;
flat out vec3 edgeSharpness;

void main()
{
	int v_n = gl_VertexID % 3;

	barycentric = vec3(equal(ivec3(2, 0, 1), ivec3(v_n)));

	vec3 wb = vec3(wd0, wd1, wd2);
	edgeSharpness = get_edge_sharpness(wb);

	/* Don't generate any fragment if there is no edge to draw. */
	vec3 pos = (!any(greaterThan(edgeSharpness, vec3(0.04))) && (v_n == 0)) ? pos1 : pos0;

	gl_Position = ModelViewProjectionMatrix * vec4(pos, 1.0);

	facing = normalize(NormalMatrix * nor).z;

#ifdef USE_WORLD_CLIP_PLANES
	world_clip_planes_calc_clip_distance((ModelMatrix * vec4(pos, 1.0)).xyz);
#endif
}

#endif /* SELECT_EDGES */
