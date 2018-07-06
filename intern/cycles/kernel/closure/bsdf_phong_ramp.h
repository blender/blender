/*
 * Adapted from Open Shading Language with this license:
 *
 * Copyright (c) 2009-2010 Sony Pictures Imageworks Inc., et al.
 * All Rights Reserved.
 *
 * Modifications Copyright 2012, Blender Foundation.
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

#ifndef __BSDF_PHONG_RAMP_H__
#define __BSDF_PHONG_RAMP_H__

CCL_NAMESPACE_BEGIN

#ifdef __OSL__

typedef ccl_addr_space struct PhongRampBsdf {
	SHADER_CLOSURE_BASE;

	float exponent;
	float3 *colors;
} PhongRampBsdf;

ccl_device float3 bsdf_phong_ramp_get_color(const float3 colors[8], float pos)
{
	int MAXCOLORS = 8;

	float npos = pos * (float)(MAXCOLORS - 1);
	int ipos = float_to_int(npos);
	if(ipos < 0)
		return colors[0];
	if(ipos >= (MAXCOLORS - 1))
		return colors[MAXCOLORS - 1];
	float offset = npos - (float)ipos;
	return colors[ipos] * (1.0f - offset) + colors[ipos+1] * offset;
}

ccl_device int bsdf_phong_ramp_setup(PhongRampBsdf *bsdf)
{
	bsdf->type = CLOSURE_BSDF_PHONG_RAMP_ID;
	bsdf->exponent = max(bsdf->exponent, 0.0f);
	return SD_BSDF|SD_BSDF_HAS_EVAL;
}

ccl_device float3 bsdf_phong_ramp_eval_reflect(const ShaderClosure *sc, const float3 I, const float3 omega_in, float *pdf)
{
	const PhongRampBsdf *bsdf = (const PhongRampBsdf*)sc;
	float m_exponent = bsdf->exponent;
	float cosNI = dot(bsdf->N, omega_in);
	float cosNO = dot(bsdf->N, I);

	if(cosNI > 0 && cosNO > 0) {
		// reflect the view vector
		float3 R = (2 * cosNO) * bsdf->N - I;
		float cosRI = dot(R, omega_in);
		if(cosRI > 0) {
			float cosp = powf(cosRI, m_exponent);
			float common = 0.5f * M_1_PI_F * cosp;
			float out = cosNI * (m_exponent + 2) * common;
			*pdf = (m_exponent + 1) * common;
			return bsdf_phong_ramp_get_color(bsdf->colors, cosp) * out;
		}
	}

	return make_float3(0.0f, 0.0f, 0.0f);
}

ccl_device float3 bsdf_phong_ramp_eval_transmit(const ShaderClosure *sc, const float3 I, const float3 omega_in, float *pdf)
{
	return make_float3(0.0f, 0.0f, 0.0f);
}

ccl_device int bsdf_phong_ramp_sample(const ShaderClosure *sc, float3 Ng, float3 I, float3 dIdx, float3 dIdy, float randu, float randv, float3 *eval, float3 *omega_in, float3 *domega_in_dx, float3 *domega_in_dy, float *pdf)
{
	const PhongRampBsdf *bsdf = (const PhongRampBsdf*)sc;
	float cosNO = dot(bsdf->N, I);
	float m_exponent = bsdf->exponent;

	if(cosNO > 0) {
		// reflect the view vector
		float3 R = (2 * cosNO) * bsdf->N - I;

#ifdef __RAY_DIFFERENTIALS__
		*domega_in_dx = (2 * dot(bsdf->N, dIdx)) * bsdf->N - dIdx;
		*domega_in_dy = (2 * dot(bsdf->N, dIdy)) * bsdf->N - dIdy;
#endif

		float3 T, B;
		make_orthonormals (R, &T, &B);
		float phi = M_2PI_F * randu;
		float cosTheta = powf(randv, 1 / (m_exponent + 1));
		float sinTheta2 = 1 - cosTheta * cosTheta;
		float sinTheta = sinTheta2 > 0 ? sqrtf(sinTheta2) : 0;
		*omega_in = (cosf(phi) * sinTheta) * T +
		            (sinf(phi) * sinTheta) * B +
		            (            cosTheta) * R;
		if(dot(Ng, *omega_in) > 0.0f)
		{
			// common terms for pdf and eval
			float cosNI = dot(bsdf->N, *omega_in);
			// make sure the direction we chose is still in the right hemisphere
			if(cosNI > 0)
			{
				float cosp = powf(cosTheta, m_exponent);
				float common = 0.5f * M_1_PI_F * cosp;
				*pdf = (m_exponent + 1) * common;
				float out = cosNI * (m_exponent + 2) * common;
				*eval = bsdf_phong_ramp_get_color(bsdf->colors, cosp) * out;
			}
		}
	}
	return LABEL_REFLECT|LABEL_GLOSSY;
}

#endif /* __OSL__ */

CCL_NAMESPACE_END

#endif /* __BSDF_PHONG_RAMP_H__ */
