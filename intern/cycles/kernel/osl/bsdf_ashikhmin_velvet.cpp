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

CCL_NAMESPACE_BEGIN

using namespace OSL;

class AshikhminVelvetClosure : public BSDFClosure {
public:
	Vec3 m_N;
	float m_sigma;
	float m_invsigma2;

	AshikhminVelvetClosure() : BSDFClosure(Labels::DIFFUSE) {}

	void setup()
	{
		m_sigma = max(m_sigma, 0.01f);
		m_invsigma2 = 1.0f / (m_sigma * m_sigma);
	}

	bool mergeable(const ClosurePrimitive *other) const {
		const AshikhminVelvetClosure *comp = (const AshikhminVelvetClosure *)other;
		return m_N == comp->m_N && m_sigma == comp->m_sigma &&
		       BSDFClosure::mergeable(other);
	}

	size_t memsize() const { return sizeof(*this); }

	const char *name() const { return "ashikhmin_velvet"; }

	void print_on(std::ostream &out) const
	{
		out << name() << " (";
		out << "(" << m_N[0] << ", " << m_N[1] << ", " << m_N[2] << "), ";
		out << m_sigma;
		out << ")";
	}

	float albedo(const Vec3 &omega_out) const
	{
		return 1.0f;
	}

	Color3 eval_reflect(const Vec3 &omega_out, const Vec3 &omega_in, float& pdf) const
	{
		float cosNO = m_N.dot(omega_out);
		float cosNI = m_N.dot(omega_in);
		if (cosNO > 0 && cosNI > 0) {
			Vec3 H = omega_in + omega_out;
			H.normalize();

			float cosNH = m_N.dot(H);
			float cosHO = fabsf(omega_out.dot(H));

			float cosNHdivHO = cosNH / cosHO;
			cosNHdivHO = max(cosNHdivHO, 0.00001f);

			float fac1 = 2 * fabsf(cosNHdivHO * cosNO);
			float fac2 = 2 * fabsf(cosNHdivHO * cosNI);

			float sinNH2 = 1 - cosNH * cosNH;
			float sinNH4 = sinNH2 * sinNH2;
			float cotangent2 =  (cosNH * cosNH) / sinNH2;

			float D = expf(-cotangent2 * m_invsigma2) * m_invsigma2 * float(M_1_PI) / sinNH4;
			float G = min(1.0f, min(fac1, fac2)); // TODO: derive G from D analytically

			float out = 0.25f * (D * G) / cosNO;

			pdf = 0.5f * (float) M_1_PI;
			return Color3(out, out, out);
		}
		return Color3(0, 0, 0);
	}

	Color3 eval_transmit(const Vec3 &omega_out, const Vec3 &omega_in, float& pdf) const
	{
		return Color3(0, 0, 0);
	}

	ustring sample(const Vec3 &Ng,
	               const Vec3 &omega_out, const Vec3 &domega_out_dx, const Vec3 &domega_out_dy,
	               float randu, float randv,
	               Vec3 &omega_in, Vec3 &domega_in_dx, Vec3 &domega_in_dy,
	               float &pdf, Color3 &eval) const
	{
		// we are viewing the surface from above - send a ray out with uniform
		// distribution over the hemisphere
		sample_uniform_hemisphere(m_N, omega_out, randu, randv, omega_in, pdf);
		if (Ng.dot(omega_in) > 0) {
			Vec3 H = omega_in + omega_out;
			H.normalize();

			float cosNI = m_N.dot(omega_in);
			float cosNO = m_N.dot(omega_out);
			float cosNH = m_N.dot(H);
			float cosHO = fabsf(omega_out.dot(H));

			float cosNHdivHO = cosNH / cosHO;
			cosNHdivHO = max(cosNHdivHO, 0.00001f);

			float fac1 = 2 * fabsf(cosNHdivHO * cosNO);
			float fac2 = 2 * fabsf(cosNHdivHO * cosNI);

			float sinNH2 = 1 - cosNH * cosNH;
			float sinNH4 = sinNH2 * sinNH2;
			float cotangent2 =  (cosNH * cosNH) / sinNH2;

			float D = expf(-cotangent2 * m_invsigma2) * m_invsigma2 * float(M_1_PI) / sinNH4;
			float G = min(1.0f, min(fac1, fac2)); // TODO: derive G from D analytically

			float power = 0.25f * (D * G) / cosNO;

			eval.setValue(power, power, power);

			// TODO: find a better approximation for the retroreflective bounce
			domega_in_dx = (2 * m_N.dot(domega_out_dx)) * m_N - domega_out_dx;
			domega_in_dy = (2 * m_N.dot(domega_out_dy)) * m_N - domega_out_dy;
			domega_in_dx *= 125;
			domega_in_dy *= 125;
		}
		else
			pdf = 0;
		return Labels::REFLECT;
	}

};



ClosureParam bsdf_ashikhmin_velvet_params[] = {
	CLOSURE_VECTOR_PARAM(AshikhminVelvetClosure, m_N),
	CLOSURE_FLOAT_PARAM(AshikhminVelvetClosure, m_sigma),
	CLOSURE_STRING_KEYPARAM("label"),
	CLOSURE_FINISH_PARAM(AshikhminVelvetClosure)
};

CLOSURE_PREPARE(bsdf_ashikhmin_velvet_prepare, AshikhminVelvetClosure)

CCL_NAMESPACE_END

