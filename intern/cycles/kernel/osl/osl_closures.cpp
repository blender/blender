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
#include "util_param.h"

CCL_NAMESPACE_BEGIN

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

void OSLShader::register_closures(OSL::ShadingSystem *ss)
{
	register_closure(ss, "diffuse", OSL_CLOSURE_BSDF_DIFFUSE_ID, bsdf_diffuse_params, bsdf_diffuse_prepare);
	register_closure(ss, "oren_nayar", OSL_CLOSURE_BSDF_OREN_NAYAR_ID, bsdf_oren_nayar_params, bsdf_oren_nayar_prepare);
	register_closure(ss, "translucent", OSL_CLOSURE_BSDF_TRANSLUCENT_ID, bsdf_translucent_params, bsdf_translucent_prepare);
	register_closure(ss, "reflection", OSL_CLOSURE_BSDF_REFLECTION_ID, bsdf_reflection_params, bsdf_reflection_prepare);
	register_closure(ss, "refraction", OSL_CLOSURE_BSDF_REFRACTION_ID, bsdf_refraction_params, bsdf_refraction_prepare);
	register_closure(ss, "transparent", OSL_CLOSURE_BSDF_TRANSPARENT_ID, bsdf_transparent_params, bsdf_transparent_prepare);
	register_closure(ss, "microfacet_ggx", OSL_CLOSURE_BSDF_MICROFACET_GGX_ID, bsdf_microfacet_ggx_params, bsdf_microfacet_ggx_prepare);
	register_closure(ss, "microfacet_ggx_refraction", OSL_CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID, bsdf_microfacet_ggx_refraction_params, bsdf_microfacet_ggx_refraction_prepare);
	register_closure(ss, "microfacet_beckmann", OSL_CLOSURE_BSDF_MICROFACET_BECKMANN_ID, bsdf_microfacet_beckmann_params, bsdf_microfacet_beckmann_prepare);
	register_closure(ss, "microfacet_beckmann_refraction", OSL_CLOSURE_BSDF_MICROFACET_BECKMANN_REFRACTION_ID, bsdf_microfacet_beckmann_refraction_params, bsdf_microfacet_beckmann_refraction_prepare);
	register_closure(ss, "ward", OSL_CLOSURE_BSDF_WARD_ID, bsdf_ward_params, bsdf_ward_prepare);
	register_closure(ss, "ashikhmin_velvet", OSL_CLOSURE_BSDF_ASHIKHMIN_VELVET_ID, bsdf_ashikhmin_velvet_params, bsdf_ashikhmin_velvet_prepare);
	register_closure(ss, "westin_backscatter", OSL_CLOSURE_BSDF_WESTIN_BACKSCATTER_ID, bsdf_westin_backscatter_params, bsdf_westin_backscatter_prepare);
	register_closure(ss, "westin_sheen", OSL_CLOSURE_BSDF_WESTIN_SHEEN_ID, bsdf_westin_sheen_params, bsdf_westin_sheen_prepare);
	register_closure(ss, "bssrdf_cubic", OSL_CLOSURE_BSSRDF_CUBIC_ID, closure_bssrdf_cubic_params, closure_bssrdf_cubic_prepare);
	register_closure(ss, "emission", OSL_CLOSURE_EMISSION_ID, closure_emission_params, closure_emission_prepare);
	register_closure(ss, "debug", OSL_CLOSURE_DEBUG_ID, closure_debug_params, closure_debug_prepare);
	register_closure(ss, "background", OSL_CLOSURE_BACKGROUND_ID, closure_background_params, closure_background_prepare);
	register_closure(ss, "holdout", OSL_CLOSURE_HOLDOUT_ID, closure_holdout_params, closure_holdout_prepare);
	register_closure(ss, "subsurface", OSL_CLOSURE_SUBSURFACE_ID, closure_subsurface_params, closure_subsurface_prepare);
}

CCL_NAMESPACE_END

