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

class BSSRDFCubicClosure : public BSSRDFClosure {
public:
	Color3 m_radius;
	Color3 m_scale;
	float m_max_radius;

	template <typename T>
	static inline T pow3(const T &x) { return x * x * x; }

	template <typename T>
	static inline T pow5(const T &x) { T x2 = x * x; return x2 * x2 * x; }

	BSSRDFCubicClosure() {}

	void setup()
	{
		// pre-compute some terms
		m_max_radius = 0;
		for (int i = 0; i < 3; i++) {
			m_scale[i] = m_radius[i] > 0 ? 4 / pow5(m_radius[i]) : 0;
			m_max_radius = std::max(m_max_radius, m_radius[i]);
		}
	}

	bool mergeable(const ClosurePrimitive *other) const {
		const BSSRDFCubicClosure *comp = (const BSSRDFCubicClosure *)other;
		return m_radius == comp->m_radius && BSSRDFClosure::mergeable(other);
	}

	size_t memsize() const { return sizeof(*this); }

	const char *name() const { return "bssrdf_cubic"; }

	void print_on(std::ostream &out) const
	{
		out << name() << " ((" << m_radius[0] << ", " << m_radius[1] << ", " << m_radius[2] << "), ("
		    << m_scale[0] << ", " << m_scale[1] << ", " << m_scale[2] << "))";
	}

	Color3 eval(float r) const
	{
		return Color3((r < m_radius.x) ? pow3(m_radius.x - r) * m_scale.x : 0,
		              (r < m_radius.y) ? pow3(m_radius.y - r) * m_scale.y : 0,
		              (r < m_radius.z) ? pow3(m_radius.z - r) * m_scale.z : 0);
	}

	float max_radius() const
	{
		return m_max_radius;
	}
};



ClosureParam closure_bssrdf_cubic_params[] = {
	CLOSURE_COLOR_PARAM(BSSRDFCubicClosure, m_radius),
	CLOSURE_STRING_KEYPARAM("label"),
	CLOSURE_FINISH_PARAM(BSSRDFCubicClosure)
};

CLOSURE_PREPARE(closure_bssrdf_cubic_prepare, BSSRDFCubicClosure)

CCL_NAMESPACE_END

