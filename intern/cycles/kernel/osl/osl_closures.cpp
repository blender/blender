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

#include <OSL/genclosure.h>
#include <OSL/oslclosure.h>

#include "osl_closures.h"
#include "osl_shader.h"

#include "util_debug.h"
#include "util_math.h"
#include "util_param.h"

#include "kernel_types.h"
#include "kernel_compat_cpu.h"
#include "split/kernel_split_data.h"
#include "kernel_globals.h"
#include "kernel_montecarlo.h"
#include "kernel_random.h"

#include "closure/alloc.h"
#include "closure/bsdf_util.h"
#include "closure/bsdf_ashikhmin_velvet.h"
#include "closure/bsdf_diffuse.h"
#include "closure/bsdf_microfacet.h"
#include "closure/bsdf_microfacet_multi.h"
#include "closure/bsdf_oren_nayar.h"
#include "closure/bsdf_reflection.h"
#include "closure/bsdf_refraction.h"
#include "closure/bsdf_transparent.h"
#include "closure/bsdf_ashikhmin_shirley.h"
#include "closure/bsdf_toon.h"
#include "closure/bsdf_hair.h"
#include "closure/volume.h"

CCL_NAMESPACE_BEGIN

using namespace OSL;

/* BSDF class definitions */

BSDF_CLOSURE_CLASS_BEGIN(Diffuse, diffuse, DiffuseBsdf, LABEL_DIFFUSE)
	CLOSURE_FLOAT3_PARAM(DiffuseClosure, params.N),
BSDF_CLOSURE_CLASS_END(Diffuse, diffuse)

BSDF_CLOSURE_CLASS_BEGIN(Translucent, translucent, DiffuseBsdf, LABEL_DIFFUSE)
	CLOSURE_FLOAT3_PARAM(TranslucentClosure, params.N),
BSDF_CLOSURE_CLASS_END(Translucent, translucent)

BSDF_CLOSURE_CLASS_BEGIN(OrenNayar, oren_nayar, OrenNayarBsdf, LABEL_DIFFUSE)
	CLOSURE_FLOAT3_PARAM(OrenNayarClosure, params.N),
	CLOSURE_FLOAT_PARAM(OrenNayarClosure, params.roughness),
BSDF_CLOSURE_CLASS_END(OrenNayar, oren_nayar)

BSDF_CLOSURE_CLASS_BEGIN(Reflection, reflection, MicrofacetBsdf, LABEL_SINGULAR)
	CLOSURE_FLOAT3_PARAM(ReflectionClosure, params.N),
BSDF_CLOSURE_CLASS_END(Reflection, reflection)

BSDF_CLOSURE_CLASS_BEGIN(Refraction, refraction, MicrofacetBsdf, LABEL_SINGULAR)
	CLOSURE_FLOAT3_PARAM(RefractionClosure, params.N),
	CLOSURE_FLOAT_PARAM(RefractionClosure, params.ior),
BSDF_CLOSURE_CLASS_END(Refraction, refraction)

BSDF_CLOSURE_CLASS_BEGIN(Transparent, transparent, ShaderClosure, LABEL_SINGULAR)
BSDF_CLOSURE_CLASS_END(Transparent, transparent)

BSDF_CLOSURE_CLASS_BEGIN(AshikhminVelvet, ashikhmin_velvet, VelvetBsdf, LABEL_DIFFUSE)
	CLOSURE_FLOAT3_PARAM(AshikhminVelvetClosure, params.N),
	CLOSURE_FLOAT_PARAM(AshikhminVelvetClosure, params.sigma),
BSDF_CLOSURE_CLASS_END(AshikhminVelvet, ashikhmin_velvet)

BSDF_CLOSURE_CLASS_BEGIN(AshikhminShirley, ashikhmin_shirley_aniso, MicrofacetBsdf, LABEL_GLOSSY|LABEL_REFLECT)
	CLOSURE_FLOAT3_PARAM(AshikhminShirleyClosure, params.N),
	CLOSURE_FLOAT3_PARAM(AshikhminShirleyClosure, params.T),
	CLOSURE_FLOAT_PARAM(AshikhminShirleyClosure, params.alpha_x),
	CLOSURE_FLOAT_PARAM(AshikhminShirleyClosure, params.alpha_y),
BSDF_CLOSURE_CLASS_END(AshikhminShirley, ashikhmin_shirley_aniso)

BSDF_CLOSURE_CLASS_BEGIN(DiffuseToon, diffuse_toon, ToonBsdf, LABEL_DIFFUSE)
	CLOSURE_FLOAT3_PARAM(DiffuseToonClosure, params.N),
	CLOSURE_FLOAT_PARAM(DiffuseToonClosure, params.size),
	CLOSURE_FLOAT_PARAM(DiffuseToonClosure, params.smooth),
BSDF_CLOSURE_CLASS_END(DiffuseToon, diffuse_toon)

BSDF_CLOSURE_CLASS_BEGIN(GlossyToon, glossy_toon, ToonBsdf, LABEL_GLOSSY)
	CLOSURE_FLOAT3_PARAM(GlossyToonClosure, params.N),
	CLOSURE_FLOAT_PARAM(GlossyToonClosure, params.size),
	CLOSURE_FLOAT_PARAM(GlossyToonClosure, params.smooth),
BSDF_CLOSURE_CLASS_END(GlossyToon, glossy_toon)

BSDF_CLOSURE_CLASS_BEGIN(MicrofacetGGX, microfacet_ggx, MicrofacetBsdf, LABEL_GLOSSY|LABEL_REFLECT)
	CLOSURE_FLOAT3_PARAM(MicrofacetGGXClosure, params.N),
	CLOSURE_FLOAT_PARAM(MicrofacetGGXClosure, params.alpha_x),
BSDF_CLOSURE_CLASS_END(MicrofacetGGX, microfacet_ggx)

BSDF_CLOSURE_CLASS_BEGIN(MicrofacetGGXAniso, microfacet_ggx_aniso, MicrofacetBsdf, LABEL_GLOSSY|LABEL_REFLECT)
	CLOSURE_FLOAT3_PARAM(MicrofacetGGXAnisoClosure, params.N),
	CLOSURE_FLOAT3_PARAM(MicrofacetGGXAnisoClosure, params.T),
	CLOSURE_FLOAT_PARAM(MicrofacetGGXAnisoClosure, params.alpha_x),
	CLOSURE_FLOAT_PARAM(MicrofacetGGXAnisoClosure, params.alpha_y),
BSDF_CLOSURE_CLASS_END(MicrofacetGGXAniso, microfacet_ggx_aniso)

BSDF_CLOSURE_CLASS_BEGIN(MicrofacetBeckmann, microfacet_beckmann, MicrofacetBsdf, LABEL_GLOSSY|LABEL_REFLECT)
	CLOSURE_FLOAT3_PARAM(MicrofacetBeckmannClosure, params.N),
	CLOSURE_FLOAT_PARAM(MicrofacetBeckmannClosure, params.alpha_x),
BSDF_CLOSURE_CLASS_END(MicrofacetBeckmann, microfacet_beckmann)

BSDF_CLOSURE_CLASS_BEGIN(MicrofacetBeckmannAniso, microfacet_beckmann_aniso, MicrofacetBsdf, LABEL_GLOSSY|LABEL_REFLECT)
	CLOSURE_FLOAT3_PARAM(MicrofacetBeckmannAnisoClosure, params.N),
	CLOSURE_FLOAT3_PARAM(MicrofacetBeckmannAnisoClosure, params.T),
	CLOSURE_FLOAT_PARAM(MicrofacetBeckmannAnisoClosure, params.alpha_x),
	CLOSURE_FLOAT_PARAM(MicrofacetBeckmannAnisoClosure, params.alpha_y),
BSDF_CLOSURE_CLASS_END(MicrofacetBeckmannAniso, microfacet_beckmann_aniso)

BSDF_CLOSURE_CLASS_BEGIN(MicrofacetGGXRefraction, microfacet_ggx_refraction, MicrofacetBsdf, LABEL_GLOSSY|LABEL_TRANSMIT)
	CLOSURE_FLOAT3_PARAM(MicrofacetGGXRefractionClosure, params.N),
	CLOSURE_FLOAT_PARAM(MicrofacetGGXRefractionClosure, params.alpha_x),
	CLOSURE_FLOAT_PARAM(MicrofacetGGXRefractionClosure, params.ior),
BSDF_CLOSURE_CLASS_END(MicrofacetGGXRefraction, microfacet_ggx_refraction)

BSDF_CLOSURE_CLASS_BEGIN(MicrofacetBeckmannRefraction, microfacet_beckmann_refraction, MicrofacetBsdf, LABEL_GLOSSY|LABEL_TRANSMIT)
	CLOSURE_FLOAT3_PARAM(MicrofacetBeckmannRefractionClosure, params.N),
	CLOSURE_FLOAT_PARAM(MicrofacetBeckmannRefractionClosure, params.alpha_x),
	CLOSURE_FLOAT_PARAM(MicrofacetBeckmannRefractionClosure, params.ior),
BSDF_CLOSURE_CLASS_END(MicrofacetBeckmannRefraction, microfacet_beckmann_refraction)

BSDF_CLOSURE_CLASS_BEGIN(HairReflection, hair_reflection, HairBsdf, LABEL_GLOSSY)
	CLOSURE_FLOAT3_PARAM(HairReflectionClosure, unused),
	CLOSURE_FLOAT_PARAM(HairReflectionClosure, params.roughness1),
	CLOSURE_FLOAT_PARAM(HairReflectionClosure, params.roughness2),
	CLOSURE_FLOAT3_PARAM(HairReflectionClosure, params.T),
	CLOSURE_FLOAT_PARAM(HairReflectionClosure, params.offset),
BSDF_CLOSURE_CLASS_END(HairReflection, hair_reflection)

BSDF_CLOSURE_CLASS_BEGIN(HairTransmission, hair_transmission, HairBsdf, LABEL_GLOSSY)
	CLOSURE_FLOAT3_PARAM(HairTransmissionClosure, unused),
	CLOSURE_FLOAT_PARAM(HairTransmissionClosure, params.roughness1),
	CLOSURE_FLOAT_PARAM(HairTransmissionClosure, params.roughness2),
	CLOSURE_FLOAT3_PARAM(HairReflectionClosure, params.T),
	CLOSURE_FLOAT_PARAM(HairReflectionClosure, params.offset),
BSDF_CLOSURE_CLASS_END(HairTransmission, hair_transmission)

VOLUME_CLOSURE_CLASS_BEGIN(VolumeHenyeyGreenstein, henyey_greenstein, HenyeyGreensteinVolume, LABEL_VOLUME_SCATTER)
	CLOSURE_FLOAT_PARAM(VolumeHenyeyGreensteinClosure, params.g),
VOLUME_CLOSURE_CLASS_END(VolumeHenyeyGreenstein, henyey_greenstein)

VOLUME_CLOSURE_CLASS_BEGIN(VolumeAbsorption, absorption, ShaderClosure, LABEL_SINGULAR)
VOLUME_CLOSURE_CLASS_END(VolumeAbsorption, absorption)

/* Registration */

static void register_closure(OSL::ShadingSystem *ss, const char *name, int id, OSL::ClosureParam *params, OSL::PrepareClosureFunc prepare)
{
	/* optimization: it's possible to not use a prepare function at all and
	 * only initialize the actual class when accessing the closure component
	 * data, but then we need to map the id to the class somehow */
	ss->register_closure(name, id, params, prepare, NULL, 16);
}

void OSLShader::register_closures(OSLShadingSystem *ss_)
{
	OSL::ShadingSystem *ss = (OSL::ShadingSystem*)ss_;
	int id = 0;

	register_closure(ss, "diffuse", id++,
		bsdf_diffuse_params(), bsdf_diffuse_prepare);
	register_closure(ss, "oren_nayar", id++,
		bsdf_oren_nayar_params(), bsdf_oren_nayar_prepare);
	register_closure(ss, "translucent", id++,
		bsdf_translucent_params(), bsdf_translucent_prepare);
	register_closure(ss, "reflection", id++,
		bsdf_reflection_params(), bsdf_reflection_prepare);
	register_closure(ss, "refraction", id++,
		bsdf_refraction_params(), bsdf_refraction_prepare);
	register_closure(ss, "transparent", id++,
		bsdf_transparent_params(), bsdf_transparent_prepare);
	register_closure(ss, "microfacet_ggx", id++,
		bsdf_microfacet_ggx_params(), bsdf_microfacet_ggx_prepare);
	register_closure(ss, "microfacet_ggx_aniso", id++,
		bsdf_microfacet_ggx_aniso_params(), bsdf_microfacet_ggx_aniso_prepare);
	register_closure(ss, "microfacet_ggx_refraction", id++,
		bsdf_microfacet_ggx_refraction_params(), bsdf_microfacet_ggx_refraction_prepare);
	register_closure(ss, "microfacet_multi_ggx", id++,
		closure_bsdf_microfacet_multi_ggx_params(), closure_bsdf_microfacet_multi_ggx_prepare);
	register_closure(ss, "microfacet_multi_ggx_glass", id++,
		closure_bsdf_microfacet_multi_ggx_glass_params(), closure_bsdf_microfacet_multi_ggx_glass_prepare);
	register_closure(ss, "microfacet_multi_ggx_aniso", id++,
		closure_bsdf_microfacet_multi_ggx_aniso_params(), closure_bsdf_microfacet_multi_ggx_aniso_prepare);
	register_closure(ss, "microfacet_beckmann", id++,
		bsdf_microfacet_beckmann_params(), bsdf_microfacet_beckmann_prepare);
	register_closure(ss, "microfacet_beckmann_aniso", id++,
		bsdf_microfacet_beckmann_aniso_params(), bsdf_microfacet_beckmann_aniso_prepare);
	register_closure(ss, "microfacet_beckmann_refraction", id++,
		bsdf_microfacet_beckmann_refraction_params(), bsdf_microfacet_beckmann_refraction_prepare);
	register_closure(ss, "ashikhmin_shirley", id++,
		bsdf_ashikhmin_shirley_aniso_params(), bsdf_ashikhmin_shirley_aniso_prepare);
	register_closure(ss, "ashikhmin_velvet", id++,
		bsdf_ashikhmin_velvet_params(), bsdf_ashikhmin_velvet_prepare);
	register_closure(ss, "diffuse_toon", id++,
		bsdf_diffuse_toon_params(), bsdf_diffuse_toon_prepare);
	register_closure(ss, "glossy_toon", id++,
		bsdf_glossy_toon_params(), bsdf_glossy_toon_prepare);

	register_closure(ss, "emission", id++,
		closure_emission_params(), closure_emission_prepare);
	register_closure(ss, "background", id++,
		closure_background_params(), closure_background_prepare);
	register_closure(ss, "holdout", id++,
		closure_holdout_params(), closure_holdout_prepare);
	register_closure(ss, "ambient_occlusion", id++,
		closure_ambient_occlusion_params(), closure_ambient_occlusion_prepare);
	register_closure(ss, "diffuse_ramp", id++,
		closure_bsdf_diffuse_ramp_params(), closure_bsdf_diffuse_ramp_prepare);
	register_closure(ss, "phong_ramp", id++,
		closure_bsdf_phong_ramp_params(), closure_bsdf_phong_ramp_prepare);
	register_closure(ss, "bssrdf_cubic", id++,
		closure_bssrdf_cubic_params(), closure_bssrdf_cubic_prepare);
	register_closure(ss, "bssrdf_gaussian", id++,
		closure_bssrdf_gaussian_params(), closure_bssrdf_gaussian_prepare);
	register_closure(ss, "bssrdf_burley", id++,
		closure_bssrdf_burley_params(), closure_bssrdf_burley_prepare);

	register_closure(ss, "hair_reflection", id++,
		bsdf_hair_reflection_params(), bsdf_hair_reflection_prepare);
	register_closure(ss, "hair_transmission", id++,
		bsdf_hair_transmission_params(), bsdf_hair_transmission_prepare);

	register_closure(ss, "henyey_greenstein", id++,
		volume_henyey_greenstein_params(), volume_henyey_greenstein_prepare);
	register_closure(ss, "absorption", id++,
		volume_absorption_params(), volume_absorption_prepare);
}

/* BSDF Closure */

bool CBSDFClosure::skip(const ShaderData *sd, int path_flag, int scattering)
{
	/* caustic options */
	if((scattering & LABEL_GLOSSY) && (path_flag & PATH_RAY_DIFFUSE)) {
		KernelGlobals *kg = sd->osl_globals;

		if((!kernel_data.integrator.caustics_reflective && (scattering & LABEL_REFLECT)) ||
		   (!kernel_data.integrator.caustics_refractive && (scattering & LABEL_TRANSMIT)))
		{
			return true;
		}
	}

	return false;
}

/* Multiscattering GGX closures */

class MicrofacetMultiClosure : public CBSDFClosure {
public:
	MicrofacetBsdf params;
	float3 color;

	MicrofacetBsdf *alloc(ShaderData *sd, int path_flag, float3 weight)
	{
		/* Technically, the MultiGGX Glass closure may also transmit. However,
		 * since this is set statically and only used for caustic flags, this
		 * is probably as good as it gets. */
	    if(!skip(sd, path_flag, LABEL_GLOSSY|LABEL_REFLECT)) {
			MicrofacetBsdf *bsdf = (MicrofacetBsdf*)bsdf_alloc_osl(sd, sizeof(MicrofacetBsdf), weight, &params);
			MicrofacetExtra *extra = (MicrofacetExtra*)closure_alloc_extra(sd, sizeof(MicrofacetExtra));
			if(bsdf && extra) {
				bsdf->extra = extra;
				bsdf->extra->color = color;
				return bsdf;
			}
		}

		return NULL;
	}
};

class MicrofacetMultiGGXClosure : public MicrofacetMultiClosure {
public:
	void setup(ShaderData *sd, int path_flag, float3 weight)
	{
		MicrofacetBsdf *bsdf = alloc(sd, path_flag, weight);
		sd->flag |= (bsdf) ? bsdf_microfacet_multi_ggx_setup(bsdf) : 0;
	}
};

ClosureParam *closure_bsdf_microfacet_multi_ggx_params()
{
	static ClosureParam params[] = {
		CLOSURE_FLOAT3_PARAM(MicrofacetMultiGGXClosure, params.N),
		CLOSURE_FLOAT_PARAM(MicrofacetMultiGGXClosure, params.alpha_x),
		CLOSURE_FLOAT3_PARAM(MicrofacetMultiGGXClosure, color),
		CLOSURE_STRING_KEYPARAM(MicrofacetMultiGGXClosure, label, "label"),
		CLOSURE_FINISH_PARAM(MicrofacetMultiGGXClosure)
	};
	return params;
}
CCLOSURE_PREPARE(closure_bsdf_microfacet_multi_ggx_prepare, MicrofacetMultiGGXClosure);

class MicrofacetMultiGGXAnisoClosure : public MicrofacetMultiClosure {
public:
	void setup(ShaderData *sd, int path_flag, float3 weight)
	{
		MicrofacetBsdf *bsdf = alloc(sd, path_flag, weight);
		sd->flag |= (bsdf) ? bsdf_microfacet_multi_ggx_aniso_setup(bsdf) : 0;
	}
};

ClosureParam *closure_bsdf_microfacet_multi_ggx_aniso_params()
{
	static ClosureParam params[] = {
		CLOSURE_FLOAT3_PARAM(MicrofacetMultiGGXClosure, params.N),
		CLOSURE_FLOAT3_PARAM(MicrofacetMultiGGXClosure, params.T),
		CLOSURE_FLOAT_PARAM(MicrofacetMultiGGXClosure, params.alpha_x),
		CLOSURE_FLOAT_PARAM(MicrofacetMultiGGXClosure, params.alpha_y),
		CLOSURE_FLOAT3_PARAM(MicrofacetMultiGGXClosure, color),
		CLOSURE_STRING_KEYPARAM(MicrofacetMultiGGXClosure, label, "label"),
		CLOSURE_FINISH_PARAM(MicrofacetMultiGGXClosure)
	};
	return params;
}
CCLOSURE_PREPARE(closure_bsdf_microfacet_multi_ggx_aniso_prepare, MicrofacetMultiGGXAnisoClosure);

class MicrofacetMultiGGXGlassClosure : public MicrofacetMultiClosure {
public:
	MicrofacetMultiGGXGlassClosure() : MicrofacetMultiClosure() {}

	void setup(ShaderData *sd, int path_flag, float3 weight)
	{
		MicrofacetBsdf *bsdf = alloc(sd, path_flag, weight);
		sd->flag |= (bsdf) ? bsdf_microfacet_multi_ggx_glass_setup(bsdf) : 0;
	}
};

ClosureParam *closure_bsdf_microfacet_multi_ggx_glass_params()
{
	static ClosureParam params[] = {
		CLOSURE_FLOAT3_PARAM(MicrofacetMultiGGXClosure, params.N),
		CLOSURE_FLOAT_PARAM(MicrofacetMultiGGXClosure, params.alpha_x),
		CLOSURE_FLOAT_PARAM(MicrofacetMultiGGXClosure, params.ior),
		CLOSURE_FLOAT3_PARAM(MicrofacetMultiGGXClosure, color),
		CLOSURE_STRING_KEYPARAM(MicrofacetMultiGGXClosure, label, "label"),
		CLOSURE_FINISH_PARAM(MicrofacetMultiGGXClosure)
	};
	return params;
}
CCLOSURE_PREPARE(closure_bsdf_microfacet_multi_ggx_glass_prepare, MicrofacetMultiGGXGlassClosure);

CCL_NAMESPACE_END

