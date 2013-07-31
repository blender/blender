/*
 * Copyright 2013, Blender Foundation.
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

/* Vector Transform */

__device void svm_node_vector_transform(KernelGlobals *kg, ShaderData *sd, float *stack, uint4 node)
{
	uint itype, ifrom, ito;
	uint vector_in, vector_out;
	
	decode_node_uchar4(node.y, &itype, &ifrom, &ito, NULL);
	decode_node_uchar4(node.z, &vector_in, &vector_out, NULL, NULL);
	
	float3 in = stack_load_float3(stack, vector_in);
	
	NodeVectorTransformType type = (NodeVectorTransformType)itype;
	NodeVectorTransformConvertSpace from = (NodeVectorTransformConvertSpace)ifrom;
	NodeVectorTransformConvertSpace to = (NodeVectorTransformConvertSpace)ito;
	
	Transform tfm;
	int is_object = (sd->object != ~0);
	int is_direction = (type == NODE_VECTOR_TRANSFORM_TYPE_VECTOR || type == NODE_VECTOR_TRANSFORM_TYPE_NORMAL);
	
	/* From world */
	if(from == NODE_VECTOR_TRANSFORM_CONVERT_SPACE_WORLD) {
		if(to == NODE_VECTOR_TRANSFORM_CONVERT_SPACE_CAMERA) {
			tfm = kernel_data.cam.worldtocamera;
			if(is_direction)
				in = transform_direction(&tfm, in);
			else
				in = transform_point(&tfm, in);
		}
		else if (to == NODE_VECTOR_TRANSFORM_CONVERT_SPACE_OBJECT && is_object) {
			if(is_direction)
				object_inverse_dir_transform(kg, sd, &in);
			else
				object_inverse_position_transform(kg, sd, &in);
		}
	}
	
	/* From camera */
	else if (from == NODE_VECTOR_TRANSFORM_CONVERT_SPACE_CAMERA) {
		if(to == NODE_VECTOR_TRANSFORM_CONVERT_SPACE_WORLD || to == NODE_VECTOR_TRANSFORM_CONVERT_SPACE_OBJECT) {
			tfm = kernel_data.cam.cameratoworld;
			if(is_direction)
				in = transform_direction(&tfm, in);
			else
				in = transform_point(&tfm, in);
		}
		if(to == NODE_VECTOR_TRANSFORM_CONVERT_SPACE_OBJECT && is_object) {
			if(is_direction)
				object_inverse_dir_transform(kg, sd, &in);
			else
				object_inverse_position_transform(kg, sd, &in);
		}
	}
	
	/* From object */
	else if(from == NODE_VECTOR_TRANSFORM_CONVERT_SPACE_OBJECT) {
		if((to == NODE_VECTOR_TRANSFORM_CONVERT_SPACE_WORLD || to == NODE_VECTOR_TRANSFORM_CONVERT_SPACE_CAMERA) && is_object) {
			if(is_direction)
				object_dir_transform(kg, sd, &in);
			else
				object_position_transform(kg, sd, &in);
		}
		if(to == NODE_VECTOR_TRANSFORM_CONVERT_SPACE_CAMERA) {
			tfm = kernel_data.cam.worldtocamera;
			if(is_direction)
				in = transform_direction(&tfm, in);
			else
				in = transform_point(&tfm, in);
		}
	}
	
	/* Normalize Normal */
	if(type == NODE_VECTOR_TRANSFORM_TYPE_NORMAL)
		in = normalize(in);
	
	/* Output */	
	if(stack_valid(vector_out)) {
			stack_store_float3(stack, vector_out, in);
	}
}

CCL_NAMESPACE_END

