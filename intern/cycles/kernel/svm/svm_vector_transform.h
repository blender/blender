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

/* Vector Transform */

ccl_device void svm_node_vector_transform(KernelGlobals *kg, ShaderData *sd, float *stack, uint4 node)
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
	bool is_object = (sd->object != OBJECT_NONE);
	bool is_direction = (type == NODE_VECTOR_TRANSFORM_TYPE_VECTOR || type == NODE_VECTOR_TRANSFORM_TYPE_NORMAL);

	/* From world */
	if(from == NODE_VECTOR_TRANSFORM_CONVERT_SPACE_WORLD) {
		if(to == NODE_VECTOR_TRANSFORM_CONVERT_SPACE_CAMERA) {
			tfm = kernel_data.cam.worldtocamera;
			if(is_direction)
				in = transform_direction(&tfm, in);
			else
				in = transform_point(&tfm, in);
		}
		else if(to == NODE_VECTOR_TRANSFORM_CONVERT_SPACE_OBJECT && is_object) {
			if(is_direction)
				object_inverse_dir_transform(kg, sd, &in);
			else
				object_inverse_position_transform(kg, sd, &in);
		}
	}

	/* From camera */
	else if(from == NODE_VECTOR_TRANSFORM_CONVERT_SPACE_CAMERA) {
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
