
#ifndef UTIL_TEX
#define UTIL_TEX
uniform sampler2DArray utilTex;
#endif /* UTIL_TEX */

vec3 generate_ray(ivec2 pix, vec3 V, vec3 N, float roughnessSquared, out float pdf)
{
	float NH;
	vec3 T, B;
	make_orthonormal_basis(N, T, B); /* Generate tangent space */
	vec3 rand = texelFetch(utilTex, ivec3(pix % LUT_SIZE, 2), 0).rba;
	vec3 H = sample_ggx(rand, roughnessSquared, N, T, B, NH); /* Microfacet normal */
	pdf = max(32e32, pdf_ggx_reflect(NH, roughnessSquared)); /* Theoretical limit of 10bit float (not in practice?) */
	return reflect(-V, H);
}

#ifdef STEP_RAYTRACE

uniform sampler2D depthBuffer;
uniform sampler2D normalBuffer;
uniform sampler2D specroughBuffer;

layout(location = 0) out vec4 hitData;
layout(location = 1) out vec4 pdfData;

void main()
{
	ivec2 fullres_texel = ivec2(gl_FragCoord.xy) * 2;
	ivec2 halfres_texel = ivec2(gl_FragCoord.xy);
	float depth = texelFetch(depthBuffer, fullres_texel, 0).r;

	/* Early out */
	if (depth == 1.0)
		discard;

	vec2 uvs = gl_FragCoord.xy * 2.0 / vec2(textureSize(depthBuffer, 0));

	/* Using view space */
	vec3 viewPosition = get_view_space_from_depth(uvs, depth);
	vec3 V = viewCameraVec;
	vec3 N = normal_decode(texelFetch(normalBuffer, fullres_texel, 0).rg, V);

	/* Retrieve pixel data */
	vec4 speccol_roughness = texelFetch(specroughBuffer, fullres_texel, 0).rgba;
	float roughness = speccol_roughness.a;
	float roughnessSquared = roughness * roughness;

	/* Generate Ray */
	float pdf;
	vec3 R = generate_ray(halfres_texel, V, N, roughnessSquared, pdf);

	/* Search for the planar reflection affecting this pixel */
	/* If no planar is found, fallback to screen space */

	/* Raycast over planar reflection */
	/* Potentially lots of time waisted here for pixels
	 * that does not have planar reflections. TODO Profile it. */
	/* TODO: Idea, rasterize boxes around planar
	 * reflection volumes (frontface culling to avoid overdraw)
	 * and do the raycasting, discard pixel that are not in influence.
	 * Add stencil test to discard the main SSR.
	 * Cons: - Potentially raytrace multiple times
	 *         if Planar Influence overlaps. */
	//float hit_dist = raycast(depthBuffer, W, R);

	/* Raycast over screen */
	float hit_dist = raycast(depthBuffer, viewPosition, R);

	vec2 hit_co = project_point(ProjectionMatrix, viewPosition + R * hit_dist).xy * 0.5 + 0.5;

	/* Check if has hit a backface */
	vec3 hit_N = normal_decode(textureLod(normalBuffer, hit_co, 0.0).rg, V);
	pdf *= step(0.0, dot(-R, hit_N));

	hitData = hit_co.xyxy;
	pdfData = vec4(pdf) * step(0.0, hit_dist);
}

#else /* STEP_RESOLVE */

uniform sampler2D colorBuffer; /* previous frame */
uniform sampler2D depthBuffer;
uniform sampler2D normalBuffer;
uniform sampler2D specroughBuffer;

uniform sampler2D hitBuffer;
uniform sampler2D pdfBuffer;

uniform int probe_count;

uniform mat4 ViewProjectionMatrix;
uniform mat4 PastViewProjectionMatrix;

out vec4 fragColor;

void fallback_cubemap(vec3 N, vec3 V, vec3 W, float roughness, float roughnessSquared, inout vec4 spec_accum)
{
	/* Specular probes */
	vec3 spec_dir = get_specular_dominant_dir(N, V, roughnessSquared);

	/* Starts at 1 because 0 is world probe */
	for (int i = 1; i < MAX_PROBE && i < probe_count && spec_accum.a < 0.999; ++i) {
		CubeData cd = probes_data[i];

		float fade = probe_attenuation_cube(cd, W);

		if (fade > 0.0) {
			vec3 spec = probe_evaluate_cube(float(i), cd, W, spec_dir, roughness);
			accumulate_light(spec, fade, spec_accum);
		}
	}

	/* World Specular */
	if (spec_accum.a < 0.999) {
		vec3 spec = probe_evaluate_world_spec(spec_dir, roughness);
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

vec2 get_reprojected_reflection(vec3 hit, vec3 pos, vec3 N)
{
	/* TODO real motion vectors */
	/* Transform to viewspace */
	// vec4(get_view_space_from_depth(uvcoords, depth), 1.0);
	// vec4(get_view_space_from_depth(uvcoords, depth), 1.0);

	/* Reproject */
	// vec3 hit_reprojected = find_reflection_incident_point(cameraPos, hit, pos, N);

	return project_point(PastViewProjectionMatrix, hit).xy * 0.5 + 0.5;
}

float screen_border_mask(vec2 past_hit_co, vec3 hit)
{
	/* Fade on current and past screen edges */
	vec4 hit_co = ViewProjectionMatrix * vec4(hit, 1.0);
	hit_co.xy = (hit_co.xy / hit_co.w) * 0.5 + 0.5;
	hit_co.zw = past_hit_co;

	const float margin = 0.002;
	const float atten = 0.05 + margin; /* Screen percentage */
	hit_co = smoothstep(margin, atten, hit_co) * (1 - smoothstep(1.0 - atten, 1.0 - margin, hit_co));
	vec2 atten_fac = min(hit_co.xy, hit_co.zw);

	float screenfade = atten_fac.x * atten_fac.y;

	return screenfade;
}

#define NUM_NEIGHBORS 4

void main()
{
	ivec2 halfres_texel = ivec2(gl_FragCoord.xy / 2.0);
	ivec2 fullres_texel = ivec2(gl_FragCoord.xy);
	vec2 uvs = gl_FragCoord.xy / vec2(textureSize(depthBuffer, 0));

	float depth = textureLod(depthBuffer, uvs, 0.0).r;

	/* Early out */
	if (depth == 1.0)
		discard;

	/* Using world space */
	vec3 viewPosition = get_view_space_from_depth(uvs, depth); /* Needed for viewCameraVec */
	vec3 worldPosition = transform_point(ViewMatrixInverse, viewPosition);
	vec3 V = cameraVec;
	vec3 N = mat3(ViewMatrixInverse) * normal_decode(texelFetch(normalBuffer, fullres_texel, 0).rg, viewCameraVec);
	vec4 speccol_roughness = texelFetch(specroughBuffer, fullres_texel, 0).rgba;
	float roughness = speccol_roughness.a;
	float roughnessSquared = roughness * roughness;

	vec4 spec_accum = vec4(0.0);

	/* Resolve SSR and compute contribution */

	/* We generate the same rays that has been generated in the raycast step.
	 * But we add this ray from our resolve pixel position, increassing accuracy. */
	vec3 ssr_accum = vec3(0.0);
	float weight_acc = 0.0;
	float mask = 0.0;
	const ivec2 neighbors[7] = ivec2[7](ivec2(0, 0), ivec2(2, 1), ivec2(-2, 1), ivec2(0, -2), ivec2(-2, -1), ivec2(2, -1), ivec2(0, 2));
	int invert_neighbor = ((((fullres_texel.x & 0x1) + (fullres_texel.y & 0x1)) & 0x1) == 0) ? 1 : -1;
	for (int i = 0; i < NUM_NEIGHBORS; i++) {
		ivec2 target_texel = halfres_texel + neighbors[i] * invert_neighbor;

		float pdf = texelFetch(pdfBuffer, target_texel, 0).r;

		/* Check if there was a hit */
		if (pdf != 0.0) {
			vec2 hit_co = texelFetch(hitBuffer, target_texel, 0).rg;

			/* Reconstruct ray */
			float hit_depth = textureLod(depthBuffer, hit_co, 0.0).r;
			vec3 hit_pos = get_world_space_from_depth(hit_co, hit_depth);

			/* Find hit position in previous frame */
			vec2 ref_uvs = get_reprojected_reflection(hit_pos, worldPosition, N);

			/* Evaluate BSDF */
			vec3 L = normalize(hit_pos - worldPosition);
			float bsdf = bsdf_ggx(N, L, V, max(1e-3, roughness));

			float weight = bsdf / max(1e-8, pdf);
			mask += screen_border_mask(ref_uvs, hit_pos);
			ssr_accum += textureLod(colorBuffer, ref_uvs, 0.0).rgb * weight;
			weight_acc += weight;
		}
	}

	if (weight_acc > 0.0) {
		accumulate_light(ssr_accum / weight_acc, mask / float(NUM_NEIGHBORS), spec_accum);
	}

	/* If SSR contribution is not 1.0, blend with cubemaps */
	if (spec_accum.a < 1.0) {
		fallback_cubemap(N, V, worldPosition, roughness, roughnessSquared, spec_accum);
	}

	fragColor = vec4(spec_accum.rgb * speccol_roughness.rgb, 1.0);
	// fragColor = vec4(texelFetch(hitBuffer, halfres_texel, 0).rgb, 1.0);
}

#endif
