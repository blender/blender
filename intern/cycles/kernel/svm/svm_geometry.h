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

/* Geometry Node */

__device void svm_node_geometry(ShaderData *sd, float *stack, uint type, uint out_offset)
{
	float3 data;

	switch(type) {
		case NODE_GEOM_P: data = sd->P; break;
		case NODE_GEOM_N: data = sd->N; break;
#ifdef __DPDU__
		case NODE_GEOM_T: data = normalize(sd->dPdu); break;
#endif
		case NODE_GEOM_I: data = sd->I; break;
		case NODE_GEOM_Ng: data = sd->Ng; break;
#ifdef __UV__
		case NODE_GEOM_uv: data = make_float3(sd->u, sd->v, 0.0f); break;
#endif
	}

	stack_store_float3(stack, out_offset, data);
}

__device void svm_node_geometry_bump_dx(ShaderData *sd, float *stack, uint type, uint out_offset)
{
#ifdef __RAY_DIFFERENTIALS__
	float3 data;

	switch(type) {
		case NODE_GEOM_P: data = sd->P + sd->dP.dx; break;
		case NODE_GEOM_uv: data = make_float3(sd->u + sd->du.dx, sd->v + sd->dv.dx, 0.0f); break;
		default: svm_node_geometry(sd, stack, type, out_offset); return;
	}

	stack_store_float3(stack, out_offset, data);
#else
	svm_node_geometry(sd, stack, type, out_offset);
#endif
}

__device void svm_node_geometry_bump_dy(ShaderData *sd, float *stack, uint type, uint out_offset)
{
#ifdef __RAY_DIFFERENTIALS__
	float3 data;

	switch(type) {
		case NODE_GEOM_P: data = sd->P + sd->dP.dy; break;
		case NODE_GEOM_uv: data = make_float3(sd->u + sd->du.dy, sd->v + sd->dv.dy, 0.0f); break;
		default: svm_node_geometry(sd, stack, type, out_offset); return;
	}

	stack_store_float3(stack, out_offset, data);
#else
	svm_node_geometry(sd, stack, type, out_offset);
#endif
}

/* Object Info */

__device void svm_node_object_info(KernelGlobals *kg, ShaderData *sd, float *stack, uint type, uint out_offset)
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

__device void svm_node_particle_info(KernelGlobals *kg, ShaderData *sd, float *stack, uint type, uint out_offset)
{
	float data;

	switch(type) {
		case NODE_INFO_PAR_AGE: {
			uint particle_id = object_particle_id(kg, sd->object);
			data = particle_age(kg, particle_id);
			stack_store_float(stack, out_offset, data);
			break;
		}
		case NODE_INFO_PAR_LIFETIME: {
			uint particle_id = object_particle_id(kg, sd->object);
			data = particle_lifetime(kg, particle_id);
			stack_store_float(stack, out_offset, data);
			break;
		}
	}
}

CCL_NAMESPACE_END

