#define MAX_STEP 256
#define MAX_REFINE_STEP 32 /* Should be max allowed stride */

uniform vec4 ssrParameters;

uniform sampler2D depthBuffer;
uniform sampler2D maxzBuffer;
uniform sampler2D minzBuffer;
uniform sampler2DArray planarDepth;

#define ssrQuality    ssrParameters.x
#define ssrThickness  ssrParameters.y
#define ssrPixelSize  ssrParameters.zw

float sample_depth(vec2 uv, int index, float lod)
{
	if (index > -1) {
		return textureLod(planarDepth, vec3(uv, index), 0.0).r;
	}
	else {
		return textureLod(maxzBuffer, uv, lod).r;
	}
}

float sample_minz_depth(vec2 uv, int index)
{
	if (index > -1) {
		return textureLod(planarDepth, vec3(uv, index), 0.0).r;
	}
	else {
		return textureLod(minzBuffer, uv, 0.0).r;
	}
}

float sample_maxz_depth(vec2 uv, int index)
{
	if (index > -1) {
		return textureLod(planarDepth, vec3(uv, index), 0.0).r;
	}
	else {
		return textureLod(maxzBuffer, uv, 0.0).r;
	}
}

vec4 sample_depth_grouped(vec4 uv1, vec4 uv2, int index, float lod)
{
	vec4 depths;
	if (index > -1) {
		depths.x = textureLod(planarDepth, vec3(uv1.xy, index), 0.0).r;
		depths.y = textureLod(planarDepth, vec3(uv1.zw, index), 0.0).r;
		depths.z = textureLod(planarDepth, vec3(uv2.xy, index), 0.0).r;
		depths.w = textureLod(planarDepth, vec3(uv2.zw, index), 0.0).r;
	}
	else {
		depths.x = textureLod(maxzBuffer, uv1.xy, lod).r;
		depths.y = textureLod(maxzBuffer, uv1.zw, lod).r;
		depths.z = textureLod(maxzBuffer, uv2.xy, lod).r;
		depths.w = textureLod(maxzBuffer, uv2.zw, lod).r;
	}
	return depths;
}

float refine_isect(float prev_delta, float curr_delta)
{
	/**
	 * Simplification of 2D intersection :
	 * r0 = (0.0, prev_ss_ray.z);
	 * r1 = (1.0, curr_ss_ray.z);
	 * d0 = (0.0, prev_hit_depth_sample);
	 * d1 = (1.0, curr_hit_depth_sample);
	 * vec2 r = r1 - r0;
	 * vec2 d = d1 - d0;
	 * vec2 isect = ((d * cross(r1, r0)) - (r * cross(d1, d0))) / cross(r,d);
	 *
	 * We only want isect.x to know how much stride we need. So it simplifies :
	 *
	 * isect_x = (cross(r1, r0) - cross(d1, d0)) / cross(r,d);
	 * isect_x = (prev_ss_ray.z - prev_hit_depth_sample.z) / cross(r,d);
	 */
	return saturate(prev_delta / (prev_delta - curr_delta));
}

void prepare_raycast(vec3 ray_origin, vec3 ray_dir, out vec4 ss_step, out vec4 ss_ray, out float max_time)
{
	/* Negate the ray direction if it goes towards the camera.
	 * This way we don't need to care if the projected point
	 * is behind the near plane. */
	float z_sign = -sign(ray_dir.z);
	vec3 ray_end = z_sign * ray_dir * 1e16 + ray_origin;

	/* Project into screen space. */
	vec3 ss_start = project_point(ProjectionMatrix, ray_origin);
	vec3 ss_end = project_point(ProjectionMatrix, ray_end);
	/* 4th component is current stride */
	ss_step = vec4(z_sign * normalize(ss_end - ss_start), 1.0);

	/* If the line is degenerate, make it cover at least one pixel
	 * to not have to handle zero-pixel extent as a special case later */
	ss_step.xy += vec2((dot(ss_step.xy, ss_step.xy) < 0.00001) ? 0.001 : 0.0);

	/* Make ss_step cover one pixel. */
	ss_step.xyz /= max(abs(ss_step.x), abs(ss_step.y));
	ss_step.xyz *= ((abs(ss_step.x) > abs(ss_step.y)) ? ssrPixelSize.x : ssrPixelSize.y);

	/* Clipping to frustum sides. */
	max_time = line_unit_box_intersect_dist(ss_start, ss_step.xyz) - 1.0;

	/* Convert to texture coords. Z component included
	 * since this is how it's stored in the depth buffer.
	 * 4th component how far we are on the ray */
	ss_ray = vec4(ss_start * 0.5 + 0.5, 0.0);
	ss_step.xyz *= 0.5;
}

/* See times_and_deltas. */
#define curr_time   times_and_deltas.x
#define prev_time   times_and_deltas.y
#define curr_delta  times_and_deltas.z
#define prev_delta  times_and_deltas.w

// #define GROUPED_FETCHES
/* Return the hit position, and negate the z component (making it positive) if not hit occured. */
vec3 raycast(int index, vec3 ray_origin, vec3 ray_dir, float ray_jitter, float roughness)
{
	vec4 ss_step, ss_start;
	float max_time;
	prepare_raycast(ray_origin, ray_dir, ss_step, ss_start, max_time);

#ifdef GROUPED_FETCHES
	ray_jitter *= 0.25;
#endif
	/* x : current_time, y: previous_time, z: previous_delta, w: current_delta */
	vec4 times_and_deltas = vec4(0.0, 0.0, 0.001, 0.001);

	float ray_time = 0.0;
	float depth_sample;

	float lod_fac = saturate(fast_sqrt(roughness) * 2.0 - 0.4);
	bool hit = false;
	float iter;
	for (iter = 1.0; !hit && (ray_time <= max_time) && (iter < MAX_STEP); iter++) {
		/* Minimum stride of 2 because we are using half res minmax zbuffer. */
		float stride = max(1.0, iter * ssrQuality) * 2.0;
		float lod = log2(stride * 0.5 * ssrQuality) * lod_fac;

		/* Save previous values. */
		times_and_deltas.xyzw = times_and_deltas.yxwz;

#ifdef GROUPED_FETCHES
		stride *= 4.0;
		vec4 jit_stride = mix(vec4(2.0), vec4(stride), vec4(0.0, 0.25, 0.5, 0.75) + ray_jitter);

		vec4 times = vec4(ray_time) + jit_stride;

		vec4 uv1 = ss_start.xyxy + ss_step.xyxy * times.xxyy;
		vec4 uv2 = ss_start.xyxy + ss_step.xyxy * times.zzww;

		vec4 depth_samples = sample_depth_grouped(uv1, uv2, index, lod);

		vec4 ray_z = ss_start.zzzz + ss_step.zzzz * times.xyzw;

		vec4 deltas = depth_samples - ray_z;
		/* Same as component wise (depth_samples <= ray_z) && (ray_time <= max_time). */
		bvec4 test = equal(step(deltas, vec4(0.0)) * step(times, vec4(max_time)), vec4(1.0));
		hit = any(test);
		if (hit) {
			vec2 m = vec2(1.0, 0.0); /* Mask */

			vec4 ret_times_and_deltas = times.wzzz * m.xxyy + deltas.wwwz * m.yyxx;
			ret_times_and_deltas      = (test.z) ? times.zyyy * m.xxyy + deltas.zzzy * m.yyxx : ret_times_and_deltas;
			ret_times_and_deltas      = (test.y) ? times.yxxx * m.xxyy + deltas.yyyx * m.yyxx : ret_times_and_deltas;
			times_and_deltas          = (test.x) ? times.xxxx * m.xyyy + deltas.xxxx * m.yyxy + times_and_deltas.yyww * m.yxyx : ret_times_and_deltas;

			depth_sample = depth_samples.w;
			depth_sample = (test.z) ? depth_samples.z : depth_sample;
			depth_sample = (test.y) ? depth_samples.y : depth_sample;
			depth_sample = (test.x) ? depth_samples.x : depth_sample;
			break;
		}
		curr_time = times.w;
		curr_delta = deltas.w;
		ray_time += stride;
#else
		float jit_stride = mix(2.0, stride, ray_jitter);

		curr_time = ray_time + jit_stride;
		vec4 ss_ray = ss_start + ss_step * curr_time;

		depth_sample = sample_depth(ss_ray.xy, index, lod);

		curr_delta = depth_sample - ss_ray.z;
		hit = (curr_delta <= 0.0) && (curr_time <= max_time);

		ray_time += stride;
#endif
	}

	curr_time = (hit) ? mix(prev_time, curr_time, refine_isect(prev_delta, curr_delta)) : curr_time;
	ray_time = (hit) ? curr_time : ray_time;

#if 0 /* Not needed if using refine_isect() */
	/* Binary search */
	for (float time_step = (curr_time - prev_time) * 0.5; time_step > 1.0; time_step /= 2.0) {
		ray_time -= time_step;
		vec4 ss_ray = ss_start + ss_step * ray_time;
		float depth_sample = sample_maxz_depth(ss_ray.xy, index);
		bool is_hit = (depth_sample - ss_ray.z <= 0.0);
		ray_time = (is_hit) ? ray_time : ray_time + time_step;
	}
#endif

	/* Clip to frustum. */
	ray_time = min(ray_time, max_time - 0.5);

	vec4 ss_ray = ss_start + ss_step * ray_time;
	vec3 hit_pos = get_view_space_from_depth(ss_ray.xy, ss_ray.z);

	/* Reject hit if not within threshold. */
	/* TODO do this check while tracing. Potentially higher quality */
	if (hit && (index == -1)) {
		float z = get_view_z_from_depth(depth_sample);
		hit = hit && ((z - hit_pos.z - ssrThickness) <= ssrThickness);
	}

	/* Tag Z if ray failed. */
	hit_pos.z *= (hit) ? 1.0 : -1.0;
	return hit_pos;
}
