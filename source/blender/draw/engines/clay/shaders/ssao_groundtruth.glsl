#define ssao_distance		matcaps_param[mat_id].ssao_params_var.x
#define ssao_factor_cavity	matcaps_param[mat_id].ssao_params_var.y
#define ssao_factor_edge	matcaps_param[mat_id].ssao_params_var.z
#define ssao_attenuation	matcaps_param[mat_id].ssao_params_var.w

/* Based on Practical Realtime Strategies for Accurate Indirect Occlusion
 * http://blog.selfshadow.com/publications/s2016-shading-course/activision/s2016_pbs_activision_occlusion.pdf
 * http://blog.selfshadow.com/publications/s2016-shading-course/activision/s2016_pbs_activision_occlusion.pptx */

#define COSINE_WEIGHTING

float integrate_arc(in float h1, in float h2, in float gamma, in float n_proj_len)
{
	float a = 0.0;
#ifdef COSINE_WEIGHTING
	float cos_gamma = cos(gamma);
	float sin_gamma_2 = 2.0 * sin(gamma);
	a += -cos(2.0 * h1 - gamma) + cos_gamma + h1 * sin_gamma_2;
	a += -cos(2.0 * h2 - gamma) + cos_gamma + h2 * sin_gamma_2;
	a *= 0.25; /* 1/4 */
	a *= n_proj_len;
#else
	/* Uniform weighting (slide 59) */
	a += 1 - cos(h1);
	a += 1 - cos(h2);
#endif
	return a;
}

float get_max_horizon(in vec2 co, in vec3 x, in vec3 omega_o, in float h)
{
	if (co.x > 1.0 || co.x < 0.0 || co.y > 1.0 || co.y < 0.0)
		return h;

	float depth = texture(depthtex, co).r;

	/* Background case */
	if (depth == 1.0)
		return h;

	vec3 s = get_view_space_from_depth(co, depth); /* s View coordinate */
	vec3 omega_s = s - x;
	float len = length(omega_s);

	if (len < ssao_distance) {
		omega_s /= len;
		h = max(h, dot(omega_s, omega_o));
	}
	return h;
}

void ssao_factors(in float depth, in vec3 normal, in vec3 position, in vec2 screenco, out float cavities, out float edges)
{
	/* Renaming */
	vec3 omega_o = -normalize(position); /* viewvec */
	vec2 x_ = screenco; /* x^ Screen coordinate */
	vec3 x = position; /* x view space coordinate */

#ifdef SPATIAL_DENOISE
	float noise_dir = (1.0 / 16.0) * float(((int(gl_FragCoord.x + gl_FragCoord.y) & 0x3) << 2) + (int(gl_FragCoord.x) & 0x3));
	float noise_offset = (1.0 / 4.0) * float(int(gl_FragCoord.y - gl_FragCoord.x) & 0x3);
#else
	float noise_dir = (1.0 / 16.0) * float(((int(gl_FragCoord.x + gl_FragCoord.y) & 0x3) << 2) + (int(gl_FragCoord.x) & 0x3));
	float noise_offset = (0.5 / 16.0) + (1.0 / 16.0) * float(((int(gl_FragCoord.x - gl_FragCoord.y) & 0x3) << 2) + (int(gl_FragCoord.x) & 0x3));
#endif

	const float phi_step = 16.0;
	const float theta_step = 16.0;
	const float m_pi = 3.14159265358979323846;
	vec2 pixel_ratio = vec2(screenres.y / screenres.x, 1.0);
	vec2 pixel_size = vec2(1.0) / screenres.xy;
	float min_stride = length(pixel_size);
	float homcco = WinMatrix[2][3] * position.z + WinMatrix[3][3];
	float n = max(min_stride * theta_step, ssao_distance / homcco); /* Search distance */

	/* Integral over PI */
	float A = 0.0;
	for (float i = 0.0; i < phi_step; i++) {
		float phi = m_pi * ((noise_dir + i) / phi_step);

		vec2 t_phi = vec2(cos(phi), sin(phi)); /* Screen space direction */

		/* Search maximum horizon angles Theta1 and Theta2 */
		float theta1 = -1.0, theta2 = -1.0; /* init at cos(pi) */
		for (float j = 0.0; j < theta_step; j++) {
			vec2 s_ = t_phi * pixel_ratio * n * ((j + noise_offset)/ theta_step); /* s^ Screen coordinate */
			vec2 co;

			co = x_ + s_;
			theta1 = get_max_horizon(co, x, omega_o, theta1);

			co = x_ - s_;
			theta2 = get_max_horizon(co, x, omega_o, theta2);
		}

		/* (Slide 54) */
		theta1 = -acos(theta1);
		theta2 = acos(theta2);

		/* Projecting Normal to Plane P defined by t_phi and omega_o */
		vec3 h = normalize(cross(vec3(t_phi, 0.0), omega_o)); /* Normal vector to Integration plane */
		vec3 t = cross(h, omega_o); /* Normal vector to plane */
		vec3 n_proj = normal - h * dot(normal, h);
		float n_proj_len = length(n_proj);
		vec3 n_proj_norm = normalize(n_proj);

		/* Clamping thetas (slide 58) */
		float gamma = sign(dot(n_proj_norm, t)) * acos(dot(normal, omega_o)); /* Angle between view vec and normal */
		theta1 = gamma + max(theta1 - gamma, -m_pi * 0.5);
		theta2 = gamma + min(theta2 - gamma, m_pi * 0.5);

		/* Solving inner integral */
		A += integrate_arc(theta1, theta2, gamma, n_proj_len);
	}

	A /= phi_step;

	cavities = 1.0 - A;
	edges = 0.0;
}