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
	
	float3 out = make_float3(0.0f, 0.0f, 0.0f);
	
	decode_node_uchar4(node.y, &itype, &ifrom, &ito, NULL);
	decode_node_uchar4(node.z, &vector_in, &vector_out, NULL, NULL);
	
	NodeVectorTransformType type = (NodeVectorTransformType)itype;
	NodeVectorTransformConvertFrom from = (NodeVectorTransformConvertFrom)ifrom;
	NodeVectorTransformConvertTo to = (NodeVectorTransformConvertTo)ito;
	
	float3 vec_in = stack_load_float3(stack, vector_in);
	
	if(stack_valid(vector_out))
		stack_store_float3(stack, vector_out, out);
}

CCL_NAMESPACE_END

