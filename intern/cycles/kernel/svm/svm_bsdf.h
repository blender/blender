/*
 * Copyright 2011, Blender Foundation.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "../closure/bsdf_ashikhmin_velvet.h"
#include "../closure/bsdf_diffuse.h"
#include "../closure/bsdf_oren_nayar.h"
#include "../closure/bsdf_phong_ramp.h"
#include "../closure/bsdf_diffuse_ramp.h"
#include "../closure/bsdf_microfacet.h"
#include "../closure/bsdf_reflection.h"
#include "../closure/bsdf_refraction.h"
#include "../closure/bsdf_transparent.h"
#ifdef __ANISOTROPIC__
#include "../closure/bsdf_ward.h"
#endif
#include "../closure/bsdf_westin.h"

CCL_NAMESPACE_BEGIN

__device int svm_bsdf_sample(const ShaderData *sd, const ShaderClosure *sc, float randu, float randv, float3 *eval, float3 *omega_in, differential3 *domega_in, float *pdf)
{
	int label;

	switch(sc->type) {
		case CLOSURE_BSDF_DIFFUSE_ID:
			label = bsdf_diffuse_sample(sc, sd->Ng, sd->I, sd->dI.dx, sd->dI.dy, randu, randv,
				eval, omega_in, &domega_in->dx, &domega_in->dy, pdf);
			break;
#ifdef __SVM__
		case CLOSURE_BSDF_OREN_NAYAR_ID:
			label = bsdf_oren_nayar_sample(sc, sd->Ng, sd->I, sd->dI.dx, sd->dI.dy, randu, randv,
				eval, omega_in, &domega_in->dx, &domega_in->dy, pdf);
			break;
		/*case CLOSURE_BSDF_PHONG_RAMP_ID:
			label = bsdf_phong_ramp_sample(sc, sd->Ng, sd->I, sd->dI.dx, sd->dI.dy, randu, randv,
				eval, omega_in, &domega_in->dx, &domega_in->dy, pdf);
			break;
		case CLOSURE_BSDF_DIFFUSE_RAMP_ID:
			label = bsdf_diffuse_ramp_sample(sc, sd->Ng, sd->I, sd->dI.dx, sd->dI.dy, randu, randv,
				eval, omega_in, &domega_in->dx, &domega_in->dy, pdf);
			break;*/
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
		case CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID:
			label = bsdf_microfacet_ggx_sample(sc, sd->Ng, sd->I, sd->dI.dx, sd->dI.dy, randu, randv,
				eval, omega_in, &domega_in->dx, &domega_in->dy, pdf);
			break;
		case CLOSURE_BSDF_MICROFACET_BECKMANN_ID:
		case CLOSURE_BSDF_MICROFACET_BECKMANN_REFRACTION_ID:
			label = bsdf_microfacet_beckmann_sample(sc, sd->Ng, sd->I, sd->dI.dx, sd->dI.dy, randu, randv,
				eval, omega_in, &domega_in->dx, &domega_in->dy, pdf);
			break;
#ifdef __ANISOTROPIC__
		case CLOSURE_BSDF_WARD_ID:
			label = bsdf_ward_sample(sc, sd->Ng, sd->I, sd->dI.dx, sd->dI.dy, randu, randv,
				eval, omega_in, &domega_in->dx, &domega_in->dy, pdf);
			break;
#endif
		case CLOSURE_BSDF_ASHIKHMIN_VELVET_ID:
			label = bsdf_ashikhmin_velvet_sample(sc, sd->Ng, sd->I, sd->dI.dx, sd->dI.dy, randu, randv,
				eval, omega_in, &domega_in->dx, &domega_in->dy, pdf);
			break;
		case CLOSURE_BSDF_WESTIN_BACKSCATTER_ID:
			label = bsdf_westin_backscatter_sample(sc, sd->Ng, sd->I, sd->dI.dx, sd->dI.dy, randu, randv,
				eval, omega_in, &domega_in->dx, &domega_in->dy, pdf);
			break;
		case CLOSURE_BSDF_WESTIN_SHEEN_ID:
			label = bsdf_westin_sheen_sample(sc, sd->Ng, sd->I, sd->dI.dx, sd->dI.dy, randu, randv,
				eval, omega_in, &domega_in->dx, &domega_in->dy, pdf);
			break;
#endif
		default:
			label = LABEL_NONE;
			break;
	}

	return label;
}

__device float3 svm_bsdf_eval(const ShaderData *sd, const ShaderClosure *sc, const float3 omega_in, float *pdf)
{
	float3 eval;

	if(dot(sd->Ng, omega_in) >= 0.0f) {
		switch(sc->type) {
			case CLOSURE_BSDF_DIFFUSE_ID:
				eval = bsdf_diffuse_eval_reflect(sc, sd->I, omega_in, pdf);
				break;
#ifdef __SVM__
			case CLOSURE_BSDF_OREN_NAYAR_ID:
				eval = bsdf_oren_nayar_eval_reflect(sc, sd->I, omega_in, pdf);
				break;
			/*case CLOSURE_BSDF_PHONG_RAMP_ID:
				eval = bsdf_phong_ramp_eval_reflect(sc, sd->I, omega_in, pdf);
				break;
			case CLOSURE_BSDF_DIFFUSE_RAMP_ID:
				eval = bsdf_diffuse_ramp_eval_reflect(sc, sd->I, omega_in, pdf);
				break;*/
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
			case CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID:
				eval = bsdf_microfacet_ggx_eval_reflect(sc, sd->I, omega_in, pdf);
				break;
			case CLOSURE_BSDF_MICROFACET_BECKMANN_ID:
			case CLOSURE_BSDF_MICROFACET_BECKMANN_REFRACTION_ID:
				eval = bsdf_microfacet_beckmann_eval_reflect(sc, sd->I, omega_in, pdf);
				break;
#ifdef __ANISOTROPIC__
			case CLOSURE_BSDF_WARD_ID:
				eval = bsdf_ward_eval_reflect(sc, sd->I, omega_in, pdf);
				break;
#endif
			case CLOSURE_BSDF_ASHIKHMIN_VELVET_ID:
				eval = bsdf_ashikhmin_velvet_eval_reflect(sc, sd->I, omega_in, pdf);
				break;
			case CLOSURE_BSDF_WESTIN_BACKSCATTER_ID:
				eval = bsdf_westin_backscatter_eval_reflect(sc, sd->I, omega_in, pdf);
				break;
			case CLOSURE_BSDF_WESTIN_SHEEN_ID:
				eval = bsdf_westin_sheen_eval_reflect(sc, sd->I, omega_in, pdf);
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
			case CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID:
				eval = bsdf_microfacet_ggx_eval_transmit(sc, sd->I, omega_in, pdf);
				break;
			case CLOSURE_BSDF_MICROFACET_BECKMANN_ID:
			case CLOSURE_BSDF_MICROFACET_BECKMANN_REFRACTION_ID:
				eval = bsdf_microfacet_beckmann_eval_transmit(sc, sd->I, omega_in, pdf);
				break;
#ifdef __ANISOTROPIC__
			case CLOSURE_BSDF_WARD_ID:
				eval = bsdf_ward_eval_transmit(sc, sd->I, omega_in, pdf);
				break;
#endif
			case CLOSURE_BSDF_ASHIKHMIN_VELVET_ID:
				eval = bsdf_ashikhmin_velvet_eval_transmit(sc, sd->I, omega_in, pdf);
				break;
			case CLOSURE_BSDF_WESTIN_BACKSCATTER_ID:
				eval = bsdf_westin_backscatter_eval_transmit(sc, sd->I, omega_in, pdf);
				break;
			case CLOSURE_BSDF_WESTIN_SHEEN_ID:
				eval = bsdf_westin_sheen_eval_transmit(sc, sd->I, omega_in, pdf);
				break;
#endif
			default:
				eval = make_float3(0.0f, 0.0f, 0.0f);
				break;
		}
	}

	return eval;
}

__device void svm_bsdf_blur(ShaderClosure *sc, float roughness)
{
	switch(sc->type) {
		case CLOSURE_BSDF_DIFFUSE_ID:
			bsdf_diffuse_blur(sc, roughness);
			break;
#ifdef __SVM__
		case CLOSURE_BSDF_OREN_NAYAR_ID:
			bsdf_oren_nayar_blur(sc, roughness);
			break;
		/*case CLOSURE_BSDF_PHONG_RAMP_ID:
			bsdf_phong_ramp_blur(sc, roughness);
			break;
		case CLOSURE_BSDF_DIFFUSE_RAMP_ID:
			bsdf_diffuse_ramp_blur(sc, roughness);
			break;*/
		case CLOSURE_BSDF_TRANSLUCENT_ID:
			bsdf_translucent_blur(sc, roughness);
			break;
		case CLOSURE_BSDF_REFLECTION_ID:
			bsdf_reflection_blur(sc, roughness);
			break;
		case CLOSURE_BSDF_REFRACTION_ID:
			bsdf_refraction_blur(sc, roughness);
			break;
		case CLOSURE_BSDF_TRANSPARENT_ID:
			bsdf_transparent_blur(sc, roughness);
			break;
		case CLOSURE_BSDF_MICROFACET_GGX_ID:
		case CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID:
			bsdf_microfacet_ggx_blur(sc, roughness);
			break;
		case CLOSURE_BSDF_MICROFACET_BECKMANN_ID:
		case CLOSURE_BSDF_MICROFACET_BECKMANN_REFRACTION_ID:
			bsdf_microfacet_beckmann_blur(sc, roughness);
			break;
#ifdef __ANISOTROPIC__
		case CLOSURE_BSDF_WARD_ID:
			bsdf_ward_blur(sc, roughness);
			break;
#endif
		case CLOSURE_BSDF_ASHIKHMIN_VELVET_ID:
			bsdf_ashikhmin_velvet_blur(sc, roughness);
			break;
		case CLOSURE_BSDF_WESTIN_BACKSCATTER_ID:
			bsdf_westin_backscatter_blur(sc, roughness);
			break;
		case CLOSURE_BSDF_WESTIN_SHEEN_ID:
			bsdf_westin_sheen_blur(sc, roughness);
			break;
#endif
		default:
			break;
	}
}

CCL_NAMESPACE_END

