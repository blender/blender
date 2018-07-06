/*
 * Copyright 2011-2013 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

CCL_NAMESPACE_BEGIN

#ifdef __BAKING__

ccl_device_inline void compute_light_pass(KernelGlobals *kg,
                                          ShaderData *sd,
                                          PathRadiance *L,
                                          uint rng_hash,
                                          int pass_filter,
                                          int sample)
{
	/* initialize master radiance accumulator */
	kernel_assert(kernel_data.film.use_light_pass);
	path_radiance_init(L, kernel_data.film.use_light_pass);

	PathRadiance L_sample;
	PathState state;
	Ray ray;
	float3 throughput = make_float3(1.0f, 1.0f, 1.0f);

	/* emission and indirect shader data memory used by various functions */
	ShaderData emission_sd, indirect_sd;

	ray.P = sd->P + sd->Ng;
	ray.D = -sd->Ng;
	ray.t = FLT_MAX;
#ifdef __CAMERA_MOTION__
	ray.time = 0.5f;
#endif

	/* init radiance */
	path_radiance_init(&L_sample, kernel_data.film.use_light_pass);

	/* init path state */
	path_state_init(kg, &emission_sd, &state, rng_hash, sample, NULL);

	/* evaluate surface shader */
	shader_eval_surface(kg, sd, &state, state.flag);

	/* TODO, disable more closures we don't need besides transparent */
	shader_bsdf_disable_transparency(kg, sd);

#ifdef __BRANCHED_PATH__
	if(!kernel_data.integrator.branched) {
		/* regular path tracer */
#endif

		/* sample ambient occlusion */
		if(pass_filter & BAKE_FILTER_AO) {
			kernel_path_ao(kg, sd, &emission_sd, &L_sample, &state, throughput, shader_bsdf_alpha(kg, sd));
		}

		/* sample emission */
		if((pass_filter & BAKE_FILTER_EMISSION) && (sd->flag & SD_EMISSION)) {
			float3 emission = indirect_primitive_emission(kg, sd, 0.0f, state.flag, state.ray_pdf);
			path_radiance_accum_emission(&L_sample, &state, throughput, emission);
		}

		bool is_sss_sample = false;

#ifdef __SUBSURFACE__
		/* sample subsurface scattering */
		if((pass_filter & BAKE_FILTER_SUBSURFACE) && (sd->flag & SD_BSSRDF)) {
			/* when mixing BSSRDF and BSDF closures we should skip BSDF lighting if scattering was successful */
			SubsurfaceIndirectRays ss_indirect;
			kernel_path_subsurface_init_indirect(&ss_indirect);
			if(kernel_path_subsurface_scatter(kg,
			                                  sd,
			                                  &emission_sd,
			                                  &L_sample,
			                                  &state,
			                                  &ray,
			                                  &throughput,
			                                  &ss_indirect))
			{
				while(ss_indirect.num_rays) {
					kernel_path_subsurface_setup_indirect(kg,
					                                      &ss_indirect,
					                                      &state,
					                                      &ray,
					                                      &L_sample,
					                                      &throughput);
					kernel_path_indirect(kg,
					                     &indirect_sd,
					                     &emission_sd,
					                     &ray,
					                     throughput,
					                     &state,
					                     &L_sample);
				}
				is_sss_sample = true;
			}
		}
#endif

		/* sample light and BSDF */
		if(!is_sss_sample && (pass_filter & (BAKE_FILTER_DIRECT | BAKE_FILTER_INDIRECT))) {
			kernel_path_surface_connect_light(kg, sd, &emission_sd, throughput, &state, &L_sample);

			if(kernel_path_surface_bounce(kg, sd, &throughput, &state, &L_sample.state, &ray)) {
#ifdef __LAMP_MIS__
				state.ray_t = 0.0f;
#endif
				/* compute indirect light */
				kernel_path_indirect(kg, &indirect_sd, &emission_sd, &ray, throughput, &state, &L_sample);

				/* sum and reset indirect light pass variables for the next samples */
				path_radiance_sum_indirect(&L_sample);
				path_radiance_reset_indirect(&L_sample);
			}
		}
#ifdef __BRANCHED_PATH__
	}
	else {
		/* branched path tracer */

		/* sample ambient occlusion */
		if(pass_filter & BAKE_FILTER_AO) {
			kernel_branched_path_ao(kg, sd, &emission_sd, &L_sample, &state, throughput);
		}

		/* sample emission */
		if((pass_filter & BAKE_FILTER_EMISSION) && (sd->flag & SD_EMISSION)) {
			float3 emission = indirect_primitive_emission(kg, sd, 0.0f, state.flag, state.ray_pdf);
			path_radiance_accum_emission(&L_sample, &state, throughput, emission);
		}

#ifdef __SUBSURFACE__
		/* sample subsurface scattering */
		if((pass_filter & BAKE_FILTER_SUBSURFACE) && (sd->flag & SD_BSSRDF)) {
			/* when mixing BSSRDF and BSDF closures we should skip BSDF lighting if scattering was successful */
			kernel_branched_path_subsurface_scatter(kg, sd, &indirect_sd,
				&emission_sd, &L_sample, &state, &ray, throughput);
		}
#endif

		/* sample light and BSDF */
		if(pass_filter & (BAKE_FILTER_DIRECT | BAKE_FILTER_INDIRECT)) {
#if defined(__EMISSION__)
			/* direct light */
			if(kernel_data.integrator.use_direct_light) {
				int all = kernel_data.integrator.sample_all_lights_direct;
				kernel_branched_path_surface_connect_light(kg,
					sd, &emission_sd, &state, throughput, 1.0f, &L_sample, all);
			}
#endif

			/* indirect light */
			kernel_branched_path_surface_indirect_light(kg,
				sd, &indirect_sd, &emission_sd, throughput, 1.0f, &state, &L_sample);
		}
	}
#endif

	/* accumulate into master L */
	path_radiance_accum_sample(L, &L_sample);
}

/* this helps with AA but it's not the real solution as it does not AA the geometry
 *  but it's better than nothing, thus committed */
ccl_device_inline float bake_clamp_mirror_repeat(float u, float max)
{
	/* use mirror repeat (like opengl texture) so that if the barycentric
	 * coordinate goes past the end of the triangle it is not always clamped
	 * to the same value, gives ugly patterns */
	u /= max;
	float fu = floorf(u);
	u = u - fu;

	return ((((int)fu) & 1)? 1.0f - u: u) * max;
}

ccl_device_inline float3 kernel_bake_shader_bsdf(KernelGlobals *kg,
                                                 ShaderData *sd,
                                                 const ShaderEvalType type)
{
	switch(type) {
		case SHADER_EVAL_DIFFUSE:
			return shader_bsdf_diffuse(kg, sd);
		case SHADER_EVAL_GLOSSY:
			return shader_bsdf_glossy(kg, sd);
		case SHADER_EVAL_TRANSMISSION:
			return shader_bsdf_transmission(kg, sd);
#ifdef __SUBSURFACE__
		case SHADER_EVAL_SUBSURFACE:
			return shader_bsdf_subsurface(kg, sd);
#endif
		default:
			kernel_assert(!"Unknown bake type passed to BSDF evaluate");
			return make_float3(0.0f, 0.0f, 0.0f);
	}
}

ccl_device float3 kernel_bake_evaluate_direct_indirect(KernelGlobals *kg,
                                                       ShaderData *sd,
                                                       PathState *state,
                                                       float3 direct,
                                                       float3 indirect,
                                                       const ShaderEvalType type,
                                                       const int pass_filter)
{
	float3 color;
	const bool is_color = (pass_filter & BAKE_FILTER_COLOR) != 0;
	const bool is_direct = (pass_filter & BAKE_FILTER_DIRECT) != 0;
	const bool is_indirect = (pass_filter & BAKE_FILTER_INDIRECT) != 0;
	float3 out = make_float3(0.0f, 0.0f, 0.0f);

	if(is_color) {
		if(is_direct || is_indirect) {
			/* Leave direct and diffuse channel colored. */
			color = make_float3(1.0f, 1.0f, 1.0f);
		}
		else {
			/* surface color of the pass only */
			shader_eval_surface(kg, sd, state, 0);
			return kernel_bake_shader_bsdf(kg, sd, type);
		}
	}
	else {
		shader_eval_surface(kg, sd, state, 0);
		color = kernel_bake_shader_bsdf(kg, sd, type);
	}

	if(is_direct) {
		out += safe_divide_even_color(direct, color);
	}

	if(is_indirect) {
		out += safe_divide_even_color(indirect, color);
	}

	return out;
}

ccl_device void kernel_bake_evaluate(KernelGlobals *kg, ccl_global uint4 *input, ccl_global float4 *output,
                                     ShaderEvalType type, int pass_filter, int i, int offset, int sample)
{
	ShaderData sd;
	PathState state = {0};
	uint4 in = input[i * 2];
	uint4 diff = input[i * 2 + 1];

	float3 out = make_float3(0.0f, 0.0f, 0.0f);

	int object = in.x;
	int prim = in.y;

	if(prim == -1)
		return;

	float u = __uint_as_float(in.z);
	float v = __uint_as_float(in.w);

	float dudx = __uint_as_float(diff.x);
	float dudy = __uint_as_float(diff.y);
	float dvdx = __uint_as_float(diff.z);
	float dvdy = __uint_as_float(diff.w);

	int num_samples = kernel_data.integrator.aa_samples;

	/* random number generator */
	uint rng_hash = cmj_hash(offset + i, kernel_data.integrator.seed);

	float filter_x, filter_y;
	if(sample == 0) {
		filter_x = filter_y = 0.5f;
	}
	else {
		path_rng_2D(kg, rng_hash, sample, num_samples, PRNG_FILTER_U, &filter_x, &filter_y);
	}

	/* subpixel u/v offset */
	if(sample > 0) {
		u = bake_clamp_mirror_repeat(u + dudx*(filter_x - 0.5f) + dudy*(filter_y - 0.5f), 1.0f);
		v = bake_clamp_mirror_repeat(v + dvdx*(filter_x - 0.5f) + dvdy*(filter_y - 0.5f), 1.0f - u);
	}

	/* triangle */
	int shader;
	float3 P, Ng;

	triangle_point_normal(kg, object, prim, u, v, &P, &Ng, &shader);

	/* light passes */
	PathRadiance L;

	shader_setup_from_sample(kg, &sd,
	                         P, Ng, Ng,
	                         shader, object, prim,
	                         u, v, 1.0f, 0.5f,
	                         !(kernel_tex_fetch(__object_flag, object) & SD_OBJECT_TRANSFORM_APPLIED),
	                         LAMP_NONE);
	sd.I = sd.N;

	/* update differentials */
	sd.dP.dx = sd.dPdu * dudx + sd.dPdv * dvdx;
	sd.dP.dy = sd.dPdu * dudy + sd.dPdv * dvdy;
	sd.du.dx = dudx;
	sd.du.dy = dudy;
	sd.dv.dx = dvdx;
	sd.dv.dy = dvdy;

	/* set RNG state for shaders that use sampling */
	state.rng_hash = rng_hash;
	state.rng_offset = 0;
	state.sample = sample;
	state.num_samples = num_samples;
	state.min_ray_pdf = FLT_MAX;

	/* light passes if we need more than color */
	if(pass_filter & ~BAKE_FILTER_COLOR)
		compute_light_pass(kg, &sd, &L, rng_hash, pass_filter, sample);

	switch(type) {
		/* data passes */
		case SHADER_EVAL_NORMAL:
		case SHADER_EVAL_ROUGHNESS:
		case SHADER_EVAL_EMISSION:
		{
			if(type != SHADER_EVAL_NORMAL || (sd.flag & SD_HAS_BUMP)) {
				int path_flag = (type == SHADER_EVAL_EMISSION) ? PATH_RAY_EMISSION : 0;
				shader_eval_surface(kg, &sd, &state, path_flag);
			}

			if(type == SHADER_EVAL_NORMAL) {
				float3 N = sd.N;
				if(sd.flag & SD_HAS_BUMP) {
					N = shader_bsdf_average_normal(kg, &sd);
				}

				/* encoding: normal = (2 * color) - 1 */
				out = N * 0.5f + make_float3(0.5f, 0.5f, 0.5f);
			}
			else if(type == SHADER_EVAL_ROUGHNESS) {
				float roughness = shader_bsdf_average_roughness(&sd);
				out = make_float3(roughness, roughness, roughness);
			}
			else {
				out = shader_emissive_eval(kg, &sd);
			}
			break;
		}
		case SHADER_EVAL_UV:
		{
			out = primitive_uv(kg, &sd);
			break;
		}
#ifdef __PASSES__
		/* light passes */
		case SHADER_EVAL_AO:
		{
			out = L.ao;
			break;
		}
		case SHADER_EVAL_COMBINED:
		{
			if((pass_filter & BAKE_FILTER_COMBINED) == BAKE_FILTER_COMBINED) {
				float alpha;
				out = path_radiance_clamp_and_sum(kg, &L, &alpha);
				break;
			}

			if((pass_filter & BAKE_FILTER_DIFFUSE_DIRECT) == BAKE_FILTER_DIFFUSE_DIRECT)
				out += L.direct_diffuse;
			if((pass_filter & BAKE_FILTER_DIFFUSE_INDIRECT) == BAKE_FILTER_DIFFUSE_INDIRECT)
				out += L.indirect_diffuse;

			if((pass_filter & BAKE_FILTER_GLOSSY_DIRECT) == BAKE_FILTER_GLOSSY_DIRECT)
				out += L.direct_glossy;
			if((pass_filter & BAKE_FILTER_GLOSSY_INDIRECT) == BAKE_FILTER_GLOSSY_INDIRECT)
				out += L.indirect_glossy;

			if((pass_filter & BAKE_FILTER_TRANSMISSION_DIRECT) == BAKE_FILTER_TRANSMISSION_DIRECT)
				out += L.direct_transmission;
			if((pass_filter & BAKE_FILTER_TRANSMISSION_INDIRECT) == BAKE_FILTER_TRANSMISSION_INDIRECT)
				out += L.indirect_transmission;

			if((pass_filter & BAKE_FILTER_SUBSURFACE_DIRECT) == BAKE_FILTER_SUBSURFACE_DIRECT)
				out += L.direct_subsurface;
			if((pass_filter & BAKE_FILTER_SUBSURFACE_INDIRECT) == BAKE_FILTER_SUBSURFACE_INDIRECT)
				out += L.indirect_subsurface;

			if((pass_filter & BAKE_FILTER_EMISSION) != 0)
				out += L.emission;

			break;
		}
		case SHADER_EVAL_SHADOW:
		{
			out = make_float3(L.shadow.x, L.shadow.y, L.shadow.z);
			break;
		}
		case SHADER_EVAL_DIFFUSE:
		{
			out = kernel_bake_evaluate_direct_indirect(kg,
			                                           &sd,
			                                           &state,
			                                           L.direct_diffuse,
			                                           L.indirect_diffuse,
			                                           type,
			                                           pass_filter);
			break;
		}
		case SHADER_EVAL_GLOSSY:
		{
			out = kernel_bake_evaluate_direct_indirect(kg,
			                                           &sd,
			                                           &state,
			                                           L.direct_glossy,
			                                           L.indirect_glossy,
			                                           type,
			                                           pass_filter);
			break;
		}
		case SHADER_EVAL_TRANSMISSION:
		{
			out = kernel_bake_evaluate_direct_indirect(kg,
			                                           &sd,
			                                           &state,
			                                           L.direct_transmission,
			                                           L.indirect_transmission,
			                                           type,
			                                           pass_filter);
			break;
		}
		case SHADER_EVAL_SUBSURFACE:
		{
#ifdef __SUBSURFACE__
			out = kernel_bake_evaluate_direct_indirect(kg,
			                                           &sd,
			                                           &state,
			                                           L.direct_subsurface,
			                                           L.indirect_subsurface,
			                                           type,
			                                           pass_filter);
#endif
			break;
		}
#endif

		/* extra */
		case SHADER_EVAL_ENVIRONMENT:
		{
			/* setup ray */
			Ray ray;

			ray.P = make_float3(0.0f, 0.0f, 0.0f);
			ray.D = normalize(P);
			ray.t = 0.0f;
#ifdef __CAMERA_MOTION__
			ray.time = 0.5f;
#endif

#ifdef __RAY_DIFFERENTIALS__
			ray.dD = differential3_zero();
			ray.dP = differential3_zero();
#endif

			/* setup shader data */
			shader_setup_from_background(kg, &sd, &ray);

			/* evaluate */
			int flag = 0; /* we can't know which type of BSDF this is for */
			out = shader_eval_background(kg, &sd, &state, flag);
			break;
		}
		default:
		{
			/* no real shader, returning the position of the verts for debugging */
			out = normalize(P);
			break;
		}
	}

	/* write output */
	const float output_fac = 1.0f/num_samples;
	const float4 scaled_result = make_float4(out.x, out.y, out.z, 1.0f) * output_fac;

	output[i] = (sample == 0)? scaled_result: output[i] + scaled_result;
}

#endif  /* __BAKING__ */

ccl_device void kernel_displace_evaluate(KernelGlobals *kg,
                                         ccl_global uint4 *input,
                                         ccl_global float4 *output,
                                         int i)
{
	ShaderData sd;
	PathState state = {0};
	uint4 in = input[i];

	/* setup shader data */
	int object = in.x;
	int prim = in.y;
	float u = __uint_as_float(in.z);
	float v = __uint_as_float(in.w);

	shader_setup_from_displace(kg, &sd, object, prim, u, v);

	/* evaluate */
	float3 P = sd.P;
	shader_eval_displacement(kg, &sd, &state);
	float3 D = sd.P - P;

	object_inverse_dir_transform(kg, &sd, &D);

	/* write output */
	output[i] += make_float4(D.x, D.y, D.z, 0.0f);
}

ccl_device void kernel_background_evaluate(KernelGlobals *kg,
                                           ccl_global uint4 *input,
                                           ccl_global float4 *output,
                                           int i)
{
	ShaderData sd;
	PathState state = {0};
	uint4 in = input[i];

	/* setup ray */
	Ray ray;
	float u = __uint_as_float(in.x);
	float v = __uint_as_float(in.y);

	ray.P = make_float3(0.0f, 0.0f, 0.0f);
	ray.D = equirectangular_to_direction(u, v);
	ray.t = 0.0f;
#ifdef __CAMERA_MOTION__
	ray.time = 0.5f;
#endif

#ifdef __RAY_DIFFERENTIALS__
	ray.dD = differential3_zero();
	ray.dP = differential3_zero();
#endif

	/* setup shader data */
	shader_setup_from_background(kg, &sd, &ray);

	/* evaluate */
	int flag = 0; /* we can't know which type of BSDF this is for */
	float3 color = shader_eval_background(kg, &sd, &state, flag);

	/* write output */
	output[i] += make_float4(color.x, color.y, color.z, 0.0f);
}

CCL_NAMESPACE_END
