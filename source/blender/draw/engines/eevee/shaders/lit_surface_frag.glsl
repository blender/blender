
uniform int light_count;
uniform vec3 cameraPos;
uniform vec3 eye;
uniform mat4 ProjectionMatrix;

uniform samplerCube probeFiltered;
uniform float lodMax;
uniform vec3 shCoefs[9];

#ifndef USE_LTC
uniform sampler2D brdfLut;
#endif
uniform sampler2DArrayShadow shadowCubes;
uniform sampler2DArrayShadow shadowMaps;
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
in vec3 worldNormal;
in vec3 viewNormal;

/* type */
#define POINT    0.0
#define SUN      1.0
#define SPOT     2.0
#define HEMI     3.0
#define AREA     4.0

vec3 light_diffuse(LightData ld, ShadingData sd, vec3 albedo)
{
	if (ld.l_type == SUN) {
		return direct_diffuse_sun(ld, sd) * albedo;
	}
	else if (ld.l_type == AREA) {
		return direct_diffuse_rectangle(ld, sd) * albedo;
	}
	else {
		return direct_diffuse_sphere(ld, sd) * albedo;
	}
}

vec3 light_specular(LightData ld, ShadingData sd, float roughness, vec3 f0)
{
	if (ld.l_type == SUN) {
		return direct_ggx_point(sd, roughness, f0);
	}
	else if (ld.l_type == AREA) {
		return direct_ggx_rectangle(ld, sd, roughness, f0);
	}
	else {
		// return direct_ggx_point(sd, roughness, f0);
		return direct_ggx_sphere(ld, sd, roughness, f0);
	}
}

float light_visibility(LightData ld, ShadingData sd)
{
	float vis = 1.0;

	if (ld.l_type == SPOT) {
		float z = dot(ld.l_forward, sd.l_vector);
		vec3 lL = sd.l_vector / z;
		float x = dot(ld.l_right, lL) / ld.l_sizex;
		float y = dot(ld.l_up, lL) / ld.l_sizey;

		float ellipse = 1.0 / sqrt(1.0 + x * x + y * y);

		float spotmask = smoothstep(0.0, 1.0, (ellipse - ld.l_spot_size) / ld.l_spot_blend);

		vis *= spotmask;
		vis *= step(0.0, -dot(sd.L, ld.l_forward));
	}
	else if (ld.l_type == AREA) {
		vis *= step(0.0, -dot(sd.L, ld.l_forward));
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
	else if (ld.l_shadowid >= MAX_SHADOW_CUBE) {
		/* Shadow Map */
		float shid = ld.l_shadowid - MAX_SHADOW_CUBE;
		ShadowMapData smd = shadows_map_data[int(shid)];
		vec4 shpos = smd.shadowmat * vec4(sd.W, 1.0);
		shpos.z -= smd.sh_map_bias * shpos.w;
		shpos.xyz /= shpos.w;

		if (shpos.w > 0.0 && min(shpos.x, shpos.y) > 0.0 && max(shpos.x, shpos.y) < 1.0) {
			vis *= texture(shadowMaps, vec4(shpos.xy, shid, shpos.z));
		}
	}
	else {
		/* Shadow Cube */
		float shid = ld.l_shadowid;
		ShadowCubeData scd = shadows_cube_data[int(shid)];

		float face;
		vec2 uvs;
		vec3 Linv = sd.L;
		vec3 Labs = abs(Linv);
		vec3 maj_axis;

		if (max(Labs.y, Labs.z) < Labs.x) {
			if (Linv.x > 0.0) {
				face = 1.0;
				uvs = vec2(1.0, -1.0) * Linv.zy / -Linv.x;
				maj_axis = vec3(1.0, 0.0, 0.0);
			}
			else {
				face = 0.0;
				uvs = -Linv.zy / Linv.x;
				maj_axis = vec3(-1.0, 0.0, 0.0);
			}
		}
		else if (max(Labs.x, Labs.z) < Labs.y) {
			if (Linv.y > 0.0) {
				face = 2.0;
				uvs = vec2(-1.0, 1.0) * Linv.xz / Linv.y;
				maj_axis = vec3(0.0, 1.0, 0.0);
			}
			else {
				face = 3.0;
				uvs = -Linv.xz / -Linv.y;
				maj_axis = vec3(0.0, -1.0, 0.0);
			}
		}
		else {
			if (Linv.z > 0.0) {
				face = 5.0;
				uvs = Linv.xy / Linv.z;
				maj_axis = vec3(0.0, 0.0, 1.0);
			}
			else {
				face = 4.0;
				uvs = vec2(-1.0, 1.0) * Linv.xy / -Linv.z;
				maj_axis = vec3(0.0, 0.0, -1.0);
			}
		}

		uvs = uvs * 0.5 + 0.5;

		/* Depth in lightspace to compare against shadow map */
		float w = dot(maj_axis, sd.l_vector);
		w -= scd.sh_map_bias * w;
		bool is_persp = (ProjectionMatrix[3][3] == 0.0);
		float shdepth = buffer_depth(is_persp, w, scd.sh_cube_far, scd.sh_cube_near);

		vis *= texture(shadowCubes, vec4(uvs, shid * 6.0 + face, shdepth));
	}

	return vis;
}

vec3 light_fresnel(LightData ld, ShadingData sd, vec3 f0)
{
	vec3 H = normalize(sd.L + sd.V);
	float NH = max(dot(sd.N, H), 1e-8);

	return F_schlick(f0, NH);
}

/* Calculation common to all bsdfs */
float light_common(inout LightData ld, inout ShadingData sd)
{
	float vis = 1.0;

	if (ld.l_type == SUN) {
		sd.L = -ld.l_forward;
	}
	else {
		sd.L = sd.l_vector / sd.l_distance;
	}

	if (ld.l_type == AREA) {
		sd.area_data.corner[0] = sd.l_vector + ld.l_right * -ld.l_sizex + ld.l_up *  ld.l_sizey;
		sd.area_data.corner[1] = sd.l_vector + ld.l_right * -ld.l_sizex + ld.l_up * -ld.l_sizey;
		sd.area_data.corner[2] = sd.l_vector + ld.l_right *  ld.l_sizex + ld.l_up * -ld.l_sizey;
		sd.area_data.corner[3] = sd.l_vector + ld.l_right *  ld.l_sizex + ld.l_up *  ld.l_sizey;
#ifndef USE_LTC
		sd.area_data.solid_angle = rectangle_solid_angle(sd.area_data);
#endif
	}

	return vis;
}

vec3 eevee_surface_lit(vec3 world_normal, vec3 albedo, vec3 f0, float roughness)
{
	ShadingData sd;
	sd.N = normalize(world_normal);
	sd.V = (ProjectionMatrix[3][3] == 0.0) /* if perspective */
	            ? normalize(cameraPos - worldPosition)
	            : normalize(eye);
	sd.W = worldPosition;
	sd.R = reflect(-sd.V, sd.N);
	sd.spec_dominant_dir = get_specular_dominant_dir(sd.N, sd.R, roughness);

	vec3 radiance = vec3(0.0);

	/* Analitic Lights */
	for (int i = 0; i < MAX_LIGHT && i < light_count; ++i) {
		LightData ld = lights_data[i];

		sd.l_vector = ld.l_position - worldPosition;
		sd.l_distance = length(sd.l_vector);

		light_common(ld, sd);

		float vis = light_visibility(ld, sd);
		vec3 spec = light_specular(ld, sd, roughness, f0);
		vec3 diff = light_diffuse(ld, sd, albedo);

		radiance += vis * (diff + spec) * ld.l_color;
	}

	/* Envmaps */
	vec2 uv = ltc_coords(dot(sd.N, sd.V), sqrt(roughness));
	vec2 brdf_lut = texture(brdfLut, uv).rg;
	vec3 Li = textureLod(probeFiltered, sd.spec_dominant_dir, roughness * lodMax).rgb;
	radiance += Li * brdf_lut.y + f0 * Li * brdf_lut.x;

	radiance += spherical_harmonics(sd.N, shCoefs) * albedo;

	return radiance;
}