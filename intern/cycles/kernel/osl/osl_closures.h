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

#ifndef __OSL_CLOSURES_H__
#define __OSL_CLOSURES_H__

#include <OSL/oslclosure.h>
#include <OSL/oslexec.h>
#include <OSL/genclosure.h>

#include "kernel_types.h"

#include "util_types.h"

CCL_NAMESPACE_BEGIN

OSL::ClosureParam *closure_emission_params();
OSL::ClosureParam *closure_background_params();
OSL::ClosureParam *closure_holdout_params();
OSL::ClosureParam *closure_ambient_occlusion_params();
OSL::ClosureParam *closure_bsdf_phong_ramp_params();

void closure_emission_prepare(OSL::RendererServices *, int id, void *data);
void closure_background_prepare(OSL::RendererServices *, int id, void *data);
void closure_holdout_prepare(OSL::RendererServices *, int id, void *data);
void closure_ambient_occlusion_prepare(OSL::RendererServices *, int id, void *data);
void closure_bsdf_phong_ramp_prepare(OSL::RendererServices *, int id, void *data);

enum {
	AmbientOcclusion = 100
};

#define CLOSURE_PREPARE(name, classname)          \
void name(RendererServices *, int id, void *data) \
{                                                 \
	memset(data, 0, sizeof(classname));           \
	new (data) classname();                       \
}

#define TO_VEC3(v) (*(OSL::Vec3 *)&(v))
#define TO_COLOR3(v) (*(OSL::Color3 *)&(v))
#define TO_FLOAT3(v) make_float3(v[0], v[1], v[2])

/* BSDF */

class CBSDFClosure : public OSL::ClosurePrimitive {
public:
	ShaderClosure sc;
	OSL::Vec3 N, T;

    CBSDFClosure(int scattering) : OSL::ClosurePrimitive(BSDF),
        m_scattering_label(scattering), m_shaderdata_flag(0) { }
    ~CBSDFClosure() { }

    int scattering() const { return m_scattering_label; }
    int shaderdata_flag() const { return m_shaderdata_flag; }
	ClosureType shaderclosure_type() const { return sc.type; }

    virtual void blur(float roughness) = 0;
    virtual float3 eval_reflect(const float3 &omega_out, const float3 &omega_in, float &pdf) const = 0;
    virtual float3 eval_transmit(const float3 &omega_out, const float3 &omega_in, float &pdf) const = 0;

    virtual int sample(const float3 &Ng,
                        const float3 &omega_out, const float3 &domega_out_dx, const float3 &domega_out_dy,
                        float randu, float randv,
                        float3 &omega_in, float3 &domega_in_dx, float3 &domega_in_dy,
                        float &pdf, float3 &eval) const = 0;

protected:
    int m_scattering_label;
	int m_shaderdata_flag;
};

#define BSDF_CLOSURE_CLASS_BEGIN(Upper, lower, svmlower, TYPE) \
\
class Upper##Closure : public CBSDFClosure { \
public: \
	Upper##Closure() : CBSDFClosure(TYPE) {} \
	size_t memsize() const { return sizeof(*this); } \
	const char *name() const { return #lower; } \
\
	void setup() \
	{ \
		sc.N = TO_FLOAT3(N); \
		sc.T = TO_FLOAT3(T); \
		m_shaderdata_flag = bsdf_##lower##_setup(&sc); \
	} \
\
	bool mergeable(const ClosurePrimitive *other) const \
	{ \
		return false; \
	} \
	\
	void blur(float roughness) \
	{ \
		bsdf_##svmlower##_blur(&sc, roughness); \
	} \
\
	void print_on(std::ostream &out) const \
	{ \
		out << name() << " ((" << sc.N[0] << ", " << sc.N[1] << ", " << sc.N[2] << "))"; \
	} \
\
	float3 eval_reflect(const float3 &omega_out, const float3 &omega_in, float& pdf) const \
	{ \
		return bsdf_##svmlower##_eval_reflect(&sc, omega_out, omega_in, &pdf); \
	} \
\
	float3 eval_transmit(const float3 &omega_out, const float3 &omega_in, float& pdf) const \
	{ \
		return bsdf_##svmlower##_eval_transmit(&sc, omega_out, omega_in, &pdf); \
	} \
\
	int sample(const float3 &Ng, \
	           const float3 &omega_out, const float3 &domega_out_dx, const float3 &domega_out_dy, \
	           float randu, float randv, \
	           float3 &omega_in, float3 &domega_in_dx, float3 &domega_in_dy, \
	           float &pdf, float3 &eval) const \
	{ \
		return bsdf_##svmlower##_sample(&sc, Ng, omega_out, domega_out_dx, domega_out_dy, \
			randu, randv, &eval, &omega_in, &domega_in_dx, &domega_in_dy, &pdf); \
	} \
}; \
\
ClosureParam *bsdf_##lower##_params() \
{ \
	static ClosureParam params[] = {

/* parameters */

#define BSDF_CLOSURE_CLASS_END(Upper, lower) \
		CLOSURE_STRING_KEYPARAM("label"), \
	    CLOSURE_FINISH_PARAM(Upper##Closure) \
	}; \
	return params; \
} \
\
CLOSURE_PREPARE(bsdf_##lower##_prepare, Upper##Closure)

CCL_NAMESPACE_END

#endif /* __OSL_CLOSURES_H__ */

