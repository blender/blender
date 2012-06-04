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

class DiffuseClosure : public BSDFClosure {
public:
    Vec3 m_N;

    DiffuseClosure() : BSDFClosure(Labels::DIFFUSE) { }

    void setup() {};

    bool mergeable (const ClosurePrimitive *other) const {
        const DiffuseClosure *comp = (const DiffuseClosure *)other;
        return m_N == comp->m_N && BSDFClosure::mergeable(other);
    }

    size_t memsize () const { return sizeof(*this); }

    const char *name () const { return "diffuse"; }

    void print_on (std::ostream &out) const
    {
        out << name() << " ((" << m_N[0] << ", " << m_N[1] << ", " << m_N[2] << "))";
    }

    float albedo (const Vec3 &omega_out) const
    {
        return 1.0f;
    }

    Color3 eval_reflect (const Vec3 &omega_out, const Vec3 &omega_in, float& pdf) const
    {
        float cos_pi = max(m_N.dot(omega_in),0.0f) * (float) M_1_PI;
        pdf = cos_pi;
        return Color3 (cos_pi, cos_pi, cos_pi);
    }

    Color3 eval_transmit (const Vec3 &omega_out, const Vec3 &omega_in, float& pdf) const
    {
        return Color3 (0, 0, 0);
    }

    ustring sample (const Vec3 &Ng,
                 const Vec3 &omega_out, const Vec3 &domega_out_dx, const Vec3 &domega_out_dy,
                 float randu, float randv,
                 Vec3 &omega_in, Vec3 &domega_in_dx, Vec3 &domega_in_dy,
                 float &pdf, Color3 &eval) const
    {
        // we are viewing the surface from the right side - send a ray out with cosine
        // distribution over the hemisphere
        sample_cos_hemisphere (m_N, omega_out, randu, randv, omega_in, pdf);
        if (Ng.dot(omega_in) > 0) {
            eval.setValue(pdf, pdf, pdf);
            // TODO: find a better approximation for the diffuse bounce
            domega_in_dx = (2 * m_N.dot(domega_out_dx)) * m_N - domega_out_dx;
            domega_in_dy = (2 * m_N.dot(domega_out_dy)) * m_N - domega_out_dy;
            domega_in_dx *= 125;
            domega_in_dy *= 125;
        } else
            pdf = 0;
        return Labels::REFLECT;
    }
};



class TranslucentClosure : public BSDFClosure {
public:
    Vec3 m_N;

    TranslucentClosure() : BSDFClosure(Labels::DIFFUSE, Back) { }

    void setup() {};

    bool mergeable (const ClosurePrimitive *other) const {
        const TranslucentClosure *comp = (const TranslucentClosure *)other;
        return m_N == comp->m_N && BSDFClosure::mergeable(other);
    }

    size_t memsize () const { return sizeof(*this); }

    const char *name () const { return "translucent"; }

    void print_on (std::ostream &out) const
    {
        out << name() << " ((" << m_N[0] << ", " << m_N[1] << ", " << m_N[2] << "))";
    }

    Color3 eval_reflect (const Vec3 &omega_out, const Vec3 &omega_in, float& pdf) const
    {
        return Color3 (0, 0, 0);
    }

    float albedo (const Vec3 &omega_out) const
    {
        return 1.0f;
    }

    Color3 eval_transmit (const Vec3 &omega_out, const Vec3 &omega_in, float& pdf) const
    {
        float cos_pi = max(-m_N.dot(omega_in), 0.0f) * (float) M_1_PI;
        pdf = cos_pi;
        return Color3 (cos_pi, cos_pi, cos_pi);
    }

    ustring sample (const Vec3 &Ng,
                 const Vec3 &omega_out, const Vec3 &domega_out_dx, const Vec3 &domega_out_dy,
                 float randu, float randv,
                 Vec3 &omega_in, Vec3 &domega_in_dx, Vec3 &domega_in_dy,
                 float &pdf, Color3 &eval) const
    {
        // we are viewing the surface from the right side - send a ray out with cosine
        // distribution over the hemisphere
        sample_cos_hemisphere (-m_N, omega_out, randu, randv, omega_in, pdf);
        if (Ng.dot(omega_in) < 0) {
            eval.setValue(pdf, pdf, pdf);
            // TODO: find a better approximation for the diffuse bounce
            domega_in_dx = (2 * m_N.dot(domega_out_dx)) * m_N - domega_out_dx;
            domega_in_dy = (2 * m_N.dot(domega_out_dy)) * m_N - domega_out_dy;
            domega_in_dx *= -125;
            domega_in_dy *= -125;
        } else
            pdf = 0;
        return Labels::TRANSMIT;
    }
};

ClosureParam bsdf_diffuse_params[] = {
    CLOSURE_VECTOR_PARAM   (DiffuseClosure, m_N),
    CLOSURE_STRING_KEYPARAM("label"),
    CLOSURE_FINISH_PARAM   (DiffuseClosure) };

ClosureParam bsdf_translucent_params[] = {
    CLOSURE_VECTOR_PARAM   (TranslucentClosure, m_N),
    CLOSURE_STRING_KEYPARAM("label"),
    CLOSURE_FINISH_PARAM   (TranslucentClosure) };

CLOSURE_PREPARE(bsdf_diffuse_prepare, DiffuseClosure)
CLOSURE_PREPARE(bsdf_translucent_prepare, TranslucentClosure)

CCL_NAMESPACE_END

