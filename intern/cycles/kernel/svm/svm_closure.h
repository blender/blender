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

CCL_NAMESPACE_BEGIN

/* Closure Nodes */

__device void svm_node_closure_bsdf(ShaderData *sd, float *stack, uint4 node, float randb)
{
	uint type, param1_offset, param2_offset;
	decode_node_uchar4(node.y, &type, &param1_offset, &param2_offset, NULL);

	float param1 = (stack_valid(param1_offset))? stack_load_float(stack, param1_offset): __int_as_float(node.z);
	float param2 = (stack_valid(param2_offset))? stack_load_float(stack, param2_offset): __int_as_float(node.w);

	switch(type) {
		case CLOSURE_BSDF_DIFFUSE_ID:
			bsdf_diffuse_setup(sd, sd->N);
			break;
		case CLOSURE_BSDF_TRANSLUCENT_ID:
			bsdf_translucent_setup(sd, sd->N);
			break;
		case CLOSURE_BSDF_TRANSPARENT_ID:
			bsdf_transparent_setup(sd);
			break;
		case CLOSURE_BSDF_REFLECTION_ID:
		case CLOSURE_BSDF_MICROFACET_GGX_ID:
		case CLOSURE_BSDF_MICROFACET_BECKMANN_ID: {
			/* roughness */
			/* index of refraction */
			float eta = clamp(1.0f-param2, 1e-5f, 1.0f - 1e-5f);
			eta = 1.0f/eta;

			/* fresnel */
			float cosNO = dot(sd->N, sd->I);
			float fresnel = fresnel_dielectric_cos(cosNO, eta);

			sd->svm_closure_weight *= fresnel;

			/* setup bsdf */
			if(type == CLOSURE_BSDF_REFLECTION_ID) {
				bsdf_reflection_setup(sd, sd->N);
			}
			else if(type == CLOSURE_BSDF_MICROFACET_BECKMANN_ID) {
				float roughness = param1;
				bsdf_microfacet_beckmann_setup(sd, sd->N, roughness, eta, false);
			}
			else {
				float roughness = param1;
				bsdf_microfacet_ggx_setup(sd, sd->N, roughness, eta, false);
			}
			break;
		}
		case CLOSURE_BSDF_REFRACTION_ID:
		case CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID:
		case CLOSURE_BSDF_MICROFACET_BECKMANN_REFRACTION_ID: {
			/* index of refraction */
			float eta = clamp(1.0f-param2, 1e-5f, 1.0f - 1e-5f);
			eta = (sd->flag & SD_BACKFACING)? eta: 1.0f/eta;

			/* fresnel */
			float cosNO = dot(sd->N, sd->I);
			float fresnel = fresnel_dielectric_cos(cosNO, eta);
			bool refract = (fresnel < randb);

			/* setup bsdf */
			if(type == CLOSURE_BSDF_REFRACTION_ID) {
				if(refract)
					bsdf_refraction_setup(sd, sd->N, eta);
				else
					bsdf_reflection_setup(sd, sd->N);
			}
			else if(type == CLOSURE_BSDF_MICROFACET_BECKMANN_REFRACTION_ID) {
				float roughness = param1;
				bsdf_microfacet_beckmann_setup(sd, sd->N, roughness, eta, refract);
			}
			else {
				float roughness = param1;
				bsdf_microfacet_ggx_setup(sd, sd->N, roughness, eta, refract);
			}
			break;
		}
#ifdef __DPDU__
		case CLOSURE_BSDF_WARD_ID: {
			float roughness_u = param1;
			float roughness_v = param2;
			bsdf_ward_setup(sd, sd->N, normalize(sd->dPdu), roughness_u, roughness_v);
			break;
		}
#endif
		case CLOSURE_BSDF_ASHIKHMIN_VELVET_ID: {
			/* sigma */
			float sigma = clamp(param1, 0.0f, 1.0f);

			/* index of refraction */
			float eta = clamp(1.0f-param2, 1e-5f, 1.0f - 1e-5f);
			eta = 1.0f/eta;

			/* fresnel */
			float cosNO = dot(sd->N, sd->I);
			float fresnel = fresnel_dielectric_cos(cosNO, eta);

			sd->svm_closure_weight *= fresnel;

			bsdf_ashikhmin_velvet_setup(sd, sd->N, sigma);
			break;
		}
		default:
			return;
	}
}

__device void svm_node_closure_emission(ShaderData *sd)
{
	sd->svm_closure = CLOSURE_EMISSION_ID;
	sd->flag |= SD_EMISSION;
}

__device void svm_node_closure_background(ShaderData *sd)
{
	sd->svm_closure = CLOSURE_BACKGROUND_ID;
}

__device void svm_node_closure_holdout(ShaderData *sd)
{
	sd->svm_closure = CLOSURE_HOLDOUT_ID;
	sd->flag |= SD_HOLDOUT;
}

/* Closure Nodes */

__device void svm_node_closure_set_weight(ShaderData *sd, uint r, uint g, uint b)
{
	sd->svm_closure_weight.x = __int_as_float(r);
	sd->svm_closure_weight.y = __int_as_float(g);
	sd->svm_closure_weight.z = __int_as_float(b);
}

__device void svm_node_emission_set_weight_total(KernelGlobals *kg, ShaderData *sd, uint r, uint g, uint b)
{
	sd->svm_closure_weight.x = __int_as_float(r);
	sd->svm_closure_weight.y = __int_as_float(g);
	sd->svm_closure_weight.z = __int_as_float(b);

	if(sd->object != ~0)
		sd->svm_closure_weight /= object_surface_area(kg, sd->object);
}

__device void svm_node_closure_weight(ShaderData *sd, float *stack, uint weight_offset)
{
	sd->svm_closure_weight = stack_load_float3(stack, weight_offset);
}

__device void svm_node_emission_weight(KernelGlobals *kg, ShaderData *sd, float *stack, uint4 node)
{
	uint color_offset = node.y;
	uint strength_offset = node.z;
	uint total_power = node.w;

	sd->svm_closure_weight = stack_load_float3(stack, color_offset)*stack_load_float(stack, strength_offset);

	if(total_power && sd->object != ~0)
		sd->svm_closure_weight /= object_surface_area(kg, sd->object);
}

__device void svm_node_mix_closure(ShaderData *sd, float *stack,
	uint weight_offset, uint node_jump, int *offset, float *randb)
{
	float weight = stack_load_float(stack, weight_offset);
	weight = clamp(weight, 0.0f, 1.0f);

	/* pick a closure and make the random number uniform over 0..1 again.
	   closure 1 starts on the next node, for closure 2 the start is at an
	   offset from the current node, so we jump */
	if(*randb < weight) {
		*offset += node_jump;
		*randb = *randb/weight;
	}
	else
		*randb = (*randb - weight)/(1.0f - weight);
}

__device void svm_node_add_closure(ShaderData *sd, float *stack, uint unused,
	uint node_jump, int *offset, float *randb, float *closure_weight)
{
	float weight = 0.5f;

	/* pick one of the two closures with probability 0.5. sampling quality
	   is not going to be great, for that we'd need to evaluate the weights
	   of the two closures being added */
	if(*randb < weight) {
		*offset += node_jump;
		*randb = *randb/weight;
	}
	else
		*randb = (*randb - weight)/(1.0f - weight);
	
	*closure_weight *= 2.0f;
}

CCL_NAMESPACE_END

