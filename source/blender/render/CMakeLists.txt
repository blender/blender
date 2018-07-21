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
	extern/include
	intern/include
	../blenkernel
	../blenlib
	../blentranslation
	../imbuf
	../depsgraph
	../makesdna
	../makesrna
	../nodes
	../physics
	../draw
	../gpu
	../../../intern/atomic
	../../../intern/guardedalloc
	../../../intern/mikktspace
	../../../intern/smoke/extern
)

set(INC_SYS
	${GLEW_INCLUDE_PATH}
)

set(SRC
	intern/source/bake_api.c
	intern/source/external_engine.c
	intern/source/imagetexture.c
	intern/source/initrender.c
	intern/source/multires_bake.c
	intern/source/pipeline.c
	intern/source/pointdensity.c
	intern/source/render_result.c
	intern/source/render_texture.c
	intern/source/zbuf.c

	extern/include/RE_engine.h
	extern/include/RE_bake.h
	extern/include/RE_multires_bake.h
	extern/include/RE_pipeline.h
	extern/include/RE_render_ext.h
	extern/include/RE_shader_ext.h
	intern/include/initrender.h
	intern/include/render_types.h
	intern/include/render_result.h
	intern/include/renderpipeline.h
	intern/include/texture.h
	intern/include/zbuf.h
)

if(WITH_PYTHON)
	add_definitions(-DWITH_PYTHON)
	list(APPEND INC
		../python
	)
endif()

if(WITH_IMAGE_OPENEXR)
	add_definitions(-DWITH_OPENEXR)
endif()

if(WITH_MOD_SMOKE)
	add_definitions(-DWITH_SMOKE)
endif()

if(WITH_FREESTYLE)
	list(APPEND INC
		../freestyle
	)
	add_definitions(-DWITH_FREESTYLE)
endif()

if(WITH_INTERNATIONAL)
	add_definitions(-DWITH_INTERNATIONAL)
endif()

if(APPLE)
	# SSE math is enabled by default on x86_64
	if(CMAKE_OSX_ARCHITECTURES MATCHES "i386")
		set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -mfpmath=sse")
		set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -mfpmath=sse")
	endif()
endif()

blender_add_lib_nolist(bf_render "${SRC}" "${INC}" "${INC_SYS}")
