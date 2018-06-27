

/*  from The Alchemy screen-space ambient obscurance algorithm
 * http://graphics.cs.williams.edu/papers/AlchemyHPG11/VV11AlchemyAO.pdf */

void ssao_factors(
        in float depth, in vec3 normal, in vec3 position, in vec2 screenco,
        out float cavities, out float edges)
{
	cavities = edges = 0.0;
	/* early out if there is no need for SSAO */
	if (ssao_factor_cavity == 0.0 && ssao_factor_edge == 0.0)
		return;

	/* take the normalized ray direction here */
	vec3 noise = texture(ssao_jitter, screenco.xy * jitter_tilling).rgb;

	/* find the offset in screen space by multiplying a point
	 * in camera space at the depth of the point by the projection matrix. */
	vec2 offset;
	float homcoord = WinMatrix[2][3] * position.z + WinMatrix[3][3];
	offset.x = WinMatrix[0][0] * ssao_distance / homcoord;
	offset.y = WinMatrix[1][1] * ssao_distance / homcoord;
	/* convert from -1.0...1.0 range to 0.0..1.0 for easy use with texture coordinates */
	offset *= 0.5;


	int num_samples = int(ssao_samples_num);

	/* Note. Putting noise usage here to put some ALU after texture fetch. */
	vec2 rotX = noise.rg;
	vec2 rotY = vec2(-rotX.y, rotX.x);

	for (int x = 0; x < num_samples; x++) {
		int sample_index = x + (int(ssao_iteration) * num_samples);
		if (sample_index > 500) {
			continue;
		}
		/* ssao_samples[x].xy is sample direction (normalized).
		 * ssao_samples[x].z is sample distance from disk center. */

		/* Rotate with random direction to get jittered result. */
		vec2 dir_jittered = vec2(dot(ssao_samples[sample_index].xy, rotX), dot(ssao_samples[sample_index].xy, rotY));
		dir_jittered.xy *= ssao_samples[sample_index].z + noise.b;

		vec2 uvcoords = screenco.xy + dir_jittered * offset;

		if (uvcoords.x > 1.0 || uvcoords.x < 0.0 || uvcoords.y > 1.0 || uvcoords.y < 0.0)
			continue;

		float depth_new = texture(depthBuffer, uvcoords).r;

		/* Handle Background case */
		bool is_background = (depth_new == 1.0);

		/* This trick provide good edge effect even if no neighboor is found. */
		vec3 pos_new = get_view_space_from_depth(uvcoords, (is_background) ? depth : depth_new);

		if (is_background)
			pos_new.z -= ssao_distance;

		vec3 dir = pos_new - position;
		float len = length(dir);
		float f_cavities = dot(dir, normal);
		float f_edge = -f_cavities;
		float f_bias = 0.05 * len + 0.0001;

		float attenuation = 1.0 / (len * (1.0 + len * len * ssao_attenuation));

		/* use minor bias here to avoid self shadowing */
		if (f_cavities > -f_bias)
			cavities += f_cavities * attenuation;

		if (f_edge > f_bias)
			edges += f_edge * attenuation;
	}

	cavities /= ssao_samples_num;
	edges /= ssao_samples_num;

	/* don't let cavity wash out the surface appearance */
	cavities = clamp(cavities * ssao_factor_cavity, 0.0, 1.0);
	edges = edges * ssao_factor_edge;
}
