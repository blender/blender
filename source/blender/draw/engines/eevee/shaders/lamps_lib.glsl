
uniform sampler2DArray shadowTexture;

#define LAMPS_LIB

layout(std140) uniform shadow_block {
	ShadowData        shadows_data[MAX_SHADOW];
	ShadowCubeData    shadows_cube_data[MAX_SHADOW_CUBE];
	ShadowCascadeData shadows_cascade_data[MAX_SHADOW_CASCADE];
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

/* ----------------------------------------------------------- */
/* ----------------------- Shadow tests ---------------------- */
/* ----------------------------------------------------------- */

float shadow_test_esm(float z, float dist, float exponent)
{
	return saturate(exp(exponent * (z - dist)));
}

float shadow_test_pcf(float z, float dist)
{
	return step(0, z - dist);
}

float shadow_test_vsm(vec2 moments, float dist, float bias, float bleed_bias)
{
	float p = 0.0;

	if (dist <= moments.x)
		p = 1.0;

	float variance = moments.y - (moments.x * moments.x);
	variance = max(variance, bias / 10.0);

	float d = moments.x - dist;
	float p_max = variance / (variance + d * d);

	/* Now reduce light-bleeding by removing the [0, x] tail and linearly rescaling (x, 1] */
	p_max = clamp((p_max - bleed_bias) / (1.0 - bleed_bias), 0.0, 1.0);

	return max(p, p_max);
}


/* ----------------------------------------------------------- */
/* ----------------------- Shadow types ---------------------- */
/* ----------------------------------------------------------- */

float shadow_cubemap(ShadowData sd, ShadowCubeData scd, float texid, vec3 W)
{
	vec3 cubevec = W - scd.position.xyz;
	float dist = length(cubevec);

	/* If fragment is out of shadowmap range, do not occlude */
	/* XXX : we check radial distance against a cubeface distance.
	 * We loose quite a bit of valid area. */
	if (dist > sd.sh_far)
		return 1.0;

	cubevec /= dist;

#if defined(SHADOW_VSM)
	vec2 moments = texture_octahedron(shadowTexture, vec4(cubevec, texid)).rg;
#else
	float z = texture_octahedron(shadowTexture, vec4(cubevec, texid)).r;
#endif

#if defined(SHADOW_VSM)
	return shadow_test_vsm(moments, dist, sd.sh_bias, sd.sh_bleed);
#elif defined(SHADOW_ESM)
	return shadow_test_esm(z, dist - sd.sh_bias, sd.sh_exp);
#else
	return shadow_test_pcf(z, dist - sd.sh_bias);
#endif
}

float evaluate_cascade(ShadowData sd, mat4 shadowmat, vec3 W, float range, float texid)
{
	vec4 shpos = shadowmat * vec4(W, 1.0);
	float dist = shpos.z * range;

#if defined(SHADOW_VSM)
	vec2 moments = texture(shadowTexture, vec3(shpos.xy, texid)).rg;
#else
	float z = texture(shadowTexture, vec3(shpos.xy, texid)).r;
#endif

	float vis;
#if defined(SHADOW_VSM)
	vis = shadow_test_vsm(moments, dist, sd.sh_bias, sd.sh_bleed);
#elif defined(SHADOW_ESM)
	vis = shadow_test_esm(z, dist - sd.sh_bias, sd.sh_exp);
#else
	vis = shadow_test_pcf(z, dist - sd.sh_bias);
#endif

	/* If fragment is out of shadowmap range, do not occlude */
	if (shpos.z < 1.0 && shpos.z > 0.0) {
		return vis;
	}
	else {
		return 1.0;
	}
}

float shadow_cascade(ShadowData sd, ShadowCascadeData scd, float texid, vec3 W)
{
	vec4 view_z = vec4(dot(W - cameraPos, cameraForward));
	vec4 weights = smoothstep(scd.split_end_distances, scd.split_start_distances.yzwx, view_z);
	weights.yzw -= weights.xyz;

	vec4 vis = vec4(1.0);
	float range = abs(sd.sh_far - sd.sh_near); /* Same factor as in get_cascade_world_distance(). */

	/* Branching using (weights > 0.0) is reaally slooow on intel so avoid it for now. */
	vis.x = evaluate_cascade(sd, scd.shadowmat[0], W, range, texid + 0);
	vis.y = evaluate_cascade(sd, scd.shadowmat[1], W, range, texid + 1);
	vis.z = evaluate_cascade(sd, scd.shadowmat[2], W, range, texid + 2);
	vis.w = evaluate_cascade(sd, scd.shadowmat[3], W, range, texid + 3);

	float weight_sum = dot(vec4(1.0), weights);
	if (weight_sum > 0.9999) {
		float vis_sum = dot(vec4(1.0), vis * weights);
		return vis_sum / weight_sum;
	}
	else {
		float vis_sum = dot(vec4(1.0), vis * step(0.001, weights));
		return mix(1.0, vis_sum, weight_sum);
	}
}

/* ----------------------------------------------------------- */
/* --------------------- Light Functions --------------------- */
/* ----------------------------------------------------------- */
#define MAX_MULTI_SHADOW 4

float light_visibility(LightData ld, vec3 W,
#ifndef VOLUMETRICS
                       vec3 viewPosition,
                       vec3 viewNormal,
#endif
                       vec4 l_vector)
{
	float vis = 1.0;

	if (ld.l_type == SPOT) {
		float z = dot(ld.l_forward, l_vector.xyz);
		vec3 lL = l_vector.xyz / z;
		float x = dot(ld.l_right, lL) / ld.l_sizex;
		float y = dot(ld.l_up, lL) / ld.l_sizey;

		float ellipse = inversesqrt(1.0 + x * x + y * y);

		float spotmask = smoothstep(0.0, 1.0, (ellipse - ld.l_spot_size) / ld.l_spot_blend);

		vis *= spotmask;
		vis *= step(0.0, -dot(l_vector.xyz, ld.l_forward));
	}
	else if (ld.l_type == AREA) {
		vis *= step(0.0, -dot(l_vector.xyz, ld.l_forward));
	}

#if !defined(VOLUMETRICS) || defined(VOLUME_SHADOW)
	/* shadowing */
	if (ld.l_shadowid >= 0.0) {
		ShadowData data = shadows_data[int(ld.l_shadowid)];

		if (ld.l_type == SUN) {
			/* TODO : MSM */
			// for (int i = 0; i < MAX_MULTI_SHADOW; ++i) {
				vis *= shadow_cascade(
					data, shadows_cascade_data[int(data.sh_data_start)],
					data.sh_tex_start, W);
			// }
		}
		else {
			/* TODO : MSM */
			// for (int i = 0; i < MAX_MULTI_SHADOW; ++i) {
				vis *= shadow_cubemap(
					data, shadows_cube_data[int(data.sh_data_start)],
					data.sh_tex_start, W);
			// }
		}

#ifndef VOLUMETRICS
		/* Only compute if not already in shadow. */
		if ((vis > 0.001) && (data.sh_contact_dist > 0.0)) {
			vec4 L = (ld.l_type != SUN) ? l_vector : vec4(-ld.l_forward, 1.0);
			float trace_distance = (ld.l_type != SUN) ? min(data.sh_contact_dist, l_vector.w) : data.sh_contact_dist;

			vec3 T, B;
			make_orthonormal_basis(L.xyz / L.w, T, B);

			vec4 rand = texelfetch_noise_tex(gl_FragCoord.xy);
			rand.zw *= fast_sqrt(rand.y) * data.sh_contact_spread;

			/* We use the full l_vector.xyz so that the spread is minimize
			 * if the shading point is further away from the light source */
			vec3 ray_dir = L.xyz + T * rand.z + B * rand.w;
			ray_dir = transform_direction(ViewMatrix, ray_dir);
			ray_dir = normalize(ray_dir);

			vec3 ray_ori = viewPosition;

			float bias = 0.5; /* Constant Bias */
			bias += 1.0 - abs(dot(viewNormal, ray_dir)); /* Angle dependant bias */
			bias *= gl_FrontFacing ? data.sh_contact_offset : -data.sh_contact_offset;

			vec3 nor_bias = viewNormal * bias;
			ray_ori += nor_bias;

			ray_dir *= trace_distance;
			ray_dir -= nor_bias;

			vec3 hit_pos = raycast(-1, ray_ori, ray_dir, data.sh_contact_thickness, rand.x,
			                       0.1, 0.001, false);

			if (hit_pos.z > 0.0) {
				hit_pos = get_view_space_from_depth(hit_pos.xy, hit_pos.z);
				float hit_dist = distance(viewPosition, hit_pos);
				float dist_ratio = hit_dist / trace_distance;
				return vis * saturate(dist_ratio * dist_ratio * dist_ratio);
			}
		}
#endif
	}
#endif

	return vis;
}

float light_diffuse(LightData ld, vec3 N, vec3 V, vec4 l_vector)
{
#ifdef USE_LTC
	if (ld.l_type == SUN) {
		return direct_diffuse_unit_disc(ld, N, V);
	}
	else if (ld.l_type == AREA) {
		return direct_diffuse_rectangle(ld, N, V, l_vector);
	}
	else {
		return direct_diffuse_sphere(ld, N, l_vector);
	}
#else
	if (ld.l_type == SUN) {
		return direct_diffuse_sun(ld, N);
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
		return direct_ggx_unit_disc(ld, N, V, roughness, f0);
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

#define MAX_SSS_SAMPLES 65
#define SSS_LUT_SIZE 64.0
#define SSS_LUT_SCALE ((SSS_LUT_SIZE - 1.0) / float(SSS_LUT_SIZE))
#define SSS_LUT_BIAS (0.5 / float(SSS_LUT_SIZE))
layout(std140) uniform sssProfile {
	vec4 kernel[MAX_SSS_SAMPLES];
	vec4 radii_max_radius;
	int sss_samples;
};

uniform sampler1D sssTexProfile;

vec3 sss_profile(float s) {
	s /= radii_max_radius.w;
	return texture(sssTexProfile, saturate(s) * SSS_LUT_SCALE + SSS_LUT_BIAS).rgb;
}

vec3 light_translucent(LightData ld, vec3 W, vec3 N, vec4 l_vector, float scale)
{
#if !defined(USE_TRANSLUCENCY) || defined(VOLUMETRICS)
	return vec3(0.0);
#else
	vec3 vis = vec3(1.0);

	/* Only shadowed light can produce translucency */
	if (ld.l_shadowid >= 0.0) {
		ShadowData data = shadows_data[int(ld.l_shadowid)];
		float delta;

		vec4 L = (ld.l_type != SUN) ? l_vector : vec4(-ld.l_forward, 1.0);

		vec3 T, B;
		make_orthonormal_basis(L.xyz / L.w, T, B);

		vec4 rand = texelfetch_noise_tex(gl_FragCoord.xy);
		rand.zw *= fast_sqrt(rand.y) * data.sh_blur;

		/* We use the full l_vector.xyz so that the spread is minimize
		 * if the shading point is further away from the light source */
		W = W + T * rand.z + B * rand.w;

		if (ld.l_type == SUN) {
			ShadowCascadeData scd = shadows_cascade_data[int(data.sh_data_start)];
			vec4 view_z = vec4(dot(W - cameraPos, cameraForward));

			vec4 weights = step(scd.split_end_distances, view_z);
			float id = abs(4.0 - dot(weights, weights));

			if (id > 3.0) {
				return vec3(0.0);
			}

			float range = abs(data.sh_far - data.sh_near); /* Same factor as in get_cascade_world_distance(). */

			vec4 shpos = scd.shadowmat[int(id)] * vec4(W, 1.0);
			float dist = shpos.z * range;

			if (shpos.z > 1.0 || shpos.z < 0.0) {
				return vec3(0.0);
			}

#if defined(SHADOW_VSM)
			vec2 moments = texture(shadowTexture, vec3(shpos.xy, data.sh_tex_start + id)).rg;
			delta = dist - moments.x;
#else
			float z = texture(shadowTexture, vec3(shpos.xy, data.sh_tex_start + id)).r;
			delta = dist - z;
#endif
		}
		else {
			vec3 cubevec = W - shadows_cube_data[int(data.sh_data_start)].position.xyz;
			float dist = length(cubevec);

			/* If fragment is out of shadowmap range, do not occlude */
			/* XXX : we check radial distance against a cubeface distance.
			 * We loose quite a bit of valid area. */
			if (dist < data.sh_far) {
				cubevec /= dist;

#if defined(SHADOW_VSM)
				vec2 moments = texture_octahedron(shadowTexture, vec4(cubevec, data.sh_tex_start)).rg;
				delta = dist - moments.x;
#else
				float z = texture_octahedron(shadowTexture, vec4(cubevec, data.sh_tex_start)).r;
				delta = dist - z;
#endif
			}
		}

		/* XXX : Removing Area Power. */
		/* TODO : put this out of the shader. */
		float falloff;
		if (ld.l_type == AREA) {
			vis *= (ld.l_sizex * ld.l_sizey * 4.0 * M_PI) * (1.0 / 80.0);
			vis *= 0.3 * 20.0 * max(0.0, dot(-ld.l_forward, l_vector.xyz / l_vector.w)); /* XXX ad hoc, empirical */
			vis /= (l_vector.w * l_vector.w);
			falloff = dot(N, l_vector.xyz / l_vector.w);
		}
		else if (ld.l_type == SUN) {
			vis *= (4.0f * ld.l_radius * ld.l_radius * M_2PI) * (1.0 / 12.5); /* Removing area light power*/
			vis *= M_2PI * 0.78; /* Matching cycles with point light. */
			vis *= 0.082; /* XXX ad hoc, empirical */
			falloff = dot(N, -ld.l_forward);
		}
		else {
			vis *= (4.0 * ld.l_radius * ld.l_radius) * (1.0 /10.0);
			vis *= 1.5; /* XXX ad hoc, empirical */
			vis /= (l_vector.w * l_vector.w);
			falloff = dot(N, l_vector.xyz / l_vector.w);
		}
		// vis *= M_1_PI; /* Normalize */

		/* Applying profile */
		vis *= sss_profile(abs(delta) / scale);

		/* No transmittance at grazing angle (hide artifacts) */
		vis *= saturate(falloff * 2.0);

		if (ld.l_type == SPOT) {
			float z = dot(ld.l_forward, l_vector.xyz);
			vec3 lL = l_vector.xyz / z;
			float x = dot(ld.l_right, lL) / ld.l_sizex;
			float y = dot(ld.l_up, lL) / ld.l_sizey;

			float ellipse = inversesqrt(1.0 + x * x + y * y);

			float spotmask = smoothstep(0.0, 1.0, (ellipse - ld.l_spot_size) / ld.l_spot_blend);

			vis *= spotmask;
			vis *= step(0.0, -dot(l_vector.xyz, ld.l_forward));
		}
		else if (ld.l_type == AREA) {
			vis *= step(0.0, -dot(l_vector.xyz, ld.l_forward));
		}
	}
	else {
		vis = vec3(0.0);
	}

	return vis;
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
