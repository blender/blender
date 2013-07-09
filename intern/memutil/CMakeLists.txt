# ***** BEGIN GPL LICENSE BLOCK *****
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# The Original Code is Copyright (C) 2006, Blender Foundation
# All rights reserved.
#
# The Original Code is: all of this file.
#
# Contributor(s): Jacques Beaurain.
#
# ***** END GPL LICENSE BLOCK *****

set(INC
	.
	..
)

set(INC_SYS

)

set(SRC
	intern/MEM_CacheLimiterC-Api.cpp
	intern/MEM_RefCountedC-Api.cpp

	MEM_Allocator.h
	MEM_CacheLimiter.h
	MEM_CacheLimiterC-Api.h
	MEM_NonCopyable.h
	MEM_RefCountPtr.h
	MEM_RefCounted.h
	MEM_RefCountedC-Api.h
	MEM_SmartPtr.h
)

blender_add_lib(bf_intern_memutil "${SRC}" "${INC}" "${INC_SYS}")
