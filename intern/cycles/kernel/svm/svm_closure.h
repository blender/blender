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

CCL_NAMESPACE_BEGIN

/* Closure Nodes */

ccl_device void svm_node_glass_setup(ShaderData *sd, MicrofacetBsdf *bsdf, int type, float eta, float roughness, bool refract)
{
	if(type == CLOSURE_BSDF_SHARP_GLASS_ID) {
		if(refract) {
			bsdf->alpha_y = 0.0f;
			bsdf->alpha_x = 0.0f;
			bsdf->ior = eta;
			ccl_fetch(sd, flag) |= bsdf_refraction_setup(bsdf);
		}
		else {
			bsdf->alpha_y = 0.0f;
			bsdf->alpha_x = 0.0f;
			bsdf->ior = 0.0f;
			ccl_fetch(sd, flag) |= bsdf_reflection_setup(bsdf);
		}
	}
	else if(type == CLOSURE_BSDF_MICROFACET_BECKMANN_GLASS_ID) {
		bsdf->alpha_x = roughness;
		bsdf->alpha_y = roughness;
		bsdf->ior = eta;

		if(refract)
			ccl_fetch(sd, flag) |= bsdf_microfacet_beckmann_refraction_setup(bsdf);
		else
			ccl_fetch(sd, flag) |= bsdf_microfacet_beckmann_setup(bsdf);
	}
	else {
		bsdf->alpha_x = roughness;
		bsdf->alpha_y = roughness;
		bsdf->ior = eta;

		if(refract)
			ccl_fetch(sd, flag) |= bsdf_microfacet_ggx_refraction_setup(bsdf);
		else
			ccl_fetch(sd, flag) |= bsdf_microfacet_ggx_setup(bsdf);
	}
}

ccl_device void svm_node_closure_bsdf(KernelGlobals *kg, ShaderData *sd, float *stack, uint4 node, int path_flag, int *offset)
{
	uint type, param1_offset, param2_offset;

	uint mix_weight_offset;
	decode_node_uchar4(node.y, &type, &param1_offset, &param2_offset, &mix_weight_offset);
	float mix_weight = (stack_valid(mix_weight_offset)? stack_load_float(stack, mix_weight_offset): 1.0f);

	/* note we read this extra node before weight check, so offset is added */
	uint4 data_node = read_node(kg, offset);

	if(mix_weight == 0.0f)
		return;

	float3 N = stack_valid(data_node.x)? stack_load_float3(stack, data_node.x): ccl_fetch(sd, N);

	float param1 = (stack_valid(param1_offset))? stack_load_float(stack, param1_offset): __uint_as_float(node.z);
	float param2 = (stack_valid(param2_offset))? stack_load_float(stack, param2_offset): __uint_as_float(node.w);

	switch(type) {
		case CLOSURE_BSDF_DIFFUSE_ID: {
			float3 weight = ccl_fetch(sd, svm_closure_weight) * mix_weight;
			OrenNayarBsdf *bsdf = (OrenNayarBsdf*)bsdf_alloc(sd, sizeof(OrenNayarBsdf), weight);

			if(bsdf) {
				bsdf->N = N;

				float roughness = param1;

				if(roughness == 0.0f) {
					ccl_fetch(sd, flag) |= bsdf_diffuse_setup((DiffuseBsdf*)bsdf);
				}
				else {
					bsdf->roughness = roughness;
					ccl_fetch(sd, flag) |= bsdf_oren_nayar_setup(bsdf);
				}
			}
			break;
		}
		case CLOSURE_BSDF_TRANSLUCENT_ID: {
			float3 weight = ccl_fetch(sd, svm_closure_weight) * mix_weight;
			DiffuseBsdf *bsdf = (DiffuseBsdf*)bsdf_alloc(sd, sizeof(DiffuseBsdf), weight);

			if(bsdf) {
				bsdf->N = N;
				ccl_fetch(sd, flag) |= bsdf_translucent_setup(bsdf);
			}
			break;
		}
		case CLOSURE_BSDF_TRANSPARENT_ID: {
			float3 weight = ccl_fetch(sd, svm_closure_weight) * mix_weight;
			ShaderClosure *bsdf = bsdf_alloc(sd, sizeof(ShaderClosure), weight);

			if(bsdf) {
				ccl_fetch(sd, flag) |= bsdf_transparent_setup(bsdf);
			}
			break;
		}
		case CLOSURE_BSDF_REFLECTION_ID:
		case CLOSURE_BSDF_MICROFACET_GGX_ID:
		case CLOSURE_BSDF_MICROFACET_BECKMANN_ID:
		case CLOSURE_BSDF_ASHIKHMIN_SHIRLEY_ID:
		case CLOSURE_BSDF_MICROFACET_MULTI_GGX_ID: {
#ifdef __CAUSTICS_TRICKS__
			if(!kernel_data.integrator.caustics_reflective && (path_flag & PATH_RAY_DIFFUSE))
				break;
#endif
			float3 weight = ccl_fetch(sd, svm_closure_weight) * mix_weight;
			MicrofacetBsdf *bsdf = (MicrofacetBsdf*)bsdf_alloc(sd, sizeof(MicrofacetBsdf), weight);

			if(bsdf) {
				bsdf->N = N;
				bsdf->alpha_x = param1;
				bsdf->alpha_y = param1;
				bsdf->ior = 0.0f;
				bsdf->extra = NULL;

				/* setup bsdf */
				if(type == CLOSURE_BSDF_REFLECTION_ID)
					ccl_fetch(sd, flag) |= bsdf_reflection_setup(bsdf);
				else if(type == CLOSURE_BSDF_MICROFACET_BECKMANN_ID)
					ccl_fetch(sd, flag) |= bsdf_microfacet_beckmann_setup(bsdf);
				else if(type == CLOSURE_BSDF_MICROFACET_GGX_ID)
					ccl_fetch(sd, flag) |= bsdf_microfacet_ggx_setup(bsdf);
				else if(type == CLOSURE_BSDF_MICROFACET_MULTI_GGX_ID) {
					kernel_assert(stack_valid(data_node.z));
					bsdf->extra = (MicrofacetExtra*)closure_alloc_extra(sd, sizeof(MicrofacetExtra));
					if(bsdf->extra) {
						bsdf->extra->color = stack_load_float3(stack, data_node.z);
						ccl_fetch(sd, flag) |= bsdf_microfacet_multi_ggx_setup(bsdf);
					}
				}
				else
					ccl_fetch(sd, flag) |= bsdf_ashikhmin_shirley_setup(bsdf);
			}

			break;
		}
		case CLOSURE_BSDF_REFRACTION_ID:
		case CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID:
		case CLOSURE_BSDF_MICROFACET_BECKMANN_REFRACTION_ID: {
#ifdef __CAUSTICS_TRICKS__
			if(!kernel_data.integrator.caustics_refractive && (path_flag & PATH_RAY_DIFFUSE))
				break;
#endif
			float3 weight = ccl_fetch(sd, svm_closure_weight) * mix_weight;
			MicrofacetBsdf *bsdf = (MicrofacetBsdf*)bsdf_alloc(sd, sizeof(MicrofacetBsdf), weight);

			if(bsdf) {
				bsdf->N = N;
				bsdf->extra = NULL;

				float eta = fmaxf(param2, 1e-5f);
				eta = (ccl_fetch(sd, flag) & SD_BACKFACING)? 1.0f/eta: eta;

				/* setup bsdf */
				if(type == CLOSURE_BSDF_REFRACTION_ID) {
					bsdf->alpha_x = 0.0f;
					bsdf->alpha_y = 0.0f;
					bsdf->ior = eta;

					ccl_fetch(sd, flag) |= bsdf_refraction_setup(bsdf);
				}
				else {
					bsdf->alpha_x = param1;
					bsdf->alpha_y = param1;
					bsdf->ior = eta;

					if(type == CLOSURE_BSDF_MICROFACET_BECKMANN_REFRACTION_ID)
						ccl_fetch(sd, flag) |= bsdf_microfacet_beckmann_refraction_setup(bsdf);
					else
						ccl_fetch(sd, flag) |= bsdf_microfacet_ggx_refraction_setup(bsdf);
				}
			}

			break;
		}
		case CLOSURE_BSDF_SHARP_GLASS_ID:
		case CLOSURE_BSDF_MICROFACET_GGX_GLASS_ID:
		case CLOSURE_BSDF_MICROFACET_BECKMANN_GLASS_ID: {
#ifdef __CAUSTICS_TRICKS__
			if(!kernel_data.integrator.caustics_reflective &&
			   !kernel_data.integrator.caustics_refractive && (path_flag & PATH_RAY_DIFFUSE))
			{
				break;
			}
#endif
			float3 weight = ccl_fetch(sd, svm_closure_weight) * mix_weight;

			/* index of refraction */
			float eta = fmaxf(param2, 1e-5f);
			eta = (ccl_fetch(sd, flag) & SD_BACKFACING)? 1.0f/eta: eta;

			/* fresnel */
			float cosNO = dot(N, ccl_fetch(sd, I));
			float fresnel = fresnel_dielectric_cos(cosNO, eta);
			float roughness = param1;

			/* reflection */
#ifdef __CAUSTICS_TRICKS__
			if(kernel_data.integrator.caustics_reflective || (path_flag & PATH_RAY_DIFFUSE) == 0)
#endif
			{
				MicrofacetBsdf *bsdf = (MicrofacetBsdf*)bsdf_alloc(sd, sizeof(MicrofacetBsdf), weight*fresnel);

				if(bsdf) {
					bsdf->N = N;
					bsdf->extra = NULL;
					svm_node_glass_setup(sd, bsdf, type, eta, roughness, false);
				}
			}

			/* refraction */
#ifdef __CAUSTICS_TRICKS__
			if(kernel_data.integrator.caustics_refractive || (path_flag & PATH_RAY_DIFFUSE) == 0)
#endif
			{
				MicrofacetBsdf *bsdf = (MicrofacetBsdf*)bsdf_alloc(sd, sizeof(MicrofacetBsdf), weight*(1.0f - fresnel));

				if(bsdf) {
					bsdf->N = N;
					bsdf->extra = NULL;
					svm_node_glass_setup(sd, bsdf, type, eta, roughness, true);
				}
			}

			break;
		}
		case CLOSURE_BSDF_MICROFACET_MULTI_GGX_GLASS_ID: {
#ifdef __CAUSTICS_TRICKS__
			if(!kernel_data.integrator.caustics_reflective && !kernel_data.integrator.caustics_refractive && (path_flag & PATH_RAY_DIFFUSE))
				break;
#endif
			float3 weight = ccl_fetch(sd, svm_closure_weight) * mix_weight;
			MicrofacetBsdf *bsdf = (MicrofacetBsdf*)bsdf_alloc(sd, sizeof(MicrofacetBsdf), weight);
			MicrofacetExtra *extra = (MicrofacetExtra*)closure_alloc_extra(sd, sizeof(MicrofacetExtra));

			if(bsdf && extra) {
				bsdf->N = N;
				bsdf->extra = extra;
				bsdf->T = make_float3(0.0f, 0.0f, 0.0f);

				bsdf->alpha_x = param1;
				bsdf->alpha_y = param1;
				float eta = fmaxf(param2, 1e-5f);
				bsdf->ior = (ccl_fetch(sd, flag) & SD_BACKFACING)? 1.0f/eta: eta;

				kernel_assert(stack_valid(data_node.z));
				bsdf->extra->color = stack_load_float3(stack, data_node.z);

				/* setup bsdf */
				ccl_fetch(sd, flag) |= bsdf_microfacet_multi_ggx_glass_setup(bsdf);
			}

			break;
		}
		case CLOSURE_BSDF_MICROFACET_BECKMANN_ANISO_ID:
		case CLOSURE_BSDF_MICROFACET_GGX_ANISO_ID:
		case CLOSURE_BSDF_MICROFACET_MULTI_GGX_ANISO_ID:
		case CLOSURE_BSDF_ASHIKHMIN_SHIRLEY_ANISO_ID: {
#ifdef __CAUSTICS_TRICKS__
			if(!kernel_data.integrator.caustics_reflective && (path_flag & PATH_RAY_DIFFUSE))
				break;
#endif
			float3 weight = ccl_fetch(sd, svm_closure_weight) * mix_weight;
			MicrofacetBsdf *bsdf = (MicrofacetBsdf*)bsdf_alloc(sd, sizeof(MicrofacetBsdf), weight);

			if(bsdf) {
				bsdf->N = N;
				bsdf->extra = NULL;
				bsdf->T = stack_load_float3(stack, data_node.y);

				/* rotate tangent */
				float rotation = stack_load_float(stack, data_node.z);

				if(rotation != 0.0f)
					bsdf->T = rotate_around_axis(bsdf->T, bsdf->N, rotation * M_2PI_F);

				/* compute roughness */
				float roughness = param1;
				float anisotropy = clamp(param2, -0.99f, 0.99f);

				if(anisotropy < 0.0f) {
					bsdf->alpha_x = roughness/(1.0f + anisotropy);
					bsdf->alpha_y = roughness*(1.0f + anisotropy);
				}
				else {
					bsdf->alpha_x = roughness*(1.0f - anisotropy);
					bsdf->alpha_y = roughness/(1.0f - anisotropy);
				}

				bsdf->ior = 0.0f;

				if(type == CLOSURE_BSDF_MICROFACET_BECKMANN_ANISO_ID) {
					ccl_fetch(sd, flag) |= bsdf_microfacet_beckmann_aniso_setup(bsdf);
				}
				else if(type == CLOSURE_BSDF_MICROFACET_GGX_ANISO_ID) {
					ccl_fetch(sd, flag) |= bsdf_microfacet_ggx_aniso_setup(bsdf);
				}
				else if(type == CLOSURE_BSDF_MICROFACET_MULTI_GGX_ANISO_ID) {
					kernel_assert(stack_valid(data_node.w));
					bsdf->extra = (MicrofacetExtra*)closure_alloc_extra(sd, sizeof(MicrofacetExtra));
					if(bsdf->extra) {
						bsdf->extra->color = stack_load_float3(stack, data_node.w);
						ccl_fetch(sd, flag) |= bsdf_microfacet_multi_ggx_aniso_setup(bsdf);
					}
				}
				else
					ccl_fetch(sd, flag) |= bsdf_ashikhmin_shirley_aniso_setup(bsdf);
			}
			break;
		}
		case CLOSURE_BSDF_ASHIKHMIN_VELVET_ID: {
			float3 weight = ccl_fetch(sd, svm_closure_weight) * mix_weight;
			VelvetBsdf *bsdf = (VelvetBsdf*)bsdf_alloc(sd, sizeof(VelvetBsdf), weight);

			if(bsdf) {
				bsdf->N = N;

				bsdf->sigma = saturate(param1);
				ccl_fetch(sd, flag) |= bsdf_ashikhmin_velvet_setup(bsdf);
			}
			break;
		}
		case CLOSURE_BSDF_GLOSSY_TOON_ID:
#ifdef __CAUSTICS_TRICKS__
			if(!kernel_data.integrator.caustics_reflective && (path_flag & PATH_RAY_DIFFUSE))
				break;
#endif
		case CLOSURE_BSDF_DIFFUSE_TOON_ID: {
			float3 weight = ccl_fetch(sd, svm_closure_weight) * mix_weight;
			ToonBsdf *bsdf = (ToonBsdf*)bsdf_alloc(sd, sizeof(ToonBsdf), weight);

			if(bsdf) {
				bsdf->N = N;
				bsdf->size = param1;
				bsdf->smooth = param2;
				
				if(type == CLOSURE_BSDF_DIFFUSE_TOON_ID)
					ccl_fetch(sd, flag) |= bsdf_diffuse_toon_setup(bsdf);
				else
					ccl_fetch(sd, flag) |= bsdf_glossy_toon_setup(bsdf);
			}
			break;
		}
#ifdef __HAIR__
		case CLOSURE_BSDF_HAIR_REFLECTION_ID:
		case CLOSURE_BSDF_HAIR_TRANSMISSION_ID: {
			float3 weight = ccl_fetch(sd, svm_closure_weight) * mix_weight;
			
			if(ccl_fetch(sd, flag) & SD_BACKFACING && ccl_fetch(sd, type) & PRIMITIVE_ALL_CURVE) {
				ShaderClosure *bsdf = bsdf_alloc(sd, sizeof(ShaderClosure), weight);

				if(bsdf) {
					/* todo: giving a fixed weight here will cause issues when
					 * mixing multiple BSDFS. energy will not be conserved and
					 * the throughput can blow up after multiple bounces. we
					 * better figure out a way to skip backfaces from rays
					 * spawned by transmission from the front */
					bsdf->weight = make_float3(1.0f, 1.0f, 1.0f);
					ccl_fetch(sd, flag) |= bsdf_transparent_setup(bsdf);
				}
			}
			else {
				HairBsdf *bsdf = (HairBsdf*)bsdf_alloc(sd, sizeof(HairBsdf), weight);

				if(bsdf) {
					bsdf->roughness1 = param1;
					bsdf->roughness2 = param2;
					bsdf->offset = -stack_load_float(stack, data_node.z);

					if(stack_valid(data_node.y)) {
						bsdf->T = normalize(stack_load_float3(stack, data_node.y));
					}
					else if(!(ccl_fetch(sd, type) & PRIMITIVE_ALL_CURVE)) {
						bsdf->T = normalize(ccl_fetch(sd, dPdv));
						bsdf->offset = 0.0f;
					}
					else
						bsdf->T = normalize(ccl_fetch(sd, dPdu));

					if(type == CLOSURE_BSDF_HAIR_REFLECTION_ID) {
						ccl_fetch(sd, flag) |= bsdf_hair_reflection_setup(bsdf);
					}
					else {
						ccl_fetch(sd, flag) |= bsdf_hair_transmission_setup(bsdf);
					}
				}
			}

			break;
		}
#endif

#ifdef __SUBSURFACE__
		case CLOSURE_BSSRDF_CUBIC_ID:
		case CLOSURE_BSSRDF_GAUSSIAN_ID:
		case CLOSURE_BSSRDF_BURLEY_ID: {
			float3 albedo = ccl_fetch(sd, svm_closure_weight);
			float3 weight = ccl_fetch(sd, svm_closure_weight) * mix_weight;
			float sample_weight = fabsf(average(weight));
			
			/* disable in case of diffuse ancestor, can't see it well then and
			 * adds considerably noise due to probabilities of continuing path
			 * getting lower and lower */
			if(path_flag & PATH_RAY_DIFFUSE_ANCESTOR)
				param1 = 0.0f;

			if(sample_weight > CLOSURE_WEIGHT_CUTOFF) {
				/* radius * scale */
				float3 radius = stack_load_float3(stack, data_node.z)*param1;
				/* sharpness */
				float sharpness = stack_load_float(stack, data_node.w);
				/* texture color blur */
				float texture_blur = param2;

				/* create one closure per color channel */
				Bssrdf *bssrdf = bssrdf_alloc(sd, make_float3(weight.x, 0.0f, 0.0f));
				if(bssrdf) {
					bssrdf->sample_weight = sample_weight;
					bssrdf->radius = radius.x;
					bssrdf->texture_blur = texture_blur;
					bssrdf->albedo = albedo.x;
					bssrdf->sharpness = sharpness;
					bssrdf->N = N;
					ccl_fetch(sd, flag) |= bssrdf_setup(bssrdf, (ClosureType)type);
				}

				bssrdf = bssrdf_alloc(sd, make_float3(0.0f, weight.y, 0.0f));
				if(bssrdf) {
					bssrdf->sample_weight = sample_weight;
					bssrdf->radius = radius.y;
					bssrdf->texture_blur = texture_blur;
					bssrdf->albedo = albedo.y;
					bssrdf->sharpness = sharpness;
					bssrdf->N = N;
					ccl_fetch(sd, flag) |= bssrdf_setup(bssrdf, (ClosureType)type);
				}

				bssrdf = bssrdf_alloc(sd, make_float3(0.0f, 0.0f, weight.z));
				if(bssrdf) {
					bssrdf->sample_weight = sample_weight;
					bssrdf->radius = radius.z;
					bssrdf->texture_blur = texture_blur;
					bssrdf->albedo = albedo.z;
					bssrdf->sharpness = sharpness;
					bssrdf->N = N;
					ccl_fetch(sd, flag) |= bssrdf_setup(bssrdf, (ClosureType)type);
				}
			}

			break;
		}
#endif
		default:
			break;
	}
}

ccl_device void svm_node_closure_volume(KernelGlobals *kg, ShaderData *sd, float *stack, uint4 node, int path_flag)
{
#ifdef __VOLUME__
	uint type, param1_offset, param2_offset;

	uint mix_weight_offset;
	decode_node_uchar4(node.y, &type, &param1_offset, &param2_offset, &mix_weight_offset);
	float mix_weight = (stack_valid(mix_weight_offset)? stack_load_float(stack, mix_weight_offset): 1.0f);

	if(mix_weight == 0.0f)
		return;

	float param1 = (stack_valid(param1_offset))? stack_load_float(stack, param1_offset): __uint_as_float(node.z);
	float param2 = (stack_valid(param2_offset))? stack_load_float(stack, param2_offset): __uint_as_float(node.w);
	float density = fmaxf(param1, 0.0f);

	switch(type) {
		case CLOSURE_VOLUME_ABSORPTION_ID: {
			float3 weight = (make_float3(1.0f, 1.0f, 1.0f) - ccl_fetch(sd, svm_closure_weight)) * mix_weight * density;
			ShaderClosure *sc = closure_alloc(sd, sizeof(ShaderClosure), CLOSURE_NONE_ID, weight);

			if(sc) {
				ccl_fetch(sd, flag) |= volume_absorption_setup(sc);
			}
			break;
		}
		case CLOSURE_VOLUME_HENYEY_GREENSTEIN_ID: {
			float3 weight = ccl_fetch(sd, svm_closure_weight) * mix_weight * density;
			HenyeyGreensteinVolume *volume = (HenyeyGreensteinVolume*)bsdf_alloc(sd, sizeof(HenyeyGreensteinVolume), weight);

			if(volume) {
				volume->g = param2; /* g */
				ccl_fetch(sd, flag) |= volume_henyey_greenstein_setup(volume);
			}
			break;
		}
		default:
			break;
	}
#endif
}

ccl_device void svm_node_closure_emission(ShaderData *sd, float *stack, uint4 node)
{
	uint mix_weight_offset = node.y;

	if(stack_valid(mix_weight_offset)) {
		float mix_weight = stack_load_float(stack, mix_weight_offset);

		if(mix_weight == 0.0f)
			return;

		closure_alloc(sd, sizeof(ShaderClosure), CLOSURE_EMISSION_ID, ccl_fetch(sd, svm_closure_weight) * mix_weight);
	}
	else
		closure_alloc(sd, sizeof(ShaderClosure), CLOSURE_EMISSION_ID, ccl_fetch(sd, svm_closure_weight));

	ccl_fetch(sd, flag) |= SD_EMISSION;
}

ccl_device void svm_node_closure_background(ShaderData *sd, float *stack, uint4 node)
{
	uint mix_weight_offset = node.y;

	if(stack_valid(mix_weight_offset)) {
		float mix_weight = stack_load_float(stack, mix_weight_offset);

		if(mix_weight == 0.0f)
			return;

		closure_alloc(sd, sizeof(ShaderClosure), CLOSURE_BACKGROUND_ID, ccl_fetch(sd, svm_closure_weight) * mix_weight);
	}
	else
		closure_alloc(sd, sizeof(ShaderClosure), CLOSURE_BACKGROUND_ID, ccl_fetch(sd, svm_closure_weight));
}

ccl_device void svm_node_closure_holdout(ShaderData *sd, float *stack, uint4 node)
{
	uint mix_weight_offset = node.y;

	if(stack_valid(mix_weight_offset)) {
		float mix_weight = stack_load_float(stack, mix_weight_offset);

		if(mix_weight == 0.0f)
			return;

		closure_alloc(sd, sizeof(ShaderClosure), CLOSURE_HOLDOUT_ID, ccl_fetch(sd, svm_closure_weight) * mix_weight);
	}
	else
		closure_alloc(sd, sizeof(ShaderClosure), CLOSURE_HOLDOUT_ID, ccl_fetch(sd, svm_closure_weight));

	ccl_fetch(sd, flag) |= SD_HOLDOUT;
}

ccl_device void svm_node_closure_ambient_occlusion(ShaderData *sd, float *stack, uint4 node)
{
	uint mix_weight_offset = node.y;

	if(stack_valid(mix_weight_offset)) {
		float mix_weight = stack_load_float(stack, mix_weight_offset);

		if(mix_weight == 0.0f)
			return;

		closure_alloc(sd, sizeof(ShaderClosure), CLOSURE_AMBIENT_OCCLUSION_ID, ccl_fetch(sd, svm_closure_weight) * mix_weight);
	}
	else
		closure_alloc(sd, sizeof(ShaderClosure), CLOSURE_AMBIENT_OCCLUSION_ID, ccl_fetch(sd, svm_closure_weight));

	ccl_fetch(sd, flag) |= SD_AO;
}

/* Closure Nodes */

ccl_device_inline void svm_node_closure_store_weight(ShaderData *sd, float3 weight)
{
	ccl_fetch(sd, svm_closure_weight) = weight;
}

ccl_device void svm_node_closure_set_weight(ShaderData *sd, uint r, uint g, uint b)
{
	float3 weight = make_float3(__uint_as_float(r), __uint_as_float(g), __uint_as_float(b));
	svm_node_closure_store_weight(sd, weight);
}

ccl_device void svm_node_closure_weight(ShaderData *sd, float *stack, uint weight_offset)
{
	float3 weight = stack_load_float3(stack, weight_offset);

	svm_node_closure_store_weight(sd, weight);
}

ccl_device void svm_node_emission_weight(KernelGlobals *kg, ShaderData *sd, float *stack, uint4 node)
{
	uint color_offset = node.y;
	uint strength_offset = node.z;

	float strength = stack_load_float(stack, strength_offset);
	float3 weight = stack_load_float3(stack, color_offset)*strength;

	svm_node_closure_store_weight(sd, weight);
}

ccl_device void svm_node_mix_closure(ShaderData *sd, float *stack, uint4 node)
{
	/* fetch weight from blend input, previous mix closures,
	 * and write to stack to be used by closure nodes later */
	uint weight_offset, in_weight_offset, weight1_offset, weight2_offset;
	decode_node_uchar4(node.y, &weight_offset, &in_weight_offset, &weight1_offset, &weight2_offset);

	float weight = stack_load_float(stack, weight_offset);
	weight = saturate(weight);

	float in_weight = (stack_valid(in_weight_offset))? stack_load_float(stack, in_weight_offset): 1.0f;

	if(stack_valid(weight1_offset))
		stack_store_float(stack, weight1_offset, in_weight*(1.0f - weight));
	if(stack_valid(weight2_offset))
		stack_store_float(stack, weight2_offset, in_weight*weight);
}

/* (Bump) normal */

ccl_device void svm_node_set_normal(KernelGlobals *kg, ShaderData *sd, float *stack, uint in_direction, uint out_normal)
{
	float3 normal = stack_load_float3(stack, in_direction);
	ccl_fetch(sd, N) = normal;
	stack_store_float3(stack, out_normal, normal);
}

CCL_NAMESPACE_END

