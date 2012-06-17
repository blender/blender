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

#include <OpenImageIO/fmath.h>

#include <OSL/genclosure.h>

#include "osl_closures.h"

#include "util_math.h"

using namespace OSL;

CCL_NAMESPACE_BEGIN

// TODO: fresnel_dielectric is only used for derivatives, could be optimized

// TODO: refactor these two classes so they share everything by the microfacet
//       distribution terms

// microfacet model with GGX facet distribution
// see http://www.graphics.cornell.edu/~bjw/microfacetbsdf.pdf
template <int Refractive = 0>
class MicrofacetGGXClosure : public BSDFClosure {
public:
	Vec3 m_N;
	float m_ag;   // width parameter (roughness)
	float m_eta;  // index of refraction (for fresnel term)
	MicrofacetGGXClosure() : BSDFClosure(Labels::GLOSSY, Refractive ? Back : Front) { m_eta = 1.0f; }

	void setup()
	{
		m_ag = clamp(m_ag, 1e-5f, 1.0f);
	}

	bool mergeable(const ClosurePrimitive *other) const {
		const MicrofacetGGXClosure *comp = (const MicrofacetGGXClosure *)other;
		return m_N == comp->m_N && m_ag == comp->m_ag &&
		       m_eta == comp->m_eta && BSDFClosure::mergeable(other);
	}

	size_t memsize() const { return sizeof(*this); }

	const char *name() const {
		return Refractive ? "microfacet_ggx_refraction" : "microfacet_ggx";
	}

	void print_on(std::ostream &out) const {
		out << name() << " (";
		out << "(" << m_N[0] << ", " << m_N[1] << ", " << m_N[2] << "), ";
		out << m_ag << ", ";
		out << m_eta;
		out << ")";
	}

	float albedo(const Vec3 &omega_out) const
	{
		return 1.0f;
	}

	Color3 eval_reflect(const Vec3 &omega_out, const Vec3 &omega_in, float& pdf) const
	{
		if (Refractive == 1) return Color3(0, 0, 0);
		float cosNO = m_N.dot(omega_out);
		float cosNI = m_N.dot(omega_in);
		if (cosNI > 0 && cosNO > 0) {
			// get half vector
			Vec3 Hr = omega_in + omega_out;
			Hr.normalize();
			// eq. 20: (F*G*D)/(4*in*on)
			// eq. 33: first we calculate D(m) with m=Hr:
			float alpha2 = m_ag * m_ag;
			float cosThetaM = m_N.dot(Hr);
			float cosThetaM2 = cosThetaM * cosThetaM;
			float tanThetaM2 = (1 - cosThetaM2) / cosThetaM2;
			float cosThetaM4 = cosThetaM2 * cosThetaM2;
			float D = alpha2 / ((float) M_PI * cosThetaM4 * (alpha2 + tanThetaM2) * (alpha2 + tanThetaM2));
			// eq. 34: now calculate G1(i,m) and G1(o,m)
			float G1o = 2 / (1 + sqrtf(1 + alpha2 * (1 - cosNO * cosNO) / (cosNO * cosNO)));
			float G1i = 2 / (1 + sqrtf(1 + alpha2 * (1 - cosNI * cosNI) / (cosNI * cosNI)));
			float G = G1o * G1i;
			float out = (G * D) * 0.25f / cosNO;
			// eq. 24
			float pm = D * cosThetaM;
			// convert into pdf of the sampled direction
			// eq. 38 - but see also:
			// eq. 17 in http://www.graphics.cornell.edu/~bjw/wardnotes.pdf
			pdf = pm * 0.25f / Hr.dot(omega_out);
			return Color3(out, out, out);
		}
		return Color3(0, 0, 0);
	}

	Color3 eval_transmit(const Vec3 &omega_out, const Vec3 &omega_in, float& pdf) const
	{
		if (Refractive == 0) return Color3(0, 0, 0);
		float cosNO = m_N.dot(omega_out);
		float cosNI = m_N.dot(omega_in);
		if (cosNO <= 0 || cosNI >= 0)
			return Color3(0, 0, 0);  // vectors on same side -- not possible
		// compute half-vector of the refraction (eq. 16)
		Vec3 ht = -(m_eta * omega_in + omega_out);
		Vec3 Ht = ht; Ht.normalize();
		float cosHO = Ht.dot(omega_out);

		float cosHI = Ht.dot(omega_in);
		// eq. 33: first we calculate D(m) with m=Ht:
		float alpha2 = m_ag * m_ag;
		float cosThetaM = m_N.dot(Ht);
		float cosThetaM2 = cosThetaM * cosThetaM;
		float tanThetaM2 = (1 - cosThetaM2) / cosThetaM2;
		float cosThetaM4 = cosThetaM2 * cosThetaM2;
		float D = alpha2 / ((float) M_PI * cosThetaM4 * (alpha2 + tanThetaM2) * (alpha2 + tanThetaM2));
		// eq. 34: now calculate G1(i,m) and G1(o,m)
		float G1o = 2 / (1 + sqrtf(1 + alpha2 * (1 - cosNO * cosNO) / (cosNO * cosNO)));
		float G1i = 2 / (1 + sqrtf(1 + alpha2 * (1 - cosNI * cosNI) / (cosNI * cosNI))); 
		float G = G1o * G1i;
		// probability
		float invHt2 = 1 / ht.dot(ht);
		pdf = D * fabsf(cosThetaM) * (fabsf(cosHI) * (m_eta * m_eta)) * invHt2;
		float out = (fabsf(cosHI * cosHO) * (m_eta * m_eta) * (G * D) * invHt2) / cosNO;
		return Color3(out, out, out);
	}

	ustring sample(const Vec3 &Ng,
	               const Vec3 &omega_out, const Vec3 &domega_out_dx, const Vec3 &domega_out_dy,
	               float randu, float randv,
	               Vec3 &omega_in, Vec3 &domega_in_dx, Vec3 &domega_in_dy,
	               float &pdf, Color3 &eval) const
	{
		float cosNO = m_N.dot(omega_out);
		if (cosNO > 0) {
			Vec3 X, Y, Z = m_N;
			make_orthonormals(Z, X, Y);
			// generate a random microfacet normal m
			// eq. 35,36:
			// we take advantage of cos(atan(x)) == 1/sqrt(1+x^2)
			//                  and sin(atan(x)) == x/sqrt(1+x^2)
			float alpha2 = m_ag * m_ag;
			float tanThetaM2 = alpha2 * randu / (1 - randu);
			float cosThetaM  = 1 / sqrtf(1 + tanThetaM2);
			float sinThetaM  = cosThetaM * sqrtf(tanThetaM2);
			float phiM = 2 * float(M_PI) * randv;
			Vec3 m = (cosf(phiM) * sinThetaM) * X +
			         (sinf(phiM) * sinThetaM) * Y +
			         cosThetaM  * Z;
			if (Refractive == 0) {
				float cosMO = m.dot(omega_out);
				if (cosMO > 0) {
					// eq. 39 - compute actual reflected direction
					omega_in = 2 * cosMO * m - omega_out;
					if (Ng.dot(omega_in) > 0) {
						// microfacet normal is visible to this ray
						// eq. 33
						float cosThetaM2 = cosThetaM * cosThetaM;
						float cosThetaM4 = cosThetaM2 * cosThetaM2;
						float D = alpha2 / (float(M_PI) * cosThetaM4 * (alpha2 + tanThetaM2) * (alpha2 + tanThetaM2));
						// eq. 24
						float pm = D * cosThetaM;
						// convert into pdf of the sampled direction
						// eq. 38 - but see also:
						// eq. 17 in http://www.graphics.cornell.edu/~bjw/wardnotes.pdf
						pdf = pm * 0.25f / cosMO;
						// eval BRDF*cosNI
						float cosNI = m_N.dot(omega_in);
						// eq. 34: now calculate G1(i,m) and G1(o,m)
						float G1o = 2 / (1 + sqrtf(1 + alpha2 * (1 - cosNO * cosNO) / (cosNO * cosNO)));
						float G1i = 2 / (1 + sqrtf(1 + alpha2 * (1 - cosNI * cosNI) / (cosNI * cosNI)));
						float G = G1o * G1i;
						// eq. 20: (F*G*D)/(4*in*on)
						float out = (G * D) * 0.25f / cosNO;
						eval.setValue(out, out, out);
						domega_in_dx = (2 * m.dot(domega_out_dx)) * m - domega_out_dx;
						domega_in_dy = (2 * m.dot(domega_out_dy)) * m - domega_out_dy;

						/* disabled for now - gives texture filtering problems */
#if 0
						// Since there is some blur to this reflection, make the
						// derivatives a bit bigger. In theory this varies with the
						// roughness but the exact relationship is complex and
						// requires more ops than are practical.
						domega_in_dx *= 10;
						domega_in_dy *= 10;
#endif
					}
				}
			}
			else {
				// CAUTION: the i and o variables are inverted relative to the paper
				// eq. 39 - compute actual refractive direction
				Vec3 R, dRdx, dRdy;
				Vec3 T, dTdx, dTdy;
				bool inside;
				fresnel_dielectric(m_eta, m, omega_out, domega_out_dx, domega_out_dy,
				                   R, dRdx, dRdy,
				                   T, dTdx, dTdy,
				                   inside);

				if (!inside) {
					omega_in = T;
					domega_in_dx = dTdx;
					domega_in_dy = dTdy;
					// eq. 33
					float cosThetaM2 = cosThetaM * cosThetaM;
					float cosThetaM4 = cosThetaM2 * cosThetaM2;
					float D = alpha2 / (float(M_PI) * cosThetaM4 * (alpha2 + tanThetaM2) * (alpha2 + tanThetaM2));
					// eq. 24
					float pm = D * cosThetaM;
					// eval BRDF*cosNI
					float cosNI = m_N.dot(omega_in);
					// eq. 34: now calculate G1(i,m) and G1(o,m)
					float G1o = 2 / (1 + sqrtf(1 + alpha2 * (1 - cosNO * cosNO) / (cosNO * cosNO)));
					float G1i = 2 / (1 + sqrtf(1 + alpha2 * (1 - cosNI * cosNI) / (cosNI * cosNI)));
					float G = G1o * G1i;
					// eq. 21
					float cosHI = m.dot(omega_in);
					float cosHO = m.dot(omega_out);
					float Ht2 = m_eta * cosHI + cosHO;
					Ht2 *= Ht2;
					float out = (fabsf(cosHI * cosHO) * (m_eta * m_eta) * (G * D)) / (cosNO * Ht2);
					// eq. 38 and eq. 17
					pdf = pm * (m_eta * m_eta) * fabsf(cosHI) / Ht2;
					eval.setValue(out, out, out);

					/* disabled for now - gives texture filtering problems */
#if 0
					// Since there is some blur to this refraction, make the
					// derivatives a bit bigger. In theory this varies with the
					// roughness but the exact relationship is complex and
					// requires more ops than are practical.
					domega_in_dx *= 10;
					domega_in_dy *= 10;
#endif
				}
			}
		}
		return Refractive ? Labels::TRANSMIT : Labels::REFLECT;
	}
};

// microfacet model with Beckmann facet distribution
// see http://www.graphics.cornell.edu/~bjw/microfacetbsdf.pdf
template <int Refractive = 0>
class MicrofacetBeckmannClosure : public BSDFClosure {
public:
	Vec3 m_N;
	float m_ab;   // width parameter (roughness)
	float m_eta;  // index of refraction (for fresnel term)
	MicrofacetBeckmannClosure() : BSDFClosure(Labels::GLOSSY, Refractive ? Back : Front) {
	}

	void setup()
	{
		m_ab = clamp(m_ab, 1e-5f, 1.0f);
	}

	bool mergeable(const ClosurePrimitive *other) const {
		const MicrofacetBeckmannClosure *comp = (const MicrofacetBeckmannClosure *)other;
		return m_N == comp->m_N && m_ab == comp->m_ab &&
		       m_eta == comp->m_eta && BSDFClosure::mergeable(other);
	}

	size_t memsize() const {
		return sizeof(*this);
	}

	const char *name() const {
		return Refractive ? "microfacet_beckmann_refraction"
			   : "microfacet_beckmann";
	}

	void print_on(std::ostream &out) const
	{
		out << name() << " (";
		out << "(" << m_N[0] << ", " << m_N[1] << ", " << m_N[2] << "), ";
		out << m_ab << ", ";
		out << m_eta;
		out << ")";
	}

	float albedo(const Vec3 &omega_out) const
	{
		return 1.0f;
	}

	Color3 eval_reflect(const Vec3 &omega_out, const Vec3 &omega_in, float& pdf) const
	{
		if (Refractive == 1) return Color3(0, 0, 0);
		float cosNO = m_N.dot(omega_out);
		float cosNI = m_N.dot(omega_in);
		if (cosNO > 0 && cosNI > 0) {
			// get half vector
			Vec3 Hr = omega_in + omega_out;
			Hr.normalize();
			// eq. 20: (F*G*D)/(4*in*on)
			// eq. 25: first we calculate D(m) with m=Hr:
			float alpha2 = m_ab * m_ab;
			float cosThetaM = m_N.dot(Hr);
			float cosThetaM2 = cosThetaM * cosThetaM;
			float tanThetaM2 = (1 - cosThetaM2) / cosThetaM2;
			float cosThetaM4 = cosThetaM2 * cosThetaM2;
			float D = expf(-tanThetaM2 / alpha2) / (float(M_PI) * alpha2 *  cosThetaM4);
			// eq. 26, 27: now calculate G1(i,m) and G1(o,m)
			float ao = 1 / (m_ab * sqrtf((1 - cosNO * cosNO) / (cosNO * cosNO)));
			float ai = 1 / (m_ab * sqrtf((1 - cosNI * cosNI) / (cosNI * cosNI)));
			float G1o = ao < 1.6f ? (3.535f * ao + 2.181f * ao * ao) / (1 + 2.276f * ao + 2.577f * ao * ao) : 1.0f;
			float G1i = ai < 1.6f ? (3.535f * ai + 2.181f * ai * ai) / (1 + 2.276f * ai + 2.577f * ai * ai) : 1.0f;
			float G = G1o * G1i;
			float out = (G * D) * 0.25f / cosNO;
			// eq. 24
			float pm = D * cosThetaM;
			// convert into pdf of the sampled direction
			// eq. 38 - but see also:
			// eq. 17 in http://www.graphics.cornell.edu/~bjw/wardnotes.pdf
			pdf = pm * 0.25f / Hr.dot(omega_out);
			return Color3(out, out, out);
		}
		return Color3(0, 0, 0);
	}

	Color3 eval_transmit(const Vec3 &omega_out, const Vec3 &omega_in, float& pdf) const
	{
		if (Refractive == 0) return Color3(0, 0, 0);
		float cosNO = m_N.dot(omega_out);
		float cosNI = m_N.dot(omega_in);
		if (cosNO <= 0 || cosNI >= 0)
			return Color3(0, 0, 0);
		// compute half-vector of the refraction (eq. 16)
		Vec3 ht = -(m_eta * omega_in + omega_out);
		Vec3 Ht = ht; Ht.normalize();
		float cosHO = Ht.dot(omega_out);

		float cosHI = Ht.dot(omega_in);
		// eq. 33: first we calculate D(m) with m=Ht:
		float alpha2 = m_ab * m_ab;
		float cosThetaM = m_N.dot(Ht);
		float cosThetaM2 = cosThetaM * cosThetaM;
		float tanThetaM2 = (1 - cosThetaM2) / cosThetaM2;
		float cosThetaM4 = cosThetaM2 * cosThetaM2;
		float D = expf(-tanThetaM2 / alpha2) / (float(M_PI) * alpha2 *  cosThetaM4);
		// eq. 26, 27: now calculate G1(i,m) and G1(o,m)
		float ao = 1 / (m_ab * sqrtf((1 - cosNO * cosNO) / (cosNO * cosNO)));
		float ai = 1 / (m_ab * sqrtf((1 - cosNI * cosNI) / (cosNI * cosNI)));
		float G1o = ao < 1.6f ? (3.535f * ao + 2.181f * ao * ao) / (1 + 2.276f * ao + 2.577f * ao * ao) : 1.0f;
		float G1i = ai < 1.6f ? (3.535f * ai + 2.181f * ai * ai) / (1 + 2.276f * ai + 2.577f * ai * ai) : 1.0f;
		float G = G1o * G1i;
		// probability
		float invHt2 = 1 / ht.dot(ht);
		pdf = D * fabsf(cosThetaM) * (fabsf(cosHI) * (m_eta * m_eta)) * invHt2;
		float out = (fabsf(cosHI * cosHO) * (m_eta * m_eta) * (G * D) * invHt2) / cosNO;
		return Color3(out, out, out);
	}

	ustring sample(const Vec3 &Ng,
	               const Vec3 &omega_out, const Vec3 &domega_out_dx, const Vec3 &domega_out_dy,
	               float randu, float randv,
	               Vec3 &omega_in, Vec3 &domega_in_dx, Vec3 &domega_in_dy,
	               float &pdf, Color3 &eval) const
	{
		float cosNO = m_N.dot(omega_out);
		if (cosNO > 0) {
			Vec3 X, Y, Z = m_N;
			make_orthonormals(Z, X, Y);
			// generate a random microfacet normal m
			// eq. 35,36:
			// we take advantage of cos(atan(x)) == 1/sqrt(1+x^2)
			//                  and sin(atan(x)) == x/sqrt(1+x^2)
			float alpha2 = m_ab * m_ab;
			float tanThetaM = sqrtf(-alpha2 * logf(1 - randu));
			float cosThetaM = 1 / sqrtf(1 + tanThetaM * tanThetaM);
			float sinThetaM = cosThetaM * tanThetaM;
			float phiM = 2 * float(M_PI) * randv;
			Vec3 m = (cosf(phiM) * sinThetaM) * X +
			         (sinf(phiM) * sinThetaM) * Y +
			         cosThetaM  * Z;
			if (Refractive == 0) {
				float cosMO = m.dot(omega_out);
				if (cosMO > 0) {
					// eq. 39 - compute actual reflected direction
					omega_in = 2 * cosMO * m - omega_out;
					if (Ng.dot(omega_in) > 0) {
						// microfacet normal is visible to this ray
						// eq. 25
						float cosThetaM2 = cosThetaM * cosThetaM;
						float tanThetaM2 = tanThetaM * tanThetaM;
						float cosThetaM4 = cosThetaM2 * cosThetaM2;
						float D = expf(-tanThetaM2 / alpha2) / (float(M_PI) * alpha2 *  cosThetaM4);
						// eq. 24
						float pm = D * cosThetaM;
						// convert into pdf of the sampled direction
						// eq. 38 - but see also:
						// eq. 17 in http://www.graphics.cornell.edu/~bjw/wardnotes.pdf
						pdf = pm * 0.25f / cosMO;
						// Eval BRDF*cosNI
						float cosNI = m_N.dot(omega_in);
						// eq. 26, 27: now calculate G1(i,m) and G1(o,m)
						float ao = 1 / (m_ab * sqrtf((1 - cosNO * cosNO) / (cosNO * cosNO)));
						float ai = 1 / (m_ab * sqrtf((1 - cosNI * cosNI) / (cosNI * cosNI)));
						float G1o = ao < 1.6f ? (3.535f * ao + 2.181f * ao * ao) / (1 + 2.276f * ao + 2.577f * ao * ao) : 1.0f;
						float G1i = ai < 1.6f ? (3.535f * ai + 2.181f * ai * ai) / (1 + 2.276f * ai + 2.577f * ai * ai) : 1.0f;
						float G = G1o * G1i;
						// eq. 20: (F*G*D)/(4*in*on)
						float out = (G * D) * 0.25f / cosNO;
						eval.setValue(out, out, out);
						domega_in_dx = (2 * m.dot(domega_out_dx)) * m - domega_out_dx;
						domega_in_dy = (2 * m.dot(domega_out_dy)) * m - domega_out_dy;

						/* disabled for now - gives texture filtering problems */
#if 0
						// Since there is some blur to this reflection, make the
						// derivatives a bit bigger. In theory this varies with the
						// roughness but the exact relationship is complex and
						// requires more ops than are practical.
						domega_in_dx *= 10;
						domega_in_dy *= 10;
#endif
					}
				}
			}
			else {
				// CAUTION: the i and o variables are inverted relative to the paper
				// eq. 39 - compute actual refractive direction
				Vec3 R, dRdx, dRdy;
				Vec3 T, dTdx, dTdy;
				bool inside;
				fresnel_dielectric(m_eta, m, omega_out, domega_out_dx, domega_out_dy,
				                   R, dRdx, dRdy,
				                   T, dTdx, dTdy,
				                   inside);
				if (!inside) {
					omega_in = T;
					domega_in_dx = dTdx;
					domega_in_dy = dTdy;
					// eq. 33
					float cosThetaM2 = cosThetaM * cosThetaM;
					float tanThetaM2 = tanThetaM * tanThetaM;
					float cosThetaM4 = cosThetaM2 * cosThetaM2;
					float D = expf(-tanThetaM2 / alpha2) / (float(M_PI) * alpha2 *  cosThetaM4);
					// eq. 24
					float pm = D * cosThetaM;
					// eval BRDF*cosNI
					float cosNI = m_N.dot(omega_in);
					// eq. 26, 27: now calculate G1(i,m) and G1(o,m)
					float ao = 1 / (m_ab * sqrtf((1 - cosNO * cosNO) / (cosNO * cosNO)));
					float ai = 1 / (m_ab * sqrtf((1 - cosNI * cosNI) / (cosNI * cosNI)));
					float G1o = ao < 1.6f ? (3.535f * ao + 2.181f * ao * ao) / (1 + 2.276f * ao + 2.577f * ao * ao) : 1.0f;
					float G1i = ai < 1.6f ? (3.535f * ai + 2.181f * ai * ai) / (1 + 2.276f * ai + 2.577f * ai * ai) : 1.0f;
					float G = G1o * G1i;
					// eq. 21
					float cosHI = m.dot(omega_in);
					float cosHO = m.dot(omega_out);
					float Ht2 = m_eta * cosHI + cosHO;
					Ht2 *= Ht2;
					float out = (fabsf(cosHI * cosHO) * (m_eta * m_eta) * (G * D)) / (cosNO * Ht2);
					// eq. 38 and eq. 17
					pdf = pm * (m_eta * m_eta) * fabsf(cosHI) / Ht2;
					eval.setValue(out, out, out);

					/* disabled for now - gives texture filtering problems */
#if 0
					// Since there is some blur to this refraction, make the
					// derivatives a bit bigger. In theory this varies with the
					// roughness but the exact relationship is complex and
					// requires more ops than are practical.
					domega_in_dx *= 10;
					domega_in_dy *= 10;
#endif
				}
			}
		}
		return Refractive ? Labels::TRANSMIT : Labels::REFLECT;
	}
};



ClosureParam bsdf_microfacet_ggx_params[] = {
	CLOSURE_VECTOR_PARAM(MicrofacetGGXClosure<0>, m_N),
	CLOSURE_FLOAT_PARAM(MicrofacetGGXClosure<0>, m_ag),
	CLOSURE_STRING_KEYPARAM("label"),
	CLOSURE_FINISH_PARAM(MicrofacetGGXClosure<0>)
};

ClosureParam bsdf_microfacet_ggx_refraction_params[] = {
	CLOSURE_VECTOR_PARAM(MicrofacetGGXClosure<1>, m_N),
	CLOSURE_FLOAT_PARAM(MicrofacetGGXClosure<1>, m_ag),
	CLOSURE_FLOAT_PARAM(MicrofacetGGXClosure<1>, m_eta),
	CLOSURE_STRING_KEYPARAM("label"),
	CLOSURE_FINISH_PARAM(MicrofacetGGXClosure<1>)
};

ClosureParam bsdf_microfacet_beckmann_params[] = {
	CLOSURE_VECTOR_PARAM(MicrofacetBeckmannClosure<0>, m_N),
	CLOSURE_FLOAT_PARAM(MicrofacetBeckmannClosure<0>, m_ab),
	CLOSURE_STRING_KEYPARAM("label"),
	CLOSURE_FINISH_PARAM(MicrofacetBeckmannClosure<0>)
};

ClosureParam bsdf_microfacet_beckmann_refraction_params[] = {
	CLOSURE_VECTOR_PARAM(MicrofacetBeckmannClosure<1>, m_N),
	CLOSURE_FLOAT_PARAM(MicrofacetBeckmannClosure<1>, m_ab),
	CLOSURE_FLOAT_PARAM(MicrofacetBeckmannClosure<1>, m_eta),
	CLOSURE_STRING_KEYPARAM("label"),
	CLOSURE_FINISH_PARAM(MicrofacetBeckmannClosure<1>)
};

CLOSURE_PREPARE(bsdf_microfacet_ggx_prepare,                 MicrofacetGGXClosure<0>)
CLOSURE_PREPARE(bsdf_microfacet_ggx_refraction_prepare,      MicrofacetGGXClosure<1>)
CLOSURE_PREPARE(bsdf_microfacet_beckmann_prepare,            MicrofacetBeckmannClosure<0>)
CLOSURE_PREPARE(bsdf_microfacet_beckmann_refraction_prepare, MicrofacetBeckmannClosure<1>)

CCL_NAMESPACE_END

