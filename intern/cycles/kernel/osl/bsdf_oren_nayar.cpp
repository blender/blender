/*
 * Copyright 2011, Blender Foundation.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include <OpenImageIO/fmath.h>
#include <OSL/genclosure.h>
#include "osl_closures.h"
#include "util_math.h"

CCL_NAMESPACE_BEGIN

using namespace OSL;


class OrenNayarClosure: public BSDFClosure {
public:
	Vec3 m_N;
	float m_sigma;
	float m_a, m_b;

	OrenNayarClosure(): BSDFClosure(Labels::DIFFUSE) {}

	void setup() {
		m_sigma = clamp(m_sigma, 0.0f, 1.0f);
		
		float div = 1.0f / (M_PI + ((3.0f * M_PI - 4.0f) / 6.0f) * m_sigma);

		m_a =    1.0f * div;
		m_b = m_sigma * div;
	}

	bool mergeable(const ClosurePrimitive* other) const {
		const OrenNayarClosure* comp = static_cast<const OrenNayarClosure*>(other);
		return
			m_N == comp->m_N &&
			m_sigma == comp->m_sigma &&
			BSDFClosure::mergeable(other);
	}

	size_t memsize() const {
		return sizeof(*this);
	}

	const char* name() const {
		return "oren_nayar";
	}

	void print_on(std::ostream& out) const {
		out << name() << " (";
		out << "(" << m_N[0] << ", " << m_N[1] << ", " << m_N[2] << "), ";
		out << m_sigma;
		out << ")";
	}

	float albedo(const Vec3& omega_out) const {
		return 1.0f;
	}

	Color3 eval_reflect(const Vec3& omega_out, const Vec3& omega_in, float& pdf) const {
		if (m_N.dot(omega_in) > 0.0f) {
			pdf = float(0.5 * M_1_PI);
			float is = get_intensity(m_N, omega_out, omega_in);
			return Color3(is, is, is);
		}
		else {
			pdf = 0.0f;
			return Color3(0.0f, 0.0f, 0.0f);
		}
	}

	Color3 eval_transmit(const Vec3& omega_out, const Vec3& omega_in, float& pdf) const {
		return Color3(0.0f, 0.0f, 0.0f);
	}

	ustring sample(
		const Vec3& Ng,
		const Vec3& omega_out, const Vec3& domega_out_dx, const Vec3& domega_out_dy,
		float randu, float randv,
		Vec3& omega_in, Vec3& domega_in_dx, Vec3& domega_in_dy,
		float& pdf, Color3& eval
	) const {
		sample_uniform_hemisphere (m_N, omega_out, randu, randv, omega_in, pdf);

		if (Ng.dot(omega_in) > 0.0f) {
			float is = get_intensity(m_N, omega_out, omega_in);
			eval.setValue(is, is, is);

			// TODO: find a better approximation for the bounce
			domega_in_dx = (2.0f * m_N.dot(domega_out_dx)) * m_N - domega_out_dx;
			domega_in_dy = (2.0f * m_N.dot(domega_out_dy)) * m_N - domega_out_dy;
			domega_in_dx *= 125.0f;
			domega_in_dy *= 125.0f;
		}
		else {
			pdf = 0.0f;
		}

		return Labels::REFLECT;
	}

private:
	float get_intensity(Vec3 const& n, Vec3 const& v, Vec3 const& l) const {
		float nl = max(n.dot(l), 0.0f);
		float nv = max(n.dot(v), 0.0f);
		float t = l.dot(v) - nl * nv;
		
		if(t > 0.0f) {
			t /= max(nl, nv) + 1e-8f;
		}
		return nl * (m_a + m_b * t);
	}
};

ClosureParam bsdf_oren_nayar_params[] = {
	CLOSURE_VECTOR_PARAM	(OrenNayarClosure, m_N),
	CLOSURE_FLOAT_PARAM		(OrenNayarClosure, m_sigma),
	CLOSURE_STRING_KEYPARAM ("label"),
	CLOSURE_FINISH_PARAM	(OrenNayarClosure)
};

CLOSURE_PREPARE(bsdf_oren_nayar_prepare, OrenNayarClosure)


CCL_NAMESPACE_END
