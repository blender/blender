
/* Based on Practical Realtime Strategies for Accurate Indirect Occlusion
 * http://blog.selfshadow.com/publications/s2016-shading-course/activision/s2016_pbs_activision_occlusion.pdf
 * http://blog.selfshadow.com/publications/s2016-shading-course/activision/s2016_pbs_activision_occlusion.pptx */

#define MAX_PHI_STEP 32
/* NOTICE : this is multiplied by 2 */
#define MAX_THETA_STEP 6.0

uniform sampler2D minMaxDepthTex;
uniform float aoDistance;
uniform float aoSamples;
uniform float aoFactor;

float sample_depth(vec2 co, int level)
{
	return textureLod(minMaxDepthTex, co, float(level)).g;
}

float get_max_horizon(vec2 co, vec3 x, float h, float step)
{
	if (co.x > 1.0 || co.x < 0.0 || co.y > 1.0 || co.y < 0.0)
		return h;

	float depth = sample_depth(co, int(step));

	/* Background case */
	if (depth == 1.0)
		return h;

	vec3 s = get_view_space_from_depth(co, depth); /* s View coordinate */
	vec3 omega_s = s - x;
	float len = length(omega_s);

	float max_h = max(h, omega_s.z / len);
	/* Blend weight after half the aoDistance to fade artifacts */
	float blend = saturate((1.0 - len / aoDistance) * 2.0);

	return mix(h, max_h, blend);
}

void gtao(vec3 normal, vec3 position, vec2 noise, out float visibility
#ifdef USE_BENT_NORMAL
	, out vec3 bent_normal
#endif
	)
{
	vec2 screenres = vec2(textureSize(minMaxDepthTex, 0)) * 2.0;
	vec2 pixel_size = vec2(1.0) / screenres.xy;

	/* Renaming */
	vec2 x_ = gl_FragCoord.xy * pixel_size; /* x^ Screen coordinate */
	vec3 x = position; /* x view space coordinate */

	/* NOTE : We set up integration domain around the camera forward axis
	 * and not the view vector like in the paper.
	 * This allows us to save a lot of dot products. */
	/* omega_o = vec3(0.0, 0.0, 1.0); */

	vec2 pixel_ratio = vec2(screenres.y / screenres.x, 1.0);
	float pixel_len = length(pixel_size);
	float homcco = ProjectionMatrix[2][3] * position.z + ProjectionMatrix[3][3];
	float max_dist = aoDistance / homcco; /* Search distance */

	/* Integral over PI */
	visibility = 0.0;
#ifdef USE_BENT_NORMAL
	bent_normal = vec3(0.0);
#endif
	for (float i = 0.0; i < aoSamples && i < MAX_PHI_STEP; i++) {
		float phi = M_PI * ((noise.r + i) / aoSamples);

		/* Rotate with random direction to get jittered result. */
		vec2 t_phi = vec2(cos(phi), sin(phi)); /* Screen space direction */

		/* Search maximum horizon angles h1 and h2 */
		float h1 = -1.0, h2 = -1.0; /* init at cos(pi) */
		float ofs = 1.5 * pixel_len;
		for (float j = 0.0; ofs < max_dist && j < MAX_THETA_STEP; j += 0.5) {
			ofs += ofs; /* Step size is doubled each iteration */

			vec2 s_ = t_phi * ofs * noise.g * pixel_ratio; /* s^ Screen coordinate */
			vec2 co;

			co = x_ + s_;
			h1 = get_max_horizon(co, x, h1, j);

			co = x_ - s_;
			h2 = get_max_horizon(co, x, h2, j);
		}

		/* (Slide 54) */
		h1 = -acos(h1);
		h2 = acos(h2);

		/* Projecting Normal to Plane P defined by t_phi and omega_o */
		vec3 h = vec3(t_phi.y, -t_phi.x, 0.0); /* Normal vector to Integration plane */
		vec3 t = vec3(-t_phi, 0.0);
		vec3 n_proj = normal - h * dot(h, normal);
		float n_proj_len = max(1e-16, length(n_proj));

		/* Clamping thetas (slide 58) */
		float cos_n = clamp(n_proj.z / n_proj_len, -1.0, 1.0);
		float n = sign(dot(n_proj, t)) * acos(cos_n); /* Angle between view vec and normal */
		h1 = n + max(h1 - n, -M_PI_2);
		h2 = n + min(h2 - n, M_PI_2);

		/* Solving inner integral */
		float sin_n = sin(n);
		float h1_2 = 2.0 * h1;
		float h2_2 = 2.0 * h2;
		float vd = (-cos(h1_2 - n) + cos_n + h1_2 * sin_n) + (-cos(h2_2 - n) + cos_n + h2_2 * sin_n);
		vd *= 0.25 * n_proj_len;
		visibility += vd;

#ifdef USE_BENT_NORMAL
		/* Finding Bent normal */
		float b_angle = (h1 + h2) / 2.0;
		/* The 0.5 factor below is here to equilibrate the accumulated vectors.
		 * (sin(b_angle) * -t_phi) will accumulate to (phi_step * result_nor.xy * 0.5).
		 * (cos(b_angle) * 0.5) will accumulate to (phi_step * result_nor.z * 0.5). */
		/* Weight sample by vd */
		bent_normal += vec3(sin(b_angle) * -t_phi, cos(b_angle) * 0.5) * vd;
#endif
	}

	visibility = min(1.0, visibility / aoSamples);

#ifdef USE_BENT_NORMAL
	/* The bent normal will show the facet look of the mesh. Try to minimize this. */
	bent_normal = normalize(mix(bent_normal / visibility, normal, visibility * visibility * visibility));
#endif

	/* Scale by user factor */
	visibility = max(0.0, mix(aoFactor, 1.0, visibility));
}

/* Multibounce approximation base on surface albedo.
 * Page 78 in the .pdf version. */
float gtao_multibounce(float visibility, vec3 albedo)
{
	/* Median luminance. Because Colored multibounce looks bad. */
	float lum = albedo.x * 0.3333;
	lum += albedo.y * 0.3333;
	lum += albedo.z * 0.3333;

	float a =  2.0404 * lum - 0.3324;
	float b = -4.7951 * lum + 0.6417;
	float c =  2.7552 * lum + 0.6903;

	float x = visibility;
	return max(x, ((x * a + b) * x + c) * x);
}