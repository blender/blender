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

#include "util/util_types.h"
#include "kernel/kernel_types.h"

#include <OSL/oslclosure.h>
#include <OSL/oslexec.h>
#include <OSL/genclosure.h>

CCL_NAMESPACE_BEGIN

OSL::ClosureParam *closure_emission_params();
OSL::ClosureParam *closure_background_params();
OSL::ClosureParam *closure_holdout_params();
OSL::ClosureParam *closure_bsdf_diffuse_ramp_params();
OSL::ClosureParam *closure_bsdf_phong_ramp_params();
OSL::ClosureParam *closure_bsdf_transparent_params();
OSL::ClosureParam *closure_bssrdf_params();
OSL::ClosureParam *closure_absorption_params();
OSL::ClosureParam *closure_henyey_greenstein_params();
OSL::ClosureParam *closure_bsdf_microfacet_multi_ggx_params();
OSL::ClosureParam *closure_bsdf_microfacet_multi_ggx_glass_params();
OSL::ClosureParam *closure_bsdf_microfacet_multi_ggx_aniso_params();
OSL::ClosureParam *closure_bsdf_microfacet_ggx_fresnel_params();
OSL::ClosureParam *closure_bsdf_microfacet_ggx_aniso_fresnel_params();
OSL::ClosureParam *closure_bsdf_microfacet_multi_ggx_fresnel_params();
OSL::ClosureParam *closure_bsdf_microfacet_multi_ggx_glass_fresnel_params();
OSL::ClosureParam *closure_bsdf_microfacet_multi_ggx_aniso_fresnel_params();
OSL::ClosureParam *closure_bsdf_principled_clearcoat_params();

void closure_emission_prepare(OSL::RendererServices *, int id, void *data);
void closure_background_prepare(OSL::RendererServices *, int id, void *data);
void closure_holdout_prepare(OSL::RendererServices *, int id, void *data);
void closure_bsdf_diffuse_ramp_prepare(OSL::RendererServices *, int id, void *data);
void closure_bsdf_phong_ramp_prepare(OSL::RendererServices *, int id, void *data);
void closure_bsdf_transparent_prepare(OSL::RendererServices *, int id, void *data);
void closure_bssrdf_prepare(OSL::RendererServices *, int id, void *data);
void closure_absorption_prepare(OSL::RendererServices *, int id, void *data);
void closure_henyey_greenstein_prepare(OSL::RendererServices *, int id, void *data);
void closure_bsdf_microfacet_multi_ggx_prepare(OSL::RendererServices *, int id, void *data);
void closure_bsdf_microfacet_multi_ggx_glass_prepare(OSL::RendererServices *, int id, void *data);
void closure_bsdf_microfacet_multi_ggx_aniso_prepare(OSL::RendererServices *, int id, void *data);
void closure_bsdf_microfacet_ggx_fresnel_prepare(OSL::RendererServices *, int id, void *data);
void closure_bsdf_microfacet_ggx_aniso_fresnel_prepare(OSL::RendererServices *, int id, void *data);
void closure_bsdf_microfacet_multi_ggx_fresnel_prepare(OSL::RendererServices *, int id, void *data);
void closure_bsdf_microfacet_multi_ggx_glass_fresnel_prepare(OSL::RendererServices *, int id, void *data);
void closure_bsdf_microfacet_multi_ggx_aniso_fresnel_prepare(OSL::RendererServices *, int id, void *data);
void closure_bsdf_principled_clearcoat_prepare(OSL::RendererServices *, int id, void *data);
void closure_bsdf_principled_hair_prepare(OSL::RendererServices *, int id, void *data);

#define CCLOSURE_PREPARE(name, classname)          \
void name(RendererServices *, int id, void *data) \
{                                                 \
	memset(data, 0, sizeof(classname));           \
	new (data) classname();                       \
}

#define CCLOSURE_PREPARE_STATIC(name, classname) static CCLOSURE_PREPARE(name, classname)

#define CLOSURE_FLOAT3_PARAM(st, fld) \
	{ TypeDesc::TypeVector, (int)reckless_offsetof(st, fld), NULL, sizeof(OSL::Vec3) }

#define TO_VEC3(v) OSL::Vec3(v.x, v.y, v.z)
#define TO_COLOR3(v) OSL::Color3(v.x, v.y, v.z)
#define TO_FLOAT3(v) make_float3(v[0], v[1], v[2])

/* Closure */

class CClosurePrimitive {
public:
	virtual void setup(ShaderData *sd, int path_flag, float3 weight) = 0;

	OSL::ustring label;
};

/* BSDF */

class CBSDFClosure : public CClosurePrimitive {
public:
	bool skip(const ShaderData *sd, int path_flag, int scattering);
};

#define BSDF_CLOSURE_CLASS_BEGIN(Upper, lower, structname, TYPE) \
\
class Upper##Closure : public CBSDFClosure { \
public: \
	structname params; \
	float3 unused; \
\
	void setup(ShaderData *sd, int path_flag, float3 weight) \
	{ \
	    if(!skip(sd, path_flag, TYPE)) { \
			structname *bsdf = (structname*)bsdf_alloc_osl(sd, sizeof(structname), weight, &params); \
			sd->flag |= (bsdf) ? bsdf_##lower##_setup(bsdf) : 0; \
		} \
	} \
}; \
\
static ClosureParam *bsdf_##lower##_params() \
{ \
	static ClosureParam params[] = {

/* parameters */

#define BSDF_CLOSURE_CLASS_END(Upper, lower) \
		CLOSURE_STRING_KEYPARAM(Upper##Closure, label, "label"), \
		CLOSURE_FINISH_PARAM(Upper##Closure) \
	}; \
	return params; \
} \
\
CCLOSURE_PREPARE_STATIC(bsdf_##lower##_prepare, Upper##Closure)

CCL_NAMESPACE_END

#endif /* __OSL_CLOSURES_H__ */
