/*
 * Copyright 2011-2017 Blender Foundation
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

#ifndef __BSDF_PRINCIPLED_SHEEN_H__
#define __BSDF_PRINCIPLED_SHEEN_H__

/* DISNEY PRINCIPLED SHEEN BRDF
 *
 * Shading model by Brent Burley (Disney): "Physically Based Shading at Disney" (2012)
 */

CCL_NAMESPACE_BEGIN

typedef ccl_addr_space struct PrincipledSheenBsdf {
	SHADER_CLOSURE_BASE;
} PrincipledSheenBsdf;

ccl_device float3 calculate_principled_sheen_brdf(const PrincipledSheenBsdf *bsdf,
	float3 N, float3 V, float3 L, float3 H, float *pdf)
{
	float NdotL = dot(N, L);
	float NdotV = dot(N, V);

	if(NdotL < 0 || NdotV < 0) {
		*pdf = 0.0f;
		return make_float3(0.0f, 0.0f, 0.0f);
	}

	float LdotH = dot(L, H);

	float value = schlick_fresnel(LdotH) * NdotL;

	return make_float3(value, value, value);
}

ccl_device int bsdf_principled_sheen_setup(PrincipledSheenBsdf *bsdf)
{
	bsdf->type = CLOSURE_BSDF_PRINCIPLED_SHEEN_ID;
	return SD_BSDF|SD_BSDF_HAS_EVAL;
}

ccl_device float3 bsdf_principled_sheen_eval_reflect(const ShaderClosure *sc, const float3 I,
	const float3 omega_in, float *pdf)
{
	const PrincipledSheenBsdf *bsdf = (const PrincipledSheenBsdf *)sc;

	float3 N = bsdf->N;
	float3 V = I; // outgoing
	float3 L = omega_in; // incoming
	float3 H = normalize(L + V);

	if(dot(N, omega_in) > 0.0f) {
		*pdf = fmaxf(dot(N, omega_in), 0.0f) * M_1_PI_F;
		return calculate_principled_sheen_brdf(bsdf, N, V, L, H, pdf);
	}
	else {
		*pdf = 0.0f;
		return make_float3(0.0f, 0.0f, 0.0f);
	}
}

ccl_device float3 bsdf_principled_sheen_eval_transmit(const ShaderClosure *sc, const float3 I,
	const float3 omega_in, float *pdf)
{
	return make_float3(0.0f, 0.0f, 0.0f);
}

ccl_device int bsdf_principled_sheen_sample(const ShaderClosure *sc,
	float3 Ng, float3 I, float3 dIdx, float3 dIdy, float randu, float randv,
	float3 *eval, float3 *omega_in, float3 *domega_in_dx,
	float3 *domega_in_dy, float *pdf)
{
	const PrincipledSheenBsdf *bsdf = (const PrincipledSheenBsdf *)sc;

	float3 N = bsdf->N;

	sample_cos_hemisphere(N, randu, randv, omega_in, pdf);

	if(dot(Ng, *omega_in) > 0) {
		float3 H = normalize(I + *omega_in);

		*eval = calculate_principled_sheen_brdf(bsdf, N, I, *omega_in, H, pdf);

#ifdef __RAY_DIFFERENTIALS__
		// TODO: find a better approximation for the diffuse bounce
		*domega_in_dx = -((2 * dot(N, dIdx)) * N - dIdx);
		*domega_in_dy = -((2 * dot(N, dIdy)) * N - dIdy);
#endif
	}
	else {
		*pdf = 0.0f;
	}
	return LABEL_REFLECT|LABEL_DIFFUSE;
}

CCL_NAMESPACE_END

#endif /* __BSDF_PRINCIPLED_SHEEN_H__ */
