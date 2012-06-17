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

#ifndef __BSDF_DIFFUSE_H__
#define __BSDF_DIFFUSE_H__

CCL_NAMESPACE_BEGIN

/* DIFFUSE */

typedef struct BsdfDiffuseClosure {
	//float3 m_N;
} BsdfDiffuseClosure;

__device void bsdf_diffuse_setup(ShaderData *sd, ShaderClosure *sc)
{
	sc->type = CLOSURE_BSDF_DIFFUSE_ID;
	sd->flag |= SD_BSDF|SD_BSDF_HAS_EVAL;
}

__device void bsdf_diffuse_blur(ShaderClosure *sc, float roughness)
{
}

__device float3 bsdf_diffuse_eval_reflect(const ShaderData *sd, const ShaderClosure *sc, const float3 I, const float3 omega_in, float *pdf)
{
	float3 m_N = sd->N;

	float cos_pi = fmaxf(dot(m_N, omega_in), 0.0f) * M_1_PI_F;
	*pdf = cos_pi;
	return make_float3(cos_pi, cos_pi, cos_pi);
}

__device float3 bsdf_diffuse_eval_transmit(const ShaderData *sd, const ShaderClosure *sc, const float3 I, const float3 omega_in, float *pdf)
{
	return make_float3(0.0f, 0.0f, 0.0f);
}

__device float bsdf_diffuse_albedo(const ShaderData *sd, const ShaderClosure *sc, const float3 I)
{
	return 1.0f;
}

__device int bsdf_diffuse_sample(const ShaderData *sd, const ShaderClosure *sc, float randu, float randv, float3 *eval, float3 *omega_in, float3 *domega_in_dx, float3 *domega_in_dy, float *pdf)
{
	float3 m_N = sd->N;

	// distribution over the hemisphere
	sample_cos_hemisphere(m_N, randu, randv, omega_in, pdf);

	if(dot(sd->Ng, *omega_in) > 0.0f) {
		*eval = make_float3(*pdf, *pdf, *pdf);
#ifdef __RAY_DIFFERENTIALS__
		// TODO: find a better approximation for the diffuse bounce
		*domega_in_dx = (2 * dot(m_N, sd->dI.dx)) * m_N - sd->dI.dx;
		*domega_in_dy = (2 * dot(m_N, sd->dI.dy)) * m_N - sd->dI.dy;
		*domega_in_dx *= 125.0f;
		*domega_in_dy *= 125.0f;
#endif
	}
	else
		*pdf = 0.0f;
	
	return LABEL_REFLECT|LABEL_DIFFUSE;
}

/* TRANSLUCENT */

typedef struct BsdfTranslucentClosure {
	//float3 m_N;
} BsdfTranslucentClosure;

__device void bsdf_translucent_setup(ShaderData *sd, ShaderClosure *sc)
{
	sc->type = CLOSURE_BSDF_TRANSLUCENT_ID;
	sd->flag |= SD_BSDF|SD_BSDF_HAS_EVAL;
}

__device void bsdf_translucent_blur(ShaderClosure *sc, float roughness)
{
}

__device float3 bsdf_translucent_eval_reflect(const ShaderData *sd, const ShaderClosure *sc, const float3 I, const float3 omega_in, float *pdf)
{
	return make_float3(0.0f, 0.0f, 0.0f);
}

__device float3 bsdf_translucent_eval_transmit(const ShaderData *sd, const ShaderClosure *sc, const float3 I, const float3 omega_in, float *pdf)
{
	float3 m_N = sd->N;

	float cos_pi = fmaxf(-dot(m_N, omega_in), 0.0f) * M_1_PI_F;
	*pdf = cos_pi;
	return make_float3 (cos_pi, cos_pi, cos_pi);
}

__device float bsdf_translucent_albedo(const ShaderData *sd, const ShaderClosure *sc, const float3 I)
{
	return 1.0f;
}

__device int bsdf_translucent_sample(const ShaderData *sd, const ShaderClosure *sc, float randu, float randv, float3 *eval, float3 *omega_in, float3 *domega_in_dx, float3 *domega_in_dy, float *pdf)
{
	float3 m_N = sd->N;

	// we are viewing the surface from the right side - send a ray out with cosine
	// distribution over the hemisphere
	sample_cos_hemisphere (-m_N, randu, randv, omega_in, pdf);
	if(dot(sd->Ng, *omega_in) < 0) {
		*eval = make_float3(*pdf, *pdf, *pdf);
#ifdef __RAY_DIFFERENTIALS__
		// TODO: find a better approximation for the diffuse bounce
		*domega_in_dx = (2 * dot(m_N, sd->dI.dx)) * m_N - sd->dI.dx;
		*domega_in_dy = (2 * dot(m_N, sd->dI.dy)) * m_N - sd->dI.dy;
		*domega_in_dx *= -125.0f;
		*domega_in_dy *= -125.0f;
#endif
	} else
		*pdf = 0;

	return LABEL_TRANSMIT|LABEL_DIFFUSE;
}

CCL_NAMESPACE_END

#endif /* __BSDF_DIFFUSE_H__ */

