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

CCL_NAMESPACE_BEGIN

using namespace OSL;

// Computes scattering properties based on Jensen's reparameterization
// described in:
//    http://graphics.ucsd.edu/~henrik/papers/fast_bssrdf/

class SubsurfaceClosure : public VolumeClosure {
public:
	float m_g;
	float m_eta;
	Color3 m_mfp, m_albedo;
	static float root_find_Rd(const float Rd0, const float A) {
		// quick exit for trivial cases
		if (Rd0 <= 0) return 0;
		const float A43 = A * 4.0f / 3.0f;
		// Find alpha such that f(alpha) = Rd (see eq.15). A simple bisection
		// method can be used because this function is monotonicaly increasing.
		float lo = 0, hi = 1;
		for (int i = 0; i < 20; i++) { // 2^20 divisions should be sufficient
			// eval function at midpoint
			float alpha = 0.5f * (lo + hi);
			float a1 = sqrtf(3 * (1 - alpha));
			float e1 = expf(-a1);
			float e2 = expf(-A43 * a1);
			float Rd = 0.5f * alpha * (1 + e2) * e1 - Rd0;
			if (fabsf(Rd) < 1e-6f)
				return alpha;  // close enough
			else if (Rd > 0)
				hi = alpha;  // root is on left side
			else
				lo = alpha;  // root is on right side
		}
		// didn't quite converge, pick result in the middle of remaining interval
		return 0.5f * (lo + hi);
	}
	SubsurfaceClosure() {
	}

	void setup()
	{
		ior(m_eta);

		if (m_g >=  0.99f) m_g =  0.99f;
		if (m_g <= -0.99f) m_g = -0.99f;

		// eq.10
		float inv_eta = 1 / m_eta;
		float Fdr = -1.440f * inv_eta * inv_eta + 0.710 * inv_eta + 0.668f + 0.0636 * m_eta;
		float A = (1 + Fdr) / (1 - Fdr);
		// compute sigma_s, sigma_a (eq.16)
		Color3 alpha_prime = Color3(root_find_Rd(m_albedo[0], A),
		                            root_find_Rd(m_albedo[1], A),
		                            root_find_Rd(m_albedo[2], A));
		Color3 sigma_t_prime = Color3(m_mfp.x > 0 ? 1.0f / (m_mfp[0] * sqrtf(3 * (1 - alpha_prime[0]))) : 0.0f,
		                              m_mfp.y > 0 ? 1.0f / (m_mfp[1] * sqrtf(3 * (1 - alpha_prime[1]))) : 0.0f,
		                              m_mfp.z > 0 ? 1.0f / (m_mfp[2] * sqrtf(3 * (1 - alpha_prime[2]))) : 0.0f);
		Color3 sigma_s_prime = alpha_prime * sigma_t_prime;

		sigma_s((1.0f / (1 - m_g)) * sigma_s_prime);
		sigma_a(sigma_t_prime - sigma_s_prime);
	}

	bool mergeable(const ClosurePrimitive *other) const {
		const SubsurfaceClosure *comp = (const SubsurfaceClosure *)other;
		return m_g == comp->m_g && VolumeClosure::mergeable(other);
	}

	size_t memsize() const { return sizeof(*this); }

	const char *name() const { return "subsurface"; }

	void print_on(std::ostream &out) const {
		out << name() << " ()";
	}

	virtual Color3 eval_phase(const Vec3 &omega_in, const Vec3 &omega_out) const {
		float costheta = omega_in.dot(omega_out);
		float ph = 0.25f * float(M_1_PI) * ((1 - m_g * m_g) / powf(1 + m_g * m_g - 2.0f * m_g * costheta, 1.5f));
		return Color3(ph, ph, ph);
	}
};



ClosureParam closure_subsurface_params[] = {
	CLOSURE_FLOAT_PARAM(SubsurfaceClosure, m_eta),
	CLOSURE_FLOAT_PARAM(SubsurfaceClosure, m_g),
	CLOSURE_COLOR_PARAM(SubsurfaceClosure, m_mfp),
	CLOSURE_COLOR_PARAM(SubsurfaceClosure, m_albedo),
	CLOSURE_STRING_KEYPARAM("label"),
	CLOSURE_FINISH_PARAM(SubsurfaceClosure)
};

CLOSURE_PREPARE(closure_subsurface_prepare, SubsurfaceClosure)

CCL_NAMESPACE_END

