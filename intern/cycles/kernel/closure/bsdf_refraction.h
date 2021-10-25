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

#ifndef __BSDF_REFRACTION_H__
#define __BSDF_REFRACTION_H__

CCL_NAMESPACE_BEGIN

/* REFRACTION */

ccl_device int bsdf_refraction_setup(MicrofacetBsdf *bsdf)
{
	bsdf->type = CLOSURE_BSDF_REFRACTION_ID;
	return SD_BSDF;
}

ccl_device float3 bsdf_refraction_eval_reflect(const ShaderClosure *sc, const float3 I, const float3 omega_in, float *pdf)
{
	return make_float3(0.0f, 0.0f, 0.0f);
}

ccl_device float3 bsdf_refraction_eval_transmit(const ShaderClosure *sc, const float3 I, const float3 omega_in, float *pdf)
{
	return make_float3(0.0f, 0.0f, 0.0f);
}

ccl_device int bsdf_refraction_sample(const ShaderClosure *sc, float3 Ng, float3 I, float3 dIdx, float3 dIdy, float randu, float randv, float3 *eval, float3 *omega_in, float3 *domega_in_dx, float3 *domega_in_dy, float *pdf)
{
	const MicrofacetBsdf *bsdf = (const MicrofacetBsdf*)sc;
	float m_eta = bsdf->ior;
	float3 N = bsdf->N;

	float3 R, T;
#ifdef __RAY_DIFFERENTIALS__
	float3 dRdx, dRdy, dTdx, dTdy;
#endif
	bool inside;
	float fresnel;
	fresnel = fresnel_dielectric(m_eta, N, I, &R, &T,
#ifdef __RAY_DIFFERENTIALS__
		dIdx, dIdy, &dRdx, &dRdy, &dTdx, &dTdy,
#endif
		&inside);

	if(!inside && fresnel != 1.0f) {
		/* Some high number for MIS. */
		*pdf = 1e6f;
		*eval = make_float3(1e6f, 1e6f, 1e6f);
		*omega_in = T;
#ifdef __RAY_DIFFERENTIALS__
		*domega_in_dx = dTdx;
		*domega_in_dy = dTdy;
#endif
	}
	return LABEL_TRANSMIT|LABEL_SINGULAR;
}

CCL_NAMESPACE_END

#endif /* __BSDF_REFRACTION_H__ */

