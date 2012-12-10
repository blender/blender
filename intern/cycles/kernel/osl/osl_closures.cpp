/*
 * Adapted from Open Shading Language with this license:
 *
 * Copyright (c) 2009-2010 Sony Pictures Imageworks Inc., closure_et al.
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
#include "kernel_montecarlo.h"

#include "closure/bsdf.h"
#include "closure/bsdf_ashikhmin_velvet.h"
#include "closure/bsdf_diffuse.h"
#include "closure/bsdf_microfacet.h"
#include "closure/bsdf_oren_nayar.h"
#include "closure/bsdf_phong_ramp.h"
#include "closure/bsdf_reflection.h"
#include "closure/bsdf_refraction.h"
#include "closure/bsdf_transparent.h"
#include "closure/bsdf_ward.h"
#include "closure/bsdf_westin.h"

CCL_NAMESPACE_BEGIN

using namespace OSL;

/* BSDF class definitions */

BSDF_CLOSURE_CLASS_BEGIN(Diffuse, diffuse, diffuse, LABEL_DIFFUSE)
	CLOSURE_VECTOR_PARAM(DiffuseClosure, N),
BSDF_CLOSURE_CLASS_END(Diffuse, diffuse)

BSDF_CLOSURE_CLASS_BEGIN(Translucent, translucent, translucent, LABEL_DIFFUSE)
	CLOSURE_VECTOR_PARAM(TranslucentClosure, N),
BSDF_CLOSURE_CLASS_END(Translucent, translucent)

BSDF_CLOSURE_CLASS_BEGIN(OrenNayar, oren_nayar, oren_nayar, LABEL_DIFFUSE)
	CLOSURE_VECTOR_PARAM(OrenNayarClosure, N),
	CLOSURE_FLOAT_PARAM(OrenNayarClosure, sc.data0),
BSDF_CLOSURE_CLASS_END(OrenNayar, oren_nayar)

BSDF_CLOSURE_CLASS_BEGIN(Reflection, reflection, reflection, LABEL_SINGULAR)
	CLOSURE_VECTOR_PARAM(ReflectionClosure, N),
BSDF_CLOSURE_CLASS_END(Reflection, reflection)

BSDF_CLOSURE_CLASS_BEGIN(Refraction, refraction, refraction, LABEL_SINGULAR)
	CLOSURE_VECTOR_PARAM(RefractionClosure, N),
	CLOSURE_FLOAT_PARAM(RefractionClosure, sc.data0),
BSDF_CLOSURE_CLASS_END(Refraction, refraction)

BSDF_CLOSURE_CLASS_BEGIN(WestinBackscatter, westin_backscatter, westin_backscatter, LABEL_GLOSSY)
	CLOSURE_VECTOR_PARAM(WestinBackscatterClosure, N),
	CLOSURE_FLOAT_PARAM(WestinBackscatterClosure, sc.data0),
BSDF_CLOSURE_CLASS_END(WestinBackscatter, westin_backscatter)

BSDF_CLOSURE_CLASS_BEGIN(WestinSheen, westin_sheen, westin_sheen, LABEL_DIFFUSE)
	CLOSURE_VECTOR_PARAM(WestinSheenClosure, N),
	CLOSURE_FLOAT_PARAM(WestinSheenClosure, sc.data0),
BSDF_CLOSURE_CLASS_END(WestinSheen, westin_sheen)

BSDF_CLOSURE_CLASS_BEGIN(Transparent, transparent, transparent, LABEL_SINGULAR)
BSDF_CLOSURE_CLASS_END(Transparent, transparent)

BSDF_CLOSURE_CLASS_BEGIN(AshikhminVelvet, ashikhmin_velvet, ashikhmin_velvet, LABEL_DIFFUSE)
	CLOSURE_VECTOR_PARAM(AshikhminVelvetClosure, N),
	CLOSURE_FLOAT_PARAM(AshikhminVelvetClosure, sc.data0),
BSDF_CLOSURE_CLASS_END(AshikhminVelvet, ashikhmin_velvet)

BSDF_CLOSURE_CLASS_BEGIN(Ward, ward, ward, LABEL_GLOSSY)
	CLOSURE_VECTOR_PARAM(WardClosure, N),
	CLOSURE_VECTOR_PARAM(WardClosure, T),
	CLOSURE_FLOAT_PARAM(WardClosure, sc.data0),
	CLOSURE_FLOAT_PARAM(WardClosure, sc.data1),
BSDF_CLOSURE_CLASS_END(Ward, ward)

BSDF_CLOSURE_CLASS_BEGIN(MicrofacetGGX, microfacet_ggx, microfacet_ggx, LABEL_GLOSSY)
	CLOSURE_VECTOR_PARAM(MicrofacetGGXClosure, N),
	CLOSURE_FLOAT_PARAM(MicrofacetGGXClosure, sc.data0),
BSDF_CLOSURE_CLASS_END(MicrofacetGGX, microfacet_ggx)

BSDF_CLOSURE_CLASS_BEGIN(MicrofacetBeckmann, microfacet_beckmann, microfacet_beckmann, LABEL_GLOSSY)
	CLOSURE_VECTOR_PARAM(MicrofacetBeckmannClosure, N),
	CLOSURE_FLOAT_PARAM(MicrofacetBeckmannClosure, sc.data0),
BSDF_CLOSURE_CLASS_END(MicrofacetBeckmann, microfacet_beckmann)

BSDF_CLOSURE_CLASS_BEGIN(MicrofacetGGXRefraction, microfacet_ggx_refraction, microfacet_ggx, LABEL_GLOSSY)
	CLOSURE_VECTOR_PARAM(MicrofacetGGXRefractionClosure, N),
	CLOSURE_FLOAT_PARAM(MicrofacetGGXRefractionClosure, sc.data0),
	CLOSURE_FLOAT_PARAM(MicrofacetGGXRefractionClosure, sc.data1),
BSDF_CLOSURE_CLASS_END(MicrofacetGGXRefraction, microfacet_ggx_refraction)

BSDF_CLOSURE_CLASS_BEGIN(MicrofacetBeckmannRefraction, microfacet_beckmann_refraction, microfacet_beckmann, LABEL_GLOSSY)
	CLOSURE_VECTOR_PARAM(MicrofacetBeckmannRefractionClosure, N),
	CLOSURE_FLOAT_PARAM(MicrofacetBeckmannRefractionClosure, sc.data0),
	CLOSURE_FLOAT_PARAM(MicrofacetBeckmannRefractionClosure, sc.data1),
BSDF_CLOSURE_CLASS_END(MicrofacetBeckmannRefraction, microfacet_beckmann_refraction)

/* Registration */

static void generic_closure_setup(OSL::RendererServices *, int id, void *data)
{
	assert(data);
	OSL::ClosurePrimitive *prim = (OSL::ClosurePrimitive *)data;
	prim->setup();
}

static bool generic_closure_compare(int id, const void *dataA, const void *dataB)
{
	assert(dataA && dataB);

	OSL::ClosurePrimitive *primA = (OSL::ClosurePrimitive *)dataA;
	OSL::ClosurePrimitive *primB = (OSL::ClosurePrimitive *)dataB;
	return primA->mergeable(primB);
}

static void register_closure(OSL::ShadingSystem *ss, const char *name, int id, OSL::ClosureParam *params, OSL::PrepareClosureFunc prepare)
{
	ss->register_closure(name, id, params, prepare, generic_closure_setup, generic_closure_compare);
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
	register_closure(ss, "microfacet_ggx_refraction", id++,
		bsdf_microfacet_ggx_refraction_params(), bsdf_microfacet_ggx_refraction_prepare);
	register_closure(ss, "microfacet_beckmann", id++,
		bsdf_microfacet_beckmann_params(), bsdf_microfacet_beckmann_prepare);
	register_closure(ss, "microfacet_beckmann_refraction", id++,
		bsdf_microfacet_beckmann_refraction_params(), bsdf_microfacet_beckmann_refraction_prepare);
	register_closure(ss, "ward", id++,
		bsdf_ward_params(), bsdf_ward_prepare);
	register_closure(ss, "ashikhmin_velvet", id++,
		bsdf_ashikhmin_velvet_params(), bsdf_ashikhmin_velvet_prepare);
	register_closure(ss, "westin_backscatter", id++,
		bsdf_westin_backscatter_params(), bsdf_westin_backscatter_prepare);
	register_closure(ss, "westin_sheen", id++,
		bsdf_westin_sheen_params(), bsdf_westin_sheen_prepare);
	register_closure(ss, "emission", id++,
		closure_emission_params(), closure_emission_prepare);
	register_closure(ss, "background", id++,
		closure_background_params(), closure_background_prepare);
	register_closure(ss, "holdout", id++,
		closure_holdout_params(), closure_holdout_prepare);
	register_closure(ss, "ambient_occlusion", id++,
		closure_ambient_occlusion_params(), closure_ambient_occlusion_prepare);
	register_closure(ss, "phong_ramp", id++,
		closure_bsdf_phong_ramp_params(), closure_bsdf_phong_ramp_prepare);
}

CCL_NAMESPACE_END

