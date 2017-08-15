
/* Based on Practical Realtime Strategies for Accurate Indirect Occlusion
 * http://blog.selfshadow.com/publications/s2016-shading-course/activision/s2016_pbs_activision_occlusion.pdf
 * http://blog.selfshadow.com/publications/s2016-shading-course/activision/s2016_pbs_activision_occlusion.pptx */

#define MAX_PHI_STEP 32
/* NOTICE : this is multiplied by 2 */
#define MAX_THETA_STEP 12

uniform vec3 aoParameters;

#define aoDistance   aoParameters.x
#define aoSamples    aoParameters.y
#define aoFactor     aoParameters.z

void get_max_horizon_grouped(vec4 co1, vec4 co2, vec3 x, float lod, inout float h)
{
	co1 *= mipRatio[int(lod + 1.0)].xyxy; /* +1 because we are using half res top level */
	co2 *= mipRatio[int(lod + 1.0)].xyxy; /* +1 because we are using half res top level */

	float depth1 = textureLod(maxzBuffer, co1.xy, floor(lod)).r;
	float depth2 = textureLod(maxzBuffer, co1.zw, floor(lod)).r;
	float depth3 = textureLod(maxzBuffer, co2.xy, floor(lod)).r;
	float depth4 = textureLod(maxzBuffer, co2.zw, floor(lod)).r;

	vec4 len, s_h;

	vec3 s1 = get_view_space_from_depth(co1.xy, depth1); /* s View coordinate */
	vec3 omega_s1 = s1 - x;
	len.x = length(omega_s1);
	s_h.x = omega_s1.z / len.x;

	vec3 s2 = get_view_space_from_depth(co1.zw, depth2); /* s View coordinate */
	vec3 omega_s2 = s2 - x;
	len.y = length(omega_s2);
	s_h.y = omega_s2.z / len.y;

	vec3 s3 = get_view_space_from_depth(co2.xy, depth3); /* s View coordinate */
	vec3 omega_s3 = s3 - x;
	len.z = length(omega_s3);
	s_h.z = omega_s3.z / len.z;

	vec3 s4 = get_view_space_from_depth(co2.zw, depth4); /* s View coordinate */
	vec3 omega_s4 = s4 - x;
	len.w = length(omega_s4);
	s_h.w = omega_s4.z / len.w;

	/* Blend weight after half the aoDistance to fade artifacts */
	vec4 blend = saturate((1.0 - len / aoDistance) * 2.0);

	h = mix(h, max(h, s_h.x), blend.x);
	h = mix(h, max(h, s_h.y), blend.y);
	h = mix(h, max(h, s_h.z), blend.z);
	h = mix(h, max(h, s_h.w), blend.w);
}

#define MAX_ITER 16
#define MAX_LOD 6.0
#define QUALITY 0.75
vec2 search_horizon_sweep(vec2 t_phi, vec3 pos, vec2 uvs, float jitter, vec2 max_dir)
{
	max_dir *= max_v2(abs(t_phi));

	/* Convert to pixel space. */
	t_phi /= vec2(textureSize(maxzBuffer, 0));

	/* Avoid division by 0 */
	t_phi += vec2(1e-5);

	jitter *= 0.25;

	/* Compute end points */
	vec2 corner1 = min(vec2(1.0) - uvs,  max_dir); /* Top right */
	vec2 corner2 = max(vec2(0.0) - uvs, -max_dir); /* Bottom left */
	vec2 iter1 = corner1 / t_phi;
	vec2 iter2 = corner2 / t_phi;

	vec2 min_iter = max(-iter1, -iter2);
	vec2 max_iter = max( iter1,  iter2);

	vec2 times = vec2(-min_v2(min_iter), min_v2(max_iter));

	vec2 h = vec2(-1.0); /* init at cos(pi) */

	/* This is freaking sexy optimized. */
	for (float i = 0.0, ofs = 4.0, time = -1.0;
		 i < MAX_ITER && time > times.x;
		 i++, time -= ofs, ofs = min(exp2(MAX_LOD) * 4.0, ofs + ofs))
	{
		vec4 t = max(times.xxxx, vec4(time) - (vec4(0.25, 0.5, 0.75, 1.0) - jitter) * ofs);
		vec4 cos1 = uvs.xyxy + t_phi.xyxy * t.xxyy;
		vec4 cos2 = uvs.xyxy + t_phi.xyxy * t.zzww;
		get_max_horizon_grouped(cos1, cos2, pos, min(MAX_LOD, i * QUALITY), h.y);
	}

	for (float i = 0.0, ofs = 4.0, time = 1.0;
		 i < MAX_ITER && time < times.y;
		 i++, time += ofs, ofs = min(exp2(MAX_LOD) * 4.0, ofs + ofs))
	{
		vec4 t = min(times.yyyy, vec4(time) + (vec4(0.25, 0.5, 0.75, 1.0) - jitter) * ofs);
		vec4 cos1 = uvs.xyxy + t_phi.xyxy * t.xxyy;
		vec4 cos2 = uvs.xyxy + t_phi.xyxy * t.zzww;
		get_max_horizon_grouped(cos1, cos2, pos, min(MAX_LOD, i * QUALITY), h.x);
	}

	return h;
}

void integrate_slice(
        float iter, vec3 x, vec3 normal, vec2 x_, vec2 noise,
        vec2 max_dir, vec2 pixel_ratio, float pixel_len,
        inout float visibility, inout vec3 bent_normal)
{
	float phi = M_PI * ((noise.r + iter) / aoSamples);

	/* Rotate with random direction to get jittered result. */
	vec2 t_phi = vec2(cos(phi), sin(phi)); /* Screen space direction */

	/* Search maximum horizon angles h1 and h2 */
	vec2 horiz = search_horizon_sweep(t_phi, x, x_, noise.g, max_dir);

	/* (Slide 54) */
	float h1 = -fast_acos(horiz.x);
	float h2 = fast_acos(horiz.y);

	/* Projecting Normal to Plane P defined by t_phi and omega_o */
	vec3 h = vec3(t_phi.y, -t_phi.x, 0.0); /* Normal vector to Integration plane */
	vec3 t = vec3(-t_phi, 0.0);
	vec3 n_proj = normal - h * dot(h, normal);
	float n_proj_len = max(1e-16, length(n_proj));

	/* Clamping thetas (slide 58) */
	float cos_n = clamp(n_proj.z / n_proj_len, -1.0, 1.0);
	float n = sign(dot(n_proj, t)) * fast_acos(cos_n); /* Angle between view vec and normal */
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

void gtao(vec3 normal, vec3 position, vec2 noise, out float visibility
#ifdef USE_BENT_NORMAL
	, out vec3 bent_normal
#endif
	)
{
	vec2 screenres = vec2(textureSize(maxzBuffer, 0)) * 2.0;
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
	vec2 max_dir = max_dist * vec2(ProjectionMatrix[0][0], ProjectionMatrix[1][1]);

	/* Integral over PI */
	visibility = 0.0;
#ifdef USE_BENT_NORMAL
	bent_normal = vec3(0.0);
#else
	vec3 bent_normal = vec3(0.0);
#endif
	for (float i = 0.0; i < MAX_PHI_STEP; i++) {
		if (i >= aoSamples) break;
		integrate_slice(i, x, normal, x_, noise, max_dir, pixel_ratio, pixel_len, visibility, bent_normal);
	}

	/* aoSamples can be 0.0 to temporary disable the effect. */
	visibility = clamp(max(1e-8, visibility) / max(1e-8, aoSamples), 1e-8, 1.0);

#ifdef USE_BENT_NORMAL
	/* The bent normal will show the facet look of the mesh. Try to minimize this. */
	bent_normal = normalize(mix(bent_normal / visibility, normal, visibility * visibility * visibility));
#endif

	/* Scale by user factor */
	visibility = pow(visibility, aoFactor);
}

/* Multibounce approximation base on surface albedo.
 * Page 78 in the .pdf version. */
float gtao_multibounce(float visibility, vec3 albedo)
{
	/* Median luminance. Because Colored multibounce looks bad. */
	float lum = dot(albedo, vec3(0.3333));

	float a =  2.0404 * lum - 0.3324;
	float b = -4.7951 * lum + 0.6417;
	float c =  2.7552 * lum + 0.6903;

	float x = visibility;
	return max(x, ((x * a + b) * x + c) * x);
}

/* Use the right occlusion  */
float occlusion_compute(vec3 N, vec3 vpos, float user_occlusion, vec2 randuv, out vec3 bent_normal)
{
#ifdef USE_AO /* Screen Space Occlusion */

	float computed_occlusion;
	vec3 vnor = mat3(ViewMatrix) * N;

#ifdef USE_BENT_NORMAL
	gtao(vnor, vpos, randuv, computed_occlusion, bent_normal);
	bent_normal = mat3(ViewMatrixInverse) * bent_normal;
#else
	gtao(vnor, vpos, randuv, computed_occlusion);
	bent_normal = N;
#endif
	return min(computed_occlusion, user_occlusion);

#else /* No added Occlusion. */

	bent_normal = N;
	return user_occlusion;

#endif
}
