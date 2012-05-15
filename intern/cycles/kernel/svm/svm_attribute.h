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

/* Attribute Node */

__device void svm_node_attr_init(KernelGlobals *kg, ShaderData *sd,
	uint4 node, NodeAttributeType *type,
	NodeAttributeType *mesh_type, AttributeElement *elem, int *offset, uint *out_offset)
{
	if(sd->object != ~0) {
		/* find attribute by unique id */
		uint id = node.y;
		uint attr_offset = sd->object*kernel_data.bvh.attributes_map_stride;
		uint4 attr_map = kernel_tex_fetch(__attributes_map, attr_offset);

		while(attr_map.x != id)
			attr_map = kernel_tex_fetch(__attributes_map, ++attr_offset);

		/* return result */
		*elem = (AttributeElement)attr_map.y;
		*offset = as_int(attr_map.z);
		*mesh_type = (NodeAttributeType)attr_map.w;
	}
	else {
		/* background */
		*elem = ATTR_ELEMENT_NONE;
		*offset = 0;
		*mesh_type = (NodeAttributeType)node.w;
	}

	*out_offset = node.z;
	*type = (NodeAttributeType)node.w;
}

__device void svm_node_attr(KernelGlobals *kg, ShaderData *sd, float *stack, uint4 node)
{
	NodeAttributeType type, mesh_type;
	AttributeElement elem;
	uint out_offset;
	int offset;

	svm_node_attr_init(kg, sd, node, &type, &mesh_type, &elem, &offset, &out_offset);

	/* fetch and store attribute */
	if(type == NODE_ATTR_FLOAT) {
		if(mesh_type == NODE_ATTR_FLOAT) {
			float f = triangle_attribute_float(kg, sd, elem, offset, NULL, NULL);
			stack_store_float(stack, out_offset, f);
		}
		else {
			float3 f = triangle_attribute_float3(kg, sd, elem, offset, NULL, NULL);
			stack_store_float(stack, out_offset, average(f));
		}
	}
	else {
		if(mesh_type == NODE_ATTR_FLOAT3) {
			float3 f = triangle_attribute_float3(kg, sd, elem, offset, NULL, NULL);
			stack_store_float3(stack, out_offset, f);
		}
		else {
			float f = triangle_attribute_float(kg, sd, elem, offset, NULL, NULL);
			stack_store_float3(stack, out_offset, make_float3(f, f, f));
		}
	}
}

__device void svm_node_attr_bump_dx(KernelGlobals *kg, ShaderData *sd, float *stack, uint4 node)
{
	NodeAttributeType type, mesh_type;
	AttributeElement elem;
	uint out_offset;
	int offset;

	svm_node_attr_init(kg, sd, node, &type, &mesh_type, &elem, &offset, &out_offset);

	/* fetch and store attribute */
	if(type == NODE_ATTR_FLOAT) {
		if(mesh_type == NODE_ATTR_FLOAT) {
			float dx;
			float f = triangle_attribute_float(kg, sd, elem, offset, &dx, NULL);
			stack_store_float(stack, out_offset, f+dx);
		}
		else {
			float3 dx;
			float3 f = triangle_attribute_float3(kg, sd, elem, offset, &dx, NULL);
			stack_store_float(stack, out_offset, average(f+dx));
		}
	}
	else {
		if(mesh_type == NODE_ATTR_FLOAT3) {
			float3 dx;
			float3 f = triangle_attribute_float3(kg, sd, elem, offset, &dx, NULL);
			stack_store_float3(stack, out_offset, f+dx);
		}
		else {
			float dx;
			float f = triangle_attribute_float(kg, sd, elem, offset, &dx, NULL);
			stack_store_float3(stack, out_offset, make_float3(f+dx, f+dx, f+dx));
		}
	}
}

__device void svm_node_attr_bump_dy(KernelGlobals *kg, ShaderData *sd, float *stack, uint4 node)
{
	NodeAttributeType type, mesh_type;
	AttributeElement elem;
	uint out_offset;
	int offset;

	svm_node_attr_init(kg, sd, node, &type, &mesh_type, &elem, &offset, &out_offset);

	/* fetch and store attribute */
	if(type == NODE_ATTR_FLOAT) {
		if(mesh_type == NODE_ATTR_FLOAT) {
			float dy;
			float f = triangle_attribute_float(kg, sd, elem, offset, NULL, &dy);
			stack_store_float(stack, out_offset, f+dy);
		}
		else {
			float3 dy;
			float3 f = triangle_attribute_float3(kg, sd, elem, offset, NULL, &dy);
			stack_store_float(stack, out_offset, average(f+dy));
		}
	}
	else {
		if(mesh_type == NODE_ATTR_FLOAT3) {
			float3 dy;
			float3 f = triangle_attribute_float3(kg, sd, elem, offset, NULL, &dy);
			stack_store_float3(stack, out_offset, f+dy);
		}
		else {
			float dy;
			float f = triangle_attribute_float(kg, sd, elem, offset, NULL, &dy);
			stack_store_float3(stack, out_offset, make_float3(f+dy, f+dy, f+dy));
		}
	}
}

CCL_NAMESPACE_END

