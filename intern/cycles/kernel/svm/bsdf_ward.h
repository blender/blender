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

#ifndef __BSDF_WARD_H__
#define __BSDF_WARD_H__

CCL_NAMESPACE_BEGIN

/* WARD */

typedef struct BsdfWardClosure {
	//float3 m_N;
	//float3 m_T;
	float m_ax;
	float m_ay;
} BsdfWardClosure;

__device void bsdf_ward_setup(ShaderData *sd, ShaderClosure *sc, float3 T, float ax, float ay)
{
	float m_ax = clamp(ax, 1e-5f, 1.0f);
	float m_ay = clamp(ay, 1e-5f, 1.0f);

	sc->data0 = m_ax;
	sc->data1 = m_ay;

	sc->type = CLOSURE_BSDF_WARD_ID;
	sd->flag |= SD_BSDF|SD_BSDF_HAS_EVAL|SD_BSDF_GLOSSY;
}

__device void bsdf_ward_blur(ShaderClosure *sc, float roughness)
{
	sc->data0 = fmaxf(roughness, sc->data0);
	sc->data1 = fmaxf(roughness, sc->data1);
}

__device float3 bsdf_ward_eval_reflect(const ShaderData *sd, const ShaderClosure *sc, const float3 I, const float3 omega_in, float *pdf)
{
	float m_ax = sc->data0;
	float m_ay = sc->data1;
	float3 m_N = sd->N;
	float3 m_T = normalize(sd->dPdu);

	float cosNO = dot(m_N, I);
	float cosNI = dot(m_N, omega_in);

	if(cosNI > 0 && cosNO > 0) {
		// get half vector and get x,y basis on the surface for anisotropy
		float3 H = normalize(omega_in + I); // normalize needed for pdf
		float3 X, Y;
		make_orthonormals_tangent(m_N, m_T, &X, &Y);
		// eq. 4
		float dotx = dot(H, X) / m_ax;
		float doty = dot(H, Y) / m_ay;
		float dotn = dot(H, m_N);
		float exp_arg = (dotx * dotx + doty * doty) / (dotn * dotn);
		float denom = (4 * M_PI_F * m_ax * m_ay * sqrtf(cosNO * cosNI));
		float exp_val = expf(-exp_arg);
		float out = cosNI * exp_val / denom;
		float oh = dot(H, I);
		denom = 4 * M_PI_F * m_ax * m_ay * oh * dotn * dotn * dotn;
		*pdf = exp_val / denom;
		return make_float3 (out, out, out);
	}
	return make_float3 (0, 0, 0);
}

__device float3 bsdf_ward_eval_transmit(const ShaderData *sd, const ShaderClosure *sc, const float3 I, const float3 omega_in, float *pdf)
{
	return make_float3(0.0f, 0.0f, 0.0f);
}

__device float bsdf_ward_albedo(const ShaderData *sd, const ShaderClosure *sc, const float3 I)
{
	return 1.0f;
}

__device int bsdf_ward_sample(const ShaderData *sd, const ShaderClosure *sc, float randu, float randv, float3 *eval, float3 *omega_in, float3 *domega_in_dx, float3 *domega_in_dy, float *pdf)
{
	float m_ax = sc->data0;
	float m_ay = sc->data1;
	float3 m_N = sd->N;
	float3 m_T = normalize(sd->dPdu);

	float cosNO = dot(m_N, sd->I);
	if(cosNO > 0) {
		// get x,y basis on the surface for anisotropy
		float3 X, Y;
		make_orthonormals_tangent(m_N, m_T, &X, &Y);
		// generate random angles for the half vector
		// eq. 7 (taking care around discontinuities to keep
		//ttoutput angle in the right quadrant)
		// we take advantage of cos(atan(x)) == 1/sqrt(1+x^2)
		//tttt  and sin(atan(x)) == x/sqrt(1+x^2)
		float alphaRatio = m_ay / m_ax;
		float cosPhi, sinPhi;
		if(randu < 0.25f) {
			float val = 4 * randu;
			float tanPhi = alphaRatio * tanf(M_PI_2_F * val);
			cosPhi = 1 / sqrtf(1 + tanPhi * tanPhi);
			sinPhi = tanPhi * cosPhi;
		} else if(randu < 0.5f) {
			float val = 1 - 4 * (0.5f - randu);
			float tanPhi = alphaRatio * tanf(M_PI_2_F * val);
			// phi = M_PI_F - phi;
			cosPhi = -1 / sqrtf(1 + tanPhi * tanPhi);
			sinPhi = -tanPhi * cosPhi;
		} else if(randu < 0.75f) {
			float val = 4 * (randu - 0.5f);
			float tanPhi = alphaRatio * tanf(M_PI_2_F * val);
			//phi = M_PI_F + phi;
			cosPhi = -1 / sqrtf(1 + tanPhi * tanPhi);
			sinPhi = tanPhi * cosPhi;
		} else {
			float val = 1 - 4 * (1 - randu);
			float tanPhi = alphaRatio * tanf(M_PI_2_F * val);
			// phi = 2 * M_PI_F - phi;
			cosPhi = 1 / sqrtf(1 + tanPhi * tanPhi);
			sinPhi = -tanPhi * cosPhi;
		}
		// eq. 6
		// we take advantage of cos(atan(x)) == 1/sqrt(1+x^2)
		//tttt  and sin(atan(x)) == x/sqrt(1+x^2)
		float thetaDenom = (cosPhi * cosPhi) / (m_ax * m_ax) + (sinPhi * sinPhi) / (m_ay * m_ay);
		float tanTheta2 = -logf(1 - randv) / thetaDenom;
		float cosTheta  = 1 / sqrtf(1 + tanTheta2);
		float sinTheta  = cosTheta * sqrtf(tanTheta2);

		float3 h; // already normalized becaused expressed from spherical coordinates
		h.x = sinTheta * cosPhi;
		h.y = sinTheta * sinPhi;
		h.z = cosTheta;
		// compute terms that are easier in local space
		float dotx = h.x / m_ax;
		float doty = h.y / m_ay;
		float dotn = h.z;
		// transform to world space
		h = h.x * X + h.y * Y + h.z * m_N;
		// generate the final sample
		float oh = dot(h, sd->I);
		*omega_in = 2.0f * oh * h - sd->I;
		if(dot(sd->Ng, *omega_in) > 0) {
			float cosNI = dot(m_N, *omega_in);
			if(cosNI > 0) {
				// eq. 9
				float exp_arg = (dotx * dotx + doty * doty) / (dotn * dotn);
				float denom = 4 * M_PI_F * m_ax * m_ay * oh * dotn * dotn * dotn;
				*pdf = expf(-exp_arg) / denom;
				// compiler will reuse expressions already computed
				denom = (4 * M_PI_F * m_ax * m_ay * sqrtf(cosNO * cosNI));
				float power = cosNI * expf(-exp_arg) / denom;
				*eval = make_float3(power, power, power);
#ifdef __RAY_DIFFERENTIALS__
				*domega_in_dx = (2 * dot(m_N, sd->dI.dx)) * m_N - sd->dI.dx;
				*domega_in_dy = (2 * dot(m_N, sd->dI.dy)) * m_N - sd->dI.dy;
				// Since there is some blur to this reflection, make the
				// derivatives a bit bigger. In theory this varies with the
				// roughness but the exact relationship is complex and
				// requires more ops than are practical.
				*domega_in_dx *= 10.0f;
				*domega_in_dy *= 10.0f;
#endif
			}
		}
	}
	return LABEL_REFLECT|LABEL_GLOSSY;
}

CCL_NAMESPACE_END

#endif /* __BSDF_WARD_H__ */

