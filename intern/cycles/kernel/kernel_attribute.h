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

#ifndef __KERNEL_ATTRIBUTE_CL__
#define __KERNEL_ATTRIBUTE_CL__

#include "util_types.h"

#ifdef __OSL__
#include <string>
#include "util_attribute.h"
#endif

CCL_NAMESPACE_BEGIN

/* note: declared in kernel.h, have to add it here because kernel.h is not available */
bool kernel_osl_use(KernelGlobals *kg);

__device_inline int find_attribute(KernelGlobals *kg, ShaderData *sd, uint id)
{

#ifdef __OSL__
	if (kernel_osl_use(kg)) {
		/* for OSL, a hash map is used to lookup the attribute by name. */
		OSLGlobals::AttributeMap &attr_map = kg->osl.attribute_map[sd->object];
		ustring stdname(std::string("std::") + std::string(attribute_standard_name((AttributeStandard)id)));
		OSLGlobals::AttributeMap::const_iterator it = attr_map.find(stdname);
		if (it != attr_map.end()) {
			const OSLGlobals::Attribute &osl_attr = it->second;
			/* return result */
			return (osl_attr.elem == ATTR_ELEMENT_NONE) ? (int)ATTR_STD_NOT_FOUND : osl_attr.offset;
		}
		else
			return (int)ATTR_STD_NOT_FOUND;
	}
	else
#endif
	{
		/* for SVM, find attribute by unique id */
		uint attr_offset = sd->object*kernel_data.bvh.attributes_map_stride;
		uint4 attr_map = kernel_tex_fetch(__attributes_map, attr_offset);
		
		while(attr_map.x != id)
			attr_map = kernel_tex_fetch(__attributes_map, ++attr_offset);
		
		/* return result */
		return (attr_map.y == ATTR_ELEMENT_NONE) ? (int)ATTR_STD_NOT_FOUND : attr_map.z;
	}
}

CCL_NAMESPACE_END

#endif /* __KERNEL_ATTRIBUTE_CL__ */
