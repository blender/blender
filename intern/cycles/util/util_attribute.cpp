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

#include "util_attribute.h"

CCL_NAMESPACE_BEGIN

const char *attribute_standard_name(AttributeStandard std)
{
	if(std == ATTR_STD_VERTEX_NORMAL)
		return "N";
	else if(std == ATTR_STD_FACE_NORMAL)
		return "Ng";
	else if(std == ATTR_STD_UV)
		return "uv";
	else if(std == ATTR_STD_GENERATED)
		return "generated";
	else if(std == ATTR_STD_POSITION_UNDEFORMED)
		return "undeformed";
	else if(std == ATTR_STD_POSITION_UNDISPLACED)
		return "undisplaced";
	else if(std == ATTR_STD_MOTION_PRE)
		return "motion_pre";
	else if(std == ATTR_STD_MOTION_POST)
		return "motion_post";
	else if(std == ATTR_STD_PARTICLE)
		return "particle";
	
	return "";
}

CCL_NAMESPACE_END
