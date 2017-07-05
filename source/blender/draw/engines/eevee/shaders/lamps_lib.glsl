
uniform sampler2DArray shadowCubes;
uniform sampler2DArrayShadow shadowCascades;

layout(std140) uniform shadow_block {
	ShadowCubeData    shadows_cube_data[MAX_SHADOW_CUBE];
	ShadowMapData     shadows_map_data[MAX_SHADOW_MAP];
	ShadowCascadeData shadows_cascade_data[MAX_SHADOW_CASCADE];
};

layout(std140) uniform probe_block {
	CubeData probes_data[MAX_PROBE];
};

layout(std140) uniform grid_block {
	GridData grids_data[MAX_GRID];
};

layout(std140) uniform planar_block {
	PlanarData planars_data[MAX_PLANAR];
};

layout(std140) uniform light_block {
	LightData lights_data[MAX_LIGHT];
};

/* type */
#define POINT    0.0
#define SUN      1.0
#define SPOT     2.0
#define HEMI     3.0
#define AREA     4.0

float shadow_cubemap(float shid, vec4 l_vector)
{
	ShadowCubeData scd = shadows_cube_data[int(shid)];

	vec3 cubevec = -l_vector.xyz / l_vector.w;
	float dist = l_vector.w - scd.sh_cube_bias;

	float z = texture_octahedron(shadowCubes, vec4(cubevec, shid)).r;

	float esm_test = saturate(exp(scd.sh_cube_exp * (z - dist)));
	// float sh_test = step(0, z - dist);

	return esm_test;
}

float shadow_cascade(float shid, vec3 W)
{
	/* Shadow Cascade */
	shid -= (MAX_SHADOW_CUBE + MAX_SHADOW_MAP);
	ShadowCascadeData smd = shadows_cascade_data[int(shid)];

	/* Finding Cascade index */
	vec4 z = vec4(-dot(cameraPos - W, cameraForward));
	vec4 comp = step(z, smd.split_distances);
	float cascade = dot(comp, comp);
	mat4 shadowmat;
	float bias;

	/* Manual Unrolling of a loop for better performance.
	 * Doing fetch directly with cascade index leads to
	 * major performance impact. (0.27ms -> 10.0ms for 1 light) */
	if (cascade == 0.0) {
		shadowmat = smd.shadowmat[0];
		bias = smd.bias[0];
	}
	else if (cascade == 1.0) {
		shadowmat = smd.shadowmat[1];
		bias = smd.bias[1];
	}
	else if (cascade == 2.0) {
		shadowmat = smd.shadowmat[2];
		bias = smd.bias[2];
	}
	else {
		shadowmat = smd.shadowmat[3];
		bias = smd.bias[3];
	}

	vec4 shpos = shadowmat * vec4(W, 1.0);
	shpos.z -= bias * shpos.w;
	shpos.xyz /= shpos.w;

	return texture(shadowCascades, vec4(shpos.xy, shid * float(MAX_CASCADE_NUM) + cascade, shpos.z));
}

float light_visibility(LightData ld, vec3 W, vec4 l_vector)
{
	float vis = 1.0;

	if (ld.l_type == SPOT) {
		float z = dot(ld.l_forward, l_vector.xyz);
		vec3 lL = l_vector.xyz / z;
		float x = dot(ld.l_right, lL) / ld.l_sizex;
		float y = dot(ld.l_up, lL) / ld.l_sizey;

		float ellipse = 1.0 / sqrt(1.0 + x * x + y * y);

		float spotmask = smoothstep(0.0, 1.0, (ellipse - ld.l_spot_size) / ld.l_spot_blend);

		vis *= spotmask;
		vis *= step(0.0, -dot(l_vector.xyz, ld.l_forward));
	}
	else if (ld.l_type == AREA) {
		vis *= step(0.0, -dot(l_vector.xyz, ld.l_forward));
	}

#if !defined(VOLUMETRICS) || defined(VOLUME_SHADOW)
	/* shadowing */
	if (ld.l_shadowid >= (MAX_SHADOW_MAP + MAX_SHADOW_CUBE)) {
		vis *= shadow_cascade(ld.l_shadowid, W);
	}
	else if (ld.l_shadowid >= 0.0) {
		vis *= shadow_cubemap(ld.l_shadowid, l_vector);
	}
#endif

	return vis;
}

float light_diffuse(LightData ld, vec3 N, vec3 V, vec4 l_vector)
{
#ifdef USE_LTC
	if (ld.l_type == SUN) {
		/* TODO disk area light */
		return direct_diffuse_sun(ld, N);
	}
	else if (ld.l_type == AREA) {
		return direct_diffuse_rectangle(ld, N, V, l_vector);
	}
	else {
		return direct_diffuse_sphere(ld, N, l_vector);
	}
#else
	if (ld.l_type == SUN) {
		return direct_diffuse_sun(ld, N, V);
	}
	else {
		return direct_diffuse_point(N, l_vector);
	}
#endif
}

vec3 light_specular(LightData ld, vec3 N, vec3 V, vec4 l_vector, float roughness, vec3 f0)
{
#ifdef USE_LTC
	if (ld.l_type == SUN) {
		/* TODO disk area light */
		return direct_ggx_sun(ld, N, V, roughness, f0);
	}
	else if (ld.l_type == AREA) {
		return direct_ggx_rectangle(ld, N, V, l_vector, roughness, f0);
	}
	else {
		return direct_ggx_sphere(ld, N, V, l_vector, roughness, f0);
	}
#else
	if (ld.l_type == SUN) {
		return direct_ggx_sun(ld, N, V, roughness, f0);
	}
	else {
		return direct_ggx_point(N, V, l_vector, roughness, f0);
	}
#endif
}

#ifdef HAIR_SHADER
void light_hair_common(
        LightData ld, vec3 N, vec3 V, vec4 l_vector, vec3 norm_view,
        out float occlu_trans, out float occlu,
        out vec3 norm_lamp, out vec3 view_vec)
{
	const float transmission = 0.3; /* Uniform internal scattering factor */

	vec3 lamp_vec;

	if (ld.l_type == SUN || ld.l_type == AREA) {
		lamp_vec = ld.l_forward;
	}
	else {
		lamp_vec = -l_vector.xyz;
	}

	norm_lamp = cross(lamp_vec, N);
	norm_lamp = normalize(cross(N, norm_lamp)); /* Normal facing lamp */

	/* Rotate view vector onto the cross(tangent, light) plane */
	view_vec = normalize(norm_lamp * dot(norm_view, V) + N * dot(N, V));

	occlu = (dot(norm_view, norm_lamp) * 0.5 + 0.5);
	occlu_trans = transmission + (occlu * (1.0 - transmission)); /* Includes transmission component */
}
#endif
