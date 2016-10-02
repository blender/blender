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

#include "../closure/bsdf_ashikhmin_velvet.h"
#include "../closure/bsdf_diffuse.h"
#include "../closure/bsdf_oren_nayar.h"
#include "../closure/bsdf_phong_ramp.h"
#include "../closure/bsdf_diffuse_ramp.h"
#include "../closure/bsdf_microfacet.h"
#include "../closure/bsdf_microfacet_multi.h"
#include "../closure/bsdf_reflection.h"
#include "../closure/bsdf_refraction.h"
#include "../closure/bsdf_transparent.h"
#include "../closure/bsdf_ashikhmin_shirley.h"
#include "../closure/bsdf_toon.h"
#include "../closure/bsdf_hair.h"
#ifdef __SUBSURFACE__
#  include "../closure/bssrdf.h"
#endif
#ifdef __VOLUME__
#  include "../closure/volume.h"
#endif

CCL_NAMESPACE_BEGIN

ccl_device_forceinline int bsdf_sample(KernelGlobals *kg,
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
			label = bsdf_diffuse_sample(sc, ccl_fetch(sd, Ng), ccl_fetch(sd, I), ccl_fetch(sd, dI).dx, ccl_fetch(sd, dI).dy, randu, randv,
				eval, omega_in, &domega_in->dx, &domega_in->dy, pdf);
			break;
#ifdef __SVM__
		case CLOSURE_BSDF_OREN_NAYAR_ID:
			label = bsdf_oren_nayar_sample(sc, ccl_fetch(sd, Ng), ccl_fetch(sd, I), ccl_fetch(sd, dI).dx, ccl_fetch(sd, dI).dy, randu, randv,
				eval, omega_in, &domega_in->dx, &domega_in->dy, pdf);
			break;
#ifdef __OSL__
		case CLOSURE_BSDF_PHONG_RAMP_ID:
			label = bsdf_phong_ramp_sample(sc, ccl_fetch(sd, Ng), ccl_fetch(sd, I), ccl_fetch(sd, dI).dx, ccl_fetch(sd, dI).dy, randu, randv,
				eval, omega_in, &domega_in->dx, &domega_in->dy, pdf);
			break;
		case CLOSURE_BSDF_DIFFUSE_RAMP_ID:
			label = bsdf_diffuse_ramp_sample(sc, ccl_fetch(sd, Ng), ccl_fetch(sd, I), ccl_fetch(sd, dI).dx, ccl_fetch(sd, dI).dy, randu, randv,
				eval, omega_in, &domega_in->dx, &domega_in->dy, pdf);
			break;
#endif
		case CLOSURE_BSDF_TRANSLUCENT_ID:
			label = bsdf_translucent_sample(sc, ccl_fetch(sd, Ng), ccl_fetch(sd, I), ccl_fetch(sd, dI).dx, ccl_fetch(sd, dI).dy, randu, randv,
				eval, omega_in, &domega_in->dx, &domega_in->dy, pdf);
			break;
		case CLOSURE_BSDF_REFLECTION_ID:
			label = bsdf_reflection_sample(sc, ccl_fetch(sd, Ng), ccl_fetch(sd, I), ccl_fetch(sd, dI).dx, ccl_fetch(sd, dI).dy, randu, randv,
				eval, omega_in, &domega_in->dx, &domega_in->dy, pdf);
			break;
		case CLOSURE_BSDF_REFRACTION_ID:
			label = bsdf_refraction_sample(sc, ccl_fetch(sd, Ng), ccl_fetch(sd, I), ccl_fetch(sd, dI).dx, ccl_fetch(sd, dI).dy, randu, randv,
				eval, omega_in, &domega_in->dx, &domega_in->dy, pdf);
			break;
		case CLOSURE_BSDF_TRANSPARENT_ID:
			label = bsdf_transparent_sample(sc, ccl_fetch(sd, Ng), ccl_fetch(sd, I), ccl_fetch(sd, dI).dx, ccl_fetch(sd, dI).dy, randu, randv,
				eval, omega_in, &domega_in->dx, &domega_in->dy, pdf);
			break;
		case CLOSURE_BSDF_MICROFACET_GGX_ID:
		case CLOSURE_BSDF_MICROFACET_GGX_ANISO_ID:
		case CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID:
			label = bsdf_microfacet_ggx_sample(kg, sc, ccl_fetch(sd, Ng), ccl_fetch(sd, I), ccl_fetch(sd, dI).dx, ccl_fetch(sd, dI).dy, randu, randv,
				eval, omega_in, &domega_in->dx, &domega_in->dy, pdf);
			break;
		case CLOSURE_BSDF_MICROFACET_MULTI_GGX_ID:
			label = bsdf_microfacet_multi_ggx_sample(kg, sc, ccl_fetch(sd, Ng), ccl_fetch(sd, I), ccl_fetch(sd, dI).dx, ccl_fetch(sd, dI).dy, randu, randv,
			        eval, omega_in,  &domega_in->dx, &domega_in->dy, pdf, &ccl_fetch(sd, lcg_state));
			break;
		case CLOSURE_BSDF_MICROFACET_MULTI_GGX_GLASS_ID:
			label = bsdf_microfacet_multi_ggx_glass_sample(kg, sc, ccl_fetch(sd, Ng), ccl_fetch(sd, I), ccl_fetch(sd, dI).dx, ccl_fetch(sd, dI).dy, randu, randv,
			        eval, omega_in,  &domega_in->dx, &domega_in->dy, pdf, &ccl_fetch(sd, lcg_state));
			break;
		case CLOSURE_BSDF_MICROFACET_BECKMANN_ID:
		case CLOSURE_BSDF_MICROFACET_BECKMANN_ANISO_ID:
		case CLOSURE_BSDF_MICROFACET_BECKMANN_REFRACTION_ID:
			label = bsdf_microfacet_beckmann_sample(kg, sc, ccl_fetch(sd, Ng), ccl_fetch(sd, I), ccl_fetch(sd, dI).dx, ccl_fetch(sd, dI).dy, randu, randv,
				eval, omega_in, &domega_in->dx, &domega_in->dy, pdf);
			break;
		case CLOSURE_BSDF_ASHIKHMIN_SHIRLEY_ID:
		case CLOSURE_BSDF_ASHIKHMIN_SHIRLEY_ANISO_ID:
			label = bsdf_ashikhmin_shirley_sample(sc, ccl_fetch(sd, Ng), ccl_fetch(sd, I), ccl_fetch(sd, dI).dx, ccl_fetch(sd, dI).dy, randu, randv,
				eval, omega_in, &domega_in->dx, &domega_in->dy, pdf);
			break;
		case CLOSURE_BSDF_ASHIKHMIN_VELVET_ID:
			label = bsdf_ashikhmin_velvet_sample(sc, ccl_fetch(sd, Ng), ccl_fetch(sd, I), ccl_fetch(sd, dI).dx, ccl_fetch(sd, dI).dy, randu, randv,
				eval, omega_in, &domega_in->dx, &domega_in->dy, pdf);
			break;
		case CLOSURE_BSDF_DIFFUSE_TOON_ID:
			label = bsdf_diffuse_toon_sample(sc, ccl_fetch(sd, Ng), ccl_fetch(sd, I), ccl_fetch(sd, dI).dx, ccl_fetch(sd, dI).dy, randu, randv,
				eval, omega_in, &domega_in->dx, &domega_in->dy, pdf);
			break;
		case CLOSURE_BSDF_GLOSSY_TOON_ID:
			label = bsdf_glossy_toon_sample(sc, ccl_fetch(sd, Ng), ccl_fetch(sd, I), ccl_fetch(sd, dI).dx, ccl_fetch(sd, dI).dy, randu, randv,
				eval, omega_in, &domega_in->dx, &domega_in->dy, pdf);
			break;
		case CLOSURE_BSDF_HAIR_REFLECTION_ID:
			label = bsdf_hair_reflection_sample(sc, ccl_fetch(sd, Ng), ccl_fetch(sd, I), ccl_fetch(sd, dI).dx, ccl_fetch(sd, dI).dy, randu, randv,
				eval, omega_in, &domega_in->dx, &domega_in->dy, pdf);
			break;
		case CLOSURE_BSDF_HAIR_TRANSMISSION_ID:
			label = bsdf_hair_transmission_sample(sc, ccl_fetch(sd, Ng), ccl_fetch(sd, I), ccl_fetch(sd, dI).dx, ccl_fetch(sd, dI).dy, randu, randv,
				eval, omega_in, &domega_in->dx, &domega_in->dy, pdf);
			break;
#endif
#ifdef __VOLUME__
		case CLOSURE_VOLUME_HENYEY_GREENSTEIN_ID:
			label = volume_henyey_greenstein_sample(sc, ccl_fetch(sd, I), ccl_fetch(sd, dI).dx, ccl_fetch(sd, dI).dy, randu, randv, eval, omega_in, &domega_in->dx, &domega_in->dy, pdf);
			break;
#endif
		default:
			label = LABEL_NONE;
			break;
	}

	return label;
}

#ifndef __KERNEL_CUDA__
ccl_device
#else
ccl_device_forceinline
#endif
float3 bsdf_eval(KernelGlobals *kg,
                 ShaderData *sd,
                 const ShaderClosure *sc,
                 const float3 omega_in,
                 float *pdf)
{
	float3 eval;

	if(dot(ccl_fetch(sd, Ng), omega_in) >= 0.0f) {
		switch(sc->type) {
			case CLOSURE_BSDF_DIFFUSE_ID:
			case CLOSURE_BSDF_BSSRDF_ID:
				eval = bsdf_diffuse_eval_reflect(sc, ccl_fetch(sd, I), omega_in, pdf);
				break;
#ifdef __SVM__
			case CLOSURE_BSDF_OREN_NAYAR_ID:
				eval = bsdf_oren_nayar_eval_reflect(sc, ccl_fetch(sd, I), omega_in, pdf);
				break;
#ifdef __OSL__
			case CLOSURE_BSDF_PHONG_RAMP_ID:
				eval = bsdf_phong_ramp_eval_reflect(sc, ccl_fetch(sd, I), omega_in, pdf);
				break;
			case CLOSURE_BSDF_DIFFUSE_RAMP_ID:
				eval = bsdf_diffuse_ramp_eval_reflect(sc, ccl_fetch(sd, I), omega_in, pdf);
				break;
#endif
			case CLOSURE_BSDF_TRANSLUCENT_ID:
				eval = bsdf_translucent_eval_reflect(sc, ccl_fetch(sd, I), omega_in, pdf);
				break;
			case CLOSURE_BSDF_REFLECTION_ID:
				eval = bsdf_reflection_eval_reflect(sc, ccl_fetch(sd, I), omega_in, pdf);
				break;
			case CLOSURE_BSDF_REFRACTION_ID:
				eval = bsdf_refraction_eval_reflect(sc, ccl_fetch(sd, I), omega_in, pdf);
				break;
			case CLOSURE_BSDF_TRANSPARENT_ID:
				eval = bsdf_transparent_eval_reflect(sc, ccl_fetch(sd, I), omega_in, pdf);
				break;
			case CLOSURE_BSDF_MICROFACET_GGX_ID:
			case CLOSURE_BSDF_MICROFACET_GGX_ANISO_ID:
			case CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID:
				eval = bsdf_microfacet_ggx_eval_reflect(sc, ccl_fetch(sd, I), omega_in, pdf);
				break;
			case CLOSURE_BSDF_MICROFACET_MULTI_GGX_ID:
				eval = bsdf_microfacet_multi_ggx_eval_reflect(sc, ccl_fetch(sd, I), omega_in, pdf, &ccl_fetch(sd, lcg_state));
				break;
			case CLOSURE_BSDF_MICROFACET_MULTI_GGX_GLASS_ID:
				eval = bsdf_microfacet_multi_ggx_glass_eval_reflect(sc, ccl_fetch(sd, I), omega_in, pdf, &ccl_fetch(sd, lcg_state));
				break;
			case CLOSURE_BSDF_MICROFACET_BECKMANN_ID:
			case CLOSURE_BSDF_MICROFACET_BECKMANN_ANISO_ID:
			case CLOSURE_BSDF_MICROFACET_BECKMANN_REFRACTION_ID:
				eval = bsdf_microfacet_beckmann_eval_reflect(sc, ccl_fetch(sd, I), omega_in, pdf);
				break;
			case CLOSURE_BSDF_ASHIKHMIN_SHIRLEY_ID:
			case CLOSURE_BSDF_ASHIKHMIN_SHIRLEY_ANISO_ID:
				eval = bsdf_ashikhmin_shirley_eval_reflect(sc, ccl_fetch(sd, I), omega_in, pdf);
				break;
			case CLOSURE_BSDF_ASHIKHMIN_VELVET_ID:
				eval = bsdf_ashikhmin_velvet_eval_reflect(sc, ccl_fetch(sd, I), omega_in, pdf);
				break;
			case CLOSURE_BSDF_DIFFUSE_TOON_ID:
				eval = bsdf_diffuse_toon_eval_reflect(sc, ccl_fetch(sd, I), omega_in, pdf);
				break;
			case CLOSURE_BSDF_GLOSSY_TOON_ID:
				eval = bsdf_glossy_toon_eval_reflect(sc, ccl_fetch(sd, I), omega_in, pdf);
				break;
			case CLOSURE_BSDF_HAIR_REFLECTION_ID:
				eval = bsdf_hair_reflection_eval_reflect(sc, ccl_fetch(sd, I), omega_in, pdf);
				break;
			case CLOSURE_BSDF_HAIR_TRANSMISSION_ID:
				eval = bsdf_hair_transmission_eval_reflect(sc, ccl_fetch(sd, I), omega_in, pdf);
				break;
#endif
#ifdef __VOLUME__
			case CLOSURE_VOLUME_HENYEY_GREENSTEIN_ID:
				eval = volume_henyey_greenstein_eval_phase(sc, ccl_fetch(sd, I), omega_in, pdf);
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
				eval = bsdf_diffuse_eval_transmit(sc, ccl_fetch(sd, I), omega_in, pdf);
				break;
#ifdef __SVM__
			case CLOSURE_BSDF_OREN_NAYAR_ID:
				eval = bsdf_oren_nayar_eval_transmit(sc, ccl_fetch(sd, I), omega_in, pdf);
				break;
			case CLOSURE_BSDF_TRANSLUCENT_ID:
				eval = bsdf_translucent_eval_transmit(sc, ccl_fetch(sd, I), omega_in, pdf);
				break;
			case CLOSURE_BSDF_REFLECTION_ID:
				eval = bsdf_reflection_eval_transmit(sc, ccl_fetch(sd, I), omega_in, pdf);
				break;
			case CLOSURE_BSDF_REFRACTION_ID:
				eval = bsdf_refraction_eval_transmit(sc, ccl_fetch(sd, I), omega_in, pdf);
				break;
			case CLOSURE_BSDF_TRANSPARENT_ID:
				eval = bsdf_transparent_eval_transmit(sc, ccl_fetch(sd, I), omega_in, pdf);
				break;
			case CLOSURE_BSDF_MICROFACET_GGX_ID:
			case CLOSURE_BSDF_MICROFACET_GGX_ANISO_ID:
			case CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID:
				eval = bsdf_microfacet_ggx_eval_transmit(sc, ccl_fetch(sd, I), omega_in, pdf);
				break;
			case CLOSURE_BSDF_MICROFACET_MULTI_GGX_ID:
				eval = bsdf_microfacet_multi_ggx_eval_transmit(sc, ccl_fetch(sd, I), omega_in, pdf, &ccl_fetch(sd, lcg_state));
				break;
			case CLOSURE_BSDF_MICROFACET_MULTI_GGX_GLASS_ID:
				eval = bsdf_microfacet_multi_ggx_glass_eval_transmit(sc, ccl_fetch(sd, I), omega_in, pdf, &ccl_fetch(sd, lcg_state));
				break;
			case CLOSURE_BSDF_MICROFACET_BECKMANN_ID:
			case CLOSURE_BSDF_MICROFACET_BECKMANN_ANISO_ID:
			case CLOSURE_BSDF_MICROFACET_BECKMANN_REFRACTION_ID:
				eval = bsdf_microfacet_beckmann_eval_transmit(sc, ccl_fetch(sd, I), omega_in, pdf);
				break;
			case CLOSURE_BSDF_ASHIKHMIN_SHIRLEY_ID:
			case CLOSURE_BSDF_ASHIKHMIN_SHIRLEY_ANISO_ID:
				eval = bsdf_ashikhmin_shirley_eval_transmit(sc, ccl_fetch(sd, I), omega_in, pdf);
				break;
			case CLOSURE_BSDF_ASHIKHMIN_VELVET_ID:
				eval = bsdf_ashikhmin_velvet_eval_transmit(sc, ccl_fetch(sd, I), omega_in, pdf);
				break;
			case CLOSURE_BSDF_DIFFUSE_TOON_ID:
				eval = bsdf_diffuse_toon_eval_transmit(sc, ccl_fetch(sd, I), omega_in, pdf);
				break;
			case CLOSURE_BSDF_GLOSSY_TOON_ID:
				eval = bsdf_glossy_toon_eval_transmit(sc, ccl_fetch(sd, I), omega_in, pdf);
				break;
			case CLOSURE_BSDF_HAIR_REFLECTION_ID:
				eval = bsdf_hair_reflection_eval_transmit(sc, ccl_fetch(sd, I), omega_in, pdf);
				break;
			case CLOSURE_BSDF_HAIR_TRANSMISSION_ID:
				eval = bsdf_hair_transmission_eval_transmit(sc, ccl_fetch(sd, I), omega_in, pdf);
				break;
#endif
#ifdef __VOLUME__
			case CLOSURE_VOLUME_HENYEY_GREENSTEIN_ID:
				eval = volume_henyey_greenstein_eval_phase(sc, ccl_fetch(sd, I), omega_in, pdf);
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
		case CLOSURE_BSDF_MICROFACET_MULTI_GGX_GLASS_ID:
			bsdf_microfacet_multi_ggx_blur(sc, roughness);
			break;
		case CLOSURE_BSDF_MICROFACET_GGX_ID:
		case CLOSURE_BSDF_MICROFACET_GGX_ANISO_ID:
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
		case CLOSURE_BSDF_MICROFACET_GGX_ANISO_ID:
		case CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID:
		case CLOSURE_BSDF_MICROFACET_MULTI_GGX_ID:
		case CLOSURE_BSDF_MICROFACET_MULTI_GGX_GLASS_ID:
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

