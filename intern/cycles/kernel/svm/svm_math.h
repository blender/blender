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

__device float safe_asinf(float a)
{
	if(a <= -1.0f)
		return -M_PI_2_F;
	else if(a >= 1.0f)
		return M_PI_2_F;

	return asinf(a);
}

__device float safe_acosf(float a)
{
	if(a <= -1.0f)
		return M_PI_F;
	else if(a >= 1.0f)
		return 0.0f;

	return acosf(a);
}

__device float safe_powf(float a, float b)
{
	if(b == 0.0f)
		return 1.0f;
	if(a == 0.0f)
		return 0.0f;
	if(a < 0.0f && b != (int)b)
		return 0.0f;
	
	return powf(a, b);
}

__device float safe_logf(float a, float b)
{
	if(a < 0.0f || b < 0.0f)
		return 0.0f;

	return logf(a)/logf(b);
}

__device float safe_divide(float a, float b)
{
	float result;

	if(b == 0.0f)
		result = 0.0f;
	else
		result = a/b;
	
	return result;
}

__device float svm_math(NodeMath type, float Fac1, float Fac2)
{
	float Fac;

	if(type == NODE_MATH_ADD)
		Fac = Fac1 + Fac2;
	else if(type == NODE_MATH_SUBTRACT)
		Fac = Fac1 - Fac2;
	else if(type == NODE_MATH_MULTIPLY)
		Fac = Fac1*Fac2;
	else if(type == NODE_MATH_DIVIDE)
		Fac = safe_divide(Fac1, Fac2);
	else if(type == NODE_MATH_SINE)
		Fac = sinf(Fac1);
	else if(type == NODE_MATH_COSINE)
		Fac = cosf(Fac1);
	else if(type == NODE_MATH_TANGENT)
		Fac = tanf(Fac1);
	else if(type == NODE_MATH_ARCSINE)
		Fac = safe_asinf(Fac1);
	else if(type == NODE_MATH_ARCCOSINE)
		Fac = safe_acosf(Fac1);
	else if(type == NODE_MATH_ARCTANGENT)
		Fac = atanf(Fac1);
	else if(type == NODE_MATH_POWER)
		Fac = safe_powf(Fac1, Fac2);
	else if(type == NODE_MATH_LOGARITHM)
		Fac = safe_logf(Fac1, Fac2);
	else if(type == NODE_MATH_MINIMUM)
		Fac = fminf(Fac1, Fac2);
	else if(type == NODE_MATH_MAXIMUM)
		Fac = fmaxf(Fac1, Fac2);
	else if(type == NODE_MATH_ROUND)
		Fac = floorf(Fac1 + 0.5f);
	else if(type == NODE_MATH_LESS_THAN)
		Fac = Fac1 < Fac2;
	else if(type == NODE_MATH_GREATER_THAN)
		Fac = Fac1 > Fac2;
	else if(type == NODE_MATH_CLAMP)
		Fac = clamp(Fac1, 0.0f, 1.0f);
	else
		Fac = 0.0f;
	
	return Fac;
}

__device float average_fac(float3 v)
{
	return (fabsf(v.x) + fabsf(v.y) + fabsf(v.z))/3.0f;
}

__device void svm_vector_math(float *Fac, float3 *Vector, NodeVectorMath type, float3 Vector1, float3 Vector2)
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

__device void svm_node_math(KernelGlobals *kg, ShaderData *sd, float *stack, uint itype, uint f1_offset, uint f2_offset, int *offset)
{
	NodeMath type = (NodeMath)itype;
	float f1 = stack_load_float(stack, f1_offset);
	float f2 = stack_load_float(stack, f2_offset);
	float f = svm_math(type, f1, f2);

	uint4 node1 = read_node(kg, offset);

	stack_store_float(stack, node1.y, f);
}

__device void svm_node_vector_math(KernelGlobals *kg, ShaderData *sd, float *stack, uint itype, uint v1_offset, uint v2_offset, int *offset)
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

