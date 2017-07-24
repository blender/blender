
#ifndef UTIL_TEX
#define UTIL_TEX
uniform sampler2DArray utilTex;
#endif /* UTIL_TEX */

#define BRDF_BIAS 0.7

vec3 generate_ray(vec3 V, vec3 N, float a2, vec3 rand, out float pdf)
{
	float NH;
	vec3 T, B;
	make_orthonormal_basis(N, T, B); /* Generate tangent space */

	/* Importance sampling bias */
	rand.x = mix(rand.x, 0.0, BRDF_BIAS);

	vec3 H = sample_ggx(rand, a2, N, T, B, NH); /* Microfacet normal */
	pdf = min(1024e32, pdf_ggx_reflect(NH, a2)); /* Theoretical limit of 16bit float */
	return reflect(-V, H);
}

#define MAX_MIP 9.0

#ifdef STEP_RAYTRACE

uniform sampler2D depthBuffer;
uniform sampler2D normalBuffer;
uniform sampler2D specroughBuffer;

layout(location = 0) out vec4 hitData;
layout(location = 1) out vec4 pdfData;

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

	/* Generate Ray */
	float pdf;
	vec3 rand = texelFetch(utilTex, ivec3(halfres_texel % LUT_SIZE, 2), 0).rba;
	vec3 R = generate_ray(V, N, a2, rand, pdf);

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
	float hit_dist = -1.0;
	/* Only raytrace if ray is above the surface normal */
	/* Note : this still fails in some cases like with normal map.
	 * We should check against the geometric normal but we don't have it at this stage. */
	if (dot(R, N) > 0.0001) {
		hit_dist = raycast(depthBuffer, viewPosition, R, rand.x);
	}

	/* TODO Do reprojection here */
	vec2 hit_co = project_point(ProjectionMatrix, viewPosition + R * hit_dist).xy * 0.5 + 0.5;

	/* Check if has hit a backface */
	vec3 hit_N = normal_decode(textureLod(normalBuffer, hit_co, 0.0).rg, V);
	hit_dist *= step(0.0, dot(-R, hit_N));

	if (hit_dist > 0.0) {
		hitData = hit_co.xyxy;
	}
	else {
		hitData = vec4(-1.0);
	}

	pdfData = vec4(pdf);
}

#else /* STEP_RESOLVE */

uniform sampler2D colorBuffer; /* previous frame */
uniform sampler2D depthBuffer;
uniform sampler2D normalBuffer;
uniform sampler2D specroughBuffer;

uniform sampler2D hitBuffer;
uniform sampler2D pdfBuffer;

uniform int probe_count;

uniform float borderFadeFactor;

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

float brightness(vec3 c)
{
	return max(max(c.r, c.g), c.b);
}

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

	const float margin = 0.003;
	float atten = borderFadeFactor + margin; /* Screen percentage */
	hit_co = smoothstep(margin, atten, hit_co) * (1 - smoothstep(1.0 - atten, 1.0 - margin, hit_co));
	vec2 atten_fac = min(hit_co.xy, hit_co.zw);

	float screenfade = atten_fac.x * atten_fac.y;

	return screenfade;
}

float view_facing_mask(vec3 V, vec3 R)
{
	/* Fade on viewing angle (strange deformations happens at R == V) */
	return smoothstep(0.95, 0.80, dot(V, R));
}


#define NUM_NEIGHBORS 9

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
	vec3 N = mat3(ViewMatrixInverse) * normal_decode(texelFetch(normalBuffer, fullres_texel, 0).rg, viewCameraVec);
	vec4 speccol_roughness = texelFetch(specroughBuffer, fullres_texel, 0).rgba;

	/* Early out */
	if (dot(speccol_roughness.rgb, vec3(1.0)) == 0.0)
		discard;

	float roughness = speccol_roughness.a;
	float roughnessSquared = max(1e-3, roughness * roughness);

	vec4 spec_accum = vec4(0.0);

	/* Resolve SSR */
	float cone_cos = cone_cosine(roughnessSquared);
	float cone_tan = sqrt(1 - cone_cos * cone_cos) / cone_cos;
	cone_tan *= mix(saturate(dot(N, V) * 2.0), 1.0, roughness); /* Elongation fit */

	vec2 source_uvs = project_point(PastViewProjectionMatrix, worldPosition).xy * 0.5 + 0.5;

	vec4 ssr_accum = vec4(0.0);
	float weight_acc = 0.0;
	float mask_acc = 0.0;
	float dist_acc = 0.0;
	float hit_acc = 0.0;
	const ivec2 neighbors[9] = ivec2[9](
		ivec2(0, 0),
		ivec2(-1,  1), ivec2(0,  1), ivec2(1,  1),
		ivec2(-1,  0),               ivec2(1,  0),
		ivec2(-1, -1), ivec2(0, -1), ivec2(1, -1)
	);
	ivec2 invert_neighbor;
	invert_neighbor.x = ((fullres_texel.x & 0x1) == 0) ? 1 : -1;
	invert_neighbor.y = ((fullres_texel.y & 0x1) == 0) ? 1 : -1;
	for (int i = 0; i < NUM_NEIGHBORS; i++) {
		ivec2 target_texel = halfres_texel + neighbors[i] * invert_neighbor;

		vec2 hit_co = texelFetch(hitBuffer, target_texel, 0).rg;

		/* Reconstruct ray */
		float hit_depth = textureLod(depthBuffer, hit_co, 0.0).r;
		vec3 hit_pos = get_world_space_from_depth(hit_co, hit_depth);

		/* Find hit position in previous frame */
		vec2 ref_uvs = get_reprojected_reflection(hit_pos, worldPosition, N);

		/* Estimate a cone footprint to sample a corresponding mipmap level */
		/* compute cone footprint Using UV distance because we are using screen space filtering */
		float cone_footprint = 1.5 * cone_tan * distance(ref_uvs, source_uvs);
		float mip = BRDF_BIAS * clamp(log2(cone_footprint * max(texture_size.x, texture_size.y)), 0.0, MAX_MIP);

		vec3 L = normalize(hit_pos - worldPosition);
#ifdef USE_NORMALIZATION
		/* Evaluate BSDF */
		float bsdf = bsdf_ggx(N, L, V, roughnessSquared);
		float pdf = texelFetch(pdfBuffer, target_texel, 0).r;

		float weight = step(0.001, pdf) * bsdf / pdf;
#else
		float weight = 1.0;
#endif

		vec3 sample = textureLod(colorBuffer, ref_uvs, mip).rgb ;

		/* Firefly removal */
		sample /= 1 + brightness(sample);

		float mask = screen_border_mask(ref_uvs, hit_pos);
		mask *= view_facing_mask(V, N);

		/* Check if there was a hit */
		ssr_accum += vec4(sample, mask) * weight * step(0.0, hit_co.x);
		weight_acc += weight;
	}

	/* Compute SSR contribution */
	if (weight_acc > 0.0) {
		ssr_accum /= weight_acc;
		/* fade between 0.5 and 1.0 roughness */
		ssr_accum.a *= saturate(2.0 - roughness * 2.0);
		accumulate_light(ssr_accum.rgb, ssr_accum.a, spec_accum);
	}

	/* If SSR contribution is not 1.0, blend with cubemaps */
	if (spec_accum.a < 1.0) {
		fallback_cubemap(N, V, worldPosition, roughness, roughnessSquared, spec_accum);
	}

	fragColor = vec4(spec_accum.rgb * speccol_roughness.rgb, 1.0);
	// vec2 _uvs = project_point(PastViewProjectionMatrix, worldPosition).xy * 0.5 + 0.5;
	// fragColor = vec4(textureLod(colorBuffer, _uvs, roughness * MAX_MIP).rgb, 1.0);
}

#endif
