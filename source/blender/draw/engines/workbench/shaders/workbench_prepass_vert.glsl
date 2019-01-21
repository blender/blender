uniform mat4 ModelViewProjectionMatrix;
uniform mat4 ModelMatrix;
uniform mat4 ModelMatrixInverse;
uniform mat4 ProjectionMatrix;
uniform mat4 ViewProjectionMatrix;
uniform mat4 ViewMatrixInverse;
uniform mat3 NormalMatrix;

#ifndef HAIR_SHADER
in vec3 pos;
in vec3 nor;
in vec2 u; /* active texture layer */
#define uv u
#else /* HAIR_SHADER */
#  ifdef V3D_SHADING_TEXTURE_COLOR
uniform samplerBuffer u; /* active texture layer */
#  endif
flat out float hair_rand;
#endif /* HAIR_SHADER */

#ifdef NORMAL_VIEWPORT_PASS_ENABLED
out vec3 normal_viewport;
#endif

#ifdef V3D_SHADING_TEXTURE_COLOR
out vec2 uv_interp;
#endif

/* From http://libnoise.sourceforge.net/noisegen/index.html */
float integer_noise(int n)
{
	n = (n >> 13) ^ n;
	int nn = (n * (n * n * 60493 + 19990303) + 1376312589) & 0x7fffffff;
	return (float(nn) / 1073741824.0);
}

void main()
{
#ifdef HAIR_SHADER
#  ifdef V3D_SHADING_TEXTURE_COLOR
	vec2 uv = hair_get_customdata_vec2(u);
#  endif
	float time, thick_time, thickness;
	vec3 pos, tan, binor;
	hair_get_pos_tan_binor_time(
	        (ProjectionMatrix[3][3] == 0.0),
	        ModelMatrixInverse,
	        ViewMatrixInverse[3].xyz, ViewMatrixInverse[2].xyz,
	        pos, tan, binor, time, thickness, thick_time);
	/* To "simulate" anisotropic shading, randomize hair normal per strand. */
	hair_rand = integer_noise(hair_get_strand_id());
	tan = normalize(tan);
	vec3 nor = normalize(cross(binor, tan));
	nor = normalize(mix(nor, -tan, hair_rand * 0.10));
	float cos_theta = (hair_rand*2.0 - 1.0) * 0.20;
	float sin_theta = sqrt(max(0.0, 1.0f - cos_theta*cos_theta));
	nor = nor * sin_theta + binor * cos_theta;
	gl_Position = ViewProjectionMatrix * vec4(pos, 1.0);
#else
	gl_Position = ModelViewProjectionMatrix * vec4(pos, 1.0);
#endif
#ifdef V3D_SHADING_TEXTURE_COLOR
	uv_interp = uv;
#endif

#ifdef NORMAL_VIEWPORT_PASS_ENABLED
	normal_viewport = NormalMatrix * nor;
#  ifndef HAIR_SHADER
	normal_viewport = normalize(normal_viewport);
#  endif
#endif

#ifdef USE_WORLD_CLIP_PLANES
	world_clip_planes_calc_clip_distance((ModelMatrix * vec4(pos, 1.0)).xyz);
#endif

}
