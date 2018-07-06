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

#ifndef __BSDF_ASHIKHMIN_VELVET_H__
#define __BSDF_ASHIKHMIN_VELVET_H__

CCL_NAMESPACE_BEGIN

typedef ccl_addr_space struct VelvetBsdf {
	SHADER_CLOSURE_BASE;

	float sigma;
	float invsigma2;
} VelvetBsdf;

ccl_device int bsdf_ashikhmin_velvet_setup(VelvetBsdf *bsdf)
{
	float sigma = fmaxf(bsdf->sigma, 0.01f);
	bsdf->invsigma2 = 1.0f/(sigma * sigma);

	bsdf->type = CLOSURE_BSDF_ASHIKHMIN_VELVET_ID;

	return SD_BSDF|SD_BSDF_HAS_EVAL;
}

ccl_device bool bsdf_ashikhmin_velvet_merge(const ShaderClosure *a, const ShaderClosure *b)
{
	const VelvetBsdf *bsdf_a = (const VelvetBsdf*)a;
	const VelvetBsdf *bsdf_b = (const VelvetBsdf*)b;

	return (isequal_float3(bsdf_a->N, bsdf_b->N)) &&
	       (bsdf_a->sigma == bsdf_b->sigma);
}

ccl_device float3 bsdf_ashikhmin_velvet_eval_reflect(const ShaderClosure *sc, const float3 I, const float3 omega_in, float *pdf)
{
	const VelvetBsdf *bsdf = (const VelvetBsdf*)sc;
	float m_invsigma2 = bsdf->invsigma2;
	float3 N = bsdf->N;

	float cosNO = dot(N, I);
	float cosNI = dot(N, omega_in);
	if(cosNO > 0 && cosNI > 0) {
		float3 H = normalize(omega_in + I);

		float cosNH = dot(N, H);
		float cosHO = fabsf(dot(I, H));

		if(!(fabsf(cosNH) < 1.0f-1e-5f && cosHO > 1e-5f))
			return make_float3(0.0f, 0.0f, 0.0f);

		float cosNHdivHO = cosNH / cosHO;
		cosNHdivHO = fmaxf(cosNHdivHO, 1e-5f);

		float fac1 = 2 * fabsf(cosNHdivHO * cosNO);
		float fac2 = 2 * fabsf(cosNHdivHO * cosNI);

		float sinNH2 = 1 - cosNH * cosNH;
		float sinNH4 = sinNH2 * sinNH2;
		float cotangent2 = (cosNH * cosNH) / sinNH2;

		float D = expf(-cotangent2 * m_invsigma2) * m_invsigma2 * M_1_PI_F / sinNH4;
		float G = min(1.0f, min(fac1, fac2)); // TODO: derive G from D analytically

		float out = 0.25f * (D * G) / cosNO;

		*pdf = 0.5f * M_1_PI_F;
		return make_float3(out, out, out);
	}

	return make_float3(0.0f, 0.0f, 0.0f);
}

ccl_device float3 bsdf_ashikhmin_velvet_eval_transmit(const ShaderClosure *sc, const float3 I, const float3 omega_in, float *pdf)
{
	return make_float3(0.0f, 0.0f, 0.0f);
}

ccl_device int bsdf_ashikhmin_velvet_sample(const ShaderClosure *sc, float3 Ng, float3 I, float3 dIdx, float3 dIdy, float randu, float randv, float3 *eval, float3 *omega_in, float3 *domega_in_dx, float3 *domega_in_dy, float *pdf)
{
	const VelvetBsdf *bsdf = (const VelvetBsdf*)sc;
	float m_invsigma2 = bsdf->invsigma2;
	float3 N = bsdf->N;

	// we are viewing the surface from above - send a ray out with uniform
	// distribution over the hemisphere
	sample_uniform_hemisphere(N, randu, randv, omega_in, pdf);

	if(dot(Ng, *omega_in) > 0) {
		float3 H = normalize(*omega_in + I);

		float cosNI = dot(N, *omega_in);
		float cosNO = dot(N, I);
		float cosNH = dot(N, H);
		float cosHO = fabsf(dot(I, H));

		if(fabsf(cosNO) > 1e-5f && fabsf(cosNH) < 1.0f-1e-5f && cosHO > 1e-5f) {
			float cosNHdivHO = cosNH / cosHO;
			cosNHdivHO = fmaxf(cosNHdivHO, 1e-5f);

			float fac1 = 2 * fabsf(cosNHdivHO * cosNO);
			float fac2 = 2 * fabsf(cosNHdivHO * cosNI);

			float sinNH2 = 1 - cosNH * cosNH;
			float sinNH4 = sinNH2 * sinNH2;
			float cotangent2 = (cosNH * cosNH) / sinNH2;

			float D = expf(-cotangent2 * m_invsigma2) * m_invsigma2 * M_1_PI_F / sinNH4;
			float G = min(1.0f, min(fac1, fac2)); // TODO: derive G from D analytically

			float power = 0.25f * (D * G) / cosNO;

			*eval = make_float3(power, power, power);

#ifdef __RAY_DIFFERENTIALS__
			// TODO: find a better approximation for the retroreflective bounce
			*domega_in_dx = (2 * dot(N, dIdx)) * N - dIdx;
			*domega_in_dy = (2 * dot(N, dIdy)) * N - dIdy;
#endif
		}
		else
			*pdf = 0.0f;
	}
	else
		*pdf = 0.0f;

	return LABEL_REFLECT|LABEL_DIFFUSE;
}

CCL_NAMESPACE_END

#endif /* __BSDF_ASHIKHMIN_VELVET_H__ */
