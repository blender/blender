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
 * limitations under the License
 */

CCL_NAMESPACE_BEGIN

ccl_device void compute_light_pass(KernelGlobals *kg, ShaderData *sd, PathRadiance *L, RNG rng,
                                   const bool is_combined, const bool is_ao, const bool is_sss, int sample)
{
	/* initialize master radiance accumulator */
	kernel_assert(kernel_data.film.use_light_pass);
	path_radiance_init(L, kernel_data.film.use_light_pass);

	PathRadiance L_sample;
	PathState state;
	Ray ray;
	float3 throughput = make_float3(1.0f, 1.0f, 1.0f);
	bool is_sss_sample = is_sss;

	/* init radiance */
	path_radiance_init(&L_sample, kernel_data.film.use_light_pass);

	/* init path state */
	path_state_init(kg, &state, &rng, sample);

	/* evaluate surface shader */
	float rbsdf = path_state_rng_1D(kg, &rng, &state, PRNG_BSDF);
	shader_eval_surface(kg, sd, rbsdf, state.flag, SHADER_CONTEXT_MAIN);

	/* TODO, disable the closures we won't need */

#ifdef __BRANCHED_PATH__
	if(!kernel_data.integrator.branched) {
		/* regular path tracer */
#endif

		/* sample ambient occlusion */
		if(is_combined || is_ao) {
			kernel_path_ao(kg, sd, &L_sample, &state, &rng, throughput);
		}

#ifdef __SUBSURFACE__
		/* sample subsurface scattering */
		if((is_combined || is_sss_sample) && (sd->flag & SD_BSSRDF)) {
			/* when mixing BSSRDF and BSDF closures we should skip BSDF lighting if scattering was successful */
			if (kernel_path_subsurface_scatter(kg, sd, &L_sample, &state, &rng, &ray, &throughput))
				is_sss_sample = true;
		}
#endif

		/* sample light and BSDF */
		if((!is_sss_sample) && (!is_ao)) {

			if(sd->flag & SD_EMISSION) {
				float3 emission = indirect_primitive_emission(kg, sd, 0.0f, state.flag, state.ray_pdf);
				path_radiance_accum_emission(&L_sample, throughput, emission, state.bounce);
			}

			kernel_path_surface_connect_light(kg, &rng, sd, throughput, &state, &L_sample);

			if(kernel_path_surface_bounce(kg, &rng, sd, &throughput, &state, &L_sample, &ray)) {
#ifdef __LAMP_MIS__
				state.ray_t = 0.0f;
#endif
				/* compute indirect light */
				kernel_path_indirect(kg, &rng, ray, throughput, 1, state, &L_sample);

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
		if(is_combined || is_ao) {
			kernel_branched_path_ao(kg, sd, &L_sample, &state, &rng, throughput);
		}

#ifdef __SUBSURFACE__
		/* sample subsurface scattering */
		if((is_combined || is_sss_sample) && (sd->flag & SD_BSSRDF)) {
			/* when mixing BSSRDF and BSDF closures we should skip BSDF lighting if scattering was successful */
			kernel_branched_path_subsurface_scatter(kg, sd, &L_sample, &state, &rng, &ray, throughput);
		}
#endif

		/* sample light and BSDF */
		if((!is_sss_sample) && (!is_ao)) {

			if(sd->flag & SD_EMISSION) {
				float3 emission = indirect_primitive_emission(kg, sd, 0.0f, state.flag, state.ray_pdf);
				path_radiance_accum_emission(&L_sample, throughput, emission, state.bounce);
			}

#if defined(__EMISSION__)
			/* direct light */
			if(kernel_data.integrator.use_direct_light) {
				bool all = kernel_data.integrator.sample_all_lights_direct;
				kernel_branched_path_surface_connect_light(kg, &rng,
					sd, &state, throughput, 1.0f, &L_sample, all);
			}
#endif

			/* indirect light */
			kernel_branched_path_surface_indirect_light(kg, &rng,
				sd, throughput, 1.0f, &state, &L_sample);
		}
	}
#endif

	/* accumulate into master L */
	path_radiance_accum_sample(L, &L_sample, 1);
}

ccl_device bool is_aa_pass(ShaderEvalType type)
{
	switch(type) {
		case SHADER_EVAL_UV:
		case SHADER_EVAL_NORMAL:
			return false;
		default:
			return true;
	}
}

ccl_device bool is_light_pass(ShaderEvalType type)
{
	switch (type) {
		case SHADER_EVAL_AO:
		case SHADER_EVAL_COMBINED:
		case SHADER_EVAL_SHADOW:
		case SHADER_EVAL_DIFFUSE_DIRECT:
		case SHADER_EVAL_GLOSSY_DIRECT:
		case SHADER_EVAL_TRANSMISSION_DIRECT:
		case SHADER_EVAL_SUBSURFACE_DIRECT:
		case SHADER_EVAL_DIFFUSE_INDIRECT:
		case SHADER_EVAL_GLOSSY_INDIRECT:
		case SHADER_EVAL_TRANSMISSION_INDIRECT:
		case SHADER_EVAL_SUBSURFACE_INDIRECT:
			return true;
		default:
			return false;
	}
}

#if 0
ccl_device_inline float bake_clamp_mirror_repeat(float u)
{
	/* use mirror repeat (like opengl texture) so that if the barycentric
	 * coordinate goes past the end of the triangle it is not always clamped
	 * to the same value, gives ugly patterns */
	float fu = floorf(u);
	u = u - fu;

	return (((int)fu) & 1)? 1.0f - u: u;
}
#endif

ccl_device void kernel_bake_evaluate(KernelGlobals *kg, ccl_global uint4 *input, ccl_global float4 *output,
                                     ShaderEvalType type, int i, int offset, int sample)
{
	ShaderData sd;
	uint4 in = input[i * 2];
	uint4 diff = input[i * 2 + 1];

	float3 out;

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
	RNG rng = cmj_hash(offset + i, 0);

#if 0
	uint rng_state = cmj_hash(i, 0);
	float filter_x, filter_y;
	path_rng_init(kg, &rng_state, sample, num_samples, &rng, 0, 0, &filter_x, &filter_y);

	/* subpixel u/v offset */
	if(sample > 0) {
		u = bake_clamp_mirror_repeat(u + dudx*(filter_x - 0.5f) + dudy*(filter_y - 0.5f));
		v = bake_clamp_mirror_repeat(v + dvdx*(filter_x - 0.5f) + dvdy*(filter_y - 0.5f));
	}
#endif

	/* triangle */
	int shader;
	float3 P, Ng;

	triangle_point_normal(kg, object, prim, u, v, &P, &Ng, &shader);

	/* dummy initilizations copied from SHADER_EVAL_DISPLACE */
	float3 I = Ng;
	float t = 0.0f;
	float time = TIME_INVALID;
	int bounce = 0;
	int transparent_bounce = 0;

	/* light passes */
	PathRadiance L;

	shader_setup_from_sample(kg, &sd, P, Ng, I, shader, object, prim, u, v, t, time, bounce, transparent_bounce);
	sd.I = sd.N;

	/* update differentials */
	sd.dP.dx = sd.dPdu * dudx + sd.dPdv * dvdx;
	sd.dP.dy = sd.dPdu * dudy + sd.dPdv * dvdy;
	sd.du.dx = dudx;
	sd.du.dy = dudy;
	sd.dv.dx = dvdx;
	sd.dv.dy = dvdy;

	/* light passes */
	if(is_light_pass(type)) {
		compute_light_pass(kg, &sd, &L, rng,
		                   (type == SHADER_EVAL_COMBINED),
		                   (type == SHADER_EVAL_AO),
		                   (type == SHADER_EVAL_SUBSURFACE_DIRECT ||
		                    type == SHADER_EVAL_SUBSURFACE_INDIRECT),
		                   sample);
	}

	switch (type) {
		/* data passes */
		case SHADER_EVAL_NORMAL:
		{
			/* compression: normal = (2 * color) - 1 */
			out = sd.N * 0.5f + make_float3(0.5f, 0.5f, 0.5f);
			break;
		}
		case SHADER_EVAL_UV:
		{
			out = primitive_uv(kg, &sd);
			break;
		}
		case SHADER_EVAL_DIFFUSE_COLOR:
		{
			shader_eval_surface(kg, &sd, 0.f, 0, SHADER_CONTEXT_MAIN);
			out = shader_bsdf_diffuse(kg, &sd);
			break;
		}
		case SHADER_EVAL_GLOSSY_COLOR:
		{
			shader_eval_surface(kg, &sd, 0.f, 0, SHADER_CONTEXT_MAIN);
			out = shader_bsdf_glossy(kg, &sd);
			break;
		}
		case SHADER_EVAL_TRANSMISSION_COLOR:
		{
			shader_eval_surface(kg, &sd, 0.f, 0, SHADER_CONTEXT_MAIN);
			out = shader_bsdf_transmission(kg, &sd);
			break;
		}
		case SHADER_EVAL_SUBSURFACE_COLOR:
		{
#ifdef __SUBSURFACE__
			shader_eval_surface(kg, &sd, 0.f, 0, SHADER_CONTEXT_MAIN);
			out = shader_bsdf_subsurface(kg, &sd);
#endif
			break;
		}
		case SHADER_EVAL_EMISSION:
		{
			shader_eval_surface(kg, &sd, 0.f, 0, SHADER_CONTEXT_EMISSION);
			out = shader_emissive_eval(kg, &sd);
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
			out = path_radiance_clamp_and_sum(kg, &L);
			break;
		}
		case SHADER_EVAL_SHADOW:
		{
			out = make_float3(L.shadow.x, L.shadow.y, L.shadow.z);
			break;
		}
		case SHADER_EVAL_DIFFUSE_DIRECT:
		{
			shader_eval_surface(kg, &sd, 0.f, 0, SHADER_CONTEXT_MAIN);
			out = safe_divide_color(L.direct_diffuse, shader_bsdf_diffuse(kg, &sd));
			break;
		}
		case SHADER_EVAL_GLOSSY_DIRECT:
		{
			shader_eval_surface(kg, &sd, 0.f, 0, SHADER_CONTEXT_MAIN);
			out = safe_divide_color(L.direct_glossy, shader_bsdf_glossy(kg, &sd));
			break;
		}
		case SHADER_EVAL_TRANSMISSION_DIRECT:
		{
			shader_eval_surface(kg, &sd, 0.f, 0, SHADER_CONTEXT_MAIN);
			out = safe_divide_color(L.direct_transmission, shader_bsdf_transmission(kg, &sd));
			break;
		}
		case SHADER_EVAL_SUBSURFACE_DIRECT:
		{
#ifdef __SUBSURFACE__
			shader_eval_surface(kg, &sd, 0.f, 0, SHADER_CONTEXT_MAIN);
			out = safe_divide_color(L.direct_subsurface, shader_bsdf_subsurface(kg, &sd));
#endif
			break;
		}
		case SHADER_EVAL_DIFFUSE_INDIRECT:
		{
			shader_eval_surface(kg, &sd, 0.f, 0, SHADER_CONTEXT_MAIN);
			out = safe_divide_color(L.indirect_diffuse, shader_bsdf_diffuse(kg, &sd));
			break;
		}
		case SHADER_EVAL_GLOSSY_INDIRECT:
		{
			shader_eval_surface(kg, &sd, 0.f, 0, SHADER_CONTEXT_MAIN);
			out = safe_divide_color(L.indirect_glossy, shader_bsdf_glossy(kg, &sd));
			break;
		}
		case SHADER_EVAL_TRANSMISSION_INDIRECT:
		{
			shader_eval_surface(kg, &sd, 0.f, 0, SHADER_CONTEXT_MAIN);
			out = safe_divide_color(L.indirect_transmission, shader_bsdf_transmission(kg, &sd));
			break;
		}
		case SHADER_EVAL_SUBSURFACE_INDIRECT:
		{
#ifdef __SUBSURFACE__
			shader_eval_surface(kg, &sd, 0.f, 0, SHADER_CONTEXT_MAIN);
			out = safe_divide_color(L.indirect_subsurface, shader_bsdf_subsurface(kg, &sd));
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
			shader_setup_from_background(kg, &sd, &ray, 0, 0);

			/* evaluate */
			int flag = 0; /* we can't know which type of BSDF this is for */
			out = shader_eval_background(kg, &sd, flag, SHADER_CONTEXT_MAIN);
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
	float output_fac = is_aa_pass(type)? 1.0f/num_samples: 1.0f;

	if(sample == 0)
		output[i] = make_float4(out.x, out.y, out.z, 1.0f) * output_fac;
	else
		output[i] += make_float4(out.x, out.y, out.z, 1.0f) * output_fac;
}

ccl_device void kernel_shader_evaluate(KernelGlobals *kg, ccl_global uint4 *input, ccl_global float4 *output, ShaderEvalType type, int i, int sample)
{
	ShaderData sd;
	uint4 in = input[i];
	float3 out;

	if(type == SHADER_EVAL_DISPLACE) {
		/* setup shader data */
		int object = in.x;
		int prim = in.y;
		float u = __uint_as_float(in.z);
		float v = __uint_as_float(in.w);

		shader_setup_from_displace(kg, &sd, object, prim, u, v);

		/* evaluate */
		float3 P = sd.P;
		shader_eval_displacement(kg, &sd, SHADER_CONTEXT_MAIN);
		out = sd.P - P;
	}
	else { // SHADER_EVAL_BACKGROUND
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
		shader_setup_from_background(kg, &sd, &ray, 0, 0);

		/* evaluate */
		int flag = 0; /* we can't know which type of BSDF this is for */
		out = shader_eval_background(kg, &sd, flag, SHADER_CONTEXT_MAIN);
	}
	
	/* write output */
	if(sample == 0)
		output[i] = make_float4(out.x, out.y, out.z, 0.0f);
	else
		output[i] += make_float4(out.x, out.y, out.z, 0.0f);
}

CCL_NAMESPACE_END

