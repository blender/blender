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
# The Original Code is Copyright (C) 2012, Blender Foundation
# All rights reserved.
#
# The Original Code is: all of this file.
#
# Contributor(s): Sergey Sharybin.
#
# ***** END GPL LICENSE BLOCK *****

set(INC
	.
	../glew-mx
	../guardedalloc
	../../source/blender/blenlib
)

set(INC_SYS

)

set(SRC
	ocio_capi.cc
	fallback_impl.cc

	ocio_capi.h
	ocio_impl.h
)

if(WITH_OPENCOLORIO)
	add_definitions(
		-DWITH_OCIO
	)

	add_definitions(${GL_DEFINITIONS})

	list(APPEND INC_SYS
		${OPENCOLORIO_INCLUDE_DIRS}
		${GLEW_INCLUDE_PATH}
	)

	list(APPEND SRC
		ocio_impl.cc
		ocio_impl_glsl.cc
	)

	if(WIN32 AND NOT MINGW)
		list(APPEND INC_SYS
			${BOOST_INCLUDE_DIR}
		)
	endif()

	data_to_c_simple(gpu_shader_display_transform.glsl SRC)
endif()


blender_add_lib(bf_intern_opencolorio "${SRC}" "${INC}" "${INC_SYS}")
