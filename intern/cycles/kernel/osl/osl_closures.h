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

CCL_NAMESPACE_BEGIN

enum {
	OSL_CLOSURE_BSDF_DIFFUSE_ID,
	OSL_CLOSURE_BSDF_OREN_NAYAR_ID,
	OSL_CLOSURE_BSDF_TRANSLUCENT_ID,
	OSL_CLOSURE_BSDF_REFLECTION_ID,
	OSL_CLOSURE_BSDF_REFRACTION_ID,
	OSL_CLOSURE_BSDF_TRANSPARENT_ID,
	OSL_CLOSURE_BSDF_MICROFACET_GGX_ID,
	OSL_CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID,
	OSL_CLOSURE_BSDF_MICROFACET_BECKMANN_ID,
	OSL_CLOSURE_BSDF_MICROFACET_BECKMANN_REFRACTION_ID,
	OSL_CLOSURE_BSDF_WARD_ID,
	OSL_CLOSURE_BSDF_PHONG_ID,
	OSL_CLOSURE_BSDF_PHONG_RAMP_ID,
	OSL_CLOSURE_BSDF_ASHIKHMIN_VELVET_ID,
	OSL_CLOSURE_BSDF_WESTIN_BACKSCATTER_ID,
	OSL_CLOSURE_BSDF_WESTIN_SHEEN_ID,
	OSL_CLOSURE_BSSRDF_CUBIC_ID,
	OSL_CLOSURE_EMISSION_ID,
	OSL_CLOSURE_DEBUG_ID,
	OSL_CLOSURE_BACKGROUND_ID,
	OSL_CLOSURE_HOLDOUT_ID,
	OSL_CLOSURE_SUBSURFACE_ID
};

extern OSL::ClosureParam bsdf_diffuse_params[];
extern OSL::ClosureParam bsdf_oren_nayar_params[];
extern OSL::ClosureParam bsdf_translucent_params[];
extern OSL::ClosureParam bsdf_reflection_params[];
extern OSL::ClosureParam bsdf_refraction_params[];
extern OSL::ClosureParam bsdf_transparent_params[];
extern OSL::ClosureParam bsdf_microfacet_ggx_params[];
extern OSL::ClosureParam bsdf_microfacet_ggx_refraction_params[];
extern OSL::ClosureParam bsdf_microfacet_beckmann_params[];
extern OSL::ClosureParam bsdf_microfacet_beckmann_refraction_params[];
extern OSL::ClosureParam bsdf_ward_params[];
extern OSL::ClosureParam bsdf_phong_params[];
extern OSL::ClosureParam bsdf_phong_ramp_params[];
extern OSL::ClosureParam bsdf_ashikhmin_velvet_params[];
extern OSL::ClosureParam bsdf_westin_backscatter_params[];
extern OSL::ClosureParam bsdf_westin_sheen_params[];
extern OSL::ClosureParam closure_bssrdf_cubic_params[];
extern OSL::ClosureParam closure_emission_params[];
extern OSL::ClosureParam closure_debug_params[];
extern OSL::ClosureParam closure_background_params[];
extern OSL::ClosureParam closure_holdout_params[];
extern OSL::ClosureParam closure_subsurface_params[];

void bsdf_diffuse_prepare(OSL::RendererServices *, int id, void *data);
void bsdf_oren_nayar_prepare(OSL::RendererServices *, int id, void *data);
void bsdf_translucent_prepare(OSL::RendererServices *, int id, void *data);
void bsdf_reflection_prepare(OSL::RendererServices *, int id, void *data);
void bsdf_refraction_prepare(OSL::RendererServices *, int id, void *data);
void bsdf_transparent_prepare(OSL::RendererServices *, int id, void *data);
void bsdf_microfacet_ggx_prepare(OSL::RendererServices *, int id, void *data);
void bsdf_microfacet_ggx_refraction_prepare(OSL::RendererServices *, int id, void *data);
void bsdf_microfacet_beckmann_prepare(OSL::RendererServices *, int id, void *data);
void bsdf_microfacet_beckmann_refraction_prepare(OSL::RendererServices *, int id, void *data);
void bsdf_ward_prepare(OSL::RendererServices *, int id, void *data);
void bsdf_phong_prepare(OSL::RendererServices *, int id, void *data);
void bsdf_phong_ramp_prepare(OSL::RendererServices *, int id, void *data);
void bsdf_ashikhmin_velvet_prepare(OSL::RendererServices *, int id, void *data);
void bsdf_westin_backscatter_prepare(OSL::RendererServices *, int id, void *data);
void bsdf_westin_sheen_prepare(OSL::RendererServices *, int id, void *data);
void closure_bssrdf_cubic_prepare(OSL::RendererServices *, int id, void *data);
void closure_emission_prepare(OSL::RendererServices *, int id, void *data);
void closure_debug_prepare(OSL::RendererServices *, int id, void *data);
void closure_background_prepare(OSL::RendererServices *, int id, void *data);
void closure_holdout_prepare(OSL::RendererServices *, int id, void *data);
void closure_subsurface_prepare(OSL::RendererServices *, int id, void *data);

#define CLOSURE_PREPARE(name, classname)          \
void name(RendererServices *, int id, void *data) \
{                                                 \
	memset(data, 0, sizeof(classname));           \
	new (data) classname();                       \
}

CCL_NAMESPACE_END

#endif /* __OSL_CLOSURES_H__ */

