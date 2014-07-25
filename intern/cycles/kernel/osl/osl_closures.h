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

#include "util_types.h"
#include "kernel_types.h"

#include <OSL/oslclosure.h>
#include <OSL/oslexec.h>
#include <OSL/genclosure.h>

CCL_NAMESPACE_BEGIN

OSL::ClosureParam *closure_emission_params();
OSL::ClosureParam *closure_background_params();
OSL::ClosureParam *closure_holdout_params();
OSL::ClosureParam *closure_ambient_occlusion_params();
OSL::ClosureParam *closure_bsdf_diffuse_ramp_params();
OSL::ClosureParam *closure_bsdf_phong_ramp_params();
OSL::ClosureParam *closure_westin_backscatter_params();
OSL::ClosureParam *closure_westin_sheen_params();
OSL::ClosureParam *closure_bssrdf_cubic_params();
OSL::ClosureParam *closure_bssrdf_gaussian_params();
OSL::ClosureParam *closure_henyey_greenstein_volume_params();

void closure_emission_prepare(OSL::RendererServices *, int id, void *data);
void closure_background_prepare(OSL::RendererServices *, int id, void *data);
void closure_holdout_prepare(OSL::RendererServices *, int id, void *data);
void closure_ambient_occlusion_prepare(OSL::RendererServices *, int id, void *data);
void closure_bsdf_diffuse_ramp_prepare(OSL::RendererServices *, int id, void *data);
void closure_bsdf_phong_ramp_prepare(OSL::RendererServices *, int id, void *data);
void closure_westin_backscatter_prepare(OSL::RendererServices *, int id, void *data);
void closure_westin_sheen_prepare(OSL::RendererServices *, int id, void *data);
void closure_bssrdf_cubic_prepare(OSL::RendererServices *, int id, void *data);
void closure_bssrdf_gaussian_prepare(OSL::RendererServices *, int id, void *data);
void closure_henyey_greenstein_volume_prepare(OSL::RendererServices *, int id, void *data);

#define CCLOSURE_PREPARE(name, classname)          \
void name(RendererServices *, int id, void *data) \
{                                                 \
	memset(data, 0, sizeof(classname));           \
	new (data) classname();                       \
}

#define CCLOSURE_PREPARE_STATIC(name, classname) static CCLOSURE_PREPARE(name, classname)

#define CLOSURE_FLOAT3_PARAM(st, fld) \
	{ TypeDesc::TypeVector, reckless_offsetof(st, fld), NULL, sizeof(OSL::Vec3) }

#define TO_VEC3(v) OSL::Vec3(v.x, v.y, v.z)
#define TO_COLOR3(v) OSL::Color3(v.x, v.y, v.z)
#define TO_FLOAT3(v) make_float3(v[0], v[1], v[2])

/* Closure */

class CClosurePrimitive {
public:
	enum Category {
		BSDF,             ///< Reflective and/or transmissive surface
		BSSRDF,           ///< Sub-surface light transfer
		Emissive,         ///< Light emission
		Background,       ///< Background emission
		Volume,           ///< Volume scattering
		Holdout,          ///< Holdout from alpha
		AmbientOcclusion, ///< Ambient occlusion
	};

	CClosurePrimitive (Category category_) : category (category_) {}
	virtual ~CClosurePrimitive() {}
	virtual void setup() {}

	Category category;
};

/* BSDF */

class CBSDFClosure : public CClosurePrimitive {
public:
	ShaderClosure sc;

	CBSDFClosure(int scattering) : CClosurePrimitive(BSDF),
	  m_scattering_label(scattering), m_shaderdata_flag(0)
	{}

	int scattering() const { return m_scattering_label; }
	int shaderdata_flag() const { return m_shaderdata_flag; }

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
	Upper##Closure() : CBSDFClosure(TYPE) \
	{ \
	} \
\
	void setup() \
	{ \
		sc.prim = NULL; \
		m_shaderdata_flag = bsdf_##lower##_setup(&sc); \
	} \
\
	void blur(float roughness) \
	{ \
	} \
\
	float3 eval_reflect(const float3 &omega_out, const float3 &omega_in, float& pdf) const \
	{ \
		pdf = 0; \
		return make_float3(0, 0, 0); \
	} \
\
	float3 eval_transmit(const float3 &omega_out, const float3 &omega_in, float& pdf) const \
	{ \
		pdf = 0; \
		return make_float3(0, 0, 0); \
	} \
\
	int sample(const float3 &Ng, \
	           const float3 &omega_out, const float3 &domega_out_dx, const float3 &domega_out_dy, \
	           float randu, float randv, \
	           float3 &omega_in, float3 &domega_in_dx, float3 &domega_in_dy, \
	           float &pdf, float3 &eval) const \
	{ \
		pdf = 0; \
		return LABEL_NONE; \
	} \
}; \
\
static ClosureParam *bsdf_##lower##_params() \
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
CCLOSURE_PREPARE_STATIC(bsdf_##lower##_prepare, Upper##Closure)


/* Volume */

class CVolumeClosure : public CClosurePrimitive {
public:
	ShaderClosure sc;

	CVolumeClosure(int scattering) : CClosurePrimitive(Volume),
	  m_scattering_label(scattering), m_shaderdata_flag(0)
	{}
	~CVolumeClosure() { }

	int scattering() const { return m_scattering_label; }
	int shaderdata_flag() const { return m_shaderdata_flag; }

protected:
	int m_scattering_label;
	int m_shaderdata_flag;
};

#define VOLUME_CLOSURE_CLASS_BEGIN(Upper, lower, TYPE) \
\
class Upper##Closure : public CVolumeClosure { \
public: \
	Upper##Closure() : CVolumeClosure(TYPE) {} \
\
	void setup() \
	{ \
		sc.prim = NULL; \
		m_shaderdata_flag = volume_##lower##_setup(&sc); \
	} \
}; \
\
static ClosureParam *volume_##lower##_params() \
{ \
	static ClosureParam params[] = {

/* parameters */

#define VOLUME_CLOSURE_CLASS_END(Upper, lower) \
		CLOSURE_STRING_KEYPARAM("label"), \
	    CLOSURE_FINISH_PARAM(Upper##Closure) \
	}; \
	return params; \
} \
\
CCLOSURE_PREPARE_STATIC(volume_##lower##_prepare, Upper##Closure)

CCL_NAMESPACE_END

#endif /* __OSL_CLOSURES_H__ */

