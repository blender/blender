
/* Based on Stochastic Screen Space Reflections
 * https://www.ea.com/frostbite/news/stochastic-screen-space-reflections */

#ifndef UTIL_TEX
#define UTIL_TEX
uniform sampler2DArray utilTex;
#endif /* UTIL_TEX */

#define BRDF_BIAS 0.7
#define MAX_MIP 9.0

uniform float fireflyFactor;
uniform float maxRoughness;

#ifdef STEP_RAYTRACE

uniform sampler2D normalBuffer;
uniform sampler2D specroughBuffer;

uniform int planar_count;
uniform float noiseOffset;

layout(location = 0) out vec4 hitData0;
layout(location = 1) out vec4 hitData1;
layout(location = 2) out vec4 hitData2;
layout(location = 3) out vec4 hitData3;

vec4 do_planar_ssr(int index, vec3 V, vec3 N, vec3 T, vec3 B, vec3 planeNormal, vec3 viewPosition, float a2, vec3 rand, float ofs)
{
	float pdf, NH;
	float jitter = fract(rand.x + ofs);

	/* Importance sampling bias */
	rand.x = mix(rand.x, 0.0, BRDF_BIAS);

	vec3 H = sample_ggx(rand, a2, N, T, B, NH); /* Microfacet normal */
	pdf = pdf_ggx_reflect(NH, a2);

	vec3 R = reflect(-V, H);
	R = reflect(R, planeNormal);

	/* If ray is bad (i.e. going below the plane) regenerate. */
	if (dot(R, planeNormal) > 0.0) {
		vec3 H = sample_ggx(rand * vec3(1.0, -1.0, -1.0), a2, N, T, B, NH); /* Microfacet normal */
		pdf = pdf_ggx_reflect(NH, a2);

		R = reflect(-V, H);
		R = reflect(R, planeNormal);
	}

	pdf = min(1024e32, pdf); /* Theoretical limit of 16bit float */
	pdf *= -1.0; /* Tag as planar ray. */

	/* Since viewspace hit position can land behind the camera in this case,
	 * we save the reflected view position (visualize it as the hit position
	 * below the reflection plane). This way it's garanted that the hit will
	 * be in front of the camera. That let us tag the bad rays with a negative
	 * sign in the Z component. */
	vec3 hit_pos = raycast(index, viewPosition, R * 1e16, 1e16, jitter, ssrQuality, a2, false);

	return vec4(hit_pos, pdf);
}

vec4 do_ssr(vec3 V, vec3 N, vec3 T, vec3 B, vec3 viewPosition, float a2, vec3 rand, float ofs)
{
	float pdf, NH;
	float jitter = fract(rand.x + ofs);

	/* Importance sampling bias */
	rand.x = mix(rand.x, 0.0, BRDF_BIAS);

	vec3 H = sample_ggx(rand, a2, N, T, B, NH); /* Microfacet normal */
	pdf = pdf_ggx_reflect(NH, a2);

	vec3 R = reflect(-V, H);
	pdf = min(1024e32, pdf); /* Theoretical limit of 16bit float */

	vec3 hit_pos = raycast(-1, viewPosition, R * 1e16, ssrThickness, jitter, ssrQuality, a2, true);

	return vec4(hit_pos, pdf);
}

void main()
{
#ifdef FULLRES
	ivec2 fullres_texel = ivec2(gl_FragCoord.xy);
	ivec2 halfres_texel = fullres_texel;
#else
	ivec2 fullres_texel = ivec2(gl_FragCoord.xy) * 2;
	ivec2 halfres_texel = ivec2(gl_FragCoord.xy);
#endif

	float depth = texelFetch(depthBuffer, fullres_texel, 0).r;

	/* Early out */
	if (depth == 1.0)
		discard;

	vec2 uvs = gl_FragCoord.xy / vec2(textureSize(depthBuffer, 0));
#ifndef FULLRES
	uvs *= 2.0;
#endif

	/* Using view space */
	vec3 viewPosition = get_view_space_from_depth(uvs, depth);
	vec3 V = viewCameraVec;
	vec3 N = normal_decode(texelFetch(normalBuffer, fullres_texel, 0).rg, V);

	/* Retrieve pixel data */
	vec4 speccol_roughness = texelFetch(specroughBuffer, fullres_texel, 0).rgba;

	/* Early out */
	if (dot(speccol_roughness.rgb, vec3(1.0)) == 0.0)
		discard;

	float roughness = speccol_roughness.a;
	float roughnessSquared = max(1e-3, roughness * roughness);
	float a2 = roughnessSquared * roughnessSquared;

	if (roughness > maxRoughness + 0.2) {
		hitData0 = hitData1 = hitData2 = hitData3 = vec4(0.0);
		return;
	}

	vec3 rand = texelFetch(utilTex, ivec3(halfres_texel % LUT_SIZE, 2), 0).rba;

	vec3 worldPosition = transform_point(ViewMatrixInverse, viewPosition);
	vec3 wN = transform_direction(ViewMatrixInverse, N);

	vec3 T, B;
	make_orthonormal_basis(N, T, B); /* Generate tangent space */

	/* Planar Reflections */
	for (int i = 0; i < MAX_PLANAR && i < planar_count; ++i) {
		PlanarData pd = planars_data[i];

		float fade = probe_attenuation_planar(pd, worldPosition, wN, 0.0);

		if (fade > 0.5) {
			/* Find view vector / reflection plane intersection. */
			/* TODO optimize, use view space for all. */
			vec3 tracePosition = line_plane_intersect(worldPosition, cameraVec, pd.pl_plane_eq);
			tracePosition = transform_point(ViewMatrix, tracePosition);
			vec3 planeNormal = transform_direction(ViewMatrix, pd.pl_normal);

			hitData0 = do_planar_ssr(i, V, N, T, B, planeNormal, tracePosition, a2, rand, 0.0);
#if (RAY_COUNT > 1)
			hitData1 = do_planar_ssr(i, V, N, T, B, planeNormal, tracePosition, a2, rand.xyz * vec3(1.0, -1.0, -1.0), 1.0 / float(RAY_COUNT));
#endif
#if (RAY_COUNT > 2)
			hitData2 = do_planar_ssr(i, V, N, T, B, planeNormal, tracePosition, a2, rand.xzy * vec3(1.0,  1.0, -1.0), 2.0 / float(RAY_COUNT));
#endif
#if (RAY_COUNT > 3)
			hitData3 = do_planar_ssr(i, V, N, T, B, planeNormal, tracePosition, a2, rand.xzy * vec3(1.0, -1.0,  1.0), 3.0 / float(RAY_COUNT));
#endif
			return;
		}
	}

	/* TODO : Raytrace together if textureGather is supported. */
	hitData0 = do_ssr(V, N, T, B, viewPosition, a2, rand, 0.0);
#if (RAY_COUNT > 1)
	hitData1 = do_ssr(V, N, T, B, viewPosition, a2, rand.xyz * vec3(1.0, -1.0, -1.0), 1.0 / float(RAY_COUNT));
#endif
#if (RAY_COUNT > 2)
	hitData2 = do_ssr(V, N, T, B, viewPosition, a2, rand.xzy * vec3(1.0,  1.0, -1.0), 2.0 / float(RAY_COUNT));
#endif
#if (RAY_COUNT > 3)
	hitData3 = do_ssr(V, N, T, B, viewPosition, a2, rand.xzy * vec3(1.0, -1.0,  1.0), 3.0 / float(RAY_COUNT));
#endif
}

#else /* STEP_RESOLVE */

uniform sampler2D prevColorBuffer; /* previous frame */
uniform sampler2D normalBuffer;
uniform sampler2D specroughBuffer;

uniform sampler2D hitBuffer0;
uniform sampler2D hitBuffer1;
uniform sampler2D hitBuffer2;
uniform sampler2D hitBuffer3;

uniform int probe_count;
uniform int planar_count;

uniform mat4 PastViewProjectionMatrix;

out vec4 fragColor;

void fallback_cubemap(vec3 N, vec3 V, vec3 W, vec3 viewPosition, float roughness, float roughnessSquared, inout vec4 spec_accum)
{
	/* Specular probes */
	vec3 spec_dir = get_specular_reflection_dominant_dir(N, V, roughnessSquared);

	vec4 rand = texture(utilTex, vec3(gl_FragCoord.xy / LUT_SIZE, 2.0));
	vec3 bent_normal;
	float final_ao = occlusion_compute(N, viewPosition, 1.0, rand.rg, bent_normal);
	final_ao = specular_occlusion(dot(N, V), final_ao, roughness);

	/* Starts at 1 because 0 is world probe */
	for (int i = 1; i < MAX_PROBE && i < probe_count && spec_accum.a < 0.999; ++i) {
		CubeData cd = probes_data[i];

		float fade = probe_attenuation_cube(cd, W);

		if (fade > 0.0) {
			vec3 spec = final_ao * probe_evaluate_cube(float(i), cd, W, spec_dir, roughness);
			accumulate_light(spec, fade, spec_accum);
		}
	}

	/* World Specular */
	if (spec_accum.a < 0.999) {
		vec3 spec = final_ao * probe_evaluate_world_spec(spec_dir, roughness);
		accumulate_light(spec, 1.0, spec_accum);
	}
}

#if 0 /* Finish reprojection with motion vectors */
vec3 get_motion_vector(vec3 pos)
{
}

/* http://bitsquid.blogspot.fr/2017/06/reprojecting-reflections_22.html */
vec3 find_reflection_incident_point(vec3 cam, vec3 hit, vec3 pos, vec3 N)
{
	float d_cam = point_plane_projection_dist(cam, pos, N);
	float d_hit = point_plane_projection_dist(hit, pos, N);

	if (d_hit < d_cam) {
		/* Swap */
		float tmp = d_cam;
		d_cam = d_hit;
		d_hit = tmp;
	}

	vec3 proj_cam = cam - (N * d_cam);
	vec3 proj_hit = hit - (N * d_hit);

	return (proj_hit - proj_cam) * d_cam / (d_cam + d_hit) + proj_cam;
}
#endif

float brightness(vec3 c)
{
	return max(max(c.r, c.g), c.b);
}

vec2 get_reprojected_reflection(vec3 hit, vec3 pos, vec3 N)
{
	/* TODO real reprojection with motion vectors, etc... */
	return project_point(PastViewProjectionMatrix, hit).xy * 0.5 + 0.5;
}

vec4 get_ssr_sample(
        sampler2D hitBuffer, PlanarData pd, float planar_index, vec3 worldPosition, vec3 N, vec3 V, float roughnessSquared,
        float cone_tan, vec2 source_uvs, vec2 texture_size, ivec2 target_texel,
        inout float weight_acc)
{
	vec4 hit_co_pdf = texelFetch(hitBuffer, target_texel, 0).rgba;
	bool has_hit = (hit_co_pdf.z > 0.0);
	bool is_planar = (hit_co_pdf.w < 0.0);
	hit_co_pdf.z = abs(hit_co_pdf.z);
	hit_co_pdf.w = abs(hit_co_pdf.w);

	/* Hit position in world space. */
	hit_co_pdf.xyz = get_view_space_from_depth(hit_co_pdf.xy, hit_co_pdf.z);
	vec3 hit_pos = transform_point(ViewMatrixInverse, hit_co_pdf.xyz);

	vec2 ref_uvs;
	vec3 hit_vec;
	float mask = 1.0;
	if (is_planar) {
		/* Reflect back the hit position to have it in non-reflected world space */
		vec3 trace_pos = line_plane_intersect(worldPosition, V, pd.pl_plane_eq);
		hit_vec = hit_pos - trace_pos;
		hit_vec = reflect(hit_vec, pd.pl_normal);
		ref_uvs = project_point(ProjectionMatrix, hit_co_pdf.xyz).xy * 0.5 + 0.5;
	}
	else {
		/* Find hit position in previous frame. */
		ref_uvs = get_reprojected_reflection(hit_pos, worldPosition, N);
		hit_vec = hit_pos - worldPosition;
		mask = screen_border_mask(gl_FragCoord.xy / texture_size);
	}
	mask = min(mask, screen_border_mask(ref_uvs));
	mask *= float(has_hit);

	float hit_dist = max(1e-8, length(hit_vec));
	vec3 L = hit_vec / hit_dist;

	float cone_footprint = hit_dist * cone_tan;

	/* Compute cone footprint in screen space. */
	float homcoord = ProjectionMatrix[2][3] * hit_co_pdf.z + ProjectionMatrix[3][3];
	cone_footprint = BRDF_BIAS * 0.5 * cone_footprint * max(ProjectionMatrix[0][0], ProjectionMatrix[1][1]) / homcoord;

	/* Estimate a cone footprint to sample a corresponding mipmap level. */
	float mip = clamp(log2(cone_footprint * max(texture_size.x, texture_size.y)), 0.0, MAX_MIP);

	/* Correct UVs for mipmaping mis-alignment */
	ref_uvs *= mip_ratio_interp(mip);

	/* Slide 54 */
	float bsdf = bsdf_ggx(N, L, V, roughnessSquared);
	float weight = step(1e-8, hit_co_pdf.w) * bsdf / max(1e-8, hit_co_pdf.w);
	weight_acc += weight;

	vec3 sample;
	if (is_planar) {
		sample = textureLod(probePlanars, vec3(ref_uvs, planar_index), mip).rgb;
	}
	else {
		sample = textureLod(prevColorBuffer, ref_uvs, mip).rgb;
	}

	/* Clamped brightness. */
	float luma = max(1e-8, brightness(sample));
	sample *= 1.0 - max(0.0, luma - fireflyFactor) / luma;

	/* Do not add light if ray has failed. */
	sample *= float(has_hit);

	/* Protection against NaNs in the history buffer.
	 * This could be removed if some previous pass has already
	 * sanitized the input. */
	if (any(isnan(sample))) {
		sample = vec3(0.0);
		weight = 0.0;
	}

	return vec4(sample, mask) * weight;
}

#define NUM_NEIGHBORS 4

void main()
{
	ivec2 fullres_texel = ivec2(gl_FragCoord.xy);
#ifdef FULLRES
	ivec2 halfres_texel = fullres_texel;
#else
	ivec2 halfres_texel = ivec2(gl_FragCoord.xy / 2.0);
#endif
	vec2 texture_size = vec2(textureSize(depthBuffer, 0));
	vec2 uvs = gl_FragCoord.xy / texture_size;
	vec3 rand = texelFetch(utilTex, ivec3(fullres_texel % LUT_SIZE, 2), 0).rba;

	float depth = textureLod(depthBuffer, uvs, 0.0).r;

	/* Early out */
	if (depth == 1.0)
		discard;

	/* Using world space */
	vec3 viewPosition = get_view_space_from_depth(uvs, depth); /* Needed for viewCameraVec */
	vec3 worldPosition = transform_point(ViewMatrixInverse, viewPosition);
	vec3 V = cameraVec;
	vec3 vN = normal_decode(texelFetch(normalBuffer, fullres_texel, 0).rg, viewCameraVec);
	vec3 N = transform_direction(ViewMatrixInverse, vN);
	vec4 speccol_roughness = texelFetch(specroughBuffer, fullres_texel, 0).rgba;

	/* Early out */
	if (dot(speccol_roughness.rgb, vec3(1.0)) == 0.0)
		discard;

	/* Find Planar Reflections affecting this pixel */
	PlanarData pd;
	float planar_index;
	for (int i = 0; i < MAX_PLANAR && i < planar_count; ++i) {
		pd = planars_data[i];

		float fade = probe_attenuation_planar(pd, worldPosition, N, 0.0);

		if (fade > 0.5) {
			planar_index = float(i);
			break;
		}
	}

	float roughness = speccol_roughness.a;
	float roughnessSquared = max(1e-3, roughness * roughness);

	vec4 spec_accum = vec4(0.0);

	/* Resolve SSR */
	float cone_cos = cone_cosine(roughnessSquared);
	float cone_tan = sqrt(1 - cone_cos * cone_cos) / cone_cos;
	cone_tan *= mix(saturate(dot(N, -V) * 2.0), 1.0, roughness); /* Elongation fit */

	vec2 source_uvs = project_point(PastViewProjectionMatrix, worldPosition).xy * 0.5 + 0.5;

	vec4 ssr_accum = vec4(0.0);
	float weight_acc = 0.0;
	const ivec2 neighbors[9] = ivec2[9](
		ivec2(0, 0),

		               ivec2(0,  1),
		ivec2(-1, -1),               ivec2(1, -1),

		ivec2(-1,  1),               ivec2(1,  1),
		               ivec2(0, -1),

		ivec2(-1,  0),               ivec2(1,  0)
	);
	ivec2 invert_neighbor;
	invert_neighbor.x = ((fullres_texel.x & 0x1) == 0) ? 1 : -1;
	invert_neighbor.y = ((fullres_texel.y & 0x1) == 0) ? 1 : -1;

	if (roughness < maxRoughness + 0.2) {
		for (int i = 0; i < NUM_NEIGHBORS; i++) {
			ivec2 target_texel = halfres_texel + neighbors[i] * invert_neighbor;

			ssr_accum += get_ssr_sample(hitBuffer0, pd, planar_index, worldPosition, N, V,
			                            roughnessSquared, cone_tan, source_uvs,
			                            texture_size, target_texel, weight_acc);
#if (RAY_COUNT > 1)
			ssr_accum += get_ssr_sample(hitBuffer1, pd, planar_index, worldPosition, N, V,
			                            roughnessSquared, cone_tan, source_uvs,
			                            texture_size, target_texel, weight_acc);
#endif
#if (RAY_COUNT > 2)
			ssr_accum += get_ssr_sample(hitBuffer2, pd, planar_index, worldPosition, N, V,
			                            roughnessSquared, cone_tan, source_uvs,
			                            texture_size, target_texel, weight_acc);
#endif
#if (RAY_COUNT > 3)
			ssr_accum += get_ssr_sample(hitBuffer3, pd, planar_index, worldPosition, N, V,
			                            roughnessSquared, cone_tan, source_uvs,
			                            texture_size, target_texel, weight_acc);
#endif
		}
	}

	/* Compute SSR contribution */
	if (weight_acc > 0.0) {
		ssr_accum /= weight_acc;
		/* fade between 0.5 and 1.0 roughness */
		ssr_accum.a *= smoothstep(maxRoughness + 0.2, maxRoughness, roughness); 
		accumulate_light(ssr_accum.rgb, ssr_accum.a, spec_accum);
	}

	/* If SSR contribution is not 1.0, blend with cubemaps */
	if (spec_accum.a < 1.0) {
		fallback_cubemap(N, V, worldPosition, viewPosition, roughness, roughnessSquared, spec_accum);
	}

	fragColor = vec4(spec_accum.rgb * speccol_roughness.rgb, 1.0);
}

#endif
