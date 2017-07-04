
#ifdef VOLUMETRICS

#ifdef COLOR_TRANSMITTANCE
layout(location = 0) out vec4 outScattering;
layout(location = 1) out vec4 outTransmittance;
#else
out vec4 outScatteringTransmittance;
#endif

uniform sampler2D depthFull;

void participating_media_properties(vec3 wpos, out vec3 extinction, out vec3 scattering, out float anisotropy)
{
	Closure cl = nodetree_exec();

	scattering = cl.scatter;
	anisotropy = cl.anisotropy;
	extinction = max(vec3(1e-8), cl.absorption + cl.scatter); /* mu_t */
}

vec3 participating_media_extinction(vec3 wpos)
{
	Closure cl = nodetree_exec();
	return max(vec3(1e-8), cl.absorption + cl.scatter); /* mu_t */
}

float phase_function_isotropic()
{
	return 1.0 / (4.0 * M_PI);
}

float phase_function(vec3 v, vec3 l, float g)
{
#if 1
	/* Henyey-Greenstein */
	float cos_theta = dot(v, l);
	g = clamp(g, -1.0 + 1e-3, 1.0 - 1e-3);
	float sqr_g = g * g;
	return (1- sqr_g) / (4.0 * M_PI * pow(1 + sqr_g - 2 * g * cos_theta, 3.0 / 2.0));
#else
	return phase_function_isotropic();
#endif
}

vec3 light_volume(LightData ld, vec4 l_vector)
{
	float power;
	float dist = max(1e-4, abs(l_vector.w - ld.l_radius));
	/* TODO : put this out of the shader. */
	/* TODO : Area lighting ? */
	/* Removing Area Power. */
	if (ld.l_type == AREA) {
		power = 0.0962 * (ld.l_sizex * ld.l_sizey * 4.0f * M_PI);
	}
	else {
		power = 0.0248 * (4.0 * ld.l_radius * ld.l_radius * M_PI * M_PI);
	}
	return ld.l_color * power / (l_vector.w * l_vector.w);
}

vec3 light_volume_shadow(LightData ld, vec3 ray_wpos, vec4 l_vector, vec3 s_extinction)
{
#ifdef VOLUME_SHADOW

#ifdef VOLUME_HOMOGENEOUS
	/* Simple extinction */
	return exp(-s_extinction * l_vector.w);
#else
	/* Heterogeneous volume shadows */
	const float numStep = 16.0;
	float dd = l_vector.w / numStep;
	vec3 L = l_vector.xyz * l_vector.w;
	vec3 shadow = vec3(1.0);
	/* start at 0.5 to sample at center of integral part */
	for (float s = 0.5; s < (numStep - 0.1); s += 1.0) {
		vec3 pos = ray_wpos + L * (s / numStep);
		vec3 s_extinction = participating_media_extinction(pos);
		shadow *= exp(-s_extinction * dd);
	}
	return shadow;
#endif /* VOLUME_HOMOGENEOUS */

#else
	return vec3(1.0);
#endif /* VOLUME_SHADOW */
}

float find_next_step(float near, float far, float noise, int iter, int iter_count)
{
	const float lambda = 0.8f; /* TODO : Parameter */

	float progress = (float(iter) + noise) / float(iter_count);

	float linear_split = mix(near, far, progress);

	if (ProjectionMatrix[3][3] == 0.0) {
		float exp_split = near * pow(far / near, progress);
		return mix(linear_split, exp_split, lambda);
	}
	else {
		return linear_split;
	}
}

/* Based on Frosbite Unified Volumetric.
 * https://www.ea.com/frostbite/news/physically-based-unified-volumetric-rendering-in-frostbite */
void main()
{
	vec2 uv = (gl_FragCoord.xy * 2.0) / ivec2(textureSize(depthFull, 0));
	float scene_depth = texelFetch(depthFull, ivec2(gl_FragCoord.xy) * 2, 0).r; /* use the same depth as in the upsample step */
	vec3 vpos = get_view_space_from_depth(uv, scene_depth);
	vec3 wpos = (ViewMatrixInverse * vec4(vpos, 1.0)).xyz;
	vec3 wdir = (ProjectionMatrix[3][3] == 0.0) ? normalize(cameraPos - wpos) : cameraForward;

	/* Note: this is NOT the distance to the camera. */
	float max_z = vpos.z;

	/* project ray to clip plane so we can integrate in even steps in clip space. */
	vec3 wdir_proj = wdir / abs(dot(cameraForward, wdir));
	float wlen = length(wdir_proj);

	/* Transmittance: How much light can get through. */
	vec3 transmittance = vec3(1.0);

	/* Scattering: Light that has been accumulated from scattered light sources. */
	vec3 scattering = vec3(0.0);

	vec3 ray_origin = (ProjectionMatrix[3][3] == 0.0)
		? cameraPos
		: (ViewMatrixInverse * vec4(get_view_space_from_depth(uv, 0.5), 1.0)).xyz;

	/* Start from near clip. TODO make start distance an option. */
	float rand = texture(utilTex, vec3(gl_FragCoord.xy / LUT_SIZE, 2.0)).r;
	/* Less noisy but noticeable patterns, could work better with temporal AA. */
	// float rand = (1.0 / 16.0) * float(((int(gl_FragCoord.x + gl_FragCoord.y) & 0x3) << 2) + (int(gl_FragCoord.x) & 0x3));
	float near = get_view_z_from_depth(0.0);
	float far  = get_view_z_from_depth(1.0);
	float dist = near;
	for (int i = 1; i < 64; ++i) {
		float new_dist = find_next_step(near, far, rand, i, 64);
		float step = dist - new_dist; /* Marching step */
		dist = new_dist;

		vec3 ray_wpos = ray_origin + wdir_proj * dist;

		/* Volume Sample */
		vec3 s_extinction, s_scattering; /* mu_a, mu_t */
		float s_anisotropy;
		participating_media_properties(ray_wpos, s_extinction, s_scattering, s_anisotropy);

		/* Evaluate each light */
		vec3 Lscat = vec3(0.0);

#if 1 /* Lights */
		for (int i = 0; i < MAX_LIGHT && i < light_count; ++i) {
			LightData ld = lights_data[i];

			vec4 l_vector;
			l_vector.xyz = ld.l_position - ray_wpos;
			l_vector.w = length(l_vector.xyz);

			float Vis = light_visibility(ld, ray_wpos, l_vector);

			vec3 Li = light_volume(ld, l_vector) * light_volume_shadow(ld, ray_wpos, l_vector, s_extinction);

			Lscat += Li * Vis * s_scattering * phase_function(-wdir, l_vector.xyz / l_vector.w, s_anisotropy);
		}
#endif

		/* Environment : Average color. */
		IrradianceData ir_data = load_irradiance_cell(0, vec3(1.0));
		Lscat += (ir_data.cubesides[0] + ir_data.cubesides[1] + ir_data.cubesides[2]) * 0.333333 * s_scattering * phase_function_isotropic();

		ir_data = load_irradiance_cell(0, vec3(-1.0));
		Lscat += (ir_data.cubesides[0] + ir_data.cubesides[1] + ir_data.cubesides[2]) * 0.333333 * s_scattering * phase_function_isotropic();

		/* Evaluate Scattering */
		float s_len = wlen * step;
		vec3 Tr = exp(-s_extinction * s_len);

		/* integrate along the current step segment */
		Lscat = (Lscat - Lscat * Tr) / s_extinction;
		/* accumulate and also take into account the transmittance from previous steps */
		scattering += transmittance * Lscat;

		/* Evaluate transmittance to view independantely */
		transmittance *= Tr;

		if (dist < max_z)
			break;
	}

#ifdef COLOR_TRANSMITTANCE
	outScattering = vec4(scattering, 1.0);
	outTransmittance = vec4(transmittance, 1.0);
#else
	float mono_transmittance = dot(transmittance, vec3(1.0)) / 3.0;

	outScatteringTransmittance = vec4(scattering, mono_transmittance);
#endif
}

#else /* STEP_UPSAMPLE */

out vec4 FragColor;

uniform sampler2D depthFull;
uniform sampler2D volumetricBuffer;

uniform mat4 ProjectionMatrix;

vec4 get_view_z_from_depth(vec4 depth)
{
	vec4 d = 2.0 * depth - 1.0;
	return -ProjectionMatrix[3][2] / (d + ProjectionMatrix[2][2]);
}

void main()
{
#if 0 /* 2 x 2 with bilinear */

	const vec4 bilinear_weights[4] = vec4[4](
		vec4(9.0 / 16.0,  3.0 / 16.0, 3.0 / 16.0, 1.0 / 16.0 ),
		vec4(3.0 / 16.0,  9.0 / 16.0, 1.0 / 16.0, 3.0 / 16.0 ),
		vec4(3.0 / 16.0,  1.0 / 16.0, 9.0 / 16.0, 3.0 / 16.0 ),
		vec4(1.0 / 16.0,  3.0 / 16.0, 3.0 / 16.0, 9.0 / 16.0 )
	);

	/* Depth aware upsampling */
	vec4 depths;
	ivec2 texel_co = ivec2(gl_FragCoord.xy * 0.5) * 2;

	/* TODO use textureGather on glsl 4.0 */
	depths.x = texelFetchOffset(depthFull, texel_co, 0, ivec2(0, 0)).r;
	depths.y = texelFetchOffset(depthFull, texel_co, 0, ivec2(2, 0)).r;
	depths.z = texelFetchOffset(depthFull, texel_co, 0, ivec2(0, 2)).r;
	depths.w = texelFetchOffset(depthFull, texel_co, 0, ivec2(2, 2)).r;

	vec4 target_depth = texelFetch(depthFull, ivec2(gl_FragCoord.xy), 0).rrrr;

	depths = get_view_z_from_depth(depths);
	target_depth = get_view_z_from_depth(target_depth);

	vec4 weights = 1.0 - step(0.05, abs(depths - target_depth));

	/* Index in range [0-3] */
	int pix_id = int(dot(mod(ivec2(gl_FragCoord.xy), 2), ivec2(1, 2)));
	weights *= bilinear_weights[pix_id];

	float weight_sum = dot(weights, vec4(1.0));

	if (weight_sum == 0.0) {
		weights.x = 1.0;
		weight_sum = 1.0;
	}

	texel_co = ivec2(gl_FragCoord.xy * 0.5);

	vec4 integration_result;
	integration_result  = texelFetchOffset(volumetricBuffer, texel_co, 0, ivec2(0, 0)) * weights.x;
	integration_result += texelFetchOffset(volumetricBuffer, texel_co, 0, ivec2(1, 0)) * weights.y;
	integration_result += texelFetchOffset(volumetricBuffer, texel_co, 0, ivec2(0, 1)) * weights.z;
	integration_result += texelFetchOffset(volumetricBuffer, texel_co, 0, ivec2(1, 1)) * weights.w;

#else /* 4 x 4 */

	/* Depth aware upsampling */
	vec4 depths[4];
	ivec2 texel_co = ivec2(gl_FragCoord.xy * 0.5) * 2;

	/* TODO use textureGather on glsl 4.0 */
	texel_co += ivec2(-2, -2);
	depths[0].x = texelFetchOffset(depthFull, texel_co, 0, ivec2(0, 0)).r;
	depths[0].y = texelFetchOffset(depthFull, texel_co, 0, ivec2(2, 0)).r;
	depths[0].z = texelFetchOffset(depthFull, texel_co, 0, ivec2(0, 2)).r;
	depths[0].w = texelFetchOffset(depthFull, texel_co, 0, ivec2(2, 2)).r;

	texel_co += ivec2(4, 0);
	depths[1].x = texelFetchOffset(depthFull, texel_co, 0, ivec2(0, 0)).r;
	depths[1].y = texelFetchOffset(depthFull, texel_co, 0, ivec2(2, 0)).r;
	depths[1].z = texelFetchOffset(depthFull, texel_co, 0, ivec2(0, 2)).r;
	depths[1].w = texelFetchOffset(depthFull, texel_co, 0, ivec2(2, 2)).r;

	texel_co += ivec2(-4, 4);
	depths[2].x = texelFetchOffset(depthFull, texel_co, 0, ivec2(0, 0)).r;
	depths[2].y = texelFetchOffset(depthFull, texel_co, 0, ivec2(2, 0)).r;
	depths[2].z = texelFetchOffset(depthFull, texel_co, 0, ivec2(0, 2)).r;
	depths[2].w = texelFetchOffset(depthFull, texel_co, 0, ivec2(2, 2)).r;

	texel_co += ivec2(4, 0);
	depths[3].x = texelFetchOffset(depthFull, texel_co, 0, ivec2(0, 0)).r;
	depths[3].y = texelFetchOffset(depthFull, texel_co, 0, ivec2(2, 0)).r;
	depths[3].z = texelFetchOffset(depthFull, texel_co, 0, ivec2(0, 2)).r;
	depths[3].w = texelFetchOffset(depthFull, texel_co, 0, ivec2(2, 2)).r;

	vec4 target_depth = texelFetch(depthFull, ivec2(gl_FragCoord.xy), 0).rrrr;

	depths[0] = get_view_z_from_depth(depths[0]);
	depths[1] = get_view_z_from_depth(depths[1]);
	depths[2] = get_view_z_from_depth(depths[2]);
	depths[3] = get_view_z_from_depth(depths[3]);

	target_depth = get_view_z_from_depth(target_depth);

	vec4 weights[4];
	weights[0] = 1.0 - step(0.05, abs(depths[0] - target_depth));
	weights[1] = 1.0 - step(0.05, abs(depths[1] - target_depth));
	weights[2] = 1.0 - step(0.05, abs(depths[2] - target_depth));
	weights[3] = 1.0 - step(0.05, abs(depths[3] - target_depth));

	float weight_sum;
	weight_sum  = dot(weights[0], vec4(1.0));
	weight_sum += dot(weights[1], vec4(1.0));
	weight_sum += dot(weights[2], vec4(1.0));
	weight_sum += dot(weights[3], vec4(1.0));

	if (weight_sum == 0.0) {
		weights[0].x = 1.0;
		weight_sum = 1.0;
	}

	texel_co = ivec2(gl_FragCoord.xy * 0.5);

	vec4 integration_result;

	texel_co += ivec2(-1, -1);
	integration_result  = texelFetchOffset(volumetricBuffer, texel_co, 0, ivec2(0, 0)) * weights[0].x;
	integration_result += texelFetchOffset(volumetricBuffer, texel_co, 0, ivec2(1, 0)) * weights[0].y;
	integration_result += texelFetchOffset(volumetricBuffer, texel_co, 0, ivec2(0, 1)) * weights[0].z;
	integration_result += texelFetchOffset(volumetricBuffer, texel_co, 0, ivec2(1, 1)) * weights[0].w;

	texel_co += ivec2(2, 0);
	integration_result += texelFetchOffset(volumetricBuffer, texel_co, 0, ivec2(0, 0)) * weights[1].x;
	integration_result += texelFetchOffset(volumetricBuffer, texel_co, 0, ivec2(1, 0)) * weights[1].y;
	integration_result += texelFetchOffset(volumetricBuffer, texel_co, 0, ivec2(0, 1)) * weights[1].z;
	integration_result += texelFetchOffset(volumetricBuffer, texel_co, 0, ivec2(1, 1)) * weights[1].w;

	texel_co += ivec2(-2, 2);
	integration_result += texelFetchOffset(volumetricBuffer, texel_co, 0, ivec2(0, 0)) * weights[2].x;
	integration_result += texelFetchOffset(volumetricBuffer, texel_co, 0, ivec2(1, 0)) * weights[2].y;
	integration_result += texelFetchOffset(volumetricBuffer, texel_co, 0, ivec2(0, 1)) * weights[2].z;
	integration_result += texelFetchOffset(volumetricBuffer, texel_co, 0, ivec2(1, 1)) * weights[2].w;

	texel_co += ivec2(2, 0);
	integration_result += texelFetchOffset(volumetricBuffer, texel_co, 0, ivec2(0, 0)) * weights[3].x;
	integration_result += texelFetchOffset(volumetricBuffer, texel_co, 0, ivec2(1, 0)) * weights[3].y;
	integration_result += texelFetchOffset(volumetricBuffer, texel_co, 0, ivec2(0, 1)) * weights[3].z;
	integration_result += texelFetchOffset(volumetricBuffer, texel_co, 0, ivec2(1, 1)) * weights[3].w;
#endif

	FragColor = integration_result / weight_sum;
}
#endif
