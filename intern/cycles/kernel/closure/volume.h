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

#ifndef __VOLUME_H__
#define __VOLUME_H__

CCL_NAMESPACE_BEGIN

/* VOLUME EXTINCTION */

ccl_device void volume_extinction_setup(ShaderData *sd, float3 weight)
{
	if(sd->flag & SD_EXTINCTION) {
		sd->closure_transparent_extinction += weight;
	}
	else {
		sd->flag |= SD_EXTINCTION;
		sd->closure_transparent_extinction = weight;
	}
}

/* HENYEY-GREENSTEIN CLOSURE */

typedef ccl_addr_space struct HenyeyGreensteinVolume {
	SHADER_CLOSURE_BASE;

	float g;
} HenyeyGreensteinVolume;

/* Given cosine between rays, return probability density that a photon bounces
 * to that direction. The g parameter controls how different it is from the
 * uniform sphere. g=0 uniform diffuse-like, g=1 close to sharp single ray. */
ccl_device float single_peaked_henyey_greenstein(float cos_theta, float g)
{
	return ((1.0f - g * g) / safe_powf(1.0f + g * g - 2.0f * g * cos_theta, 1.5f)) * (M_1_PI_F * 0.25f);
};

ccl_device int volume_henyey_greenstein_setup(HenyeyGreensteinVolume *volume)
{
	volume->type = CLOSURE_VOLUME_HENYEY_GREENSTEIN_ID;

	/* clamp anisotropy to avoid delta function */
	volume->g = signf(volume->g) * min(fabsf(volume->g), 1.0f - 1e-3f);

	return SD_SCATTER;
}

ccl_device bool volume_henyey_greenstein_merge(const ShaderClosure *a, const ShaderClosure *b)
{
	const HenyeyGreensteinVolume *volume_a = (const HenyeyGreensteinVolume*)a;
	const HenyeyGreensteinVolume *volume_b = (const HenyeyGreensteinVolume*)b;

	return (volume_a->g == volume_b->g);
}

ccl_device float3 volume_henyey_greenstein_eval_phase(const ShaderClosure *sc, const float3 I, float3 omega_in, float *pdf)
{
	const HenyeyGreensteinVolume *volume = (const HenyeyGreensteinVolume*)sc;
	float g = volume->g;

	/* note that I points towards the viewer */
	if(fabsf(g) < 1e-3f) {
		*pdf = M_1_PI_F * 0.25f;
	}
	else {
		float cos_theta = dot(-I, omega_in);
		*pdf = single_peaked_henyey_greenstein(cos_theta, g);
	}

	return make_float3(*pdf, *pdf, *pdf);
}

ccl_device float3 henyey_greenstrein_sample(float3 D, float g, float randu, float randv, float *pdf)
{
	/* match pdf for small g */
	float cos_theta;
	bool isotropic = fabsf(g) < 1e-3f;

	if(isotropic) {
		cos_theta = (1.0f - 2.0f * randu);
		if(pdf) {
			*pdf = M_1_PI_F * 0.25f;
		}
	}
	else {
		float k = (1.0f - g * g) / (1.0f - g + 2.0f * g * randu);
		cos_theta = (1.0f + g * g - k * k) / (2.0f * g);
		if(pdf) {
			*pdf = single_peaked_henyey_greenstein(cos_theta, g);
		}
	}

	float sin_theta = safe_sqrtf(1.0f - cos_theta * cos_theta);
	float phi = M_2PI_F * randv;
	float3 dir = make_float3(sin_theta * cosf(phi), sin_theta * sinf(phi), cos_theta);

	float3 T, B;
	make_orthonormals(D, &T, &B);
	dir = dir.x * T + dir.y * B + dir.z * D;

	return dir;
}

ccl_device int volume_henyey_greenstein_sample(const ShaderClosure *sc, float3 I, float3 dIdx, float3 dIdy, float randu, float randv,
	float3 *eval, float3 *omega_in, float3 *domega_in_dx, float3 *domega_in_dy, float *pdf)
{
	const HenyeyGreensteinVolume *volume = (const HenyeyGreensteinVolume*)sc;
	float g = volume->g;

	/* note that I points towards the viewer and so is used negated */
	*omega_in = henyey_greenstrein_sample(-I, g, randu, randv, pdf);
	*eval = make_float3(*pdf, *pdf, *pdf); /* perfect importance sampling */

#ifdef __RAY_DIFFERENTIALS__
	/* todo: implement ray differential estimation */
	*domega_in_dx = make_float3(0.0f, 0.0f, 0.0f);
	*domega_in_dy = make_float3(0.0f, 0.0f, 0.0f);
#endif

	return LABEL_VOLUME_SCATTER;
}

/* VOLUME CLOSURE */

ccl_device float3 volume_phase_eval(const ShaderData *sd, const ShaderClosure *sc, float3 omega_in, float *pdf)
{
	kernel_assert(sc->type == CLOSURE_VOLUME_HENYEY_GREENSTEIN_ID);

	return volume_henyey_greenstein_eval_phase(sc, sd->I, omega_in, pdf);
}

ccl_device int volume_phase_sample(const ShaderData *sd, const ShaderClosure *sc, float randu,
	float randv, float3 *eval, float3 *omega_in, differential3 *domega_in, float *pdf)
{
	int label;

	switch(sc->type) {
		case CLOSURE_VOLUME_HENYEY_GREENSTEIN_ID:
			label = volume_henyey_greenstein_sample(sc, sd->I, sd->dI.dx, sd->dI.dy, randu, randv, eval, omega_in, &domega_in->dx, &domega_in->dy, pdf);
			break;
		default:
			*eval = make_float3(0.0f, 0.0f, 0.0f);
			label = LABEL_NONE;
			break;
	}

	return label;
}

CCL_NAMESPACE_END

#endif
