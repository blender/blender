
/* Based on Practical Realtime Strategies for Accurate Indirect Occlusion
 * http://blog.selfshadow.com/publications/s2016-shading-course/activision/s2016_pbs_activision_occlusion.pdf
 * http://blog.selfshadow.com/publications/s2016-shading-course/activision/s2016_pbs_activision_occlusion.pptx */

#if defined(MESH_SHADER)
# if !defined(USE_ALPHA_HASH)
#  if !defined(USE_ALPHA_CLIP)
#   if !defined(SHADOW_SHADER)
#    if !defined(USE_MULTIPLY)
#     if !defined(USE_ALPHA_BLEND)
#      define ENABLE_DEFERED_AO
#     endif
#    endif
#   endif
#  endif
# endif
#endif

#ifndef ENABLE_DEFERED_AO
# if defined(STEP_RESOLVE)
#  define ENABLE_DEFERED_AO
# endif
#endif

#define MAX_PHI_STEP 32
#define MAX_SEARCH_ITER 32
#define MAX_LOD 6.0

#ifndef UTIL_TEX
#define UTIL_TEX
uniform sampler2DArray utilTex;
#endif /* UTIL_TEX */

uniform vec4 aoParameters[2];
uniform sampler2DArray horizonBuffer;

/* Cannot use textureSize(horizonBuffer) when rendering to it */
uniform ivec2 aoHorizonTexSize;

#define aoDistance   aoParameters[0].x
#define aoSamples    aoParameters[0].y
#define aoFactor     aoParameters[0].z
#define aoInvSamples aoParameters[0].w

#define aoOffset     aoParameters[1].x /* UNUSED */
#define aoBounceFac  aoParameters[1].y
#define aoQuality    aoParameters[1].z
#define aoSettings   aoParameters[1].w

#define USE_AO            1
#define USE_BENT_NORMAL   2
#define USE_DENOISE       4

vec2 pack_horizons(vec2 v) { return v * 0.5 + 0.5; }
vec2 unpack_horizons(vec2 v) { return v * 2.0 - 1.0; }

/* Returns the texel coordinate in horizonBuffer
 * for a given fullscreen coord */
ivec2 get_hr_co(ivec2 fs_co)
{
	bvec2 quarter = notEqual(fs_co & ivec2(1), ivec2(0));

	ivec2 hr_co = fs_co / 2;
	hr_co += ivec2(quarter) * (aoHorizonTexSize / 2);

	return hr_co;
}

/* Returns the texel coordinate in fullscreen (depthBuffer)
 * for a given horizonBuffer coord */
ivec2 get_fs_co(ivec2 hr_co)
{
	hr_co *= 2;
	bvec2 quarter = greaterThanEqual(hr_co, aoHorizonTexSize);

	hr_co -= ivec2(quarter) * (aoHorizonTexSize - 1);

	return hr_co;
}

/* Returns the phi angle in horizonBuffer
 * for a given horizonBuffer coord */
float get_phi(ivec2 hr_co, ivec2 fs_co, float sample)
{
	bvec2 quarter = greaterThanEqual(hr_co, aoHorizonTexSize / 2);
	ivec2 tex_co = ((int(aoSettings) & USE_DENOISE) != 0) ? hr_co - ivec2(quarter) * (aoHorizonTexSize / 2) : fs_co;
	float blue_noise = texture(utilTex, vec3((vec2(tex_co) + 0.5) / LUT_SIZE, 2.0)).r;

	float phi = sample * aoInvSamples;

	if ((int(aoSettings) & USE_DENOISE) != 0) {
		/* Interleaved jitter for spatial 2x2 denoising */
		phi += 0.25 * aoInvSamples * (float(quarter.x) + 2.0 * float(quarter.y));
		blue_noise *= 0.25;
	}
	/* Blue noise is scaled to cover the rest of the range. */
	phi += aoInvSamples * blue_noise;
	phi *= M_PI;

	return phi;
}

/* Returns direction jittered offset for a given fullscreen coord */
float get_offset(ivec2 fs_co, float sample)
{
	float offset = sample * aoInvSamples;

	/* Interleaved jitter for spatial 2x2 denoising */
	offset += 0.25 * dot(vec2(1.0), vec2(fs_co & 1));
	offset += texture(utilTex, vec3((vec2(fs_co / 2) + 0.5 + 16.0) / LUT_SIZE, 2.0)).r;
	return offset;
}

/* Returns maximum screen distance an AO ray can travel for a given view depth */
vec2 get_max_dir(float view_depth)
{
	float homcco = ProjectionMatrix[2][3] * view_depth + ProjectionMatrix[3][3];
	float max_dist = aoDistance / homcco;
	return vec2(ProjectionMatrix[0][0], ProjectionMatrix[1][1]) * max_dist;
}

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

vec2 search_horizon_sweep(float phi, vec3 pos, vec2 uvs, float jitter, vec2 max_dir)
{
	vec2 t_phi = vec2(cos(phi), sin(phi)); /* Screen space direction */

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
		 i < MAX_SEARCH_ITER && time > times.x;
		 i++, time -= ofs, ofs = min(exp2(MAX_LOD) * 4.0, ofs + ofs * aoQuality))
	{
		vec4 t = max(times.xxxx, vec4(time) - (vec4(0.25, 0.5, 0.75, 1.0) - jitter) * ofs);
		vec4 cos1 = uvs.xyxy + t_phi.xyxy * t.xxyy;
		vec4 cos2 = uvs.xyxy + t_phi.xyxy * t.zzww;
		float lod = min(MAX_LOD, max(i - jitter * 4.0, 0.0) * aoQuality);
		get_max_horizon_grouped(cos1, cos2, pos, lod, h.y);
	}

	for (float i = 0.0, ofs = 4.0, time = 1.0;
		 i < MAX_SEARCH_ITER && time < times.y;
		 i++, time += ofs, ofs = min(exp2(MAX_LOD) * 4.0, ofs + ofs * aoQuality))
	{
		vec4 t = min(times.yyyy, vec4(time) + (vec4(0.25, 0.5, 0.75, 1.0) - jitter) * ofs);
		vec4 cos1 = uvs.xyxy + t_phi.xyxy * t.xxyy;
		vec4 cos2 = uvs.xyxy + t_phi.xyxy * t.zzww;
		float lod = min(MAX_LOD, max(i - jitter * 4.0, 0.0) * aoQuality);
		get_max_horizon_grouped(cos1, cos2, pos, lod, h.x);
	}

	return h;
}

void integrate_slice(vec3 normal, float phi, vec2 horizons, inout float visibility, inout vec3 bent_normal)
{
	/* TODO OPTI Could be precomputed. */
	vec2 t_phi = vec2(cos(phi), sin(phi)); /* Screen space direction */

	/* Projecting Normal to Plane P defined by t_phi and omega_o */
	vec3 np = vec3(t_phi.y, -t_phi.x, 0.0); /* Normal vector to Integration plane */
	vec3 t = vec3(-t_phi, 0.0);
	vec3 n_proj = normal - np * dot(np, normal);
	float n_proj_len = max(1e-16, length(n_proj));

	float cos_n = clamp(n_proj.z / n_proj_len, -1.0, 1.0);
	float n = sign(dot(n_proj, t)) * fast_acos(cos_n); /* Angle between view vec and normal */

	/* (Slide 54) */
	vec2 h = fast_acos(horizons);
	h.x = -h.x;

	/* Clamping thetas (slide 58) */
	h.x = n + max(h.x - n, -M_PI_2);
	h.y = n + min(h.y - n, M_PI_2);

	/* Solving inner integral */
	vec2 h_2 = 2.0 * h;
	vec2 vd = -cos(h_2 - n) + cos_n + h_2 * sin(n);
	float vis = (vd.x + vd.y) * 0.25 * n_proj_len;

	visibility += vis;

	/* Finding Bent normal */
	float b_angle = (h.x + h.y) * 0.5;
	/* The 0.5 factor below is here to equilibrate the accumulated vectors.
	 * (sin(b_angle) * -t_phi) will accumulate to (phi_step * result_nor.xy * 0.5).
	 * (cos(b_angle) * 0.5) will accumulate to (phi_step * result_nor.z * 0.5). */
	bent_normal += vec3(sin(b_angle) * -t_phi, cos(b_angle) * 0.5);
}

void denoise_ao(vec3 normal, float frag_depth, inout float visibility, inout vec3 bent_normal)
{
	vec2 d_sign = vec2(ivec2(gl_FragCoord.xy) & 1) - 0.5;

	if ((int(aoSettings) & USE_DENOISE) == 0) {
		d_sign *= 0.0;
	}

	/* 2x2 Bilateral Filter using derivatives. */
	vec2 n_step = step(-0.2, -abs(vec2(length(dFdx(normal)), length(dFdy(normal)))));
	vec2 z_step = step(-0.1, -abs(vec2(dFdx(frag_depth), dFdy(frag_depth))));

	visibility -= dFdx(visibility) * d_sign.x * z_step.x * n_step.x;
	visibility -= dFdy(visibility) * d_sign.y * z_step.y * n_step.y;

	bent_normal -= dFdx(bent_normal) * d_sign.x * z_step.x * n_step.x;
	bent_normal -= dFdy(bent_normal) * d_sign.y * z_step.y * n_step.y;
}

void gtao_deferred(vec3 normal, vec3 position, float frag_depth, out float visibility, out vec3 bent_normal)
{
	vec2 uvs = get_uvs_from_view(position);

	vec4 texel_size = vec4(-1.0, -1.0, 1.0, 1.0) / vec2(textureSize(depthBuffer, 0)).xyxy;

	ivec2 fs_co = ivec2(gl_FragCoord.xy);
	ivec2 hr_co = get_hr_co(fs_co);

	bent_normal = vec3(0.0);
	visibility = 0.0;

	for (float i = 0.0; i < MAX_PHI_STEP; i++) {
		if (i >= aoSamples) break;

		vec2 horiz = unpack_horizons(texelFetch(horizonBuffer, ivec3(hr_co, int(i)), 0).rg);
		float phi = get_phi(hr_co, fs_co, i);

		integrate_slice(normal, phi, horiz.xy, visibility, bent_normal);
	}

	visibility *= aoInvSamples;
	bent_normal = normalize(bent_normal);
}

void gtao(vec3 normal, vec3 position, vec2 noise, out float visibility, out vec3 bent_normal)
{
	vec2 uvs = get_uvs_from_view(position);

	float homcco = ProjectionMatrix[2][3] * position.z + ProjectionMatrix[3][3];
	float max_dist = aoDistance / homcco; /* Search distance */
	vec2 max_dir = max_dist * vec2(ProjectionMatrix[0][0], ProjectionMatrix[1][1]);

	bent_normal = vec3(0.0);
	visibility = 0.0;

	for (float i = 0.0; i < MAX_PHI_STEP; i++) {
		if (i >= aoSamples) break;

		float phi = M_PI * (i + noise.x) * aoInvSamples;
		vec2 horizons = search_horizon_sweep(phi, position, uvs, noise.g, max_dir);

		integrate_slice(normal, phi, horizons, visibility, bent_normal);
	}

	visibility *= aoInvSamples;
	bent_normal = normalize(bent_normal);
}

/* Multibounce approximation base on surface albedo.
 * Page 78 in the .pdf version. */
float gtao_multibounce(float visibility, vec3 albedo)
{
	if (aoBounceFac == 0.0) return visibility;

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
#ifndef USE_REFRACTION
	if ((int(aoSettings) & USE_AO) > 0) {
		float visibility;
		vec3 vnor = mat3(ViewMatrix) * N;

#ifdef ENABLE_DEFERED_AO
		gtao_deferred(vnor, vpos, gl_FragCoord.z, visibility, bent_normal);
#else
		gtao(vnor, vpos, randuv, visibility, bent_normal);
#endif
		denoise_ao(vnor, gl_FragCoord.z, visibility, bent_normal);

		/* Prevent some problems down the road. */
		visibility = max(1e-3, visibility);

		if ((int(aoSettings) & USE_BENT_NORMAL) != 0) {
			/* The bent normal will show the facet look of the mesh. Try to minimize this. */
			float mix_fac = visibility * visibility;
			bent_normal = normalize(mix(bent_normal, vnor, mix_fac));

			bent_normal = transform_direction(ViewMatrixInverse, bent_normal);
		}
		else {
			bent_normal = N;
		}

		/* Scale by user factor */
		visibility = pow(visibility, aoFactor);

		return min(visibility, user_occlusion);
	}
#endif

	bent_normal = N;
	return user_occlusion;
}
