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

__device float3 svm_background_offset(KernelGlobals *kg)
{
	Transform cameratoworld = kernel_data.cam.cameratoworld;
	return make_float3(cameratoworld.x.w, cameratoworld.y.w, cameratoworld.z.w);
}

__device void svm_node_tex_coord(KernelGlobals *kg, ShaderData *sd, float *stack, uint type, uint out_offset)
{
	float3 data;

	switch(type) {
		case NODE_TEXCO_OBJECT: {
			if(sd->object != ~0) {
				Transform tfm = object_fetch_transform(kg, sd->object, OBJECT_INVERSE_TRANSFORM);
				data = transform_point(&tfm, sd->P);
			}
			else
				data = sd->P;
			break;
		}
		case NODE_TEXCO_NORMAL: {
			if(sd->object != ~0) {
				Transform tfm = object_fetch_transform(kg, sd->object, OBJECT_INVERSE_TRANSFORM);
				data = transform_direction(&tfm, sd->N);
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
			Transform tfm = kernel_data.cam.worldtondc;

			if(sd->object != ~0)
				data = transform_perspective(&tfm, sd->P);
			else
				data = transform_perspective(&tfm, sd->P + svm_background_offset(kg));
			break;
		}
		case NODE_TEXCO_REFLECTION: {
			if(sd->object != ~0)
				data = sd->I - 2.0f*dot(sd->N, sd->I)*sd->N;
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
				Transform tfm = object_fetch_transform(kg, sd->object, OBJECT_INVERSE_TRANSFORM);
				data = transform_point(&tfm, sd->P + sd->dP.dx);
			}
			else
				data = sd->P + sd->dP.dx;
			break;
		}
		case NODE_TEXCO_NORMAL: {
			if(sd->object != ~0) {
				Transform tfm = object_fetch_transform(kg, sd->object, OBJECT_INVERSE_TRANSFORM);
				data = transform_direction(&tfm, sd->N);
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
			Transform tfm = kernel_data.cam.worldtondc;

			if(sd->object != ~0)
				data = transform_perspective(&tfm, sd->P + sd->dP.dx);
			else
				data = transform_perspective(&tfm, sd->P + sd->dP.dx + svm_background_offset(kg));
			break;
		}
		case NODE_TEXCO_REFLECTION: {
			if(sd->object != ~0)
				data = sd->I - 2.0f*dot(sd->N, sd->I)*sd->N;
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
				Transform tfm = object_fetch_transform(kg, sd->object, OBJECT_INVERSE_TRANSFORM);
				data = transform_point(&tfm, sd->P + sd->dP.dy);
			}
			else
				data = sd->P + sd->dP.dy;
			break;
		}
		case NODE_TEXCO_NORMAL: {
			if(sd->object != ~0) {
				Transform tfm = object_fetch_transform(kg, sd->object, OBJECT_INVERSE_TRANSFORM);
				data = normalize(transform_direction(&tfm, sd->N));
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
			Transform tfm = kernel_data.cam.worldtondc;

			if(sd->object != ~0)
				data = transform_perspective(&tfm, sd->P + sd->dP.dy);
			else
				data = transform_perspective(&tfm, sd->P + sd->dP.dy + svm_background_offset(kg));
			break;
		}
		case NODE_TEXCO_REFLECTION: {
			if(sd->object != ~0)
				data = sd->I - 2.0f*dot(sd->N, sd->I)*sd->N;
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

