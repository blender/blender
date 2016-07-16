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

ccl_device_inline uint subd_triangle_patch(KernelGlobals *kg, const ShaderData *sd);

ccl_device_inline uint attribute_primitive_type(KernelGlobals *kg, const ShaderData *sd)
{
#ifdef __HAIR__
	if(ccl_fetch(sd, type) & PRIMITIVE_ALL_CURVE) {
		return ATTR_PRIM_CURVE;
	}
	else
#endif
	if(subd_triangle_patch(kg, sd) != ~0) {
		return ATTR_PRIM_SUBD;
	}
	else {
		return ATTR_PRIM_TRIANGLE;
	}
}

/* Find attribute based on ID */

ccl_device_inline int find_attribute(KernelGlobals *kg, const ShaderData *sd, uint id, AttributeElement *elem)
{
	if(ccl_fetch(sd, object) == PRIM_NONE)
		return (int)ATTR_STD_NOT_FOUND;

	/* for SVM, find attribute by unique id */
	uint attr_offset = ccl_fetch(sd, object)*kernel_data.bvh.attributes_map_stride;
	attr_offset += attribute_primitive_type(kg, sd);
	uint4 attr_map = kernel_tex_fetch(__attributes_map, attr_offset);
	
	while(attr_map.x != id) {
		if(UNLIKELY(attr_map.x == ATTR_STD_NONE)) {
			return ATTR_STD_NOT_FOUND;
		}
		attr_offset += ATTR_PRIM_TYPES;
		attr_map = kernel_tex_fetch(__attributes_map, attr_offset);
	}

	*elem = (AttributeElement)attr_map.y;
	
	if(ccl_fetch(sd, prim) == PRIM_NONE && (AttributeElement)attr_map.y != ATTR_ELEMENT_MESH)
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

