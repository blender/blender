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
# The Original Code is Copyright (C) 2018, Blender Foundation
# All rights reserved.
#
# ***** END GPL LICENSE BLOCK *****

set(INC 
	.
	intern
	../blenkernel
	../blenlib
	../blenfont
	../depsgraph
	../makesdna
	../makesrna
	../bmesh
	../render/extern/include
	../../../intern/elbeem/extern
	../../../intern/guardedalloc
	../../../intern/eigen
)

set(INC_SYS
	${ZLIB_INCLUDE_DIRS}
)

set(SRC
	intern/FX_shader_util.h

	intern/FX_shader_util.c
	intern/FX_shader_blur.c
	intern/FX_shader_colorize.c
	intern/FX_shader_flip.c
	intern/FX_shader_light.c
	intern/FX_shader_pixel.c
	intern/FX_shader_rim.c
	intern/FX_shader_swirl.c
	intern/FX_shader_wave.c

	FX_shader_types.h
)

if(WITH_INTERNATIONAL)
	add_definitions(-DWITH_INTERNATIONAL)
endif()

add_definitions(${GL_DEFINITIONS})

blender_add_lib(bf_shader_fx "${SRC}" "${INC}" "${INC_SYS}")
