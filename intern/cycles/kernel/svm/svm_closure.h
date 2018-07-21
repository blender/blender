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

/* Hair Melanin */

ccl_device_inline float3 sigma_from_concentration(float eumelanin, float pheomelanin)
{
	return eumelanin*make_float3(0.506f, 0.841f, 1.653f) + pheomelanin*make_float3(0.343f, 0.733f, 1.924f);
}

ccl_device_inline float3 sigma_from_reflectance(float3 color, float azimuthal_roughness)
{
	float x = azimuthal_roughness;
	float roughness_fac = (((((0.245f*x) + 5.574f)*x - 10.73f)*x + 2.532f)*x - 0.215f)*x + 5.969f;
	float3 sigma = log3(color) / roughness_fac;
	return sigma * sigma;
}

/* Closure Nodes */

ccl_device void svm_node_glass_setup(ShaderData *sd, MicrofacetBsdf *bsdf, int type, float eta, float roughness, bool refract)
{
	if(type == CLOSURE_BSDF_SHARP_GLASS_ID) {
		if(refract) {
			bsdf->alpha_y = 0.0f;
			bsdf->alpha_x = 0.0f;
			bsdf->ior = eta;
			sd->flag |= bsdf_refraction_setup(bsdf);
		}
		else {
			bsdf->alpha_y = 0.0f;
			bsdf->alpha_x = 0.0f;
			bsdf->ior = 0.0f;
			sd->flag |= bsdf_reflection_setup(bsdf);
		}
	}
	else if(type == CLOSURE_BSDF_MICROFACET_BECKMANN_GLASS_ID) {
		bsdf->alpha_x = roughness;
		bsdf->alpha_y = roughness;
		bsdf->ior = eta;

		if(refract)
			sd->flag |= bsdf_microfacet_beckmann_refraction_setup(bsdf);
		else
			sd->flag |= bsdf_microfacet_beckmann_setup(bsdf);
	}
	else {
		bsdf->alpha_x = roughness;
		bsdf->alpha_y = roughness;
		bsdf->ior = eta;

		if(refract)
			sd->flag |= bsdf_microfacet_ggx_refraction_setup(bsdf);
		else
			sd->flag |= bsdf_microfacet_ggx_setup(bsdf);
	}
}

ccl_device void svm_node_closure_bsdf(KernelGlobals *kg, ShaderData *sd, float *stack, uint4 node, ShaderType shader_type, int path_flag, int *offset)
{
	uint type, param1_offset, param2_offset;

	uint mix_weight_offset;
	decode_node_uchar4(node.y, &type, &param1_offset, &param2_offset, &mix_weight_offset);
	float mix_weight = (stack_valid(mix_weight_offset)? stack_load_float(stack, mix_weight_offset): 1.0f);

	/* note we read this extra node before weight check, so offset is added */
	uint4 data_node = read_node(kg, offset);

	/* Only compute BSDF for surfaces, transparent variable is shared with volume extinction. */
	if(mix_weight == 0.0f || shader_type != SHADER_TYPE_SURFACE) {
		if(type == CLOSURE_BSDF_PRINCIPLED_ID) {
			/* Read all principled BSDF extra data to get the right offset. */
			read_node(kg, offset);
			read_node(kg, offset);
			read_node(kg, offset);
			read_node(kg, offset);
		}

		return;
	}

	float3 N = stack_valid(data_node.x)? stack_load_float3(stack, data_node.x): sd->N;

	float param1 = (stack_valid(param1_offset))? stack_load_float(stack, param1_offset): __uint_as_float(node.z);
	float param2 = (stack_valid(param2_offset))? stack_load_float(stack, param2_offset): __uint_as_float(node.w);

	switch(type) {
#ifdef __PRINCIPLED__
		case CLOSURE_BSDF_PRINCIPLED_ID: {
			uint specular_offset, roughness_offset, specular_tint_offset, anisotropic_offset, sheen_offset,
				sheen_tint_offset, clearcoat_offset, clearcoat_roughness_offset, eta_offset, transmission_offset,
				anisotropic_rotation_offset, transmission_roughness_offset;
			uint4 data_node2 = read_node(kg, offset);

			float3 T = stack_load_float3(stack, data_node.y);
			decode_node_uchar4(data_node.z, &specular_offset, &roughness_offset, &specular_tint_offset, &anisotropic_offset);
			decode_node_uchar4(data_node.w, &sheen_offset, &sheen_tint_offset, &clearcoat_offset, &clearcoat_roughness_offset);
			decode_node_uchar4(data_node2.x, &eta_offset, &transmission_offset, &anisotropic_rotation_offset, &transmission_roughness_offset);

			// get Disney principled parameters
			float metallic = param1;
			float subsurface = param2;
			float specular = stack_load_float(stack, specular_offset);
			float roughness = stack_load_float(stack, roughness_offset);
			float specular_tint = stack_load_float(stack, specular_tint_offset);
			float anisotropic = stack_load_float(stack, anisotropic_offset);
			float sheen = stack_load_float(stack, sheen_offset);
			float sheen_tint = stack_load_float(stack, sheen_tint_offset);
			float clearcoat = stack_load_float(stack, clearcoat_offset);
			float clearcoat_roughness = stack_load_float(stack, clearcoat_roughness_offset);
			float transmission = stack_load_float(stack, transmission_offset);
			float anisotropic_rotation = stack_load_float(stack, anisotropic_rotation_offset);
			float transmission_roughness = stack_load_float(stack, transmission_roughness_offset);
			float eta = fmaxf(stack_load_float(stack, eta_offset), 1e-5f);

			ClosureType distribution = (ClosureType) data_node2.y;
			ClosureType subsurface_method = (ClosureType) data_node2.z;

			/* rotate tangent */
			if(anisotropic_rotation != 0.0f)
				T = rotate_around_axis(T, N, anisotropic_rotation * M_2PI_F);

			/* calculate ior */
			float ior = (sd->flag & SD_BACKFACING) ? 1.0f / eta : eta;

			// calculate fresnel for refraction
			float cosNO = dot(N, sd->I);
			float fresnel = fresnel_dielectric_cos(cosNO, ior);

			// calculate weights of the diffuse and specular part
			float diffuse_weight = (1.0f - saturate(metallic)) * (1.0f - saturate(transmission));

			float final_transmission = saturate(transmission) * (1.0f - saturate(metallic));
			float specular_weight = (1.0f - final_transmission);

			// get the base color
			uint4 data_base_color = read_node(kg, offset);
			float3 base_color = stack_valid(data_base_color.x) ? stack_load_float3(stack, data_base_color.x) :
				make_float3(__uint_as_float(data_base_color.y), __uint_as_float(data_base_color.z), __uint_as_float(data_base_color.w));

			// get the additional clearcoat normal and subsurface scattering radius
			uint4 data_cn_ssr = read_node(kg, offset);
			float3 clearcoat_normal = stack_valid(data_cn_ssr.x) ? stack_load_float3(stack, data_cn_ssr.x) : sd->N;
			float3 subsurface_radius = stack_valid(data_cn_ssr.y) ? stack_load_float3(stack, data_cn_ssr.y) : make_float3(1.0f, 1.0f, 1.0f);

			// get the subsurface color
			uint4 data_subsurface_color = read_node(kg, offset);
			float3 subsurface_color = stack_valid(data_subsurface_color.x) ? stack_load_float3(stack, data_subsurface_color.x) :
				make_float3(__uint_as_float(data_subsurface_color.y), __uint_as_float(data_subsurface_color.z), __uint_as_float(data_subsurface_color.w));

			float3 weight = sd->svm_closure_weight * mix_weight;

#ifdef __SUBSURFACE__
			float3 mixed_ss_base_color = subsurface_color * subsurface + base_color * (1.0f - subsurface);
			float3 subsurf_weight = weight * mixed_ss_base_color * diffuse_weight;

			/* disable in case of diffuse ancestor, can't see it well then and
			 * adds considerably noise due to probabilities of continuing path
			 * getting lower and lower */
			if(path_flag & PATH_RAY_DIFFUSE_ANCESTOR) {
				subsurface = 0.0f;

				/* need to set the base color in this case such that the
				 * rays get the correctly mixed color after transmitting
				 * the object */
				base_color = mixed_ss_base_color;
			}

			/* diffuse */
			if(fabsf(average(mixed_ss_base_color)) > CLOSURE_WEIGHT_CUTOFF) {
				if(subsurface <= CLOSURE_WEIGHT_CUTOFF && diffuse_weight > CLOSURE_WEIGHT_CUTOFF) {
					float3 diff_weight = weight * base_color * diffuse_weight;

					PrincipledDiffuseBsdf *bsdf = (PrincipledDiffuseBsdf*)bsdf_alloc(sd, sizeof(PrincipledDiffuseBsdf), diff_weight);

					if(bsdf) {
						bsdf->N = N;
						bsdf->roughness = roughness;

						/* setup bsdf */
						sd->flag |= bsdf_principled_diffuse_setup(bsdf);
					}
				}
				else if(subsurface > CLOSURE_WEIGHT_CUTOFF) {
					Bssrdf *bssrdf = bssrdf_alloc(sd, subsurf_weight);

					if(bssrdf) {
						bssrdf->radius = subsurface_radius * subsurface;
						bssrdf->albedo = (subsurface_method == CLOSURE_BSSRDF_PRINCIPLED_ID)? subsurface_color:  mixed_ss_base_color;
						bssrdf->texture_blur = 0.0f;
						bssrdf->sharpness = 0.0f;
						bssrdf->N = N;
						bssrdf->roughness = roughness;

						/* setup bsdf */
						sd->flag |= bssrdf_setup(sd, bssrdf, subsurface_method);
					}
				}
			}
#else
			/* diffuse */
			if(diffuse_weight > CLOSURE_WEIGHT_CUTOFF) {
				float3 diff_weight = weight * base_color * diffuse_weight;

				PrincipledDiffuseBsdf *bsdf = (PrincipledDiffuseBsdf*)bsdf_alloc(sd, sizeof(PrincipledDiffuseBsdf), diff_weight);

				if(bsdf) {
					bsdf->N = N;
					bsdf->roughness = roughness;

					/* setup bsdf */
					sd->flag |= bsdf_principled_diffuse_setup(bsdf);
				}
			}
#endif

			/* sheen */
			if(diffuse_weight > CLOSURE_WEIGHT_CUTOFF && sheen > CLOSURE_WEIGHT_CUTOFF) {
				float m_cdlum = linear_rgb_to_gray(kg, base_color);
				float3 m_ctint = m_cdlum > 0.0f ? base_color / m_cdlum : make_float3(1.0f, 1.0f, 1.0f); // normalize lum. to isolate hue+sat

				/* color of the sheen component */
				float3 sheen_color = make_float3(1.0f, 1.0f, 1.0f) * (1.0f - sheen_tint) + m_ctint * sheen_tint;

				float3 sheen_weight = weight * sheen * sheen_color * diffuse_weight;

				PrincipledSheenBsdf *bsdf = (PrincipledSheenBsdf*)bsdf_alloc(sd, sizeof(PrincipledSheenBsdf), sheen_weight);

				if(bsdf) {
					bsdf->N = N;

					/* setup bsdf */
					sd->flag |= bsdf_principled_sheen_setup(bsdf);
				}
			}

			/* specular reflection */
#ifdef __CAUSTICS_TRICKS__
			if(kernel_data.integrator.caustics_reflective || (path_flag & PATH_RAY_DIFFUSE) == 0) {
#endif
				if(specular_weight > CLOSURE_WEIGHT_CUTOFF && (specular > CLOSURE_WEIGHT_CUTOFF || metallic > CLOSURE_WEIGHT_CUTOFF)) {
					float3 spec_weight = weight * specular_weight;

					MicrofacetBsdf *bsdf = (MicrofacetBsdf*)bsdf_alloc(sd, sizeof(MicrofacetBsdf), spec_weight);
					if(!bsdf) {
						break;
					}

					MicrofacetExtra *extra = (MicrofacetExtra*)closure_alloc_extra(sd, sizeof(MicrofacetExtra));
					if(!extra) {
						break;
					}

					bsdf->N = N;
					bsdf->ior = (2.0f / (1.0f - safe_sqrtf(0.08f * specular))) - 1.0f;
					bsdf->T = T;
					bsdf->extra = extra;

					float aspect = safe_sqrtf(1.0f - anisotropic * 0.9f);
					float r2 = roughness * roughness;

					bsdf->alpha_x = r2 / aspect;
					bsdf->alpha_y = r2 * aspect;

					float m_cdlum = 0.3f * base_color.x + 0.6f * base_color.y + 0.1f * base_color.z; // luminance approx.
					float3 m_ctint = m_cdlum > 0.0f ? base_color / m_cdlum : make_float3(0.0f, 0.0f, 0.0f); // normalize lum. to isolate hue+sat
					float3 tmp_col = make_float3(1.0f, 1.0f, 1.0f) * (1.0f - specular_tint) + m_ctint * specular_tint;

					bsdf->extra->cspec0 = (specular * 0.08f * tmp_col) * (1.0f - metallic) + base_color * metallic;
					bsdf->extra->color = base_color;
					bsdf->extra->clearcoat = 0.0f;

					/* setup bsdf */
					if(distribution == CLOSURE_BSDF_MICROFACET_GGX_GLASS_ID || roughness <= 0.075f) /* use single-scatter GGX */
						sd->flag |= bsdf_microfacet_ggx_aniso_fresnel_setup(bsdf, sd);
					else /* use multi-scatter GGX */
						sd->flag |= bsdf_microfacet_multi_ggx_aniso_fresnel_setup(bsdf, sd);
				}
#ifdef __CAUSTICS_TRICKS__
			}
#endif

			/* BSDF */
#ifdef __CAUSTICS_TRICKS__
			if(kernel_data.integrator.caustics_reflective || kernel_data.integrator.caustics_refractive || (path_flag & PATH_RAY_DIFFUSE) == 0) {
#endif
				if(final_transmission > CLOSURE_WEIGHT_CUTOFF) {
					float3 glass_weight = weight * final_transmission;
					float3 cspec0 = base_color * specular_tint + make_float3(1.0f, 1.0f, 1.0f) * (1.0f - specular_tint);

					if(roughness <= 5e-2f || distribution == CLOSURE_BSDF_MICROFACET_GGX_GLASS_ID) { /* use single-scatter GGX */
						float refl_roughness = roughness;

						/* reflection */
#ifdef __CAUSTICS_TRICKS__
						if(kernel_data.integrator.caustics_reflective || (path_flag & PATH_RAY_DIFFUSE) == 0)
#endif
						{
							MicrofacetBsdf *bsdf = (MicrofacetBsdf*)bsdf_alloc(sd, sizeof(MicrofacetBsdf), glass_weight*fresnel);
							if(!bsdf) {
								break;
							}

							MicrofacetExtra *extra = (MicrofacetExtra*)closure_alloc_extra(sd, sizeof(MicrofacetExtra));
							if(!extra) {
								break;
							}

							bsdf->N = N;
							bsdf->T = make_float3(0.0f, 0.0f, 0.0f);
							bsdf->extra = extra;

							bsdf->alpha_x = refl_roughness * refl_roughness;
							bsdf->alpha_y = refl_roughness * refl_roughness;
							bsdf->ior = ior;

							bsdf->extra->color = base_color;
							bsdf->extra->cspec0 = cspec0;
							bsdf->extra->clearcoat = 0.0f;

							/* setup bsdf */
							sd->flag |= bsdf_microfacet_ggx_fresnel_setup(bsdf, sd);
						}

						/* refraction */
#ifdef __CAUSTICS_TRICKS__
						if(kernel_data.integrator.caustics_refractive || (path_flag & PATH_RAY_DIFFUSE) == 0)
#endif
						{
							MicrofacetBsdf *bsdf = (MicrofacetBsdf*)bsdf_alloc(sd, sizeof(MicrofacetBsdf), base_color*glass_weight*(1.0f - fresnel));
							if(!bsdf) {
								break;
							}

							bsdf->N = N;
							bsdf->T = make_float3(0.0f, 0.0f, 0.0f);
							bsdf->extra = NULL;

							if(distribution == CLOSURE_BSDF_MICROFACET_GGX_GLASS_ID)
								transmission_roughness = 1.0f - (1.0f - refl_roughness) * (1.0f - transmission_roughness);
							else
								transmission_roughness = refl_roughness;

							bsdf->alpha_x = transmission_roughness * transmission_roughness;
							bsdf->alpha_y = transmission_roughness * transmission_roughness;
							bsdf->ior = ior;

							/* setup bsdf */
							sd->flag |= bsdf_microfacet_ggx_refraction_setup(bsdf);
						}
					}
					else { /* use multi-scatter GGX */
						MicrofacetBsdf *bsdf = (MicrofacetBsdf*)bsdf_alloc(sd, sizeof(MicrofacetBsdf), glass_weight);
						if(!bsdf) {
							break;
						}

						MicrofacetExtra *extra = (MicrofacetExtra*)closure_alloc_extra(sd, sizeof(MicrofacetExtra));
						if(!extra) {
							break;
						}

						bsdf->N = N;
						bsdf->extra = extra;
						bsdf->T = make_float3(0.0f, 0.0f, 0.0f);

						bsdf->alpha_x = roughness * roughness;
						bsdf->alpha_y = roughness * roughness;
						bsdf->ior = ior;

						bsdf->extra->color = base_color;
						bsdf->extra->cspec0 = cspec0;
						bsdf->extra->clearcoat = 0.0f;

						/* setup bsdf */
						sd->flag |= bsdf_microfacet_multi_ggx_glass_fresnel_setup(bsdf, sd);
					}
				}
#ifdef __CAUSTICS_TRICKS__
			}
#endif

			/* clearcoat */
#ifdef __CAUSTICS_TRICKS__
			if(kernel_data.integrator.caustics_reflective || (path_flag & PATH_RAY_DIFFUSE) == 0) {
#endif
				if(clearcoat > CLOSURE_WEIGHT_CUTOFF) {
					MicrofacetBsdf *bsdf = (MicrofacetBsdf*)bsdf_alloc(sd, sizeof(MicrofacetBsdf), weight);
					if(!bsdf) {
						break;
					}

					MicrofacetExtra *extra = (MicrofacetExtra*)closure_alloc_extra(sd, sizeof(MicrofacetExtra));
					if(!extra) {
						break;
					}

					bsdf->N = clearcoat_normal;
					bsdf->T = make_float3(0.0f, 0.0f, 0.0f);
					bsdf->ior = 1.5f;
					bsdf->extra = extra;

					bsdf->alpha_x = clearcoat_roughness * clearcoat_roughness;
					bsdf->alpha_y = clearcoat_roughness * clearcoat_roughness;

					bsdf->extra->color = make_float3(0.0f, 0.0f, 0.0f);
					bsdf->extra->cspec0 = make_float3(0.04f, 0.04f, 0.04f);
					bsdf->extra->clearcoat = clearcoat;

					/* setup bsdf */
					sd->flag |= bsdf_microfacet_ggx_clearcoat_setup(bsdf, sd);
				}
#ifdef __CAUSTICS_TRICKS__
			}
#endif

			break;
		}
#endif  /* __PRINCIPLED__ */
		case CLOSURE_BSDF_DIFFUSE_ID: {
			float3 weight = sd->svm_closure_weight * mix_weight;
			OrenNayarBsdf *bsdf = (OrenNayarBsdf*)bsdf_alloc(sd, sizeof(OrenNayarBsdf), weight);

			if(bsdf) {
				bsdf->N = N;

				float roughness = param1;

				if(roughness == 0.0f) {
					sd->flag |= bsdf_diffuse_setup((DiffuseBsdf*)bsdf);
				}
				else {
					bsdf->roughness = roughness;
					sd->flag |= bsdf_oren_nayar_setup(bsdf);
				}
			}
			break;
		}
		case CLOSURE_BSDF_TRANSLUCENT_ID: {
			float3 weight = sd->svm_closure_weight * mix_weight;
			DiffuseBsdf *bsdf = (DiffuseBsdf*)bsdf_alloc(sd, sizeof(DiffuseBsdf), weight);

			if(bsdf) {
				bsdf->N = N;
				sd->flag |= bsdf_translucent_setup(bsdf);
			}
			break;
		}
		case CLOSURE_BSDF_TRANSPARENT_ID: {
			float3 weight = sd->svm_closure_weight * mix_weight;
			bsdf_transparent_setup(sd, weight, path_flag);
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
			float3 weight = sd->svm_closure_weight * mix_weight;
			MicrofacetBsdf *bsdf = (MicrofacetBsdf*)bsdf_alloc(sd, sizeof(MicrofacetBsdf), weight);

			if(!bsdf) {
				break;
			}

			float roughness = sqr(param1);

			bsdf->N = N;
			bsdf->T = make_float3(0.0f, 0.0f, 0.0f);
			bsdf->alpha_x = roughness;
			bsdf->alpha_y = roughness;
			bsdf->ior = 0.0f;
			bsdf->extra = NULL;

			/* setup bsdf */
			if(type == CLOSURE_BSDF_REFLECTION_ID)
				sd->flag |= bsdf_reflection_setup(bsdf);
			else if(type == CLOSURE_BSDF_MICROFACET_BECKMANN_ID)
				sd->flag |= bsdf_microfacet_beckmann_setup(bsdf);
			else if(type == CLOSURE_BSDF_MICROFACET_GGX_ID)
				sd->flag |= bsdf_microfacet_ggx_setup(bsdf);
			else if(type == CLOSURE_BSDF_MICROFACET_MULTI_GGX_ID) {
				kernel_assert(stack_valid(data_node.z));
				bsdf->extra = (MicrofacetExtra*)closure_alloc_extra(sd, sizeof(MicrofacetExtra));
				if(bsdf->extra) {
					bsdf->extra->color = stack_load_float3(stack, data_node.z);
					bsdf->extra->cspec0 = make_float3(0.0f, 0.0f, 0.0f);
					bsdf->extra->clearcoat = 0.0f;
					sd->flag |= bsdf_microfacet_multi_ggx_setup(bsdf);
				}
			}
			else {
				sd->flag |= bsdf_ashikhmin_shirley_setup(bsdf);
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
			float3 weight = sd->svm_closure_weight * mix_weight;
			MicrofacetBsdf *bsdf = (MicrofacetBsdf*)bsdf_alloc(sd, sizeof(MicrofacetBsdf), weight);

			if(bsdf) {
				bsdf->N = N;
				bsdf->T = make_float3(0.0f, 0.0f, 0.0f);
				bsdf->extra = NULL;

				float eta = fmaxf(param2, 1e-5f);
				eta = (sd->flag & SD_BACKFACING)? 1.0f/eta: eta;

				/* setup bsdf */
				if(type == CLOSURE_BSDF_REFRACTION_ID) {
					bsdf->alpha_x = 0.0f;
					bsdf->alpha_y = 0.0f;
					bsdf->ior = eta;

					sd->flag |= bsdf_refraction_setup(bsdf);
				}
				else {
					float roughness = sqr(param1);
					bsdf->alpha_x = roughness;
					bsdf->alpha_y = roughness;
					bsdf->ior = eta;

					if(type == CLOSURE_BSDF_MICROFACET_BECKMANN_REFRACTION_ID)
						sd->flag |= bsdf_microfacet_beckmann_refraction_setup(bsdf);
					else
						sd->flag |= bsdf_microfacet_ggx_refraction_setup(bsdf);
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
			float3 weight = sd->svm_closure_weight * mix_weight;

			/* index of refraction */
			float eta = fmaxf(param2, 1e-5f);
			eta = (sd->flag & SD_BACKFACING)? 1.0f/eta: eta;

			/* fresnel */
			float cosNO = dot(N, sd->I);
			float fresnel = fresnel_dielectric_cos(cosNO, eta);
			float roughness = sqr(param1);

			/* reflection */
#ifdef __CAUSTICS_TRICKS__
			if(kernel_data.integrator.caustics_reflective || (path_flag & PATH_RAY_DIFFUSE) == 0)
#endif
			{
				MicrofacetBsdf *bsdf = (MicrofacetBsdf*)bsdf_alloc(sd, sizeof(MicrofacetBsdf), weight*fresnel);

				if(bsdf) {
					bsdf->N = N;
					bsdf->T = make_float3(0.0f, 0.0f, 0.0f);
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
					bsdf->T = make_float3(0.0f, 0.0f, 0.0f);
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
			float3 weight = sd->svm_closure_weight * mix_weight;
			MicrofacetBsdf *bsdf = (MicrofacetBsdf*)bsdf_alloc(sd, sizeof(MicrofacetBsdf), weight);
			if(!bsdf) {
				break;
			}

			MicrofacetExtra *extra = (MicrofacetExtra*)closure_alloc_extra(sd, sizeof(MicrofacetExtra));
			if(!extra) {
				break;
			}

			bsdf->N = N;
			bsdf->extra = extra;
			bsdf->T = make_float3(0.0f, 0.0f, 0.0f);

			float roughness = sqr(param1);
			bsdf->alpha_x = roughness;
			bsdf->alpha_y = roughness;
			float eta = fmaxf(param2, 1e-5f);
			bsdf->ior = (sd->flag & SD_BACKFACING)? 1.0f/eta: eta;

			kernel_assert(stack_valid(data_node.z));
			bsdf->extra->color = stack_load_float3(stack, data_node.z);
			bsdf->extra->cspec0 = make_float3(0.0f, 0.0f, 0.0f);
			bsdf->extra->clearcoat = 0.0f;

			/* setup bsdf */
			sd->flag |= bsdf_microfacet_multi_ggx_glass_setup(bsdf);
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
			float3 weight = sd->svm_closure_weight * mix_weight;
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
				float roughness = sqr(param1);
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
					sd->flag |= bsdf_microfacet_beckmann_aniso_setup(bsdf);
				}
				else if(type == CLOSURE_BSDF_MICROFACET_GGX_ANISO_ID) {
					sd->flag |= bsdf_microfacet_ggx_aniso_setup(bsdf);
				}
				else if(type == CLOSURE_BSDF_MICROFACET_MULTI_GGX_ANISO_ID) {
					kernel_assert(stack_valid(data_node.w));
					bsdf->extra = (MicrofacetExtra*)closure_alloc_extra(sd, sizeof(MicrofacetExtra));
					if(bsdf->extra) {
						bsdf->extra->color = stack_load_float3(stack, data_node.w);
						bsdf->extra->cspec0 = make_float3(0.0f, 0.0f, 0.0f);
						bsdf->extra->clearcoat = 0.0f;
						sd->flag |= bsdf_microfacet_multi_ggx_aniso_setup(bsdf);
					}
				}
				else
					sd->flag |= bsdf_ashikhmin_shirley_aniso_setup(bsdf);
			}
			break;
		}
		case CLOSURE_BSDF_ASHIKHMIN_VELVET_ID: {
			float3 weight = sd->svm_closure_weight * mix_weight;
			VelvetBsdf *bsdf = (VelvetBsdf*)bsdf_alloc(sd, sizeof(VelvetBsdf), weight);

			if(bsdf) {
				bsdf->N = N;

				bsdf->sigma = saturate(param1);
				sd->flag |= bsdf_ashikhmin_velvet_setup(bsdf);
			}
			break;
		}
		case CLOSURE_BSDF_GLOSSY_TOON_ID:
#ifdef __CAUSTICS_TRICKS__
			if(!kernel_data.integrator.caustics_reflective && (path_flag & PATH_RAY_DIFFUSE))
				break;
			ATTR_FALLTHROUGH;
#endif
		case CLOSURE_BSDF_DIFFUSE_TOON_ID: {
			float3 weight = sd->svm_closure_weight * mix_weight;
			ToonBsdf *bsdf = (ToonBsdf*)bsdf_alloc(sd, sizeof(ToonBsdf), weight);

			if(bsdf) {
				bsdf->N = N;
				bsdf->size = param1;
				bsdf->smooth = param2;

				if(type == CLOSURE_BSDF_DIFFUSE_TOON_ID)
					sd->flag |= bsdf_diffuse_toon_setup(bsdf);
				else
					sd->flag |= bsdf_glossy_toon_setup(bsdf);
			}
			break;
		}
#ifdef __HAIR__
		case CLOSURE_BSDF_HAIR_PRINCIPLED_ID: {
			uint4 data_node2 = read_node(kg, offset);
			uint4 data_node3 = read_node(kg, offset);
			uint4 data_node4 = read_node(kg, offset);

			float3 weight = sd->svm_closure_weight * mix_weight;

			uint offset_ofs, ior_ofs, color_ofs, parametrization;
			decode_node_uchar4(data_node.y, &offset_ofs, &ior_ofs, &color_ofs, &parametrization);
			float alpha = stack_load_float_default(stack, offset_ofs, data_node.z);
			float ior = stack_load_float_default(stack, ior_ofs, data_node.w);

			uint coat_ofs, melanin_ofs, melanin_redness_ofs, absorption_coefficient_ofs;
			decode_node_uchar4(data_node2.x, &coat_ofs, &melanin_ofs, &melanin_redness_ofs, &absorption_coefficient_ofs);

			uint tint_ofs, random_ofs, random_color_ofs, random_roughness_ofs;
			decode_node_uchar4(data_node3.x, &tint_ofs, &random_ofs, &random_color_ofs, &random_roughness_ofs);

			const AttributeDescriptor attr_descr_random = find_attribute(kg, sd, data_node4.y);
			float random = 0.0f;
			if (attr_descr_random.offset != ATTR_STD_NOT_FOUND) {
				random = primitive_attribute_float(kg, sd, attr_descr_random, NULL, NULL);
			}
			else {
				random = stack_load_float_default(stack, random_ofs, data_node3.y);
			}


			PrincipledHairBSDF *bsdf = (PrincipledHairBSDF*)bsdf_alloc(sd, sizeof(PrincipledHairBSDF), weight);
			if(bsdf) {
				PrincipledHairExtra *extra = (PrincipledHairExtra*)closure_alloc_extra(sd, sizeof(PrincipledHairExtra));

				if (!extra)
					break;

				/* Random factors range: [-randomization/2, +randomization/2]. */
				float random_roughness = stack_load_float_default(stack, random_roughness_ofs, data_node3.w);
				float factor_random_roughness = 1.0f + 2.0f*(random - 0.5f)*random_roughness;
				float roughness = param1 * factor_random_roughness;
				float radial_roughness = param2 * factor_random_roughness;

				/* Remap Coat value to [0, 100]% of Roughness. */
				float coat = stack_load_float_default(stack, coat_ofs, data_node2.y);
				float m0_roughness = 1.0f - clamp(coat, 0.0f, 1.0f);

				bsdf->N = N;
				bsdf->v = roughness;
				bsdf->s = radial_roughness;
				bsdf->m0_roughness = m0_roughness;
				bsdf->alpha = alpha;
				bsdf->eta = ior;
				bsdf->extra = extra;

				switch(parametrization) {
					case NODE_PRINCIPLED_HAIR_DIRECT_ABSORPTION: {
						float3 absorption_coefficient = stack_load_float3(stack, absorption_coefficient_ofs);
						bsdf->sigma = absorption_coefficient;
						break;
					}
					case NODE_PRINCIPLED_HAIR_PIGMENT_CONCENTRATION: {
						float melanin = stack_load_float_default(stack, melanin_ofs, data_node2.z);
						float melanin_redness = stack_load_float_default(stack, melanin_redness_ofs, data_node2.w);

						/* Randomize melanin.  */
						float random_color = stack_load_float_default(stack, random_color_ofs, data_node3.z);
						random_color = clamp(random_color, 0.0f, 1.0f);
						float factor_random_color = 1.0f + 2.0f * (random - 0.5f) * random_color;
						melanin *= factor_random_color;

						/* Map melanin 0..inf from more perceptually linear 0..1. */
						melanin = -logf(fmaxf(1.0f - melanin, 0.0001f));

						/* Benedikt Bitterli's melanin ratio remapping. */
						float eumelanin = melanin * (1.0f - melanin_redness);
						float pheomelanin = melanin * melanin_redness;
						float3 melanin_sigma = sigma_from_concentration(eumelanin, pheomelanin);

						/* Optional tint. */
						float3 tint = stack_load_float3(stack, tint_ofs);
						float3 tint_sigma = sigma_from_reflectance(tint, radial_roughness);

						bsdf->sigma = melanin_sigma + tint_sigma;
						break;
					}
					case NODE_PRINCIPLED_HAIR_REFLECTANCE: {
						float3 color = stack_load_float3(stack, color_ofs);
						bsdf->sigma = sigma_from_reflectance(color, radial_roughness);
						break;
					}
					default: {
						/* Fallback to brownish hair, same as defaults for melanin. */
						kernel_assert(!"Invalid Principled Hair parametrization!");
						bsdf->sigma = sigma_from_concentration(0.0f, 0.8054375f);
						break;
					}
				}

				sd->flag |= bsdf_principled_hair_setup(sd, bsdf);
			}
			break;
		}
		case CLOSURE_BSDF_HAIR_REFLECTION_ID:
		case CLOSURE_BSDF_HAIR_TRANSMISSION_ID: {
			float3 weight = sd->svm_closure_weight * mix_weight;

			if(sd->flag & SD_BACKFACING && sd->type & PRIMITIVE_ALL_CURVE) {
				/* todo: giving a fixed weight here will cause issues when
				 * mixing multiple BSDFS. energy will not be conserved and
				 * the throughput can blow up after multiple bounces. we
				 * better figure out a way to skip backfaces from rays
				 * spawned by transmission from the front */
				bsdf_transparent_setup(sd, make_float3(1.0f, 1.0f, 1.0f), path_flag);
			}
			else {
				HairBsdf *bsdf = (HairBsdf*)bsdf_alloc(sd, sizeof(HairBsdf), weight);

				if(bsdf) {
					bsdf->N = N;
					bsdf->roughness1 = param1;
					bsdf->roughness2 = param2;
					bsdf->offset = -stack_load_float(stack, data_node.z);

					if(stack_valid(data_node.y)) {
						bsdf->T = normalize(stack_load_float3(stack, data_node.y));
					}
					else if(!(sd->type & PRIMITIVE_ALL_CURVE)) {
						bsdf->T = normalize(sd->dPdv);
						bsdf->offset = 0.0f;
					}
					else
						bsdf->T = normalize(sd->dPdu);

					if(type == CLOSURE_BSDF_HAIR_REFLECTION_ID) {
						sd->flag |= bsdf_hair_reflection_setup(bsdf);
					}
					else {
						sd->flag |= bsdf_hair_transmission_setup(bsdf);
					}
				}
			}

			break;
		}
#endif  /* __HAIR__ */

#ifdef __SUBSURFACE__
		case CLOSURE_BSSRDF_CUBIC_ID:
		case CLOSURE_BSSRDF_GAUSSIAN_ID:
		case CLOSURE_BSSRDF_BURLEY_ID:
		case CLOSURE_BSSRDF_RANDOM_WALK_ID: {
			float3 weight = sd->svm_closure_weight * mix_weight;
			Bssrdf *bssrdf = bssrdf_alloc(sd, weight);

			if(bssrdf) {
				/* disable in case of diffuse ancestor, can't see it well then and
				 * adds considerably noise due to probabilities of continuing path
				 * getting lower and lower */
				if(path_flag & PATH_RAY_DIFFUSE_ANCESTOR)
					param1 = 0.0f;

				bssrdf->radius = stack_load_float3(stack, data_node.z)*param1;
				bssrdf->albedo = sd->svm_closure_weight;
				bssrdf->texture_blur = param2;
				bssrdf->sharpness = stack_load_float(stack, data_node.w);
				bssrdf->N = N;
				bssrdf->roughness = 0.0f;
				sd->flag |= bssrdf_setup(sd, bssrdf, (ClosureType)type);
			}

			break;
		}
#endif
		default:
			break;
	}
}

ccl_device void svm_node_closure_volume(KernelGlobals *kg, ShaderData *sd, float *stack, uint4 node, ShaderType shader_type)
{
#ifdef __VOLUME__
	/* Only sum extinction for volumes, variable is shared with surface transparency. */
	if(shader_type != SHADER_TYPE_VOLUME) {
		return;
	}

	uint type, density_offset, anisotropy_offset;

	uint mix_weight_offset;
	decode_node_uchar4(node.y, &type, &density_offset, &anisotropy_offset, &mix_weight_offset);
	float mix_weight = (stack_valid(mix_weight_offset)? stack_load_float(stack, mix_weight_offset): 1.0f);

	if(mix_weight == 0.0f) {
		return;
	}

	float density = (stack_valid(density_offset))? stack_load_float(stack, density_offset): __uint_as_float(node.z);
	density = mix_weight * fmaxf(density, 0.0f);

	/* Compute scattering coefficient. */
	float3 weight = sd->svm_closure_weight;

	if(type == CLOSURE_VOLUME_ABSORPTION_ID) {
		weight = make_float3(1.0f, 1.0f, 1.0f) - weight;
	}

	weight *= density;

	/* Add closure for volume scattering. */
	if(type == CLOSURE_VOLUME_HENYEY_GREENSTEIN_ID) {
		HenyeyGreensteinVolume *volume = (HenyeyGreensteinVolume*)bsdf_alloc(sd, sizeof(HenyeyGreensteinVolume), weight);

		if(volume) {
			float anisotropy = (stack_valid(anisotropy_offset))? stack_load_float(stack, anisotropy_offset): __uint_as_float(node.w);
			volume->g = anisotropy; /* g */
			sd->flag |= volume_henyey_greenstein_setup(volume);
		}
	}

	/* Sum total extinction weight. */
	volume_extinction_setup(sd, weight);
#endif
}

ccl_device void svm_node_principled_volume(KernelGlobals *kg, ShaderData *sd, float *stack, uint4 node, ShaderType shader_type, int path_flag, int *offset)
{
#ifdef __VOLUME__
	uint4 value_node = read_node(kg, offset);
	uint4 attr_node = read_node(kg, offset);

	/* Only sum extinction for volumes, variable is shared with surface transparency. */
	if(shader_type != SHADER_TYPE_VOLUME) {
		return;
	}

	uint density_offset, anisotropy_offset, absorption_color_offset, mix_weight_offset;
	decode_node_uchar4(node.y, &density_offset, &anisotropy_offset, &absorption_color_offset, &mix_weight_offset);
	float mix_weight = (stack_valid(mix_weight_offset)? stack_load_float(stack, mix_weight_offset): 1.0f);

	if(mix_weight == 0.0f) {
		return;
	}

	/* Compute density. */
	float primitive_density = 1.0f;
	float density = (stack_valid(density_offset))? stack_load_float(stack, density_offset): __uint_as_float(value_node.x);
	density = mix_weight * fmaxf(density, 0.0f);

	if(density > CLOSURE_WEIGHT_CUTOFF) {
		/* Density and color attribute lookup if available. */
		const AttributeDescriptor attr_density = find_attribute(kg, sd, attr_node.x);
		if(attr_density.offset != ATTR_STD_NOT_FOUND) {
			primitive_density = primitive_attribute_float(kg, sd, attr_density, NULL, NULL);
			density = fmaxf(density * primitive_density, 0.0f);
		}
	}

	if(density > CLOSURE_WEIGHT_CUTOFF) {
		/* Compute scattering color. */
		float3 color = sd->svm_closure_weight;

		const AttributeDescriptor attr_color = find_attribute(kg, sd, attr_node.y);
		if(attr_color.offset != ATTR_STD_NOT_FOUND) {
			color *= primitive_attribute_float3(kg, sd, attr_color, NULL, NULL);
		}

		/* Add closure for volume scattering. */
		HenyeyGreensteinVolume *volume = (HenyeyGreensteinVolume*)bsdf_alloc(sd, sizeof(HenyeyGreensteinVolume), color * density);
		if(volume) {
			float anisotropy = (stack_valid(anisotropy_offset))? stack_load_float(stack, anisotropy_offset): __uint_as_float(value_node.y);
			volume->g = anisotropy;
			sd->flag |= volume_henyey_greenstein_setup(volume);
		}

		/* Add extinction weight. */
		float3 zero = make_float3(0.0f, 0.0f, 0.0f);
		float3 one = make_float3(1.0f, 1.0f, 1.0f);
		float3 absorption_color = max(sqrt(stack_load_float3(stack, absorption_color_offset)), zero);
		float3 absorption = max(one - color, zero) * max(one - absorption_color, zero);
		volume_extinction_setup(sd, (color + absorption) * density);
	}

	/* Compute emission. */
	if(path_flag & PATH_RAY_SHADOW) {
		/* Don't need emission for shadows. */
		return;
	}

	uint emission_offset, emission_color_offset, blackbody_offset, temperature_offset;
	decode_node_uchar4(node.z, &emission_offset, &emission_color_offset, &blackbody_offset, &temperature_offset);
	float emission = (stack_valid(emission_offset))? stack_load_float(stack, emission_offset): __uint_as_float(value_node.z);
	float blackbody = (stack_valid(blackbody_offset))? stack_load_float(stack, blackbody_offset): __uint_as_float(value_node.w);

	if(emission > CLOSURE_WEIGHT_CUTOFF) {
		float3 emission_color = stack_load_float3(stack, emission_color_offset);
		emission_setup(sd, emission * emission_color);
	}

	if(blackbody > CLOSURE_WEIGHT_CUTOFF) {
		float T = stack_load_float(stack, temperature_offset);

		/* Add flame temperature from attribute if available. */
		const AttributeDescriptor attr_temperature = find_attribute(kg, sd, attr_node.z);
		if(attr_temperature.offset != ATTR_STD_NOT_FOUND) {
			float temperature = primitive_attribute_float(kg, sd, attr_temperature, NULL, NULL);
			T *= fmaxf(temperature, 0.0f);
		}

		T = fmaxf(T, 0.0f);

		/* Stefan-Boltzmann law. */
		float T4 = sqr(sqr(T));
		float sigma = 5.670373e-8f * 1e-6f / M_PI_F;
		float intensity = sigma * mix(1.0f, T4, blackbody);

		if(intensity > CLOSURE_WEIGHT_CUTOFF) {
			float3 blackbody_tint = stack_load_float3(stack, node.w);
			float3 bb = blackbody_tint * intensity * svm_math_blackbody_color(T);
			emission_setup(sd, bb);
		}
	}
#endif
}

ccl_device void svm_node_closure_emission(ShaderData *sd, float *stack, uint4 node)
{
	uint mix_weight_offset = node.y;
	float3 weight = sd->svm_closure_weight;

	if(stack_valid(mix_weight_offset)) {
		float mix_weight = stack_load_float(stack, mix_weight_offset);

		if(mix_weight == 0.0f)
			return;

		weight *= mix_weight;
	}

	emission_setup(sd, weight);
}

ccl_device void svm_node_closure_background(ShaderData *sd, float *stack, uint4 node)
{
	uint mix_weight_offset = node.y;
	float3 weight = sd->svm_closure_weight;

	if(stack_valid(mix_weight_offset)) {
		float mix_weight = stack_load_float(stack, mix_weight_offset);

		if(mix_weight == 0.0f)
			return;

		weight *= mix_weight;
	}

	background_setup(sd, weight);
}

ccl_device void svm_node_closure_holdout(ShaderData *sd, float *stack, uint4 node)
{
	uint mix_weight_offset = node.y;

	if(stack_valid(mix_weight_offset)) {
		float mix_weight = stack_load_float(stack, mix_weight_offset);

		if(mix_weight == 0.0f)
			return;

		closure_alloc(sd, sizeof(ShaderClosure), CLOSURE_HOLDOUT_ID, sd->svm_closure_weight * mix_weight);
	}
	else
		closure_alloc(sd, sizeof(ShaderClosure), CLOSURE_HOLDOUT_ID, sd->svm_closure_weight);

	sd->flag |= SD_HOLDOUT;
}

/* Closure Nodes */

ccl_device_inline void svm_node_closure_store_weight(ShaderData *sd, float3 weight)
{
	sd->svm_closure_weight = weight;
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
	sd->N = normal;
	stack_store_float3(stack, out_normal, normal);
}

CCL_NAMESPACE_END
