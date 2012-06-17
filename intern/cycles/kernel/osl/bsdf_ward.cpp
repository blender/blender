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

// anisotropic ward - leaks energy at grazing angles
// see http://www.graphics.cornell.edu/~bjw/wardnotes.pdf 
class WardClosure : public BSDFClosure {
public:
	Vec3 m_N;
	Vec3 m_T;
	float m_ax, m_ay;
	WardClosure() : BSDFClosure(Labels::GLOSSY) {}

	void setup()
	{
		m_ax = clamp(m_ax, 1e-5f, 1.0f);
		m_ay = clamp(m_ay, 1e-5f, 1.0f);
	}

	bool mergeable(const ClosurePrimitive *other) const {
		const WardClosure *comp = (const WardClosure *)other;
		return m_N == comp->m_N && m_T == comp->m_T &&
		       m_ax == comp->m_ax && m_ay == comp->m_ay &&
		       BSDFClosure::mergeable(other);
	}

	size_t memsize() const { return sizeof(*this); }

	const char *name() const { return "ward"; }

	void print_on(std::ostream &out) const {
		out << name() << " ((";
		out << m_N[0] << ", " << m_N[1] << ", " << m_N[2] << "), (";
		out << m_T[0] << ", " << m_T[1] << ", " << m_T[2] << "), ";
		out << m_ax << ", " << m_ay << ")";
	}

	float albedo(const Vec3 &omega_out) const
	{
		return 1.0f;
	}

	Color3 eval_reflect(const Vec3 &omega_out, const Vec3 &omega_in, float& pdf) const
	{
		float cosNO = m_N.dot(omega_out);
		float cosNI = m_N.dot(omega_in);
		if (cosNI > 0 && cosNO > 0) {
			// get half vector and get x,y basis on the surface for anisotropy
			Vec3 H = omega_in + omega_out;
			H.normalize();  // normalize needed for pdf
			Vec3 X, Y;
			make_orthonormals(m_N, m_T, X, Y);
			// eq. 4
			float dotx = H.dot(X) / m_ax;
			float doty = H.dot(Y) / m_ay;
			float dotn = H.dot(m_N);
			float exp_arg = (dotx * dotx + doty * doty) / (dotn * dotn);
			float denom = (4 * (float) M_PI * m_ax * m_ay * sqrtf(cosNO * cosNI));
			float exp_val = expf(-exp_arg);
			float out = cosNI * exp_val / denom;
			float oh = H.dot(omega_out);
			denom = 4 * (float) M_PI * m_ax * m_ay * oh * dotn * dotn * dotn;
			pdf = exp_val / denom;
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
		float cosNO = m_N.dot(omega_out);
		if (cosNO > 0) {
			// get x,y basis on the surface for anisotropy
			Vec3 X, Y;
			make_orthonormals(m_N, m_T, X, Y);
			// generate random angles for the half vector
			// eq. 7 (taking care around discontinuities to keep
			//        output angle in the right quadrant)
			// we take advantage of cos(atan(x)) == 1/sqrt(1+x^2)
			//                  and sin(atan(x)) == x/sqrt(1+x^2)
			float alphaRatio = m_ay / m_ax;
			float cosPhi, sinPhi;
			if (randu < 0.25f) {
				float val = 4 * randu;
				float tanPhi = alphaRatio * tanf((float) M_PI_2 * val);
				cosPhi = 1 / sqrtf(1 + tanPhi * tanPhi);
				sinPhi = tanPhi * cosPhi;
			}
			else if (randu < 0.5) {
				float val = 1 - 4 * (0.5f - randu);
				float tanPhi = alphaRatio * tanf((float) M_PI_2 * val);
				// phi = (float) M_PI - phi;
				cosPhi = -1 / sqrtf(1 + tanPhi * tanPhi);
				sinPhi = -tanPhi * cosPhi;
			}
			else if (randu < 0.75f) {
				float val = 4 * (randu - 0.5f);
				float tanPhi = alphaRatio * tanf((float) M_PI_2 * val);
				//phi = (float) M_PI + phi;
				cosPhi = -1 / sqrtf(1 + tanPhi * tanPhi);
				sinPhi = tanPhi * cosPhi;
			}
			else {
				float val = 1 - 4 * (1 - randu);
				float tanPhi = alphaRatio * tanf((float) M_PI_2 * val);
				// phi = 2 * (float) M_PI - phi;
				cosPhi = 1 / sqrtf(1 + tanPhi * tanPhi);
				sinPhi = -tanPhi * cosPhi;
			}
			// eq. 6
			// we take advantage of cos(atan(x)) == 1/sqrt(1+x^2)
			//                  and sin(atan(x)) == x/sqrt(1+x^2)
			float thetaDenom = (cosPhi * cosPhi) / (m_ax * m_ax) + (sinPhi * sinPhi) / (m_ay * m_ay);
			float tanTheta2 = -logf(1 - randv) / thetaDenom;
			float cosTheta  = 1 / sqrtf(1 + tanTheta2);
			float sinTheta  = cosTheta * sqrtf(tanTheta2);

			Vec3 h; // already normalized becaused expressed from spherical coordinates
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
			float oh = h.dot(omega_out);
			omega_in.x = 2 * oh * h.x - omega_out.x;
			omega_in.y = 2 * oh * h.y - omega_out.y;
			omega_in.z = 2 * oh * h.z - omega_out.z;
			if (Ng.dot(omega_in) > 0) {
				float cosNI = m_N.dot(omega_in);
				if (cosNI > 0) {
					// eq. 9
					float exp_arg = (dotx * dotx + doty * doty) / (dotn * dotn);
					float denom = 4 * (float) M_PI * m_ax * m_ay * oh * dotn * dotn * dotn;
					pdf = expf(-exp_arg) / denom;
					// compiler will reuse expressions already computed
					denom = (4 * (float) M_PI * m_ax * m_ay * sqrtf(cosNO * cosNI));
					float power = cosNI * expf(-exp_arg) / denom;
					eval.setValue(power, power, power);
					domega_in_dx = (2 * m_N.dot(domega_out_dx)) * m_N - domega_out_dx;
					domega_in_dy = (2 * m_N.dot(domega_out_dy)) * m_N - domega_out_dy;

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
		return Labels::REFLECT;
	}
};



ClosureParam bsdf_ward_params[] = {
	CLOSURE_VECTOR_PARAM(WardClosure, m_N),
	CLOSURE_VECTOR_PARAM(WardClosure, m_T),
	CLOSURE_FLOAT_PARAM(WardClosure, m_ax),
	CLOSURE_FLOAT_PARAM(WardClosure, m_ay),
	CLOSURE_STRING_KEYPARAM("label"),
	CLOSURE_FINISH_PARAM(WardClosure)
};

CLOSURE_PREPARE(bsdf_ward_prepare, WardClosure)

CCL_NAMESPACE_END

