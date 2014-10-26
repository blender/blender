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
 * limitations under the License
 */

CCL_NAMESPACE_BEGIN

ccl_device float average_fac(float3 v)
{
	return (fabsf(v.x) + fabsf(v.y) + fabsf(v.z))/3.0f;
}

ccl_device void svm_vector_math(float *Fac, float3 *Vector, NodeVectorMath type, float3 Vector1, float3 Vector2)
{
	if(type == NODE_VECTOR_MATH_ADD) {
		*Vector = Vector1 + Vector2;
		*Fac = average_fac(*Vector);
	}
	else if(type == NODE_VECTOR_MATH_SUBTRACT) {
		*Vector = Vector1 - Vector2;
		*Fac = average_fac(*Vector);
	}
	else if(type == NODE_VECTOR_MATH_AVERAGE) {
		*Fac = len(Vector1 + Vector2);
		*Vector = normalize(Vector1 + Vector2);
	}
	else if(type == NODE_VECTOR_MATH_DOT_PRODUCT) {
		*Fac = dot(Vector1, Vector2);
		*Vector = make_float3(0.0f, 0.0f, 0.0f);
	}
	else if(type == NODE_VECTOR_MATH_CROSS_PRODUCT) {
		float3 c = cross(Vector1, Vector2);
		*Fac = len(c);
		*Vector = normalize(c);
	}
	else if(type == NODE_VECTOR_MATH_NORMALIZE) {
		*Fac = len(Vector1);
		*Vector = normalize(Vector1);
	}
	else {
		*Fac = 0.0f;
		*Vector = make_float3(0.0f, 0.0f, 0.0f);
	}
}

/* Nodes */

ccl_device void svm_node_math(KernelGlobals *kg, ShaderData *sd, float *stack, uint itype, uint f1_offset, uint f2_offset, int *offset)
{
	NodeMath type = (NodeMath)itype;
	float f1 = stack_load_float(stack, f1_offset);
	float f2 = stack_load_float(stack, f2_offset);
	float f = svm_math(type, f1, f2);

	uint4 node1 = read_node(kg, offset);

	stack_store_float(stack, node1.y, f);
}

ccl_device void svm_node_vector_math(KernelGlobals *kg, ShaderData *sd, float *stack, uint itype, uint v1_offset, uint v2_offset, int *offset)
{
	NodeVectorMath type = (NodeVectorMath)itype;
	float3 v1 = stack_load_float3(stack, v1_offset);
	float3 v2 = stack_load_float3(stack, v2_offset);
	float f;
	float3 v;

	svm_vector_math(&f, &v, type, v1, v2);

	uint4 node1 = read_node(kg, offset);

	if(stack_valid(node1.y)) stack_store_float(stack, node1.y, f);
	if(stack_valid(node1.z)) stack_store_float3(stack, node1.z, v);
}

CCL_NAMESPACE_END

