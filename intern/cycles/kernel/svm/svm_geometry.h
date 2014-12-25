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

/* Geometry Node */

ccl_device void svm_node_geometry(KernelGlobals *kg, ShaderData *sd, float *stack, uint type, uint out_offset)
{
	float3 data;

	switch(type) {
		case NODE_GEOM_P: data = sd->P; break;
		case NODE_GEOM_N: data = sd->N; break;
#ifdef __DPDU__
		case NODE_GEOM_T: data = primitive_tangent(kg, sd); break;
#endif
		case NODE_GEOM_I: data = sd->I; break;
		case NODE_GEOM_Ng: data = sd->Ng; break;
#ifdef __UV__
		case NODE_GEOM_uv: data = make_float3(sd->u, sd->v, 0.0f); break;
#endif
	}

	stack_store_float3(stack, out_offset, data);
}

ccl_device void svm_node_geometry_bump_dx(KernelGlobals *kg, ShaderData *sd, float *stack, uint type, uint out_offset)
{
#ifdef __RAY_DIFFERENTIALS__
	float3 data;

	switch(type) {
		case NODE_GEOM_P: data = sd->P + sd->dP.dx; break;
		case NODE_GEOM_uv: data = make_float3(sd->u + sd->du.dx, sd->v + sd->dv.dx, 0.0f); break;
		default: svm_node_geometry(kg, sd, stack, type, out_offset); return;
	}

	stack_store_float3(stack, out_offset, data);
#else
	svm_node_geometry(kg, sd, stack, type, out_offset);
#endif
}

ccl_device void svm_node_geometry_bump_dy(KernelGlobals *kg, ShaderData *sd, float *stack, uint type, uint out_offset)
{
#ifdef __RAY_DIFFERENTIALS__
	float3 data;

	switch(type) {
		case NODE_GEOM_P: data = sd->P + sd->dP.dy; break;
		case NODE_GEOM_uv: data = make_float3(sd->u + sd->du.dy, sd->v + sd->dv.dy, 0.0f); break;
		default: svm_node_geometry(kg, sd, stack, type, out_offset); return;
	}

	stack_store_float3(stack, out_offset, data);
#else
	svm_node_geometry(kg, sd, stack, type, out_offset);
#endif
}

/* Object Info */

ccl_device void svm_node_object_info(KernelGlobals *kg, ShaderData *sd, float *stack, uint type, uint out_offset)
{
	float data;

	switch(type) {
		case NODE_INFO_OB_LOCATION: {
			stack_store_float3(stack, out_offset, object_location(kg, sd));
			return;
		}
		case NODE_INFO_OB_INDEX: data = object_pass_id(kg, sd->object); break;
		case NODE_INFO_MAT_INDEX: data = shader_pass_id(kg, sd); break;
		case NODE_INFO_OB_RANDOM: data = object_random_number(kg, sd->object); break;
		default: data = 0.0f; break;
	}

	stack_store_float(stack, out_offset, data);
}

/* Particle Info */

ccl_device void svm_node_particle_info(KernelGlobals *kg, ShaderData *sd, float *stack, uint type, uint out_offset)
{
	switch(type) {
		case NODE_INFO_PAR_INDEX: {
			int particle_id = object_particle_id(kg, sd->object);
			stack_store_float(stack, out_offset, particle_index(kg, particle_id));
			break;
		}
		case NODE_INFO_PAR_AGE: {
			int particle_id = object_particle_id(kg, sd->object);
			stack_store_float(stack, out_offset, particle_age(kg, particle_id));
			break;
		}
		case NODE_INFO_PAR_LIFETIME: {
			int particle_id = object_particle_id(kg, sd->object);
			stack_store_float(stack, out_offset, particle_lifetime(kg, particle_id));
			break;
		}
		case NODE_INFO_PAR_LOCATION: {
			int particle_id = object_particle_id(kg, sd->object);
			stack_store_float3(stack, out_offset, particle_location(kg, particle_id));
			break;
		}
#if 0	/* XXX float4 currently not supported in SVM stack */
		case NODE_INFO_PAR_ROTATION: {
			int particle_id = object_particle_id(kg, sd->object);
			stack_store_float4(stack, out_offset, particle_rotation(kg, particle_id));
			break;
		}
#endif
		case NODE_INFO_PAR_SIZE: {
			int particle_id = object_particle_id(kg, sd->object);
			stack_store_float(stack, out_offset, particle_size(kg, particle_id));
			break;
		}
		case NODE_INFO_PAR_VELOCITY: {
			int particle_id = object_particle_id(kg, sd->object);
			stack_store_float3(stack, out_offset, particle_velocity(kg, particle_id));
			break;
		}
		case NODE_INFO_PAR_ANGULAR_VELOCITY: {
			int particle_id = object_particle_id(kg, sd->object);
			stack_store_float3(stack, out_offset, particle_angular_velocity(kg, particle_id));
			break;
		}
	}
}

#ifdef __HAIR__

/* Hair Info */

ccl_device void svm_node_hair_info(KernelGlobals *kg, ShaderData *sd, float *stack, uint type, uint out_offset)
{
	float data;
	float3 data3;

	switch(type) {
		case NODE_INFO_CURVE_IS_STRAND: {
			data = (sd->type & PRIMITIVE_ALL_CURVE) != 0;
			stack_store_float(stack, out_offset, data);
			break;
		}
		case NODE_INFO_CURVE_INTERCEPT:
			break; /* handled as attribute */
		case NODE_INFO_CURVE_THICKNESS: {
			data = curve_thickness(kg, sd);
			stack_store_float(stack, out_offset, data);
			break;
		}
		/*case NODE_INFO_CURVE_FADE: {
			data = sd->curve_transparency;
			stack_store_float(stack, out_offset, data);
			break;
		}*/
		case NODE_INFO_CURVE_TANGENT_NORMAL: {
			data3 = curve_tangent_normal(kg, sd);
			stack_store_float3(stack, out_offset, data3);
			break;
		}
	}
}
#endif

CCL_NAMESPACE_END

