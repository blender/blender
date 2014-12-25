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

/* Attributes
 *
 * We support an arbitrary number of attributes on various mesh elements.
 * On vertices, triangles, curve keys, curves, meshes and volume grids.
 * Most of the code for attribute reading is in the primitive files.
 *
 * Lookup of attributes is different between OSL and SVM, as OSL is ustring
 * based while for SVM we use integer ids. */

/* Find attribute based on ID */

ccl_device_inline int find_attribute(KernelGlobals *kg, const ShaderData *sd, uint id, AttributeElement *elem)
{
	if(sd->object == PRIM_NONE)
		return (int)ATTR_STD_NOT_FOUND;

	/* for SVM, find attribute by unique id */
	uint attr_offset = sd->object*kernel_data.bvh.attributes_map_stride;
#ifdef __HAIR__
	attr_offset = (sd->type & PRIMITIVE_ALL_CURVE)? attr_offset + ATTR_PRIM_CURVE: attr_offset;
#endif
	uint4 attr_map = kernel_tex_fetch(__attributes_map, attr_offset);
	
	while(attr_map.x != id) {
		attr_offset += ATTR_PRIM_TYPES;
		attr_map = kernel_tex_fetch(__attributes_map, attr_offset);
	}

	*elem = (AttributeElement)attr_map.y;
	
	if(sd->prim == PRIM_NONE && (AttributeElement)attr_map.y != ATTR_ELEMENT_MESH)
		return ATTR_STD_NOT_FOUND;

	/* return result */
	return (attr_map.y == ATTR_ELEMENT_NONE) ? (int)ATTR_STD_NOT_FOUND : (int)attr_map.z;
}

/* Transform matrix attribute on meshes */

ccl_device Transform primitive_attribute_matrix(KernelGlobals *kg, const ShaderData *sd, int offset)
{
	Transform tfm;

	tfm.x = kernel_tex_fetch(__attributes_float3, offset + 0);
	tfm.y = kernel_tex_fetch(__attributes_float3, offset + 1);
	tfm.z = kernel_tex_fetch(__attributes_float3, offset + 2);
	tfm.w = kernel_tex_fetch(__attributes_float3, offset + 3);

	return tfm;
}

CCL_NAMESPACE_END

