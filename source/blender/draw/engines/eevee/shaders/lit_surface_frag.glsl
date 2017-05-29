
uniform int light_count;
uniform vec3 cameraPos;
uniform vec3 eye;
uniform mat4 ProjectionMatrix;

uniform sampler2D probeFiltered;
uniform float lodMax;
uniform vec3 shCoefs[9];

#ifndef USE_LTC
uniform sampler2D brdfLut;
#endif
uniform sampler2DArray shadowCubes;
uniform sampler2DArrayShadow shadowCascades;

layout(std140) uniform light_block {
	LightData lights_data[MAX_LIGHT];
};

layout(std140) uniform shadow_block {
	ShadowCubeData    shadows_cube_data[MAX_SHADOW_CUBE];
	ShadowMapData     shadows_map_data[MAX_SHADOW_MAP];
	ShadowCascadeData shadows_cascade_data[MAX_SHADOW_CASCADE];
};

in vec3 worldPosition;
in vec3 viewPosition;

#ifdef USE_FLAT_NORMAL
flat in vec3 worldNormal;
flat in vec3 viewNormal;
#else
in vec3 worldNormal;
in vec3 viewNormal;
#endif

/* type */
#define POINT    0.0
#define SUN      1.0
#define SPOT     2.0
#define HEMI     3.0
#define AREA     4.0

vec2 mapping_octahedron(vec3 cubevec, vec2 texel_size)
{
	/* projection onto octahedron */
	cubevec /= dot( vec3(1), abs(cubevec) );

	/* out-folding of the downward faces */
	if ( cubevec.z < 0.0 ) {
		cubevec.xy = (1.0 - abs(cubevec.yx)) * sign(cubevec.xy);
	}

	/* mapping to [0;1]ˆ2 texture space */
	vec2 uvs = cubevec.xy * (0.5) + 0.5;

	/* edge filtering fix */
	uvs *= 1.0 - 2.0 * texel_size;
	uvs += texel_size;

	return uvs;
}

vec4 textureLod_octahedron(sampler2D tex, vec3 cubevec, float lod)
{
	vec2 texelSize = 1.0 / vec2(textureSize(tex, int(lodMax)));

	vec2 uvs = mapping_octahedron(cubevec, texelSize);

	return textureLod(tex, uvs, lod);
}

vec4 texture_octahedron(sampler2DArray tex, vec4 cubevec)
{
	vec2 texelSize = 1.0 / vec2(textureSize(tex, 0));

	vec2 uvs = mapping_octahedron(cubevec.xyz, texelSize);

	return texture(tex, vec3(uvs, cubevec.w));
}

void light_shade(
        LightData ld, ShadingData sd, vec3 albedo, float roughness, vec3 f0,
        out vec3 diffuse, out vec3 specular)
{
#ifdef USE_LTC
	if (ld.l_type == SUN) {
		diffuse = direct_diffuse_sun(ld, sd) * albedo;
		/* TODO disk area light */
		specular = direct_ggx_point(sd, roughness, f0);
	}
	else if (ld.l_type == AREA) {
		diffuse =  direct_diffuse_rectangle(ld, sd) * albedo;
		specular =  direct_ggx_rectangle(ld, sd, roughness, f0);
	}
	else {
		diffuse =  direct_diffuse_sphere(ld, sd) * albedo;
		specular =  direct_ggx_sphere(ld, sd, roughness, f0);
	}
#else
	if (ld.l_type == SUN) {
		diffuse = direct_diffuse_sun(ld, sd) * albedo;
	}
	else {
		diffuse = direct_diffuse_point(ld, sd) * albedo;
	}
	specular = direct_ggx_point(sd, roughness, f0);
#endif
}

void light_visibility(LightData ld, ShadingData sd, out float vis)
{
	vis = 1.0;

	if (ld.l_type == SPOT) {
		float z = dot(ld.l_forward, sd.l_vector);
		vec3 lL = sd.l_vector / z;
		float x = dot(ld.l_right, lL) / ld.l_sizex;
		float y = dot(ld.l_up, lL) / ld.l_sizey;

		float ellipse = 1.0 / sqrt(1.0 + x * x + y * y);

		float spotmask = smoothstep(0.0, 1.0, (ellipse - ld.l_spot_size) / ld.l_spot_blend);

		vis *= spotmask;
		vis *= step(0.0, -dot(sd.l_vector, ld.l_forward));
	}
	else if (ld.l_type == AREA) {
		vis *= step(0.0, -dot(sd.l_vector, ld.l_forward));
	}

	/* shadowing */
	if (ld.l_shadowid >= (MAX_SHADOW_MAP + MAX_SHADOW_CUBE)) {
		/* Shadow Cascade */
		float shid = ld.l_shadowid - (MAX_SHADOW_CUBE + MAX_SHADOW_MAP);
		ShadowCascadeData smd = shadows_cascade_data[int(shid)];

		/* Finding Cascade index */
		vec4 z = vec4(-dot(cameraPos - worldPosition, normalize(eye)));
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

		vec4 shpos = shadowmat * vec4(sd.W, 1.0);
		shpos.z -= bias * shpos.w;
		shpos.xyz /= shpos.w;

		vis *= texture(shadowCascades, vec4(shpos.xy, shid * float(MAX_CASCADE_NUM) + cascade, shpos.z));
	}
	else if (ld.l_shadowid >= 0.0) {
		/* Shadow Cube */
		float shid = ld.l_shadowid;
		ShadowCubeData scd = shadows_cube_data[int(shid)];

		vec3 cubevec = sd.W - ld.l_position;
		float dist = length(cubevec);

		float z = texture_octahedron(shadowCubes, vec4(cubevec, shid)).r;

		float esm_test = min(1.0, exp(-5.0 * dist) * z);
		float sh_test = step(0, z - dist);

		vis *= esm_test;
	}
}

vec3 eevee_surface_lit(vec3 world_normal, vec3 albedo, vec3 f0, float roughness, float ao)
{
	float roughnessSquared = roughness * roughness;

	ShadingData sd;
	sd.N = normalize(world_normal);
	sd.V = (ProjectionMatrix[3][3] == 0.0) /* if perspective */
	            ? normalize(cameraPos - worldPosition)
	            : normalize(eye);
	sd.W = worldPosition;

	vec3 radiance = vec3(0.0);
	vec3 indirect_radiance = vec3(0.0);

	/* Analitic Lights */
	for (int i = 0; i < MAX_LIGHT && i < light_count; ++i) {
		LightData ld = lights_data[i];
		vec3 diff, spec;
		float vis;

		sd.l_vector = ld.l_position - worldPosition;

		light_visibility(ld, sd, vis);
		light_shade(ld, sd, albedo, roughnessSquared, f0, diff, spec);

		radiance += vis * (diff + spec) * ld.l_color;
	}

	vec3 spec_dir = get_specular_dominant_dir(sd.N, reflect(-sd.V, sd.N), roughnessSquared);

	/* Envmaps */
	vec2 uv = lut_coords(dot(sd.N, sd.V), roughness);
	vec3 brdf_lut = texture(brdfLut, uv).rgb;
	vec3 Li = textureLod_octahedron(probeFiltered, spec_dir, roughness * lodMax).rgb;
	indirect_radiance += Li * F_ibl(f0, brdf_lut.rg);
	indirect_radiance += spherical_harmonics(sd.N, shCoefs) * albedo;

	return radiance + indirect_radiance * ao;
}