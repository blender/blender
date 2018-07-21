/*
 * Copyright 2011-2013 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "kernel/closure/bsdf_ashikhmin_velvet.h"
#include "kernel/closure/bsdf_diffuse.h"
#include "kernel/closure/bsdf_oren_nayar.h"
#include "kernel/closure/bsdf_phong_ramp.h"
#include "kernel/closure/bsdf_diffuse_ramp.h"
#include "kernel/closure/bsdf_microfacet.h"
#include "kernel/closure/bsdf_microfacet_multi.h"
#include "kernel/closure/bsdf_reflection.h"
#include "kernel/closure/bsdf_refraction.h"
#include "kernel/closure/bsdf_transparent.h"
#include "kernel/closure/bsdf_ashikhmin_shirley.h"
#include "kernel/closure/bsdf_toon.h"
#include "kernel/closure/bsdf_hair.h"
#include "kernel/closure/bsdf_hair_principled.h"
#include "kernel/closure/bsdf_principled_diffuse.h"
#include "kernel/closure/bsdf_principled_sheen.h"
#include "kernel/closure/bssrdf.h"
#include "kernel/closure/volume.h"

CCL_NAMESPACE_BEGIN

/* Returns the square of the roughness of the closure if it has roughness,
 * 0 for singular closures and 1 otherwise. */
ccl_device_inline float bsdf_get_specular_roughness_squared(const ShaderClosure *sc)
{
	if(CLOSURE_IS_BSDF_SINGULAR(sc->type)) {
		return 0.0f;
	}

	if(CLOSURE_IS_BSDF_MICROFACET(sc->type)) {
		MicrofacetBsdf *bsdf = (MicrofacetBsdf*)sc;
		return bsdf->alpha_x*bsdf->alpha_y;
	}

	return 1.0f;
}

ccl_device_inline float bsdf_get_roughness_squared(const ShaderClosure *sc)
{
	/* This version includes diffuse, mainly for baking Principled BSDF
	 * where specular and metallic zero otherwise does not bake the
	 * specified roughness parameter. */
	if(sc->type == CLOSURE_BSDF_OREN_NAYAR_ID) {
		OrenNayarBsdf *bsdf = (OrenNayarBsdf*)sc;
		return sqr(sqr(bsdf->roughness));
	}

	if(sc->type == CLOSURE_BSDF_PRINCIPLED_DIFFUSE_ID) {
		PrincipledDiffuseBsdf *bsdf = (PrincipledDiffuseBsdf*)sc;
		return sqr(sqr(bsdf->roughness));
	}

	if(CLOSURE_IS_BSDF_DIFFUSE(sc->type)) {
		return 0.0f;
	}

	return bsdf_get_specular_roughness_squared(sc);
}

ccl_device_inline int bsdf_sample(KernelGlobals *kg,
                                  ShaderData *sd,
                                  const ShaderClosure *sc,
                                  float randu,
                                  float randv,
                                  float3 *eval,
                                  float3 *omega_in,
                                  differential3 *domega_in,
                                  float *pdf)
{
	int label;

	switch(sc->type) {
		case CLOSURE_BSDF_DIFFUSE_ID:
		case CLOSURE_BSDF_BSSRDF_ID:
			label = bsdf_diffuse_sample(sc, sd->Ng, sd->I, sd->dI.dx, sd->dI.dy, randu, randv,
				eval, omega_in, &domega_in->dx, &domega_in->dy, pdf);
			break;
#ifdef __SVM__
		case CLOSURE_BSDF_OREN_NAYAR_ID:
			label = bsdf_oren_nayar_sample(sc, sd->Ng, sd->I, sd->dI.dx, sd->dI.dy, randu, randv,
				eval, omega_in, &domega_in->dx, &domega_in->dy, pdf);
			break;
#ifdef __OSL__
		case CLOSURE_BSDF_PHONG_RAMP_ID:
			label = bsdf_phong_ramp_sample(sc, sd->Ng, sd->I, sd->dI.dx, sd->dI.dy, randu, randv,
				eval, omega_in, &domega_in->dx, &domega_in->dy, pdf);
			break;
		case CLOSURE_BSDF_DIFFUSE_RAMP_ID:
			label = bsdf_diffuse_ramp_sample(sc, sd->Ng, sd->I, sd->dI.dx, sd->dI.dy, randu, randv,
				eval, omega_in, &domega_in->dx, &domega_in->dy, pdf);
			break;
#endif
		case CLOSURE_BSDF_TRANSLUCENT_ID:
			label = bsdf_translucent_sample(sc, sd->Ng, sd->I, sd->dI.dx, sd->dI.dy, randu, randv,
				eval, omega_in, &domega_in->dx, &domega_in->dy, pdf);
			break;
		case CLOSURE_BSDF_REFLECTION_ID:
			label = bsdf_reflection_sample(sc, sd->Ng, sd->I, sd->dI.dx, sd->dI.dy, randu, randv,
				eval, omega_in, &domega_in->dx, &domega_in->dy, pdf);
			break;
		case CLOSURE_BSDF_REFRACTION_ID:
			label = bsdf_refraction_sample(sc, sd->Ng, sd->I, sd->dI.dx, sd->dI.dy, randu, randv,
				eval, omega_in, &domega_in->dx, &domega_in->dy, pdf);
			break;
		case CLOSURE_BSDF_TRANSPARENT_ID:
			label = bsdf_transparent_sample(sc, sd->Ng, sd->I, sd->dI.dx, sd->dI.dy, randu, randv,
				eval, omega_in, &domega_in->dx, &domega_in->dy, pdf);
			break;
		case CLOSURE_BSDF_MICROFACET_GGX_ID:
		case CLOSURE_BSDF_MICROFACET_GGX_FRESNEL_ID:
		case CLOSURE_BSDF_MICROFACET_GGX_CLEARCOAT_ID:
		case CLOSURE_BSDF_MICROFACET_GGX_ANISO_ID:
		case CLOSURE_BSDF_MICROFACET_GGX_ANISO_FRESNEL_ID:
		case CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID:
			label = bsdf_microfacet_ggx_sample(kg, sc, sd->Ng, sd->I, sd->dI.dx, sd->dI.dy, randu, randv,
				eval, omega_in, &domega_in->dx, &domega_in->dy, pdf);
			break;
		case CLOSURE_BSDF_MICROFACET_MULTI_GGX_ID:
		case CLOSURE_BSDF_MICROFACET_MULTI_GGX_FRESNEL_ID:
			label = bsdf_microfacet_multi_ggx_sample(kg, sc, sd->Ng, sd->I, sd->dI.dx, sd->dI.dy, randu, randv,
			        eval, omega_in,  &domega_in->dx, &domega_in->dy, pdf, &sd->lcg_state);
			break;
		case CLOSURE_BSDF_MICROFACET_MULTI_GGX_GLASS_ID:
		case CLOSURE_BSDF_MICROFACET_MULTI_GGX_GLASS_FRESNEL_ID:
			label = bsdf_microfacet_multi_ggx_glass_sample(kg, sc, sd->Ng, sd->I, sd->dI.dx, sd->dI.dy, randu, randv,
			        eval, omega_in,  &domega_in->dx, &domega_in->dy, pdf, &sd->lcg_state);
			break;
		case CLOSURE_BSDF_MICROFACET_BECKMANN_ID:
		case CLOSURE_BSDF_MICROFACET_BECKMANN_ANISO_ID:
		case CLOSURE_BSDF_MICROFACET_BECKMANN_REFRACTION_ID:
			label = bsdf_microfacet_beckmann_sample(kg, sc, sd->Ng, sd->I, sd->dI.dx, sd->dI.dy, randu, randv,
				eval, omega_in, &domega_in->dx, &domega_in->dy, pdf);
			break;
		case CLOSURE_BSDF_ASHIKHMIN_SHIRLEY_ID:
		case CLOSURE_BSDF_ASHIKHMIN_SHIRLEY_ANISO_ID:
			label = bsdf_ashikhmin_shirley_sample(sc, sd->Ng, sd->I, sd->dI.dx, sd->dI.dy, randu, randv,
				eval, omega_in, &domega_in->dx, &domega_in->dy, pdf);
			break;
		case CLOSURE_BSDF_ASHIKHMIN_VELVET_ID:
			label = bsdf_ashikhmin_velvet_sample(sc, sd->Ng, sd->I, sd->dI.dx, sd->dI.dy, randu, randv,
				eval, omega_in, &domega_in->dx, &domega_in->dy, pdf);
			break;
		case CLOSURE_BSDF_DIFFUSE_TOON_ID:
			label = bsdf_diffuse_toon_sample(sc, sd->Ng, sd->I, sd->dI.dx, sd->dI.dy, randu, randv,
				eval, omega_in, &domega_in->dx, &domega_in->dy, pdf);
			break;
		case CLOSURE_BSDF_GLOSSY_TOON_ID:
			label = bsdf_glossy_toon_sample(sc, sd->Ng, sd->I, sd->dI.dx, sd->dI.dy, randu, randv,
				eval, omega_in, &domega_in->dx, &domega_in->dy, pdf);
			break;
		case CLOSURE_BSDF_HAIR_REFLECTION_ID:
			label = bsdf_hair_reflection_sample(sc, sd->Ng, sd->I, sd->dI.dx, sd->dI.dy, randu, randv,
				eval, omega_in, &domega_in->dx, &domega_in->dy, pdf);
			break;
		case CLOSURE_BSDF_HAIR_TRANSMISSION_ID:
			label = bsdf_hair_transmission_sample(sc, sd->Ng, sd->I, sd->dI.dx, sd->dI.dy, randu, randv,
				eval, omega_in, &domega_in->dx, &domega_in->dy, pdf);
			break;
		case CLOSURE_BSDF_HAIR_PRINCIPLED_ID:
			label = bsdf_principled_hair_sample(kg, sc, sd, randu, randv,
				eval, omega_in, &domega_in->dx, &domega_in->dy, pdf);
			break;
#ifdef __PRINCIPLED__
		case CLOSURE_BSDF_PRINCIPLED_DIFFUSE_ID:
		case CLOSURE_BSDF_BSSRDF_PRINCIPLED_ID:
			label = bsdf_principled_diffuse_sample(sc, sd->Ng, sd->I, sd->dI.dx, sd->dI.dy, randu, randv,
				eval, omega_in, &domega_in->dx, &domega_in->dy, pdf);
			break;
		case CLOSURE_BSDF_PRINCIPLED_SHEEN_ID:
			label = bsdf_principled_sheen_sample(sc, sd->Ng, sd->I, sd->dI.dx, sd->dI.dy, randu, randv,
				eval, omega_in, &domega_in->dx, &domega_in->dy, pdf);
			break;
#endif  /* __PRINCIPLED__ */
#endif
#ifdef __VOLUME__
		case CLOSURE_VOLUME_HENYEY_GREENSTEIN_ID:
			label = volume_henyey_greenstein_sample(sc, sd->I, sd->dI.dx, sd->dI.dy, randu, randv, eval, omega_in, &domega_in->dx, &domega_in->dy, pdf);
			break;
#endif
		default:
			label = LABEL_NONE;
			break;
	}

	/* Test if BSDF sample should be treated as transparent for background. */
	if(label & LABEL_TRANSMIT) {
		float threshold_squared = kernel_data.background.transparent_roughness_squared_threshold;

		if(threshold_squared >= 0.0f) {
			if(bsdf_get_specular_roughness_squared(sc) <= threshold_squared) {
				label |= LABEL_TRANSMIT_TRANSPARENT;
			}
		}
	}

	return label;
}

#ifndef __KERNEL_CUDA__
ccl_device
#else
ccl_device_inline
#endif
float3 bsdf_eval(KernelGlobals *kg,
                 ShaderData *sd,
                 const ShaderClosure *sc,
                 const float3 omega_in,
                 float *pdf)
{
	float3 eval;

	if(dot(sd->Ng, omega_in) >= 0.0f) {
		switch(sc->type) {
			case CLOSURE_BSDF_DIFFUSE_ID:
			case CLOSURE_BSDF_BSSRDF_ID:
				eval = bsdf_diffuse_eval_reflect(sc, sd->I, omega_in, pdf);
				break;
#ifdef __SVM__
			case CLOSURE_BSDF_OREN_NAYAR_ID:
				eval = bsdf_oren_nayar_eval_reflect(sc, sd->I, omega_in, pdf);
				break;
#ifdef __OSL__
			case CLOSURE_BSDF_PHONG_RAMP_ID:
				eval = bsdf_phong_ramp_eval_reflect(sc, sd->I, omega_in, pdf);
				break;
			case CLOSURE_BSDF_DIFFUSE_RAMP_ID:
				eval = bsdf_diffuse_ramp_eval_reflect(sc, sd->I, omega_in, pdf);
				break;
#endif
			case CLOSURE_BSDF_TRANSLUCENT_ID:
				eval = bsdf_translucent_eval_reflect(sc, sd->I, omega_in, pdf);
				break;
			case CLOSURE_BSDF_REFLECTION_ID:
				eval = bsdf_reflection_eval_reflect(sc, sd->I, omega_in, pdf);
				break;
			case CLOSURE_BSDF_REFRACTION_ID:
				eval = bsdf_refraction_eval_reflect(sc, sd->I, omega_in, pdf);
				break;
			case CLOSURE_BSDF_TRANSPARENT_ID:
				eval = bsdf_transparent_eval_reflect(sc, sd->I, omega_in, pdf);
				break;
			case CLOSURE_BSDF_MICROFACET_GGX_ID:
			case CLOSURE_BSDF_MICROFACET_GGX_FRESNEL_ID:
			case CLOSURE_BSDF_MICROFACET_GGX_CLEARCOAT_ID:
			case CLOSURE_BSDF_MICROFACET_GGX_ANISO_ID:
			case CLOSURE_BSDF_MICROFACET_GGX_ANISO_FRESNEL_ID:
			case CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID:
				eval = bsdf_microfacet_ggx_eval_reflect(sc, sd->I, omega_in, pdf);
				break;
			case CLOSURE_BSDF_MICROFACET_MULTI_GGX_ID:
			case CLOSURE_BSDF_MICROFACET_MULTI_GGX_FRESNEL_ID:
				eval = bsdf_microfacet_multi_ggx_eval_reflect(sc, sd->I, omega_in, pdf, &sd->lcg_state);
				break;
			case CLOSURE_BSDF_MICROFACET_MULTI_GGX_GLASS_ID:
			case CLOSURE_BSDF_MICROFACET_MULTI_GGX_GLASS_FRESNEL_ID:
				eval = bsdf_microfacet_multi_ggx_glass_eval_reflect(sc, sd->I, omega_in, pdf, &sd->lcg_state);
				break;
			case CLOSURE_BSDF_MICROFACET_BECKMANN_ID:
			case CLOSURE_BSDF_MICROFACET_BECKMANN_ANISO_ID:
			case CLOSURE_BSDF_MICROFACET_BECKMANN_REFRACTION_ID:
				eval = bsdf_microfacet_beckmann_eval_reflect(sc, sd->I, omega_in, pdf);
				break;
			case CLOSURE_BSDF_ASHIKHMIN_SHIRLEY_ID:
			case CLOSURE_BSDF_ASHIKHMIN_SHIRLEY_ANISO_ID:
				eval = bsdf_ashikhmin_shirley_eval_reflect(sc, sd->I, omega_in, pdf);
				break;
			case CLOSURE_BSDF_ASHIKHMIN_VELVET_ID:
				eval = bsdf_ashikhmin_velvet_eval_reflect(sc, sd->I, omega_in, pdf);
				break;
			case CLOSURE_BSDF_DIFFUSE_TOON_ID:
				eval = bsdf_diffuse_toon_eval_reflect(sc, sd->I, omega_in, pdf);
				break;
			case CLOSURE_BSDF_GLOSSY_TOON_ID:
				eval = bsdf_glossy_toon_eval_reflect(sc, sd->I, omega_in, pdf);
				break;
			case CLOSURE_BSDF_HAIR_PRINCIPLED_ID:
				eval = bsdf_principled_hair_eval(kg, sd, sc, omega_in, pdf);
				break;
			case CLOSURE_BSDF_HAIR_REFLECTION_ID:
				eval = bsdf_hair_reflection_eval_reflect(sc, sd->I, omega_in, pdf);
				break;
			case CLOSURE_BSDF_HAIR_TRANSMISSION_ID:
				eval = bsdf_hair_transmission_eval_reflect(sc, sd->I, omega_in, pdf);
				break;
#ifdef __PRINCIPLED__
			case CLOSURE_BSDF_PRINCIPLED_DIFFUSE_ID:
			case CLOSURE_BSDF_BSSRDF_PRINCIPLED_ID:
				eval = bsdf_principled_diffuse_eval_reflect(sc, sd->I, omega_in, pdf);
				break;
			case CLOSURE_BSDF_PRINCIPLED_SHEEN_ID:
				eval = bsdf_principled_sheen_eval_reflect(sc, sd->I, omega_in, pdf);
				break;
#endif  /* __PRINCIPLED__ */
#endif
#ifdef __VOLUME__
			case CLOSURE_VOLUME_HENYEY_GREENSTEIN_ID:
				eval = volume_henyey_greenstein_eval_phase(sc, sd->I, omega_in, pdf);
				break;
#endif
			default:
				eval = make_float3(0.0f, 0.0f, 0.0f);
				break;
		}
	}
	else {
		switch(sc->type) {
			case CLOSURE_BSDF_DIFFUSE_ID:
			case CLOSURE_BSDF_BSSRDF_ID:
				eval = bsdf_diffuse_eval_transmit(sc, sd->I, omega_in, pdf);
				break;
#ifdef __SVM__
			case CLOSURE_BSDF_OREN_NAYAR_ID:
				eval = bsdf_oren_nayar_eval_transmit(sc, sd->I, omega_in, pdf);
				break;
			case CLOSURE_BSDF_TRANSLUCENT_ID:
				eval = bsdf_translucent_eval_transmit(sc, sd->I, omega_in, pdf);
				break;
			case CLOSURE_BSDF_REFLECTION_ID:
				eval = bsdf_reflection_eval_transmit(sc, sd->I, omega_in, pdf);
				break;
			case CLOSURE_BSDF_REFRACTION_ID:
				eval = bsdf_refraction_eval_transmit(sc, sd->I, omega_in, pdf);
				break;
			case CLOSURE_BSDF_TRANSPARENT_ID:
				eval = bsdf_transparent_eval_transmit(sc, sd->I, omega_in, pdf);
				break;
			case CLOSURE_BSDF_MICROFACET_GGX_ID:
			case CLOSURE_BSDF_MICROFACET_GGX_FRESNEL_ID:
			case CLOSURE_BSDF_MICROFACET_GGX_CLEARCOAT_ID:
			case CLOSURE_BSDF_MICROFACET_GGX_ANISO_ID:
			case CLOSURE_BSDF_MICROFACET_GGX_ANISO_FRESNEL_ID:
			case CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID:
				eval = bsdf_microfacet_ggx_eval_transmit(sc, sd->I, omega_in, pdf);
				break;
			case CLOSURE_BSDF_MICROFACET_MULTI_GGX_ID:
			case CLOSURE_BSDF_MICROFACET_MULTI_GGX_FRESNEL_ID:
				eval = bsdf_microfacet_multi_ggx_eval_transmit(sc, sd->I, omega_in, pdf, &sd->lcg_state);
				break;
			case CLOSURE_BSDF_MICROFACET_MULTI_GGX_GLASS_ID:
			case CLOSURE_BSDF_MICROFACET_MULTI_GGX_GLASS_FRESNEL_ID:
				eval = bsdf_microfacet_multi_ggx_glass_eval_transmit(sc, sd->I, omega_in, pdf, &sd->lcg_state);
				break;
			case CLOSURE_BSDF_MICROFACET_BECKMANN_ID:
			case CLOSURE_BSDF_MICROFACET_BECKMANN_ANISO_ID:
			case CLOSURE_BSDF_MICROFACET_BECKMANN_REFRACTION_ID:
				eval = bsdf_microfacet_beckmann_eval_transmit(sc, sd->I, omega_in, pdf);
				break;
			case CLOSURE_BSDF_ASHIKHMIN_SHIRLEY_ID:
			case CLOSURE_BSDF_ASHIKHMIN_SHIRLEY_ANISO_ID:
				eval = bsdf_ashikhmin_shirley_eval_transmit(sc, sd->I, omega_in, pdf);
				break;
			case CLOSURE_BSDF_ASHIKHMIN_VELVET_ID:
				eval = bsdf_ashikhmin_velvet_eval_transmit(sc, sd->I, omega_in, pdf);
				break;
			case CLOSURE_BSDF_DIFFUSE_TOON_ID:
				eval = bsdf_diffuse_toon_eval_transmit(sc, sd->I, omega_in, pdf);
				break;
			case CLOSURE_BSDF_GLOSSY_TOON_ID:
				eval = bsdf_glossy_toon_eval_transmit(sc, sd->I, omega_in, pdf);
				break;
			case CLOSURE_BSDF_HAIR_PRINCIPLED_ID:
				eval = bsdf_principled_hair_eval(kg, sd, sc, omega_in, pdf);
				break;
			case CLOSURE_BSDF_HAIR_REFLECTION_ID:
				eval = bsdf_hair_reflection_eval_transmit(sc, sd->I, omega_in, pdf);
				break;
			case CLOSURE_BSDF_HAIR_TRANSMISSION_ID:
				eval = bsdf_hair_transmission_eval_transmit(sc, sd->I, omega_in, pdf);
				break;
#ifdef __PRINCIPLED__
			case CLOSURE_BSDF_PRINCIPLED_DIFFUSE_ID:
			case CLOSURE_BSDF_BSSRDF_PRINCIPLED_ID:
				eval = bsdf_principled_diffuse_eval_transmit(sc, sd->I, omega_in, pdf);
				break;
			case CLOSURE_BSDF_PRINCIPLED_SHEEN_ID:
				eval = bsdf_principled_sheen_eval_transmit(sc, sd->I, omega_in, pdf);
				break;
#endif  /* __PRINCIPLED__ */
#endif
#ifdef __VOLUME__
			case CLOSURE_VOLUME_HENYEY_GREENSTEIN_ID:
				eval = volume_henyey_greenstein_eval_phase(sc, sd->I, omega_in, pdf);
				break;
#endif
			default:
				eval = make_float3(0.0f, 0.0f, 0.0f);
				break;
		}
	}

	return eval;
}

ccl_device void bsdf_blur(KernelGlobals *kg, ShaderClosure *sc, float roughness)
{
	/* ToDo: do we want to blur volume closures? */
#ifdef __SVM__
	switch(sc->type) {
		case CLOSURE_BSDF_MICROFACET_MULTI_GGX_ID:
		case CLOSURE_BSDF_MICROFACET_MULTI_GGX_FRESNEL_ID:
		case CLOSURE_BSDF_MICROFACET_MULTI_GGX_GLASS_ID:
		case CLOSURE_BSDF_MICROFACET_MULTI_GGX_GLASS_FRESNEL_ID:
			bsdf_microfacet_multi_ggx_blur(sc, roughness);
			break;
		case CLOSURE_BSDF_MICROFACET_GGX_ID:
		case CLOSURE_BSDF_MICROFACET_GGX_FRESNEL_ID:
		case CLOSURE_BSDF_MICROFACET_GGX_CLEARCOAT_ID:
		case CLOSURE_BSDF_MICROFACET_GGX_ANISO_ID:
		case CLOSURE_BSDF_MICROFACET_GGX_ANISO_FRESNEL_ID:
		case CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID:
			bsdf_microfacet_ggx_blur(sc, roughness);
			break;
		case CLOSURE_BSDF_MICROFACET_BECKMANN_ID:
		case CLOSURE_BSDF_MICROFACET_BECKMANN_ANISO_ID:
		case CLOSURE_BSDF_MICROFACET_BECKMANN_REFRACTION_ID:
			bsdf_microfacet_beckmann_blur(sc, roughness);
			break;
		case CLOSURE_BSDF_ASHIKHMIN_SHIRLEY_ID:
		case CLOSURE_BSDF_ASHIKHMIN_SHIRLEY_ANISO_ID:
			bsdf_ashikhmin_shirley_blur(sc, roughness);
			break;
		case CLOSURE_BSDF_HAIR_PRINCIPLED_ID:
			bsdf_principled_hair_blur(sc, roughness);
			break;
		default:
			break;
	}
#endif
}

ccl_device bool bsdf_merge(ShaderClosure *a, ShaderClosure *b)
{
#ifdef __SVM__
	switch(a->type) {
		case CLOSURE_BSDF_TRANSPARENT_ID:
			return true;
		case CLOSURE_BSDF_DIFFUSE_ID:
		case CLOSURE_BSDF_BSSRDF_ID:
		case CLOSURE_BSDF_TRANSLUCENT_ID:
			return bsdf_diffuse_merge(a, b);
		case CLOSURE_BSDF_OREN_NAYAR_ID:
			return bsdf_oren_nayar_merge(a, b);
		case CLOSURE_BSDF_REFLECTION_ID:
		case CLOSURE_BSDF_REFRACTION_ID:
		case CLOSURE_BSDF_MICROFACET_GGX_ID:
		case CLOSURE_BSDF_MICROFACET_GGX_FRESNEL_ID:
		case CLOSURE_BSDF_MICROFACET_GGX_CLEARCOAT_ID:
		case CLOSURE_BSDF_MICROFACET_GGX_ANISO_ID:
		case CLOSURE_BSDF_MICROFACET_GGX_ANISO_FRESNEL_ID:
		case CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID:
		case CLOSURE_BSDF_MICROFACET_MULTI_GGX_ID:
		case CLOSURE_BSDF_MICROFACET_MULTI_GGX_FRESNEL_ID:
		case CLOSURE_BSDF_MICROFACET_MULTI_GGX_GLASS_ID:
		case CLOSURE_BSDF_MICROFACET_MULTI_GGX_GLASS_FRESNEL_ID:
		case CLOSURE_BSDF_MICROFACET_BECKMANN_ID:
		case CLOSURE_BSDF_MICROFACET_BECKMANN_ANISO_ID:
		case CLOSURE_BSDF_MICROFACET_BECKMANN_REFRACTION_ID:
		case CLOSURE_BSDF_ASHIKHMIN_SHIRLEY_ID:
		case CLOSURE_BSDF_ASHIKHMIN_SHIRLEY_ANISO_ID:
			return bsdf_microfacet_merge(a, b);
		case CLOSURE_BSDF_ASHIKHMIN_VELVET_ID:
			return bsdf_ashikhmin_velvet_merge(a, b);
		case CLOSURE_BSDF_DIFFUSE_TOON_ID:
		case CLOSURE_BSDF_GLOSSY_TOON_ID:
			return bsdf_toon_merge(a, b);
		case CLOSURE_BSDF_HAIR_REFLECTION_ID:
		case CLOSURE_BSDF_HAIR_TRANSMISSION_ID:
			return bsdf_hair_merge(a, b);
#ifdef __PRINCIPLED__
		case CLOSURE_BSDF_PRINCIPLED_DIFFUSE_ID:
		case CLOSURE_BSDF_BSSRDF_PRINCIPLED_ID:
			return bsdf_principled_diffuse_merge(a, b);
#endif
#ifdef __VOLUME__
		case CLOSURE_VOLUME_HENYEY_GREENSTEIN_ID:
			return volume_henyey_greenstein_merge(a, b);
#endif
		default:
			return false;
	}
#else
	return false;
#endif
}

CCL_NAMESPACE_END
