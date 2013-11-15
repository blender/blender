/*
 * Adapted from Open Shading Language with this license:
 *
 * Copyright (c) 2009-2010 Sony Pictures Imageworks Inc., et al.
 * All Rights Reserved.
 *
 * Modifications Copyright 2011, Blender Foundation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * * Neither the name of Sony Pictures Imageworks nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __BSDF_WESTIN_H__
#define __BSDF_WESTIN_H__

CCL_NAMESPACE_BEGIN

/* WESTIN BACKSCATTER */

ccl_device int bsdf_westin_backscatter_setup(ShaderClosure *sc)
{
	float roughness = sc->data0;
	roughness = clamp(roughness, 1e-5f, 1.0f);
	float m_invroughness = 1.0f/roughness;

	sc->type = CLOSURE_BSDF_WESTIN_BACKSCATTER_ID;
	sc->data0 = m_invroughness;

	return SD_BSDF|SD_BSDF_HAS_EVAL|SD_BSDF_GLOSSY;
}

ccl_device void bsdf_westin_backscatter_blur(ShaderClosure *sc, float roughness)
{
	float m_invroughness = sc->data0;
	m_invroughness = min(1.0f/roughness, m_invroughness);
	sc->data0 = m_invroughness;
}

ccl_device float3 bsdf_westin_backscatter_eval_reflect(const ShaderClosure *sc, const float3 I, const float3 omega_in, float *pdf)
{
	float m_invroughness = sc->data0;
	float3 N = sc->N;

	// pdf is implicitly 0 (no indirect sampling)
	float cosNO = dot(N, I);
	float cosNI = dot(N, omega_in);
	if(cosNO > 0 && cosNI > 0) {
		float cosine = dot(I, omega_in);
		*pdf = cosine > 0 ? (m_invroughness + 1) * powf(cosine, m_invroughness) : 0;
		*pdf *= 0.5f * M_1_PI_F;
		return make_float3 (*pdf, *pdf, *pdf);
	}
	return make_float3 (0, 0, 0);
}

ccl_device float3 bsdf_westin_backscatter_eval_transmit(const ShaderClosure *sc, const float3 I, const float3 omega_in, float *pdf)
{
	return make_float3(0.0f, 0.0f, 0.0f);
}

ccl_device int bsdf_westin_backscatter_sample(const ShaderClosure *sc, float3 Ng, float3 I, float3 dIdx, float3 dIdy, float randu, float randv, float3 *eval, float3 *omega_in, float3 *domega_in_dx, float3 *domega_in_dy, float *pdf)
{
	float m_invroughness = sc->data0;
	float3 N = sc->N;

	float cosNO = dot(N, I);
	if(cosNO > 0) {
#ifdef __RAY_DIFFERENTIALS__
		*domega_in_dx = dIdx;
		*domega_in_dy = dIdy;
#endif
		float3 T, B;
		make_orthonormals (I, &T, &B);
		float phi = M_2PI_F * randu;
		float cosTheta = powf(randv, 1 / (m_invroughness + 1));
		float sinTheta2 = 1 - cosTheta * cosTheta;
		float sinTheta = sinTheta2 > 0 ? sqrtf(sinTheta2) : 0;
		*omega_in = (cosf(phi) * sinTheta) * T +
				   (sinf(phi) * sinTheta) * B +
				   (cosTheta) * I;
		if(dot(Ng, *omega_in) > 0)
		{
			// common terms for pdf and eval
			float cosNI = dot(N, *omega_in);
			// make sure the direction we chose is still in the right hemisphere
			if(cosNI > 0)
			{
				*pdf = 0.5f * M_1_PI_F * powf(cosTheta, m_invroughness);
				*pdf = (m_invroughness + 1) * (*pdf);
				*eval = make_float3(*pdf, *pdf, *pdf);
			}
		}
	}
	return LABEL_REFLECT|LABEL_GLOSSY;
}

/* WESTIN SHEEN */

ccl_device int bsdf_westin_sheen_setup(ShaderClosure *sc)
{
	/* float edginess = sc->data0; */
	sc->type = CLOSURE_BSDF_WESTIN_SHEEN_ID;
	return SD_BSDF|SD_BSDF_HAS_EVAL|SD_BSDF_GLOSSY;
}

ccl_device void bsdf_westin_sheen_blur(ShaderClosure *sc, float roughness)
{
}

ccl_device float3 bsdf_westin_sheen_eval_reflect(const ShaderClosure *sc, const float3 I, const float3 omega_in, float *pdf)
{
	float m_edginess = sc->data0;
	float3 N = sc->N;

	// pdf is implicitly 0 (no indirect sampling)
	float cosNO = dot(N, I);
	float cosNI = dot(N, omega_in);
	if(cosNO > 0 && cosNI > 0) {
		float sinNO2 = 1 - cosNO * cosNO;
		*pdf = cosNI * M_1_PI_F;
		float westin = sinNO2 > 0 ? powf(sinNO2, 0.5f * m_edginess) * (*pdf) : 0;
		return make_float3 (westin, westin, westin);
	}
	return make_float3 (0, 0, 0);
}

ccl_device float3 bsdf_westin_sheen_eval_transmit(const ShaderClosure *sc, const float3 I, const float3 omega_in, float *pdf)
{
	return make_float3(0.0f, 0.0f, 0.0f);
}

ccl_device int bsdf_westin_sheen_sample(const ShaderClosure *sc, float3 Ng, float3 I, float3 dIdx, float3 dIdy, float randu, float randv, float3 *eval, float3 *omega_in, float3 *domega_in_dx, float3 *domega_in_dy, float *pdf)
{
	float m_edginess = sc->data0;
	float3 N = sc->N;

	// we are viewing the surface from the right side - send a ray out with cosine
	// distribution over the hemisphere
	sample_cos_hemisphere(N, randu, randv, omega_in, pdf);
	if(dot(Ng, *omega_in) > 0) {
		// TODO: account for sheen when sampling
		float cosNO = dot(N, I);
		float sinNO2 = 1 - cosNO * cosNO;
		float westin = sinNO2 > 0 ? powf(sinNO2, 0.5f * m_edginess) * (*pdf) : 0;
		*eval = make_float3(westin, westin, westin);
#ifdef __RAY_DIFFERENTIALS__
		// TODO: find a better approximation for the diffuse bounce
		*domega_in_dx = (2 * dot(N, dIdx)) * N - dIdx;
		*domega_in_dy = (2 * dot(N, dIdy)) * N - dIdy;
#endif
	}
	else {
		pdf = 0;
	}
	return LABEL_REFLECT|LABEL_DIFFUSE;
}

CCL_NAMESPACE_END

#endif /* __BSDF_WESTIN_H__ */

