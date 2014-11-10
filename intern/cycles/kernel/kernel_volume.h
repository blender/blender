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

/* Events for probalistic scattering */

typedef enum VolumeIntegrateResult {
	VOLUME_PATH_SCATTERED = 0,
	VOLUME_PATH_ATTENUATED = 1,
	VOLUME_PATH_MISSED = 2
} VolumeIntegrateResult;

/* Volume shader properties
 *
 * extinction coefficient = absorption coefficient + scattering coefficient
 * sigma_t = sigma_a + sigma_s */

typedef struct VolumeShaderCoefficients {
	float3 sigma_a;
	float3 sigma_s;
	float3 emission;
} VolumeShaderCoefficients;

/* evaluate shader to get extinction coefficient at P */
ccl_device bool volume_shader_extinction_sample(KernelGlobals *kg, ShaderData *sd, PathState *state, float3 P, float3 *extinction)
{
	sd->P = P;
	shader_eval_volume(kg, sd, state->volume_stack, PATH_RAY_SHADOW, SHADER_CONTEXT_SHADOW);

	if(!(sd->flag & (SD_ABSORPTION|SD_SCATTER)))
		return false;

	float3 sigma_t = make_float3(0.0f, 0.0f, 0.0f);

	for(int i = 0; i < sd->num_closure; i++) {
		const ShaderClosure *sc = &sd->closure[i];

		if(CLOSURE_IS_VOLUME(sc->type))
			sigma_t += sc->weight;
	}

	*extinction = sigma_t;
	return true;
}

/* evaluate shader to get absorption, scattering and emission at P */
ccl_device bool volume_shader_sample(KernelGlobals *kg, ShaderData *sd, PathState *state, float3 P, VolumeShaderCoefficients *coeff)
{
	sd->P = P;
	shader_eval_volume(kg, sd, state->volume_stack, state->flag, SHADER_CONTEXT_VOLUME);

	if(!(sd->flag & (SD_ABSORPTION|SD_SCATTER|SD_EMISSION)))
		return false;
	
	coeff->sigma_a = make_float3(0.0f, 0.0f, 0.0f);
	coeff->sigma_s = make_float3(0.0f, 0.0f, 0.0f);
	coeff->emission = make_float3(0.0f, 0.0f, 0.0f);

	for(int i = 0; i < sd->num_closure; i++) {
		const ShaderClosure *sc = &sd->closure[i];

		if(sc->type == CLOSURE_VOLUME_ABSORPTION_ID)
			coeff->sigma_a += sc->weight;
		else if(sc->type == CLOSURE_EMISSION_ID)
			coeff->emission += sc->weight;
		else if(CLOSURE_IS_VOLUME(sc->type))
			coeff->sigma_s += sc->weight;
	}

	/* when at the max number of bounces, treat scattering as absorption */
	if(sd->flag & SD_SCATTER) {
		if(state->volume_bounce >= kernel_data.integrator.max_volume_bounce) {
			coeff->sigma_a += coeff->sigma_s;
			coeff->sigma_s = make_float3(0.0f, 0.0f, 0.0f);
			sd->flag &= ~SD_SCATTER;
			sd->flag |= SD_ABSORPTION;
		}
	}

	return true;
}

ccl_device float3 volume_color_transmittance(float3 sigma, float t)
{
	return make_float3(expf(-sigma.x * t), expf(-sigma.y * t), expf(-sigma.z * t));
}

ccl_device float kernel_volume_channel_get(float3 value, int channel)
{
	return (channel == 0)? value.x: ((channel == 1)? value.y: value.z);
}

ccl_device bool volume_stack_is_heterogeneous(KernelGlobals *kg, VolumeStack *stack)
{
	for(int i = 0; stack[i].shader != SHADER_NONE; i++) {
		int shader_flag = kernel_tex_fetch(__shader_flag, (stack[i].shader & SHADER_MASK)*2);

		if(shader_flag & SD_HETEROGENEOUS_VOLUME)
			return true;
	}

	return false;
}

ccl_device int volume_stack_sampling_method(KernelGlobals *kg, VolumeStack *stack)
{
	if(kernel_data.integrator.num_all_lights == 0)
		return 0;

	int method = -1;

	for(int i = 0; stack[i].shader != SHADER_NONE; i++) {
		int shader_flag = kernel_tex_fetch(__shader_flag, (stack[i].shader & SHADER_MASK)*2);

		if(shader_flag & SD_VOLUME_MIS) {
			return SD_VOLUME_MIS;
		}
		else if(shader_flag & SD_VOLUME_EQUIANGULAR) {
			if(method == 0)
				return SD_VOLUME_MIS;

			method = SD_VOLUME_EQUIANGULAR;
		}
		else {
			if(method == SD_VOLUME_EQUIANGULAR)
				return SD_VOLUME_MIS;

			method = 0;
		}
	}

	return method;
}

/* Volume Shadows
 *
 * These functions are used to attenuate shadow rays to lights. Both absorption
 * and scattering will block light, represented by the extinction coefficient. */

/* homogeneous volume: assume shader evaluation at the starts gives
 * the extinction coefficient for the entire line segment */
ccl_device void kernel_volume_shadow_homogeneous(KernelGlobals *kg, PathState *state, Ray *ray, ShaderData *sd, float3 *throughput)
{
	float3 sigma_t;

	if(volume_shader_extinction_sample(kg, sd, state, ray->P, &sigma_t))
		*throughput *= volume_color_transmittance(sigma_t, ray->t);
}

/* heterogeneous volume: integrate stepping through the volume until we
 * reach the end, get absorbed entirely, or run out of iterations */
ccl_device void kernel_volume_shadow_heterogeneous(KernelGlobals *kg, PathState *state, Ray *ray, ShaderData *sd, float3 *throughput)
{
	float3 tp = *throughput;
	const float tp_eps = 1e-6f; /* todo: this is likely not the right value */

	/* prepare for stepping */
	int max_steps = kernel_data.integrator.volume_max_steps;
	float step = kernel_data.integrator.volume_step_size;
	float random_jitter_offset = lcg_step_float(&state->rng_congruential) * step;

	/* compute extinction at the start */
	float t = 0.0f;

	float3 sum = make_float3(0.0f, 0.0f, 0.0f);

	for(int i = 0; i < max_steps; i++) {
		/* advance to new position */
		float new_t = min(ray->t, (i+1) * step);
		float dt = new_t - t;

		/* use random position inside this segment to sample shader */
		if(new_t == ray->t)
			random_jitter_offset = lcg_step_float(&state->rng_congruential) * dt;

		float3 new_P = ray->P + ray->D * (t + random_jitter_offset);
		float3 sigma_t;

		/* compute attenuation over segment */
		if(volume_shader_extinction_sample(kg, sd, state, new_P, &sigma_t)) {
			/* Compute expf() only for every Nth step, to save some calculations
			 * because exp(a)*exp(b) = exp(a+b), also do a quick tp_eps check then. */

			sum += (-sigma_t * (new_t - t));
			if((i & 0x07) == 0) { /* ToDo: Other interval? */
				tp = *throughput * make_float3(expf(sum.x), expf(sum.y), expf(sum.z));

				/* stop if nearly all light is blocked */
				if(tp.x < tp_eps && tp.y < tp_eps && tp.z < tp_eps)
					break;
			}
		}

		/* stop if at the end of the volume */
		t = new_t;
		if(t == ray->t) {
			/* Update throughput in case we haven't done it above */
			tp = *throughput * make_float3(expf(sum.x), expf(sum.y), expf(sum.z));
			break;
		}
	}

	*throughput = tp;
}

/* get the volume attenuation over line segment defined by ray, with the
 * assumption that there are no surfaces blocking light between the endpoints */
ccl_device_noinline void kernel_volume_shadow(KernelGlobals *kg, PathState *state, Ray *ray, float3 *throughput)
{
	ShaderData sd;
	shader_setup_from_volume(kg, &sd, ray, state->bounce, state->transparent_bounce);

	if(volume_stack_is_heterogeneous(kg, state->volume_stack))
		kernel_volume_shadow_heterogeneous(kg, state, ray, &sd, throughput);
	else
		kernel_volume_shadow_homogeneous(kg, state, ray, &sd, throughput);
}

/* Equi-angular sampling as in:
 * "Importance Sampling Techniques for Path Tracing in Participating Media" */

ccl_device float kernel_volume_equiangular_sample(Ray *ray, float3 light_P, float xi, float *pdf)
{
	float t = ray->t;

	float delta = dot((light_P - ray->P) , ray->D);
	float D = sqrtf(len_squared(light_P - ray->P) - delta * delta);
	float theta_a = -atan2f(delta, D);
	float theta_b = atan2f(t - delta, D);
	float t_ = D * tanf((xi * theta_b) + (1 - xi) * theta_a);

	*pdf = D / ((theta_b - theta_a) * (D * D + t_ * t_));

	return min(t, delta + t_); /* min is only for float precision errors */
}

ccl_device float kernel_volume_equiangular_pdf(Ray *ray, float3 light_P, float sample_t)
{
	float delta = dot((light_P - ray->P) , ray->D);
	float D = sqrtf(len_squared(light_P - ray->P) - delta * delta);

	float t = ray->t;
	float t_ = sample_t - delta;

	float theta_a = -atan2f(delta, D);
	float theta_b = atan2f(t - delta, D);

	float pdf = D / ((theta_b - theta_a) * (D * D + t_ * t_));

	return pdf;
}

/* Distance sampling */

ccl_device float kernel_volume_distance_sample(float max_t, float3 sigma_t, int channel, float xi, float3 *transmittance, float3 *pdf)
{
	/* xi is [0, 1[ so log(0) should never happen, division by zero is
	 * avoided because sample_sigma_t > 0 when SD_SCATTER is set */
	float sample_sigma_t = kernel_volume_channel_get(sigma_t, channel);
	float3 full_transmittance = volume_color_transmittance(sigma_t, max_t);
	float sample_transmittance = kernel_volume_channel_get(full_transmittance, channel);

	float sample_t = min(max_t, -logf(1.0f - xi*(1.0f - sample_transmittance))/sample_sigma_t);

	*transmittance = volume_color_transmittance(sigma_t, sample_t);
	*pdf = (sigma_t * *transmittance)/(make_float3(1.0f, 1.0f, 1.0f) - full_transmittance);

	/* todo: optimization: when taken together with hit/miss decision,
	 * the full_transmittance cancels out drops out and xi does not
	 * need to be remapped */

	return sample_t;
}

ccl_device float3 kernel_volume_distance_pdf(float max_t, float3 sigma_t, float sample_t)
{
	float3 full_transmittance = volume_color_transmittance(sigma_t, max_t);
	float3 transmittance = volume_color_transmittance(sigma_t, sample_t);

	return (sigma_t * transmittance)/(make_float3(1.0f, 1.0f, 1.0f) - full_transmittance);
}

/* Emission */

ccl_device float3 kernel_volume_emission_integrate(VolumeShaderCoefficients *coeff, int closure_flag, float3 transmittance, float t)
{
	/* integral E * exp(-sigma_t * t) from 0 to t = E * (1 - exp(-sigma_t * t))/sigma_t
	 * this goes to E * t as sigma_t goes to zero
	 *
	 * todo: we should use an epsilon to avoid precision issues near zero sigma_t */
	float3 emission = coeff->emission;

	if(closure_flag & SD_ABSORPTION) {
		float3 sigma_t = coeff->sigma_a + coeff->sigma_s;

		emission.x *= (sigma_t.x > 0.0f)? (1.0f - transmittance.x)/sigma_t.x: t;
		emission.y *= (sigma_t.y > 0.0f)? (1.0f - transmittance.y)/sigma_t.y: t;
		emission.z *= (sigma_t.z > 0.0f)? (1.0f - transmittance.z)/sigma_t.z: t;
	}
	else
		emission *= t;
	
	return emission;
}

/* Volume Path */

/* homogeneous volume: assume shader evaluation at the start gives
 * the volume shading coefficient for the entire line segment */
ccl_device VolumeIntegrateResult kernel_volume_integrate_homogeneous(KernelGlobals *kg,
	PathState *state, Ray *ray, ShaderData *sd, PathRadiance *L, float3 *throughput,
	RNG *rng, bool probalistic_scatter)
{
	VolumeShaderCoefficients coeff;

	if(!volume_shader_sample(kg, sd, state, ray->P, &coeff))
		return VOLUME_PATH_MISSED;

	int closure_flag = sd->flag;
	float t = ray->t;
	float3 new_tp;

#ifdef __VOLUME_SCATTER__
	/* randomly scatter, and if we do t is shortened */
	if(closure_flag & SD_SCATTER) {
		/* extinction coefficient */
		float3 sigma_t = coeff.sigma_a + coeff.sigma_s;

		/* pick random color channel, we use the Veach one-sample
		 * model with balance heuristic for the channels */
		float rphase = path_state_rng_1D_for_decision(kg, rng, state, PRNG_PHASE);
		int channel = (int)(rphase*3.0f);
		sd->randb_closure = rphase*3.0f - channel;

		/* decide if we will hit or miss */
		bool scatter = true;
		float xi = path_state_rng_1D_for_decision(kg, rng, state, PRNG_SCATTER_DISTANCE);

		if(probalistic_scatter) {
			float sample_sigma_t = kernel_volume_channel_get(sigma_t, channel);
			float sample_transmittance = expf(-sample_sigma_t * t);

			if(1.0f - xi >= sample_transmittance) {
				scatter = true;

				/* rescale random number so we can reuse it */
				xi = 1.0f - (1.0f - xi - sample_transmittance)/(1.0f - sample_transmittance);

			}
			else
				scatter = false;
		}

		if(scatter) {
			/* scattering */
			float3 pdf;
			float3 transmittance;
			float sample_t;

			/* distance sampling */
			sample_t = kernel_volume_distance_sample(ray->t, sigma_t, channel, xi, &transmittance, &pdf);

			/* modifiy pdf for hit/miss decision */
			if(probalistic_scatter)
				pdf *= make_float3(1.0f, 1.0f, 1.0f) - volume_color_transmittance(sigma_t, t);

			new_tp = *throughput * coeff.sigma_s * transmittance / average(pdf);
			t = sample_t;
		}
		else {
			/* no scattering */
			float3 transmittance = volume_color_transmittance(sigma_t, t);
			float pdf = average(transmittance);
			new_tp = *throughput * transmittance / pdf;
		}
	}
	else 
#endif
	if(closure_flag & SD_ABSORPTION) {
		/* absorption only, no sampling needed */
		float3 transmittance = volume_color_transmittance(coeff.sigma_a, t);
		new_tp = *throughput * transmittance;
	}

	/* integrate emission attenuated by extinction */
	if(L && (closure_flag & SD_EMISSION)) {
		float3 sigma_t = coeff.sigma_a + coeff.sigma_s;
		float3 transmittance = volume_color_transmittance(sigma_t, ray->t);
		float3 emission = kernel_volume_emission_integrate(&coeff, closure_flag, transmittance, ray->t);
		path_radiance_accum_emission(L, *throughput, emission, state->bounce);
	}

	/* modify throughput */
	if(closure_flag & (SD_ABSORPTION|SD_SCATTER)) {
		*throughput = new_tp;

		/* prepare to scatter to new direction */
		if(t < ray->t) {
			/* adjust throughput and move to new location */
			sd->P = ray->P + t*ray->D;

			return VOLUME_PATH_SCATTERED;
		}
	}

	return VOLUME_PATH_ATTENUATED;
}

/* heterogeneous volume distance sampling: integrate stepping through the
 * volume until we reach the end, get absorbed entirely, or run out of
 * iterations. this does probalistically scatter or get transmitted through
 * for path tracing where we don't want to branch. */
ccl_device VolumeIntegrateResult kernel_volume_integrate_heterogeneous_distance(KernelGlobals *kg,
	PathState *state, Ray *ray, ShaderData *sd, PathRadiance *L, float3 *throughput, RNG *rng)
{
	float3 tp = *throughput;
	const float tp_eps = 1e-6f; /* todo: this is likely not the right value */

	/* prepare for stepping */
	int max_steps = kernel_data.integrator.volume_max_steps;
	float step_size = kernel_data.integrator.volume_step_size;
	float random_jitter_offset = lcg_step_float(&state->rng_congruential) * step_size;

	/* compute coefficients at the start */
	float t = 0.0f;
	float3 accum_transmittance = make_float3(1.0f, 1.0f, 1.0f);

	/* pick random color channel, we use the Veach one-sample
	 * model with balance heuristic for the channels */
	float xi = path_state_rng_1D_for_decision(kg, rng, state, PRNG_SCATTER_DISTANCE);
	float rphase = path_state_rng_1D_for_decision(kg, rng, state, PRNG_PHASE);
	int channel = (int)(rphase*3.0f);
	sd->randb_closure = rphase*3.0f - channel;
	bool has_scatter = false;

	for(int i = 0; i < max_steps; i++) {
		/* advance to new position */
		float new_t = min(ray->t, (i+1) * step_size);
		float dt = new_t - t;

		/* use random position inside this segment to sample shader */
		if(new_t == ray->t)
			random_jitter_offset = lcg_step_float(&state->rng_congruential) * dt;

		float3 new_P = ray->P + ray->D * (t + random_jitter_offset);
		VolumeShaderCoefficients coeff;

		/* compute segment */
		if(volume_shader_sample(kg, sd, state, new_P, &coeff)) {
			int closure_flag = sd->flag;
			float3 new_tp;
			float3 transmittance;
			bool scatter = false;

			/* distance sampling */
#ifdef __VOLUME_SCATTER__
			if((closure_flag & SD_SCATTER) || (has_scatter && (closure_flag & SD_ABSORPTION))) {
				has_scatter = true;

				float3 sigma_t = coeff.sigma_a + coeff.sigma_s;
				float3 sigma_s = coeff.sigma_s;

				/* compute transmittance over full step */
				transmittance = volume_color_transmittance(sigma_t, dt);

				/* decide if we will scatter or continue */
				float sample_transmittance = kernel_volume_channel_get(transmittance, channel);

				if(1.0f - xi >= sample_transmittance) {
					/* compute sampling distance */
					float sample_sigma_t = kernel_volume_channel_get(sigma_t, channel);
					float new_dt = -logf(1.0f - xi)/sample_sigma_t;
					new_t = t + new_dt;

					/* transmittance and pdf */
					float3 new_transmittance = volume_color_transmittance(sigma_t, new_dt);
					float3 pdf = sigma_t * new_transmittance;

					/* throughput */
					new_tp = tp * sigma_s * new_transmittance / average(pdf);
					scatter = true;
				}
				else {
					/* throughput */
					float pdf = average(transmittance);
					new_tp = tp * transmittance / pdf;

					/* remap xi so we can reuse it and keep thing stratified */
					xi = 1.0f - (1.0f - xi)/sample_transmittance;
				}
			}
			else 
#endif
			if(closure_flag & SD_ABSORPTION) {
				/* absorption only, no sampling needed */
				float3 sigma_a = coeff.sigma_a;

				transmittance = volume_color_transmittance(sigma_a, dt);
				new_tp = tp * transmittance;
			}

			/* integrate emission attenuated by absorption */
			if(L && (closure_flag & SD_EMISSION)) {
				float3 emission = kernel_volume_emission_integrate(&coeff, closure_flag, transmittance, dt);
				path_radiance_accum_emission(L, tp, emission, state->bounce);
			}

			/* modify throughput */
			if(closure_flag & (SD_ABSORPTION|SD_SCATTER)) {
				tp = new_tp;

				/* stop if nearly all light blocked */
				if(tp.x < tp_eps && tp.y < tp_eps && tp.z < tp_eps) {
					tp = make_float3(0.0f, 0.0f, 0.0f);
					break;
				}
			}

			/* prepare to scatter to new direction */
			if(scatter) {
				/* adjust throughput and move to new location */
				sd->P = ray->P + new_t*ray->D;
				*throughput = tp;

				return VOLUME_PATH_SCATTERED;
			}
			else {
				/* accumulate transmittance */
				accum_transmittance *= transmittance;
			}
		}

		/* stop if at the end of the volume */
		t = new_t;
		if(t == ray->t)
			break;
	}

	*throughput = tp;

	return VOLUME_PATH_ATTENUATED;
}

/* get the volume attenuation and emission over line segment defined by
 * ray, with the assumption that there are no surfaces blocking light
 * between the endpoints. distance sampling is used to decide if we will
 * scatter or not. */
ccl_device_noinline VolumeIntegrateResult kernel_volume_integrate(KernelGlobals *kg,
	PathState *state, ShaderData *sd, Ray *ray, PathRadiance *L, float3 *throughput, RNG *rng, bool heterogeneous)
{
	/* workaround to fix correlation bug in T38710, can find better solution
	 * in random number generator later, for now this is done here to not impact
	 * performance of rendering without volumes */
	RNG tmp_rng = cmj_hash(*rng, state->rng_offset);

	shader_setup_from_volume(kg, sd, ray, state->bounce, state->transparent_bounce);

	if(heterogeneous)
		return kernel_volume_integrate_heterogeneous_distance(kg, state, ray, sd, L, throughput, &tmp_rng);
	else
		return kernel_volume_integrate_homogeneous(kg, state, ray, sd, L, throughput, &tmp_rng, true);
}

/* Decoupled Volume Sampling
 *
 * VolumeSegment is list of coefficients and transmittance stored at all steps
 * through a volume. This can then latter be used for decoupled sampling as in:
 * "Importance Sampling Techniques for Path Tracing in Participating Media"
 *
 * On the GPU this is only supported for homogeneous volumes (1 step), due to
 * no support for malloc/free and too much stack usage with a fix size array. */

typedef struct VolumeStep {
	float3 sigma_s;				/* scatter coefficient */
	float3 sigma_t;				/* extinction coefficient */
	float3 accum_transmittance;	/* accumulated transmittance including this step */
	float3 cdf_distance;		/* cumulative density function for distance sampling */
	float t;					/* distance at end of this step */
	float shade_t;				/* jittered distance where shading was done in step */
	int closure_flag;			/* shader evaluation closure flags */
} VolumeStep;

typedef struct VolumeSegment {
	VolumeStep stack_step;      /* stack storage for homogenous step, to avoid malloc */
	VolumeStep *steps;			/* recorded steps */
	int numsteps;				/* number of steps */
	int closure_flag;			/* accumulated closure flags from all steps */

	float3 accum_emission;		/* accumulated emission at end of segment */
	float3 accum_transmittance;	/* accumulated transmittance at end of segment */

	int sampling_method;		/* volume sampling method */
} VolumeSegment;

/* record volume steps to the end of the volume.
 *
 * it would be nice if we could only record up to the point that we need to scatter,
 * but the entire segment is needed to do always scattering, rather than probalistically
 * hitting or missing the volume. if we don't know the transmittance at the end of the
 * volume we can't generate stratified distance samples up to that transmittance */
ccl_device void kernel_volume_decoupled_record(KernelGlobals *kg, PathState *state,
	Ray *ray, ShaderData *sd, VolumeSegment *segment, bool heterogeneous)
{
	const float tp_eps = 1e-6f; /* todo: this is likely not the right value */

	/* prepare for volume stepping */
	int max_steps;
	float step_size, random_jitter_offset;

	if(heterogeneous) {
		max_steps = kernel_data.integrator.volume_max_steps;
		step_size = kernel_data.integrator.volume_step_size;
		random_jitter_offset = lcg_step_float(&state->rng_congruential) * step_size;

		/* compute exact steps in advance for malloc */
		max_steps = max((int)ceilf(ray->t/step_size), 1);
		segment->steps = (VolumeStep*)malloc(sizeof(VolumeStep)*max_steps);
	}
	else {
		max_steps = 1;
		step_size = ray->t;
		random_jitter_offset = 0.0f;
		segment->steps = &segment->stack_step;
	}
	
	/* init accumulation variables */
	float3 accum_emission = make_float3(0.0f, 0.0f, 0.0f);
	float3 accum_transmittance = make_float3(1.0f, 1.0f, 1.0f);
	float3 cdf_distance = make_float3(0.0f, 0.0f, 0.0f);
	float t = 0.0f;

	segment->numsteps = 0;
	segment->closure_flag = 0;

	VolumeStep *step = segment->steps;

	for(int i = 0; i < max_steps; i++, step++) {
		/* advance to new position */
		float new_t = min(ray->t, (i+1) * step_size);
		float dt = new_t - t;

		/* use random position inside this segment to sample shader */
		if(heterogeneous && new_t == ray->t)
			random_jitter_offset = lcg_step_float(&state->rng_congruential) * dt;

		float3 new_P = ray->P + ray->D * (t + random_jitter_offset);
		VolumeShaderCoefficients coeff;

		/* compute segment */
		if(volume_shader_sample(kg, sd, state, new_P, &coeff)) {
			int closure_flag = sd->flag;
			float3 sigma_t = coeff.sigma_a + coeff.sigma_s;

			/* compute accumulated transmittance */
			float3 transmittance = volume_color_transmittance(sigma_t, dt);

			/* compute emission attenuated by absorption */
			if(closure_flag & SD_EMISSION) {
				float3 emission = kernel_volume_emission_integrate(&coeff, closure_flag, transmittance, dt);
				accum_emission += accum_transmittance * emission;
			}

			accum_transmittance *= transmittance;

			/* compute pdf for distance sampling */
			float3 pdf_distance = dt * accum_transmittance * coeff.sigma_s;
			cdf_distance = cdf_distance + pdf_distance;

			/* write step data */
			step->sigma_t = sigma_t;
			step->sigma_s = coeff.sigma_s;
			step->closure_flag = closure_flag;

			segment->closure_flag |= closure_flag;
		}
		else {
			/* store empty step (todo: skip consecutive empty steps) */
			step->sigma_t = make_float3(0.0f, 0.0f, 0.0f);
			step->sigma_s = make_float3(0.0f, 0.0f, 0.0f);
			step->closure_flag = 0;
		}

		step->accum_transmittance = accum_transmittance;
		step->cdf_distance = cdf_distance;
		step->t = new_t;
		step->shade_t = t + random_jitter_offset;

		segment->numsteps++;

		/* stop if at the end of the volume */
		t = new_t;
		if(t == ray->t)
			break;

		/* stop if nearly all light blocked */
		if(accum_transmittance.x < tp_eps && accum_transmittance.y < tp_eps && accum_transmittance.z < tp_eps)
			break;
	}

	/* store total emission and transmittance */
	segment->accum_emission = accum_emission;
	segment->accum_transmittance = accum_transmittance;

	/* normalize cumulative density function for distance sampling */
	VolumeStep *last_step = segment->steps + segment->numsteps - 1;

	if(!is_zero(last_step->cdf_distance)) {
		VolumeStep *step = &segment->steps[0];
		int numsteps = segment->numsteps;
		float3 inv_cdf_distance_sum = safe_invert_color(last_step->cdf_distance);

		for(int i = 0; i < numsteps; i++, step++)
			step->cdf_distance *= inv_cdf_distance_sum;
	}
}

ccl_device void kernel_volume_decoupled_free(KernelGlobals *kg, VolumeSegment *segment)
{
	if(segment->steps != &segment->stack_step)
		free(segment->steps);
}

/* scattering for homogeneous and heterogeneous volumes, using decoupled ray
 * marching. unlike the non-decoupled functions, these do not do probalistic
 * scattering, they always scatter if there is any non-zero scattering
 * coefficient.
 *
 * these also do not do emission or modify throughput. 
 * 
 * function is expected to return VOLUME_PATH_SCATTERED when probalistic_scatter is false */
ccl_device VolumeIntegrateResult kernel_volume_decoupled_scatter(
	KernelGlobals *kg, PathState *state, Ray *ray, ShaderData *sd,
	float3 *throughput, float rphase, float rscatter,
	const VolumeSegment *segment, const float3 *light_P, bool probalistic_scatter)
{
	kernel_assert(segment->closure_flag & SD_SCATTER);

	/* pick random color channel, we use the Veach one-sample
	 * model with balance heuristic for the channels */
	int channel = (int)(rphase*3.0f);
	sd->randb_closure = rphase*3.0f - channel;
	float xi = rscatter;

	/* probalistic scattering decision based on transmittance */
	if(probalistic_scatter) {
		float sample_transmittance = kernel_volume_channel_get(segment->accum_transmittance, channel);

		if(1.0f - xi >= sample_transmittance) {
			/* rescale random number so we can reuse it */
			xi = 1.0f - (1.0f - xi - sample_transmittance)/(1.0f - sample_transmittance);
		}
		else {
			*throughput /= sample_transmittance;
			return VOLUME_PATH_MISSED;
		}
	}

	VolumeStep *step;
	float3 transmittance;
	float pdf, sample_t;
	float mis_weight = 1.0f;
	bool distance_sample = true;
	bool use_mis = false;

	if(segment->sampling_method && light_P) {
		if(segment->sampling_method == SD_VOLUME_MIS) {
			/* multiple importance sample: randomly pick between
			 * equiangular and distance sampling strategy */
			if(xi < 0.5f) {
				xi *= 2.0f;
			}
			else {
				xi = (xi - 0.5f)*2.0f;
				distance_sample = false;
			}

			use_mis = true;
		}
		else {
			/* only equiangular sampling */
			distance_sample = false;
		}
	}

	/* distance sampling */
	if(distance_sample) {
		/* find step in cdf */
		step = segment->steps;

		float prev_t = 0.0f;
		float3 step_pdf_distance = make_float3(1.0f, 1.0f, 1.0f);

		if(segment->numsteps > 1) {
			float prev_cdf = 0.0f;
			float step_cdf = 1.0f;
			float3 prev_cdf_distance = make_float3(0.0f, 0.0f, 0.0f);

			for(int i = 0; ; i++, step++) {
				/* todo: optimize using binary search */
				step_cdf = kernel_volume_channel_get(step->cdf_distance, channel);

				if(xi < step_cdf || i == segment->numsteps-1)
					break;

				prev_cdf = step_cdf;
				prev_t = step->t;
				prev_cdf_distance = step->cdf_distance;
			}

			/* remap xi so we can reuse it */
			xi = (xi - prev_cdf)/(step_cdf - prev_cdf);

			/* pdf for picking step */
			step_pdf_distance = step->cdf_distance - prev_cdf_distance;
		}

		/* determine range in which we will sample */
		float step_t = step->t - prev_t;

		/* sample distance and compute transmittance */
		float3 distance_pdf;
		sample_t = prev_t + kernel_volume_distance_sample(step_t, step->sigma_t, channel, xi, &transmittance, &distance_pdf);

		/* modifiy pdf for hit/miss decision */
		if(probalistic_scatter)
			distance_pdf *= make_float3(1.0f, 1.0f, 1.0f) - segment->accum_transmittance;

		pdf = average(distance_pdf * step_pdf_distance);

		/* multiple importance sampling */
		if(use_mis) {
			float equi_pdf = kernel_volume_equiangular_pdf(ray, *light_P, sample_t);
			mis_weight = 2.0f*power_heuristic(pdf, equi_pdf);
		}
	}
	/* equi-angular sampling */
	else {
		/* sample distance */
		sample_t = kernel_volume_equiangular_sample(ray, *light_P, xi, &pdf);

		/* find step in which sampled distance is located */
		step = segment->steps;

		float prev_t = 0.0f;
		float3 step_pdf_distance = make_float3(1.0f, 1.0f, 1.0f);

		if(segment->numsteps > 1) {
			float3 prev_cdf_distance = make_float3(0.0f, 0.0f, 0.0f);

			int numsteps = segment->numsteps;
			int high = numsteps - 1;
			int low = 0;
			int mid;

			while(low < high) {
				mid = (low + high) >> 1;

				if(sample_t < step[mid].t)
					high = mid;
				else if(sample_t >= step[mid + 1].t)
					low = mid + 1;
				else {
					/* found our interval in step[mid] .. step[mid+1] */
					prev_t = step[mid].t;
					prev_cdf_distance = step[mid].cdf_distance;
					step += mid+1;
					break;
				}
			}

			if(low >= numsteps - 1) {
				prev_t = step[numsteps - 1].t;
				prev_cdf_distance = step[numsteps-1].cdf_distance;
				step += numsteps - 1;
			}

			/* pdf for picking step with distance sampling */
			step_pdf_distance = step->cdf_distance - prev_cdf_distance;
		}

		/* determine range in which we will sample */
		float step_t = step->t - prev_t;
		float step_sample_t = sample_t - prev_t;

		/* compute transmittance */
		transmittance = volume_color_transmittance(step->sigma_t, step_sample_t);

		/* multiple importance sampling */
		if(use_mis) {
			float3 distance_pdf3 = kernel_volume_distance_pdf(step_t, step->sigma_t, step_sample_t);
			float distance_pdf = average(distance_pdf3 * step_pdf_distance);
			mis_weight = 2.0f*power_heuristic(pdf, distance_pdf);
		}
	}

	/* compute transmittance up to this step */
	if(step != segment->steps)
		transmittance *= (step-1)->accum_transmittance;

	/* modify throughput */
	*throughput *= step->sigma_s * transmittance * (mis_weight / pdf);

	/* evaluate shader to create closures at shading point */
	if(segment->numsteps > 1) {
		sd->P = ray->P + step->shade_t*ray->D;

		VolumeShaderCoefficients coeff;
		volume_shader_sample(kg, sd, state, sd->P, &coeff);
	}

	/* move to new position */
	sd->P = ray->P + sample_t*ray->D;

	return VOLUME_PATH_SCATTERED;
}

/* decide if we need to use decoupled or not */
ccl_device bool kernel_volume_use_decoupled(KernelGlobals *kg, bool heterogeneous, bool direct, int sampling_method)
{
	/* decoupled ray marching for heterogenous volumes not supported on the GPU,
	 * which also means equiangular and multiple importance sampling is not
	 * support for that case */
#ifdef __KERNEL_GPU__
	if(heterogeneous)
		return false;
#endif

	/* equiangular and multiple importance sampling only implemented for decoupled */
	if(sampling_method != 0)
		return true;

	/* for all light sampling use decoupled, reusing shader evaluations is
	 * typically faster in that case */
	if(direct)
		return kernel_data.integrator.sample_all_lights_direct;
	else
		return kernel_data.integrator.sample_all_lights_indirect;
}

/* Volume Stack
 *
 * This is an array of object/shared ID's that the current segment of the path
 * is inside of. */

ccl_device void kernel_volume_stack_init(KernelGlobals *kg,
                                         Ray *ray,
                                         VolumeStack *stack)
{
	/* NULL ray happens in the baker, does it need proper initialization of
	 * camera in volume?
	 */
	if(!kernel_data.cam.is_inside_volume || ray == NULL) {
		/* Camera is guaranteed to be in the air, only take background volume
		 * into account in this case.
		 */
		if(kernel_data.background.volume_shader != SHADER_NONE) {
			stack[0].shader = kernel_data.background.volume_shader;
			stack[0].object = PRIM_NONE;
			stack[1].shader = SHADER_NONE;
		}
		else {
			stack[0].shader = SHADER_NONE;
		}
		return;
	}

	Ray volume_ray = *ray;
	volume_ray.t = FLT_MAX;

	int stack_index = 0, enclosed_index = 0;
	int enclosed_volumes[VOLUME_STACK_SIZE];

	while(stack_index < VOLUME_STACK_SIZE - 1 &&
	      enclosed_index < VOLUME_STACK_SIZE - 1)
	{
		Intersection isect;
		if(!scene_intersect_volume(kg, &volume_ray, &isect)) {
			break;
		}

		ShaderData sd;
		shader_setup_from_ray(kg, &sd, &isect, &volume_ray, 0, 0);
		if(sd.flag & SD_BACKFACING) {
			/* If ray exited the volume and never entered to that volume
			 * it means that camera is inside such a volume.
			 */
			bool is_enclosed = false;
			for(int i = 0; i < enclosed_index; ++i) {
				if(enclosed_volumes[i] == sd.object) {
					is_enclosed = true;
					break;
				}
			}
			if(is_enclosed == false) {
				stack[stack_index].object = sd.object;
				stack[stack_index].shader = sd.shader;
				++stack_index;
			}
		}
		else {
			/* If ray from camera enters the volume, this volume shouldn't
			 * be added to the stak on exit.
			 */
			enclosed_volumes[enclosed_index++] = sd.object;
		}

		/* Move ray forward. */
		volume_ray.P = ray_offset(sd.P, -sd.Ng);
	}
	/* stack_index of 0 means quick checks outside of the kernel gave false
	 * positive, nothing to worry about, just we've wasted quite a few of
	 * ticks just to come into conclusion that camera is in the air.
	 *
	 * In this case we're doing the same above -- check whether background has
	 * volume.
	 */
	if(stack_index == 0 && kernel_data.background.volume_shader == SHADER_NONE) {
		stack[0].shader = kernel_data.background.volume_shader;
		stack[0].object = PRIM_NONE;
		stack[1].shader = SHADER_NONE;
	}
	else {
		stack[stack_index].shader = SHADER_NONE;
	}
}

ccl_device void kernel_volume_stack_enter_exit(KernelGlobals *kg, ShaderData *sd, VolumeStack *stack)
{
	/* todo: we should have some way for objects to indicate if they want the
	 * world shader to work inside them. excluding it by default is problematic
	 * because non-volume objects can't be assumed to be closed manifolds */

	if(!(sd->flag & SD_HAS_VOLUME))
		return;
	
	if(sd->flag & SD_BACKFACING) {
		/* exit volume object: remove from stack */
		for(int i = 0; stack[i].shader != SHADER_NONE; i++) {
			if(stack[i].object == sd->object) {
				/* shift back next stack entries */
				do {
					stack[i] = stack[i+1];
					i++;
				}
				while(stack[i].shader != SHADER_NONE);

				return;
			}
		}
	}
	else {
		/* enter volume object: add to stack */
		int i;

		for(i = 0; stack[i].shader != SHADER_NONE; i++) {
			/* already in the stack? then we have nothing to do */
			if(stack[i].object == sd->object)
				return;
		}

		/* if we exceed the stack limit, ignore */
		if(i >= VOLUME_STACK_SIZE-1)
			return;

		/* add to the end of the stack */
		stack[i].shader = sd->shader;
		stack[i].object = sd->object;
		stack[i+1].shader = SHADER_NONE;
	}
}

CCL_NAMESPACE_END
