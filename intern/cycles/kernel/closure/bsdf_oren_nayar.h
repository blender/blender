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

#ifndef __BSDF_OREN_NAYAR_H__
#define __BSDF_OREN_NAYAR_H__

CCL_NAMESPACE_BEGIN

ccl_device float3 bsdf_oren_nayar_get_intensity(const ShaderClosure *sc, float3 n, float3 v, float3 l)
{
	float nl = max(dot(n, l), 0.0f);
	float nv = max(dot(n, v), 0.0f);
	float t = dot(l, v) - nl * nv;

	if(t > 0.0f)
		t /= max(nl, nv) + FLT_MIN;
	float is = nl * (sc->data0 + sc->data1 * t);
	return make_float3(is, is, is);
}

ccl_device int bsdf_oren_nayar_setup(ShaderClosure *sc)
{
	float sigma = sc->data0;

	sc->type = CLOSURE_BSDF_OREN_NAYAR_ID;

	sigma = clamp(sigma, 0.0f, 1.0f);

	float div = 1.0f / (M_PI_F + ((3.0f * M_PI_F - 4.0f) / 6.0f) * sigma);

	sc->data0 = 1.0f * div;
	sc->data1 = sigma * div;

	return SD_BSDF|SD_BSDF_HAS_EVAL;
}

ccl_device void bsdf_oren_nayar_blur(ShaderClosure *sc, float roughness)
{
}

ccl_device float3 bsdf_oren_nayar_eval_reflect(const ShaderClosure *sc, const float3 I, const float3 omega_in, float *pdf)
{
	if(dot(sc->N, omega_in) > 0.0f) {
		*pdf = 0.5f * M_1_PI_F;
		return bsdf_oren_nayar_get_intensity(sc, sc->N, I, omega_in);
	}
	else {
		*pdf = 0.0f;
		return make_float3(0.0f, 0.0f, 0.0f);
	}
}

ccl_device float3 bsdf_oren_nayar_eval_transmit(const ShaderClosure *sc, const float3 I, const float3 omega_in, float *pdf)
{
	return make_float3(0.0f, 0.0f, 0.0f);
}

ccl_device int bsdf_oren_nayar_sample(const ShaderClosure *sc, float3 Ng, float3 I, float3 dIdx, float3 dIdy, float randu, float randv, float3 *eval, float3 *omega_in, float3 *domega_in_dx, float3 *domega_in_dy, float *pdf)
{
	sample_uniform_hemisphere(sc->N, randu, randv, omega_in, pdf);

	if(dot(Ng, *omega_in) > 0.0f) {
		*eval = bsdf_oren_nayar_get_intensity(sc, sc->N, I, *omega_in);

#ifdef __RAY_DIFFERENTIALS__
		// TODO: find a better approximation for the bounce
		*domega_in_dx = (2.0f * dot(sc->N, dIdx)) * sc->N - dIdx;
		*domega_in_dy = (2.0f * dot(sc->N, dIdy)) * sc->N - dIdy;
#endif
	}
	else {
		*pdf = 0.0f;
		*eval = make_float3(0.0f, 0.0f, 0.0f);
	}

	return LABEL_REFLECT|LABEL_DIFFUSE;
}


CCL_NAMESPACE_END

#endif /* __BSDF_OREN_NAYAR_H__ */
