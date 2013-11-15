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

#ifndef __BSDF_HAIR_H__
#define __BSDF_HAIR_H__

CCL_NAMESPACE_BEGIN


ccl_device void bsdf_hair_reflection_blur(ShaderClosure *sc, float roughness)
{
}

ccl_device void bsdf_hair_transmission_blur(ShaderClosure *sc, float roughness)
{
}

ccl_device int bsdf_hair_reflection_setup(ShaderClosure *sc)
{
	sc->type = CLOSURE_BSDF_HAIR_REFLECTION_ID;
	sc->data0 = clamp(sc->data0, 0.001f, 1.0f);
	sc->data1 = clamp(sc->data1, 0.001f, 1.0f);
	return SD_BSDF|SD_BSDF_HAS_EVAL|SD_BSDF_GLOSSY;
}

ccl_device int bsdf_hair_transmission_setup(ShaderClosure *sc)
{
	sc->type = CLOSURE_BSDF_HAIR_TRANSMISSION_ID;
	sc->data0 = clamp(sc->data0, 0.001f, 1.0f);
	sc->data1 = clamp(sc->data1, 0.001f, 1.0f);
	return SD_BSDF|SD_BSDF_HAS_EVAL|SD_BSDF_GLOSSY;
}

ccl_device float3 bsdf_hair_reflection_eval_reflect(const ShaderClosure *sc, const float3 I, const float3 omega_in, float *pdf)
{
#ifdef __HAIR__
	float offset = sc->offset;
	float3 Tg = sc->T;
#else
	float offset = 0.0f;
	float3 Tg = make_float3(1.0f, 0.0f, 0.0f);
#endif
	float roughness1 = sc->data0;
	float roughness2 = sc->data1;

	float Iz = dot(Tg, I);
	float3 locy = normalize(I - Tg * Iz);
	//float3 locx = cross(locy, Tg);

	float theta_r = M_PI_2_F - safe_acosf(Iz);

	float omega_in_z = dot(Tg, omega_in);
	float3 omega_in_y = normalize(omega_in - Tg * omega_in_z);

	float theta_i = M_PI_2_F - safe_acosf(omega_in_z);
	float cosphi_i = dot(omega_in_y, locy);

	if(M_PI_2_F - fabsf(theta_i) < 0.001f || cosphi_i < 0.0f){
		*pdf = 0.0f;
		return make_float3(*pdf, *pdf, *pdf);
	}

	float phi_i = safe_acosf(cosphi_i) / roughness2;
	phi_i = fabsf(phi_i) < M_PI_F ? phi_i : M_PI_F;
	float costheta_i = cosf(theta_i);

	float a_R = atan2f(((M_PI_2_F + theta_r) * 0.5f - offset) / roughness1, 1.0f);
	float b_R = atan2f(((-M_PI_2_F + theta_r) * 0.5f - offset) / roughness1, 1.0f);

	float theta_h = (theta_i + theta_r) * 0.5f;
	float t = theta_h - offset;

	float phi_pdf = cos(phi_i * 0.5f) * 0.25f / roughness2;
	float theta_pdf = roughness1 / (2 * (t*t + roughness1*roughness1) * (a_R - b_R)* costheta_i);
	*pdf = phi_pdf * theta_pdf;

	return make_float3(*pdf, *pdf, *pdf);
}

ccl_device float3 bsdf_hair_transmission_eval_reflect(const ShaderClosure *sc, const float3 I, const float3 omega_in, float *pdf)
{
	return make_float3(0.0f, 0.0f, 0.0f);
}


ccl_device float3 bsdf_hair_reflection_eval_transmit(const ShaderClosure *sc, const float3 I, const float3 omega_in, float *pdf)
{
	return make_float3(0.0f, 0.0f, 0.0f);
}

ccl_device float3 bsdf_hair_transmission_eval_transmit(const ShaderClosure *sc, const float3 I, const float3 omega_in, float *pdf)
{
#ifdef __HAIR__
	float offset = sc->offset;
	float3 Tg = sc->T;
#else
	float offset = 0.0f;
	float3 Tg = make_float3(1.0f, 0.0f, 0.0f);
#endif
	float roughness1 = sc->data0;
	float roughness2 = sc->data1;
	float Iz = dot(Tg, I);
	float3 locy = normalize(I - Tg * Iz);
	//float3 locx = cross(locy, Tg);

	float theta_r = M_PI_2_F - safe_acosf(Iz);

	float omega_in_z = dot(Tg, omega_in);
	float3 omega_in_y = normalize(omega_in - Tg * omega_in_z);

	float theta_i = M_PI_2_F - safe_acosf(omega_in_z);
	float phi_i = safe_acosf(dot(omega_in_y, locy));

	if(M_PI_2_F - fabsf(theta_i) < 0.001f){
		*pdf = 0.0f;
		return make_float3(*pdf, *pdf, *pdf);
	}

	float costheta_i = cosf(theta_i);

	float a_TT = atan2f(((M_PI_2_F + theta_r)/2 - offset) / roughness1, 1.0f);
	float b_TT = atan2f(((-M_PI_2_F + theta_r)/2 - offset) / roughness1, 1.0f);
	float c_TT = 2 * atan2f(M_PI_2_F / roughness2, 1.0f);

	float theta_h = (theta_i + theta_r) / 2;
	float t = theta_h - offset;
	float phi = fabsf(phi_i);

	float p = M_PI_F - phi;
	float theta_pdf = roughness1 / (2 * (t*t + roughness1 * roughness1) * (a_TT - b_TT)*costheta_i);
	float phi_pdf = roughness2 / (c_TT * (p * p + roughness2 * roughness2));

	*pdf = phi_pdf * theta_pdf;
	return make_float3(*pdf, *pdf, *pdf);
}

ccl_device int bsdf_hair_reflection_sample(const ShaderClosure *sc, float3 Ng, float3 I, float3 dIdx, float3 dIdy, float randu, float randv, float3 *eval, float3 *omega_in, float3 *domega_in_dx, float3 *domega_in_dy, float *pdf)
{
#ifdef __HAIR__
	float offset = sc->offset;
	float3 Tg = sc->T;
#else
	float offset = 0.0f;
	float3 Tg = make_float3(1.0f, 0.0f, 0.0f);
#endif
	float roughness1 = sc->data0;
	float roughness2 = sc->data1;
	float Iz = dot(Tg, I);
	float3 locy = normalize(I - Tg * Iz);
	float3 locx = cross(locy, Tg);
	float theta_r = M_PI_2_F - safe_acosf(Iz);

	float a_R = atan2f(((M_PI_2_F + theta_r) * 0.5f - offset) / roughness1, 1.0f);
	float b_R = atan2f(((-M_PI_2_F + theta_r) * 0.5f - offset) / roughness1, 1.0f);

	float t = roughness1 * tanf(randu * (a_R - b_R) + b_R);

	float theta_h = t + offset;
	float theta_i = 2 * theta_h - theta_r;
	float costheta_i = cosf(theta_i);
	float sintheta_i = sinf(theta_i);

	float phi = 2 * safe_asinf(1 - 2 * randv) * roughness2;

	float phi_pdf = cos(phi * 0.5f) * 0.25f / roughness2;

	float theta_pdf = roughness1 / (2 * (t*t + roughness1*roughness1) * (a_R - b_R)*costheta_i);

	*omega_in =(cosf(phi) * costheta_i) * locy -
			   (sinf(phi) * costheta_i) * locx +
			   (            sintheta_i) * Tg;

	//differentials - TODO: find a better approximation for the reflective bounce
#ifdef __RAY_DIFFERENTIALS__
	*domega_in_dx = 2 * dot(locy, dIdx) * locy - dIdx;
	*domega_in_dy = 2 * dot(locy, dIdy) * locy - dIdy;
#endif

	*pdf = fabsf(phi_pdf * theta_pdf);
	if(M_PI_2_F - fabsf(theta_i) < 0.001f)
		*pdf = 0.0f;

	*eval = make_float3(*pdf, *pdf, *pdf);

	if(dot(locy, *omega_in) < 0.0f) {
		return LABEL_REFLECT|LABEL_TRANSMIT|LABEL_GLOSSY;
	}
	
	return LABEL_REFLECT|LABEL_GLOSSY;
}

ccl_device int bsdf_hair_transmission_sample(const ShaderClosure *sc, float3 Ng, float3 I, float3 dIdx, float3 dIdy, float randu, float randv, float3 *eval, float3 *omega_in, float3 *domega_in_dx, float3 *domega_in_dy, float *pdf)
{
#ifdef __HAIR__
	float offset = sc->offset;
	float3 Tg = sc->T;
#else
	float offset = 0.0f;
	float3 Tg = make_float3(1.0f, 0.0f, 0.0f);
#endif
	float roughness1 = sc->data0;
	float roughness2 = sc->data1;
	float Iz = dot(Tg, I);
	float3 locy = normalize(I - Tg * Iz);
	float3 locx = cross(locy, Tg);
	float theta_r = M_PI_2_F - safe_acosf(Iz);

	float a_TT = atan2f(((M_PI_2_F + theta_r)/2 - offset) / roughness1, 1.0f);
	float b_TT = atan2f(((-M_PI_2_F + theta_r)/2 - offset) / roughness1, 1.0f);
	float c_TT = 2 * atan2f(M_PI_2_F / roughness2, 1.0f);

	float t = roughness1 * tanf(randu * (a_TT - b_TT) + b_TT);

	float theta_h = t + offset;
	float theta_i = 2 * theta_h - theta_r;
	float costheta_i = cosf(theta_i);
	float sintheta_i = sinf(theta_i);

	float p = roughness2 * tanf(c_TT * (randv - 0.5f));
	float phi = p + M_PI_F;
	float theta_pdf = roughness1 / (2 * (t*t + roughness1*roughness1) * (a_TT - b_TT) * costheta_i);
	float phi_pdf = roughness2 / (c_TT * (p * p + roughness2 * roughness2));

	*omega_in =(cosf(phi) * costheta_i) * locy -
			   (sinf(phi) * costheta_i) * locx +
			   (            sintheta_i) * Tg;

	//differentials - TODO: find a better approximation for the transmission bounce
#ifdef __RAY_DIFFERENTIALS__
	*domega_in_dx = 2 * dot(locy, dIdx) * locy - dIdx;
	*domega_in_dy = 2 * dot(locy, dIdy) * locy - dIdy;
#endif

	*pdf = fabsf(phi_pdf * theta_pdf);
	if(M_PI_2_F - fabsf(theta_i) < 0.001f){
		*pdf = 0.0f;
	}

	*eval = make_float3(*pdf, *pdf, *pdf);

	if(dot(locy, *omega_in) < 0.0f)
		return LABEL_TRANSMIT|LABEL_GLOSSY;
	
	return LABEL_GLOSSY;
}

CCL_NAMESPACE_END

#endif /* __BSDF_HAIR_H__ */

