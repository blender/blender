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

#ifndef __BSDF_MICROFACET_H__
#define __BSDF_MICROFACET_H__

CCL_NAMESPACE_BEGIN

/* Approximate erf and erfinv implementations.
 * Implementation comes straight from Wikipedia:
 *
 * http://en.wikipedia.org/wiki/Error_function
 *
 * Some constants are baked into the code.
 */

ccl_device_inline float approx_erff_do(float x)
{
	/* Such a clamp doesn't give much distortion to the output value
	 * and gives quite a few of the speedup.
	 */
	if(x > 3.0f) {
		return 1.0f;
	}
	float t = 1.0f / (1.0f + 0.47047f*x);
	return  (1.0f -
	         t*(0.3480242f + t*(-0.0958798f + t*0.7478556f)) * expf(-x*x));
}

ccl_device_inline float approx_erff(float x)
{
	if(x >= 0.0f) {
		return approx_erff_do(x);
	}
	else {
		return -approx_erff_do(-x);
	}
}

ccl_device_inline float approx_erfinvf_do(float x)
{
	if(x <= 0.7f) {
		const float x2 = x * x;
		const float a1 =  0.886226899f;
		const float a2 = -1.645349621f;
		const float a3 =  0.914624893f;
		const float a4 = -0.140543331f;
		const float b1 = -2.118377725f;
		const float b2 =  1.442710462f;
		const float b3 = -0.329097515f;
		const float b4 =  0.012229801f;
		return x * (((a4 * x2 + a3) * x2 + a2) * x2 + a1) /
		          ((((b4 * x2 + b3) * x2 + b2) * x2 + b1) * x2 + 1.0f);
	}
	else {
		const float c1 = -1.970840454f;
		const float c2 = -1.624906493f;
		const float c3 =  3.429567803f;
		const float c4 =  1.641345311;
		const float d1 =  3.543889200f;
		const float d2 =  1.637067800f;
		const float z = sqrtf(-logf((1.0f - x) * 0.5f));
		return (((c4 * z + c3) * z + c2) * z + c1) /
		        ((d2 * z + d1) * z + 1.0f);
	}
}

ccl_device_inline float approx_erfinvf(float x)
{
	if(x >= 0.0f) {
		return approx_erfinvf_do(x);
	}
	else {
		return -approx_erfinvf_do(-x);
	}
}

/* Beckmann and GGX microfacet importance sampling from:
 * 
 * Importance Sampling Microfacet-Based BSDFs using the Distribution of Visible Normals.
 * E. Heitz and E. d'Eon, EGSR 2014 */

ccl_device_inline void microfacet_beckmann_sample_slopes(
	KernelGlobals *kg,
	const float cos_theta_i, const float sin_theta_i,
	float randu, float randv, float *slope_x, float *slope_y,
	float *G1i)
{
	/* special case (normal incidence) */
	if(cos_theta_i >= 0.99999f) {
		const float r = sqrtf(-logf(randu));
		const float phi = M_2PI_F * randv;
		*slope_x = r * cosf(phi);
		*slope_y = r * sinf(phi);
		*G1i = 1.0f;
		return;
	}

	/* precomputations */
	const float tan_theta_i = sin_theta_i/cos_theta_i;
	const float inv_a = tan_theta_i;
	const float a = 1.0f/inv_a;
	const float erf_a = approx_erff(a);
	const float exp_a2 = expf(-a*a);
	const float SQRT_PI_INV = 0.56418958354f;
	const float Lambda = 0.5f*(erf_a - 1.0f) + (0.5f*SQRT_PI_INV)*(exp_a2*inv_a);
	const float G1 = 1.0f/(1.0f + Lambda); /* masking */

	*G1i = G1;

#if 0
	const float C = 1.0f - G1 * erf_a;

	/* sample slope X */
	if(randu < C) {
		/* rescale randu */
		randu = randu / C;
		const float w_1 = 0.5f * SQRT_PI_INV * sin_theta_i * exp_a2;
		const float w_2 = cos_theta_i * (0.5f - 0.5f*erf_a);
		const float p = w_1 / (w_1 + w_2);

		if(randu < p) {
			randu = randu / p;
			*slope_x = -sqrtf(-logf(randu*exp_a2));
		}
		else {
			randu = (randu - p) / (1.0f - p);
			*slope_x = approx_erfinvf(randu - 1.0f - randu*erf_a);
		}
	}
	else {
		/* rescale randu */
		randu = (randu - C) / (1.0f - C);
		*slope_x = approx_erfinvf((-1.0f + 2.0f*randu)*erf_a);

		const float p = (-(*slope_x)*sin_theta_i + cos_theta_i) / (2.0f*cos_theta_i);

		if(randv > p) {
			*slope_x = -(*slope_x);
			randv = (randv - p) / (1.0f - p);
		}
		else
			randv = randv / p;
	}

	/* sample slope Y */
	*slope_y = approx_erfinvf(2.0f*randv - 1.0f);
#else
	/* use precomputed table, because it better preserves stratification
	 * of the random number pattern */
	int beckmann_table_offset = kernel_data.tables.beckmann_offset;

	*slope_x = lookup_table_read_2D(kg, randu, cos_theta_i,
		beckmann_table_offset, BECKMANN_TABLE_SIZE, BECKMANN_TABLE_SIZE);
	*slope_y = approx_erfinvf(2.0f*randv - 1.0f);
#endif

}

ccl_device_inline void microfacet_ggx_sample_slopes(
	const float cos_theta_i, const float sin_theta_i,
	float randu, float randv, float *slope_x, float *slope_y,
	float *G1i)
{
	/* special case (normal incidence) */
	if(cos_theta_i >= 0.99999f) {
		const float r = sqrtf(randu/(1.0f - randu));
		const float phi = M_2PI_F * randv;
		*slope_x = r * cosf(phi);
		*slope_y = r * sinf(phi);
		*G1i = 1.0f;

		return;
	}

	/* precomputations */
	const float tan_theta_i = sin_theta_i/cos_theta_i;
	const float G1_inv = 0.5f * (1.0f + safe_sqrtf(1.0f + tan_theta_i*tan_theta_i));

	*G1i = 1.0f/G1_inv;

	/* sample slope_x */
	const float A = 2.0f*randu*G1_inv - 1.0f;
	const float AA = A*A;
	const float tmp = 1.0f/(AA - 1.0f);
	const float B = tan_theta_i;
	const float BB = B*B;
	const float D = safe_sqrtf(BB*(tmp*tmp) - (AA - BB)*tmp);
	const float slope_x_1 = B*tmp - D;
	const float slope_x_2 = B*tmp + D;
	*slope_x = (A < 0.0f || slope_x_2*tan_theta_i > 1.0f)? slope_x_1: slope_x_2;

	/* sample slope_y */
	float S;

	if(randv > 0.5f) {
		S = 1.0f;
		randv = 2.0f*(randv - 0.5f);
	}
	else {
		S = -1.0f;
		randv = 2.0f*(0.5f - randv);
	}

	const float z = (randv*(randv*(randv*0.27385f - 0.73369f) + 0.46341f)) / (randv*(randv*(randv*0.093073f + 0.309420f) - 1.000000f) + 0.597999f);
	*slope_y = S * z * safe_sqrtf(1.0f + (*slope_x)*(*slope_x));
}

ccl_device_inline float3 microfacet_sample_stretched(
	KernelGlobals *kg, const float3 omega_i,
	const float alpha_x, const float alpha_y,
	const float randu, const float randv,
	bool beckmann, float *G1i)
{
	/* 1. stretch omega_i */
	float3 omega_i_ = make_float3(alpha_x * omega_i.x, alpha_y * omega_i.y, omega_i.z);
	omega_i_ = normalize(omega_i_);

	/* get polar coordinates of omega_i_ */
	float costheta_ = 1.0f;
	float sintheta_ = 0.0f;
	float cosphi_ = 1.0f;
	float sinphi_ = 0.0f;

	if(omega_i_.z < 0.99999f) {
		costheta_ = omega_i_.z;
		sintheta_ = safe_sqrtf(1.0f - costheta_*costheta_);

		float invlen = 1.0f/sintheta_;
		cosphi_ = omega_i_.x * invlen;
		sinphi_ = omega_i_.y * invlen;
	}

	/* 2. sample P22_{omega_i}(x_slope, y_slope, 1, 1) */
	float slope_x, slope_y;

	if(beckmann) {
		microfacet_beckmann_sample_slopes(kg, costheta_, sintheta_,
			randu, randv, &slope_x, &slope_y, G1i);
	}
	else {
		microfacet_ggx_sample_slopes(costheta_, sintheta_,
			randu, randv, &slope_x, &slope_y, G1i);
	}

	/* 3. rotate */
	float tmp = cosphi_*slope_x - sinphi_*slope_y;
	slope_y = sinphi_*slope_x + cosphi_*slope_y;
	slope_x = tmp;

	/* 4. unstretch */
	slope_x = alpha_x * slope_x;
	slope_y = alpha_y * slope_y;

	/* 5. compute normal */
	return normalize(make_float3(-slope_x, -slope_y, 1.0f));
} 

/* GGX microfacet with Smith shadow-masking from:
 *
 * Microfacet Models for Refraction through Rough Surfaces
 * B. Walter, S. R. Marschner, H. Li, K. E. Torrance, EGSR 2007
 *
 * Anisotropic from:
 *
 * Understanding the Masking-Shadowing Function in Microfacet-Based BRDFs
 * E. Heitz, Research Report 2014
 *
 * Anisotropy is only supported for reflection currently, but adding it for
 * transmission is just a matter of copying code from reflection if needed. */

ccl_device int bsdf_microfacet_ggx_setup(ShaderClosure *sc)
{
	sc->data0 = clamp(sc->data0, 0.0f, 1.0f); /* alpha_x */
	sc->data1 = sc->data0; /* alpha_y */
	
	sc->type = CLOSURE_BSDF_MICROFACET_GGX_ID;

	return SD_BSDF|SD_BSDF_HAS_EVAL|SD_BSDF_GLOSSY;
}

ccl_device int bsdf_microfacet_ggx_aniso_setup(ShaderClosure *sc)
{
	sc->data0 = clamp(sc->data0, 0.0f, 1.0f); /* alpha_x */
	sc->data1 = clamp(sc->data1, 0.0f, 1.0f); /* alpha_y */
	
	sc->type = CLOSURE_BSDF_MICROFACET_GGX_ANISO_ID;

	return SD_BSDF|SD_BSDF_HAS_EVAL|SD_BSDF_GLOSSY;
}

ccl_device int bsdf_microfacet_ggx_refraction_setup(ShaderClosure *sc)
{
	sc->data0 = clamp(sc->data0, 0.0f, 1.0f); /* alpha_x */
	sc->data1 = sc->data0; /* alpha_y */

	sc->type = CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID;

	return SD_BSDF|SD_BSDF_HAS_EVAL|SD_BSDF_GLOSSY;
}

ccl_device void bsdf_microfacet_ggx_blur(ShaderClosure *sc, float roughness)
{
	sc->data0 = fmaxf(roughness, sc->data0); /* alpha_x */
	sc->data1 = fmaxf(roughness, sc->data1); /* alpha_y */
}

ccl_device float3 bsdf_microfacet_ggx_eval_reflect(const ShaderClosure *sc, const float3 I, const float3 omega_in, float *pdf)
{
	float alpha_x = sc->data0;
	float alpha_y = sc->data1;
	int m_refractive = sc->type == CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID;
	float3 N = sc->N;

	if(m_refractive || fmaxf(alpha_x, alpha_y) <= 1e-4f)
		return make_float3(0, 0, 0);

	float cosNO = dot(N, I);
	float cosNI = dot(N, omega_in);

	if(cosNI > 0 && cosNO > 0) {
		/* get half vector */
		float3 m = normalize(omega_in + I);
		float alpha2 = alpha_x * alpha_y;
		float D, G1o, G1i;

		if(alpha_x == alpha_y) {
			/* isotropic
			 * eq. 20: (F*G*D)/(4*in*on)
			 * eq. 33: first we calculate D(m) */
			float cosThetaM = dot(N, m);
			float cosThetaM2 = cosThetaM * cosThetaM;
			float cosThetaM4 = cosThetaM2 * cosThetaM2;
			float tanThetaM2 = (1 - cosThetaM2) / cosThetaM2;
			D = alpha2 / (M_PI_F * cosThetaM4 * (alpha2 + tanThetaM2) * (alpha2 + tanThetaM2));

			/* eq. 34: now calculate G1(i,m) and G1(o,m) */
			G1o = 2 / (1 + safe_sqrtf(1 + alpha2 * (1 - cosNO * cosNO) / (cosNO * cosNO)));
			G1i = 2 / (1 + safe_sqrtf(1 + alpha2 * (1 - cosNI * cosNI) / (cosNI * cosNI))); 
		}
		else {
			/* anisotropic */
			float3 X, Y, Z = N;
			make_orthonormals_tangent(Z, sc->T, &X, &Y);

			/* distribution */
			float3 local_m = make_float3(dot(X, m), dot(Y, m), dot(Z, m));
			float slope_x = -local_m.x/(local_m.z*alpha_x);
			float slope_y = -local_m.y/(local_m.z*alpha_y);
			float slope_len = 1 + slope_x*slope_x + slope_y*slope_y;

			float cosThetaM = local_m.z;
			float cosThetaM2 = cosThetaM * cosThetaM;
			float cosThetaM4 = cosThetaM2 * cosThetaM2;

			D = 1 / ((slope_len * slope_len) * M_PI_F * alpha2 * cosThetaM4);

			/* G1(i,m) and G1(o,m) */
			float tanThetaO2 = (1 - cosNO * cosNO) / (cosNO * cosNO);
			float cosPhiO = dot(I, X);
			float sinPhiO = dot(I, Y);

			float alphaO2 = (cosPhiO*cosPhiO)*(alpha_x*alpha_x) + (sinPhiO*sinPhiO)*(alpha_y*alpha_y);
			alphaO2 /= cosPhiO*cosPhiO + sinPhiO*sinPhiO;

			G1o = 2 / (1 + safe_sqrtf(1 + alphaO2 * tanThetaO2));

			float tanThetaI2 = (1 - cosNI * cosNI) / (cosNI * cosNI);
			float cosPhiI = dot(omega_in, X);
			float sinPhiI = dot(omega_in, Y);

			float alphaI2 = (cosPhiI*cosPhiI)*(alpha_x*alpha_x) + (sinPhiI*sinPhiI)*(alpha_y*alpha_y);
			alphaI2 /= cosPhiI*cosPhiI + sinPhiI*sinPhiI;

			G1i = 2 / (1 + safe_sqrtf(1 + alphaI2 * tanThetaI2));
		}

		float G = G1o * G1i;

		/* eq. 20 */
		float common = D * 0.25f / cosNO;
		float out = G * common;

		/* eq. 2 in distribution of visible normals sampling
		 * pm = Dw = G1o * dot(m, I) * D / dot(N, I); */

		/* eq. 38 - but see also:
		 * eq. 17 in http://www.graphics.cornell.edu/~bjw/wardnotes.pdf
		 * pdf = pm * 0.25 / dot(m, I); */
		*pdf = G1o * common;

		return make_float3(out, out, out);
	}

	return make_float3(0, 0, 0);
}

ccl_device float3 bsdf_microfacet_ggx_eval_transmit(const ShaderClosure *sc, const float3 I, const float3 omega_in, float *pdf)
{
	float alpha_x = sc->data0;
	float alpha_y = sc->data1;
	float m_eta = sc->data2;
	int m_refractive = sc->type == CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID;
	float3 N = sc->N;

	if(!m_refractive || fmaxf(alpha_x, alpha_y) <= 1e-4f)
		return make_float3(0, 0, 0);

	float cosNO = dot(N, I);
	float cosNI = dot(N, omega_in);

	if(cosNO <= 0 || cosNI >= 0)
		return make_float3(0, 0, 0); /* vectors on same side -- not possible */

	/* compute half-vector of the refraction (eq. 16) */
	float3 ht = -(m_eta * omega_in + I);
	float3 Ht = normalize(ht);
	float cosHO = dot(Ht, I);
	float cosHI = dot(Ht, omega_in);

	/* those situations makes chi+ terms in eq. 33, 34 be zero */
	if(dot(Ht, N) <= 0.0f || cosHO * cosNO <= 0.0f || cosHI * cosNI <= 0.0f)
		return make_float3(0.0f, 0.0f, 0.0f);

	float D, G1o, G1i;

	/* eq. 33: first we calculate D(m) with m=Ht: */
	float alpha2 = alpha_x * alpha_y;
	float cosThetaM = dot(N, Ht);
	float cosThetaM2 = cosThetaM * cosThetaM;
	float tanThetaM2 = (1 - cosThetaM2) / cosThetaM2;
	float cosThetaM4 = cosThetaM2 * cosThetaM2;
	D = alpha2 / (M_PI_F * cosThetaM4 * (alpha2 + tanThetaM2) * (alpha2 + tanThetaM2));

	/* eq. 34: now calculate G1(i,m) and G1(o,m) */
	G1o = 2 / (1 + safe_sqrtf(1 + alpha2 * (1 - cosNO * cosNO) / (cosNO * cosNO)));
	G1i = 2 / (1 + safe_sqrtf(1 + alpha2 * (1 - cosNI * cosNI) / (cosNI * cosNI))); 

	float G = G1o * G1i;

	/* probability */
	float Ht2 = dot(ht, ht);

	/* eq. 2 in distribution of visible normals sampling
	 * pm = Dw = G1o * dot(m, I) * D / dot(N, I); */

	/* out = fabsf(cosHI * cosHO) * (m_eta * m_eta) * G * D / (cosNO * Ht2)
	 * pdf = pm * (m_eta * m_eta) * fabsf(cosHI) / Ht2 */
	float common = D * (m_eta * m_eta) / (cosNO * Ht2);
	float out = G * fabsf(cosHI * cosHO) * common;
	*pdf = G1o * cosHO * fabsf(cosHI) * common;

	return make_float3(out, out, out);
}

ccl_device int bsdf_microfacet_ggx_sample(KernelGlobals *kg, const ShaderClosure *sc, float3 Ng, float3 I, float3 dIdx, float3 dIdy, float randu, float randv, float3 *eval, float3 *omega_in, float3 *domega_in_dx, float3 *domega_in_dy, float *pdf)
{
	float alpha_x = sc->data0;
	float alpha_y = sc->data1;
	int m_refractive = sc->type == CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID;
	float3 N = sc->N;

	float cosNO = dot(N, I);
	if(cosNO > 0) {
		float3 X, Y, Z = N;

		if(alpha_x == alpha_y)
			make_orthonormals(Z, &X, &Y);
		else
			make_orthonormals_tangent(Z, sc->T, &X, &Y);

		/* importance sampling with distribution of visible normals. vectors are
		 * transformed to local space before and after */
		float3 local_I = make_float3(dot(X, I), dot(Y, I), cosNO);
		float3 local_m;
		float G1o;

		local_m = microfacet_sample_stretched(kg, local_I, alpha_x, alpha_y,
			randu, randv, false, &G1o);

		float3 m = X*local_m.x + Y*local_m.y + Z*local_m.z;
		float cosThetaM = local_m.z;

		/* reflection or refraction? */
		if(!m_refractive) {
			float cosMO = dot(m, I);

			if(cosMO > 0) {
				/* eq. 39 - compute actual reflected direction */
				*omega_in = 2 * cosMO * m - I;

				if(dot(Ng, *omega_in) > 0) {
					if(fmaxf(alpha_x, alpha_y) <= 1e-4f) {
						/* some high number for MIS */
						*pdf = 1e6f;
						*eval = make_float3(1e6f, 1e6f, 1e6f);
					}
					else {
						/* microfacet normal is visible to this ray */
						/* eq. 33 */
						float alpha2 = alpha_x * alpha_y;
						float D, G1i;

						if(alpha_x == alpha_y) {
							/* isotropic */
							float cosThetaM2 = cosThetaM * cosThetaM;
							float cosThetaM4 = cosThetaM2 * cosThetaM2;
							float tanThetaM2 = 1/(cosThetaM2) - 1;
							D = alpha2 / (M_PI_F * cosThetaM4 * (alpha2 + tanThetaM2) * (alpha2 + tanThetaM2));

							/* eval BRDF*cosNI */
							float cosNI = dot(N, *omega_in);

							/* eq. 34: now calculate G1(i,m) */
							G1i = 2 / (1 + safe_sqrtf(1 + alpha2 * (1 - cosNI * cosNI) / (cosNI * cosNI))); 
						}
						else {
							/* anisotropic distribution */
							float3 local_m = make_float3(dot(X, m), dot(Y, m), dot(Z, m));
							float slope_x = -local_m.x/(local_m.z*alpha_x);
							float slope_y = -local_m.y/(local_m.z*alpha_y);
							float slope_len = 1 + slope_x*slope_x + slope_y*slope_y;

							float cosThetaM = local_m.z;
							float cosThetaM2 = cosThetaM * cosThetaM;
							float cosThetaM4 = cosThetaM2 * cosThetaM2;

							D = 1 / ((slope_len * slope_len) * M_PI_F * alpha2 * cosThetaM4);

							/* calculate G1(i,m) */
							float cosNI = dot(N, *omega_in);

							float tanThetaI2 = (1 - cosNI * cosNI) / (cosNI * cosNI);
							float cosPhiI = dot(*omega_in, X);
							float sinPhiI = dot(*omega_in, Y);

							float alphaI2 = (cosPhiI*cosPhiI)*(alpha_x*alpha_x) + (sinPhiI*sinPhiI)*(alpha_y*alpha_y);
							alphaI2 /= cosPhiI*cosPhiI + sinPhiI*sinPhiI;

							G1i = 2 / (1 + safe_sqrtf(1 + alphaI2 * tanThetaI2));
						}

						/* see eval function for derivation */
						float common = (G1o * D) * 0.25f / cosNO;
						float out = G1i * common;
						*pdf = common;

						*eval = make_float3(out, out, out);
					}

#ifdef __RAY_DIFFERENTIALS__
					*domega_in_dx = (2 * dot(m, dIdx)) * m - dIdx;
					*domega_in_dy = (2 * dot(m, dIdy)) * m - dIdy;
#endif
				}
			}
		}
		else {
			/* CAUTION: the i and o variables are inverted relative to the paper
			 * eq. 39 - compute actual refractive direction */
			float3 R, T;
#ifdef __RAY_DIFFERENTIALS__
			float3 dRdx, dRdy, dTdx, dTdy;
#endif
			float m_eta = sc->data2;
			bool inside;

			fresnel_dielectric(m_eta, m, I, &R, &T,
#ifdef __RAY_DIFFERENTIALS__
				dIdx, dIdy, &dRdx, &dRdy, &dTdx, &dTdy,
#endif
				&inside);
			
			if(!inside) {

				*omega_in = T;
#ifdef __RAY_DIFFERENTIALS__
				*domega_in_dx = dTdx;
				*domega_in_dy = dTdy;
#endif

				if(fmaxf(alpha_x, alpha_y) <= 1e-4f || fabsf(m_eta - 1.0f) < 1e-4f) {
					/* some high number for MIS */
					*pdf = 1e6f;
					*eval = make_float3(1e6f, 1e6f, 1e6f);
				}
				else {
					/* eq. 33 */
					float alpha2 = alpha_x * alpha_y;
					float cosThetaM2 = cosThetaM * cosThetaM;
					float cosThetaM4 = cosThetaM2 * cosThetaM2;
					float tanThetaM2 = 1/(cosThetaM2) - 1;
					float D = alpha2 / (M_PI_F * cosThetaM4 * (alpha2 + tanThetaM2) * (alpha2 + tanThetaM2));

					/* eval BRDF*cosNI */
					float cosNI = dot(N, *omega_in);

					/* eq. 34: now calculate G1(i,m) */
					float G1i = 2 / (1 + safe_sqrtf(1 + alpha2 * (1 - cosNI * cosNI) / (cosNI * cosNI))); 

					/* eq. 21 */
					float cosHI = dot(m, *omega_in);
					float cosHO = dot(m, I);
					float Ht2 = m_eta * cosHI + cosHO;
					Ht2 *= Ht2;

					/* see eval function for derivation */
					float common = (G1o * D) * (m_eta * m_eta) / (cosNO * Ht2);
					float out = G1i * fabsf(cosHI * cosHO) * common;
					*pdf = cosHO * fabsf(cosHI) * common;

					*eval = make_float3(out, out, out);
				}
			}
		}
	}
	return (m_refractive) ? LABEL_TRANSMIT|LABEL_GLOSSY : LABEL_REFLECT|LABEL_GLOSSY;
}

/* Beckmann microfacet with Smith shadow-masking from:
 *
 * Microfacet Models for Refraction through Rough Surfaces
 * B. Walter, S. R. Marschner, H. Li, K. E. Torrance, EGSR 2007 */

ccl_device int bsdf_microfacet_beckmann_setup(ShaderClosure *sc)
{
	sc->data0 = clamp(sc->data0, 0.0f, 1.0f); /* alpha_x */
	sc->data1 = sc->data0; /* alpha_y */

	sc->type = CLOSURE_BSDF_MICROFACET_BECKMANN_ID;
	return SD_BSDF|SD_BSDF_HAS_EVAL|SD_BSDF_GLOSSY;
}

ccl_device int bsdf_microfacet_beckmann_aniso_setup(ShaderClosure *sc)
{
	sc->data0 = clamp(sc->data0, 0.0f, 1.0f); /* alpha_x */
	sc->data1 = clamp(sc->data1, 0.0f, 1.0f); /* alpha_y */

	sc->type = CLOSURE_BSDF_MICROFACET_BECKMANN_ANISO_ID;
	return SD_BSDF|SD_BSDF_HAS_EVAL|SD_BSDF_GLOSSY;
}

ccl_device int bsdf_microfacet_beckmann_refraction_setup(ShaderClosure *sc)
{
	sc->data0 = clamp(sc->data0, 0.0f, 1.0f); /* alpha_x */
	sc->data1 = sc->data0; /* alpha_y */

	sc->type = CLOSURE_BSDF_MICROFACET_BECKMANN_REFRACTION_ID;
	return SD_BSDF|SD_BSDF_HAS_EVAL|SD_BSDF_GLOSSY;
}

ccl_device void bsdf_microfacet_beckmann_blur(ShaderClosure *sc, float roughness)
{
	sc->data0 = fmaxf(roughness, sc->data0); /* alpha_x */
	sc->data1 = fmaxf(roughness, sc->data1); /* alpha_y */
}

ccl_device float3 bsdf_microfacet_beckmann_eval_reflect(const ShaderClosure *sc, const float3 I, const float3 omega_in, float *pdf)
{
	float alpha_x = sc->data0;
	float alpha_y = sc->data1;
	int m_refractive = sc->type == CLOSURE_BSDF_MICROFACET_BECKMANN_REFRACTION_ID;
	float3 N = sc->N;

	if(m_refractive || fmaxf(alpha_x, alpha_y) <= 1e-4f)
		return make_float3(0, 0, 0);

	float cosNO = dot(N, I);
	float cosNI = dot(N, omega_in);

	if(cosNO > 0 && cosNI > 0) {
		/* get half vector */
		float3 m = normalize(omega_in + I);

		float alpha2 = alpha_x * alpha_y;
		float D, G1o, G1i;

		if(alpha_x == alpha_y) {
			/* isotropic
			 * eq. 20: (F*G*D)/(4*in*on)
			 * eq. 25: first we calculate D(m) */
			float cosThetaM = dot(N, m);
			float cosThetaM2 = cosThetaM * cosThetaM;
			float tanThetaM2 = (1 - cosThetaM2) / cosThetaM2;
			float cosThetaM4 = cosThetaM2 * cosThetaM2;
			D = expf(-tanThetaM2 / alpha2) / (M_PI_F * alpha2 * cosThetaM4);

			/* eq. 26, 27: now calculate G1(i,m) and G1(o,m) */
			float ao = 1 / (alpha_x * safe_sqrtf((1 - cosNO * cosNO) / (cosNO * cosNO)));
			float ai = 1 / (alpha_x * safe_sqrtf((1 - cosNI * cosNI) / (cosNI * cosNI)));
			G1o = ao < 1.6f ? (3.535f * ao + 2.181f * ao * ao) / (1 + 2.276f * ao + 2.577f * ao * ao) : 1.0f;
			G1i = ai < 1.6f ? (3.535f * ai + 2.181f * ai * ai) / (1 + 2.276f * ai + 2.577f * ai * ai) : 1.0f;
		}
		else {
			/* anisotropic */
			float3 X, Y, Z = N;
			make_orthonormals_tangent(Z, sc->T, &X, &Y);

			/* distribution */
			float3 local_m = make_float3(dot(X, m), dot(Y, m), dot(Z, m));
			float slope_x = -local_m.x/(local_m.z*alpha_x);
			float slope_y = -local_m.y/(local_m.z*alpha_y);

			float cosThetaM = local_m.z;
			float cosThetaM2 = cosThetaM * cosThetaM;
			float cosThetaM4 = cosThetaM2 * cosThetaM2;

			D = expf(-slope_x*slope_x - slope_y*slope_y) / (M_PI_F * alpha2 * cosThetaM4);

			/* G1(i,m) and G1(o,m) */
			float tanThetaO2 = (1 - cosNO * cosNO) / (cosNO * cosNO);
			float cosPhiO = dot(I, X);
			float sinPhiO = dot(I, Y);

			float alphaO2 = (cosPhiO*cosPhiO)*(alpha_x*alpha_x) + (sinPhiO*sinPhiO)*(alpha_y*alpha_y);
			alphaO2 /= cosPhiO*cosPhiO + sinPhiO*sinPhiO;

			float tanThetaI2 = (1 - cosNI * cosNI) / (cosNI * cosNI);
			float cosPhiI = dot(omega_in, X);
			float sinPhiI = dot(omega_in, Y);

			float alphaI2 = (cosPhiI*cosPhiI)*(alpha_x*alpha_x) + (sinPhiI*sinPhiI)*(alpha_y*alpha_y);
			alphaI2 /= cosPhiI*cosPhiI + sinPhiI*sinPhiI;

			float ao = 1 / (safe_sqrtf(alphaO2 * tanThetaO2));
			float ai = 1 / (safe_sqrtf(alphaI2 * tanThetaI2));
			G1o = ao < 1.6f ? (3.535f * ao + 2.181f * ao * ao) / (1 + 2.276f * ao + 2.577f * ao * ao) : 1.0f;
			G1i = ai < 1.6f ? (3.535f * ai + 2.181f * ai * ai) / (1 + 2.276f * ai + 2.577f * ai * ai) : 1.0f;
		}

		float G = G1o * G1i;

		/* eq. 20 */
		float common = D * 0.25f / cosNO;
		float out = G * common;

		/* eq. 2 in distribution of visible normals sampling
		 * pm = Dw = G1o * dot(m, I) * D / dot(N, I); */

		/* eq. 38 - but see also:
		 * eq. 17 in http://www.graphics.cornell.edu/~bjw/wardnotes.pdf
		 * pdf = pm * 0.25 / dot(m, I); */
		*pdf = G1o * common;

		return make_float3(out, out, out);
	}

	return make_float3(0, 0, 0);
}

ccl_device float3 bsdf_microfacet_beckmann_eval_transmit(const ShaderClosure *sc, const float3 I, const float3 omega_in, float *pdf)
{
	float alpha_x = sc->data0;
	float alpha_y = sc->data1;
	float m_eta = sc->data2;
	int m_refractive = sc->type == CLOSURE_BSDF_MICROFACET_BECKMANN_REFRACTION_ID;
	float3 N = sc->N;

	if(!m_refractive || fmaxf(alpha_x, alpha_y) <= 1e-4f)
		return make_float3(0, 0, 0);

	float cosNO = dot(N, I);
	float cosNI = dot(N, omega_in);

	if(cosNO <= 0 || cosNI >= 0)
		return make_float3(0, 0, 0);

	/* compute half-vector of the refraction (eq. 16) */
	float3 ht = -(m_eta * omega_in + I);
	float3 Ht = normalize(ht);
	float cosHO = dot(Ht, I);
	float cosHI = dot(Ht, omega_in);

	/* those situations makes chi+ terms in eq. 25, 27 be zero */
	if(dot(Ht, N) <= 0.0f || cosHO * cosNO <= 0.0f || cosHI * cosNI <= 0.0f)
		return make_float3(0.0f, 0.0f, 0.0f);

	/* eq. 25: first we calculate D(m) with m=Ht: */
	float alpha2 = alpha_x * alpha_y;
	float cosThetaM = min(dot(N, Ht), 1.0f);
	float cosThetaM2 = cosThetaM * cosThetaM;
	float tanThetaM2 = (1 - cosThetaM2) / cosThetaM2;
	float cosThetaM4 = cosThetaM2 * cosThetaM2;
	float D = expf(-tanThetaM2 / alpha2) / (M_PI_F * alpha2 *  cosThetaM4);

	/* eq. 26, 27: now calculate G1(i,m) and G1(o,m) */
	float ao = 1 / (alpha_x * safe_sqrtf((1 - cosNO * cosNO) / (cosNO * cosNO)));
	float ai = 1 / (alpha_x * safe_sqrtf((1 - cosNI * cosNI) / (cosNI * cosNI)));
	float G1o = ao < 1.6f ? (3.535f * ao + 2.181f * ao * ao) / (1 + 2.276f * ao + 2.577f * ao * ao) : 1.0f;
	float G1i = ai < 1.6f ? (3.535f * ai + 2.181f * ai * ai) / (1 + 2.276f * ai + 2.577f * ai * ai) : 1.0f;
	float G = G1o * G1i;

	/* probability */
	float Ht2 = dot(ht, ht);

	/* eq. 2 in distribution of visible normals sampling
	 * pm = Dw = G1o * dot(m, I) * D / dot(N, I); */

	/* out = fabsf(cosHI * cosHO) * (m_eta * m_eta) * G * D / (cosNO * Ht2)
	 * pdf = pm * (m_eta * m_eta) * fabsf(cosHI) / Ht2 */
	float common = D * (m_eta * m_eta) / (cosNO * Ht2);
	float out = G * fabsf(cosHI * cosHO) * common;
	*pdf = G1o * cosHO * fabsf(cosHI) * common;

	return make_float3(out, out, out);
}

ccl_device int bsdf_microfacet_beckmann_sample(KernelGlobals *kg, const ShaderClosure *sc, float3 Ng, float3 I, float3 dIdx, float3 dIdy, float randu, float randv, float3 *eval, float3 *omega_in, float3 *domega_in_dx, float3 *domega_in_dy, float *pdf)
{
	float alpha_x = sc->data0;
	float alpha_y = sc->data1;
	int m_refractive = sc->type == CLOSURE_BSDF_MICROFACET_BECKMANN_REFRACTION_ID;
	float3 N = sc->N;

	float cosNO = dot(N, I);
	if(cosNO > 0) {
		float3 X, Y, Z = N;

		if(alpha_x == alpha_y)
			make_orthonormals(Z, &X, &Y);
		else
			make_orthonormals_tangent(Z, sc->T, &X, &Y);

		/* importance sampling with distribution of visible normals. vectors are
		 * transformed to local space before and after */
		float3 local_I = make_float3(dot(X, I), dot(Y, I), cosNO);
		float3 local_m;
		float G1o;

		local_m = microfacet_sample_stretched(kg, local_I, alpha_x, alpha_x,
			randu, randv, true, &G1o);

		float3 m = X*local_m.x + Y*local_m.y + Z*local_m.z;
		float cosThetaM = local_m.z;

		/* reflection or refraction? */
		if(!m_refractive) {
			float cosMO = dot(m, I);

			if(cosMO > 0) {
				/* eq. 39 - compute actual reflected direction */
				*omega_in = 2 * cosMO * m - I;

				if(dot(Ng, *omega_in) > 0) {
					if(fmaxf(alpha_x, alpha_y) <= 1e-4f) {
						/* some high number for MIS */
						*pdf = 1e6f;
						*eval = make_float3(1e6f, 1e6f, 1e6f);
					}
					else {
						/* microfacet normal is visible to this ray
						 * eq. 25 */
						float alpha2 = alpha_x * alpha_y;
						float D, G1i;

						if(alpha_x == alpha_y) {
							/* istropic distribution */
							float cosThetaM2 = cosThetaM * cosThetaM;
							float cosThetaM4 = cosThetaM2 * cosThetaM2;
							float tanThetaM2 = 1/(cosThetaM2) - 1;
							D = expf(-tanThetaM2 / alpha2) / (M_PI_F * alpha2 *  cosThetaM4);

							/* eval BRDF*cosNI */
							float cosNI = dot(N, *omega_in);

							/* eq. 26, 27: now calculate G1(i,m) */
							float ai = 1 / (alpha_x * safe_sqrtf((1 - cosNI * cosNI) / (cosNI * cosNI)));
							G1i = ai < 1.6f ? (3.535f * ai + 2.181f * ai * ai) / (1 + 2.276f * ai + 2.577f * ai * ai) : 1.0f;
						}
						else {
							/* anisotropic distribution */
							float3 local_m = make_float3(dot(X, m), dot(Y, m), dot(Z, m));
							float slope_x = -local_m.x/(local_m.z*alpha_x);
							float slope_y = -local_m.y/(local_m.z*alpha_y);

							float cosThetaM = local_m.z;
							float cosThetaM2 = cosThetaM * cosThetaM;
							float cosThetaM4 = cosThetaM2 * cosThetaM2;

							D = expf(-slope_x*slope_x - slope_y*slope_y) / (M_PI_F * alpha2 * cosThetaM4);

							/* G1(i,m) */
							float cosNI = dot(N, *omega_in);
							float tanThetaI2 = (1 - cosNI * cosNI) / (cosNI * cosNI);
							float cosPhiI = dot(*omega_in, X);
							float sinPhiI = dot(*omega_in, Y);

							float alphaI2 = (cosPhiI*cosPhiI)*(alpha_x*alpha_x) + (sinPhiI*sinPhiI)*(alpha_y*alpha_y);
							alphaI2 /= cosPhiI*cosPhiI + sinPhiI*sinPhiI;

							float ai = 1 / (safe_sqrtf(alphaI2 * tanThetaI2));
							G1i = ai < 1.6f ? (3.535f * ai + 2.181f * ai * ai) / (1 + 2.276f * ai + 2.577f * ai * ai) : 1.0f;
						}

						float G = G1o * G1i;

						/* see eval function for derivation */
						float common = D * 0.25f / cosNO;
						float out = G * common;
						*pdf = G1o * common;

						*eval = make_float3(out, out, out);
					}

#ifdef __RAY_DIFFERENTIALS__
					*domega_in_dx = (2 * dot(m, dIdx)) * m - dIdx;
					*domega_in_dy = (2 * dot(m, dIdy)) * m - dIdy;
#endif
				}
			}
		}
		else {
			/* CAUTION: the i and o variables are inverted relative to the paper
			 * eq. 39 - compute actual refractive direction */
			float3 R, T;
#ifdef __RAY_DIFFERENTIALS__
			float3 dRdx, dRdy, dTdx, dTdy;
#endif
			float m_eta = sc->data2;
			bool inside;

			fresnel_dielectric(m_eta, m, I, &R, &T,
#ifdef __RAY_DIFFERENTIALS__
				dIdx, dIdy, &dRdx, &dRdy, &dTdx, &dTdy,
#endif
				&inside);

			if(!inside) {
				*omega_in = T;

#ifdef __RAY_DIFFERENTIALS__
				*domega_in_dx = dTdx;
				*domega_in_dy = dTdy;
#endif

				if(fmaxf(alpha_x, alpha_y) <= 1e-4f || fabsf(m_eta - 1.0f) < 1e-4f) {
					/* some high number for MIS */
					*pdf = 1e6f;
					*eval = make_float3(1e6f, 1e6f, 1e6f);
				}
				else {
					/* eq. 33 */
					float alpha2 = alpha_x * alpha_y;
					float cosThetaM2 = cosThetaM * cosThetaM;
					float cosThetaM4 = cosThetaM2 * cosThetaM2;
					float tanThetaM2 = 1/(cosThetaM2) - 1;
					float D = expf(-tanThetaM2 / alpha2) / (M_PI_F * alpha2 *  cosThetaM4);

					/* eval BRDF*cosNI */
					float cosNI = dot(N, *omega_in);

					/* eq. 26, 27: now calculate G1(i,m) */
					float ai = 1 / (alpha_x * safe_sqrtf((1 - cosNI * cosNI) / (cosNI * cosNI)));
					float G1i = ai < 1.6f ? (3.535f * ai + 2.181f * ai * ai) / (1 + 2.276f * ai + 2.577f * ai * ai) : 1.0f;
					float G = G1o * G1i;

					/* eq. 21 */
					float cosHI = dot(m, *omega_in);
					float cosHO = dot(m, I);
					float Ht2 = m_eta * cosHI + cosHO;
					Ht2 *= Ht2;

					/* see eval function for derivation */
					float common = D * (m_eta * m_eta) / (cosNO * Ht2);
					float out = G * fabsf(cosHI * cosHO) * common;
					*pdf = G1o * cosHO * fabsf(cosHI) * common;

					*eval = make_float3(out, out, out);
				}
			}
		}
	}
	return (m_refractive) ? LABEL_TRANSMIT|LABEL_GLOSSY : LABEL_REFLECT|LABEL_GLOSSY;
}

CCL_NAMESPACE_END

#endif /* __BSDF_MICROFACET_H__ */

