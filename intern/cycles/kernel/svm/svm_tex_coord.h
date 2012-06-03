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

/* Texture Coordinate Node */

__device_inline float3 svm_background_offset(KernelGlobals *kg)
{
	Transform cameratoworld = kernel_data.cam.cameratoworld;
	return make_float3(cameratoworld.x.w, cameratoworld.y.w, cameratoworld.z.w);
}

__device_inline float3 svm_world_to_ndc(KernelGlobals *kg, ShaderData *sd, float3 P)
{
	if(kernel_data.cam.type != CAMERA_PANORAMA) {
		if(sd->object != ~0)
			P += svm_background_offset(kg);

		Transform tfm = kernel_data.cam.worldtondc;
		return transform_perspective(&tfm, P);
	}
	else {
		Transform tfm = kernel_data.cam.worldtocamera;

		if(sd->object != ~0)
			P = normalize(transform_point(&tfm, P));
		else
			P = normalize(transform_direction(&tfm, P));

		float2 uv = direction_to_panorama(kg, P);

		return make_float3(uv.x, uv.y, 0.0f);
	}
}

__device void svm_node_tex_coord(KernelGlobals *kg, ShaderData *sd, float *stack, uint type, uint out_offset)
{
	float3 data;

	switch(type) {
		case NODE_TEXCO_OBJECT: {
			if(sd->object != ~0) {
				data = sd->P;
				object_inverse_position_transform(kg, sd, &data);
			}
			else
				data = sd->P;
			break;
		}
		case NODE_TEXCO_NORMAL: {
			if(sd->object != ~0) {
				data = sd->N;
				object_inverse_normal_transform(kg, sd, &data);
			}
			else
				data = sd->N;
			break;
		}
		case NODE_TEXCO_CAMERA: {
			Transform tfm = kernel_data.cam.worldtocamera;

			if(sd->object != ~0)
				data = transform_point(&tfm, sd->P);
			else
				data = transform_point(&tfm, sd->P + svm_background_offset(kg));
			break;
		}
		case NODE_TEXCO_WINDOW: {
			data = svm_world_to_ndc(kg, sd, sd->P);
			break;
		}
		case NODE_TEXCO_REFLECTION: {
			if(sd->object != ~0)
				data = 2.0f*dot(sd->N, sd->I)*sd->N - sd->I;
			else
				data = sd->I;
			break;
		}
	}

	stack_store_float3(stack, out_offset, data);
}

__device void svm_node_tex_coord_bump_dx(KernelGlobals *kg, ShaderData *sd, float *stack, uint type, uint out_offset)
{
#ifdef __RAY_DIFFERENTIALS__
	float3 data;

	switch(type) {
		case NODE_TEXCO_OBJECT: {
			if(sd->object != ~0) {
				data = sd->P + sd->dP.dx;
				object_inverse_position_transform(kg, sd, &data);
			}
			else
				data = sd->P + sd->dP.dx;
			break;
		}
		case NODE_TEXCO_NORMAL: {
			if(sd->object != ~0) {
				data = sd->N;
				object_inverse_normal_transform(kg, sd, &data);
			}
			else
				data = sd->N;
			break;
		}
		case NODE_TEXCO_CAMERA: {
			Transform tfm = kernel_data.cam.worldtocamera;

			if(sd->object != ~0)
				data = transform_point(&tfm, sd->P + sd->dP.dx);
			else
				data = transform_point(&tfm, sd->P + sd->dP.dx + svm_background_offset(kg));
			break;
		}
		case NODE_TEXCO_WINDOW: {
			data = svm_world_to_ndc(kg, sd, sd->P + sd->dP.dx);
			break;
		}
		case NODE_TEXCO_REFLECTION: {
			if(sd->object != ~0)
				data = 2.0f*dot(sd->N, sd->I)*sd->N - sd->I;
			else
				data = sd->I;
			break;
		}
	}

	stack_store_float3(stack, out_offset, data);
#else
	svm_node_tex_coord(kg, sd, stack, type, out_offset);
#endif
}

__device void svm_node_tex_coord_bump_dy(KernelGlobals *kg, ShaderData *sd, float *stack, uint type, uint out_offset)
{
#ifdef __RAY_DIFFERENTIALS__
	float3 data;

	switch(type) {
		case NODE_TEXCO_OBJECT: {
			if(sd->object != ~0) {
				data = sd->P + sd->dP.dy;
				object_inverse_position_transform(kg, sd, &data);
			}
			else
				data = sd->P + sd->dP.dy;
			break;
		}
		case NODE_TEXCO_NORMAL: {
			if(sd->object != ~0) {
				data = sd->N;
				object_inverse_normal_transform(kg, sd, &data);
			}
			else
				data = sd->N;
			break;
		}
		case NODE_TEXCO_CAMERA: {
			Transform tfm = kernel_data.cam.worldtocamera;

			if(sd->object != ~0)
				data = transform_point(&tfm, sd->P + sd->dP.dy);
			else
				data = transform_point(&tfm, sd->P + sd->dP.dy + svm_background_offset(kg));
			break;
		}
		case NODE_TEXCO_WINDOW: {
			data = svm_world_to_ndc(kg, sd, sd->P + sd->dP.dy);
			break;
		}
		case NODE_TEXCO_REFLECTION: {
			if(sd->object != ~0)
				data = 2.0f*dot(sd->N, sd->I)*sd->N - sd->I;
			else
				data = sd->I;
			break;
		}
	}

	stack_store_float3(stack, out_offset, data);
#else
	svm_node_tex_coord(kg, sd, stack, type, out_offset);
#endif
}

CCL_NAMESPACE_END

